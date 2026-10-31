# Local English/Vietnamese protection

Implemented after the owner selected automatic local English protection with a
manual override, then requested a fast machine-learning prediction score.

## What is implemented

After each composing key Bami compares the original input keys and the Vietnamese
composition, and applies the selected interpretation to the preview/application. It prefers English when the local dictionary recognizes the keys,
or when the Telex classifier score reaches the embedded threshold (currently
0.8000000119). Otherwise it keeps the engine's normal result, including the
existing optional non-Vietnamese restoration policy.

**Shift+Backspace** commits the other candidate without adding a space. It works
with both dictionary and ML decisions. The shortcut applies to the active word,
not arbitrary previously committed text. It reads the raw Shift modifier because
Fcitx 5.0.14 normalizes Shift away from Backspace.

- English dictionary: system Enchant 2, default `en_US`, configurable by tag.
- ML: embedded 16,392-byte hashed character-ngram logistic classifier.
- Inference: pure Go, no allocations in the standalone scorer, no network calls,
  Python process, external ML runtime, kernel module, or custom daemon.
- Model scope: ASCII alphabetic words of 4–32 bytes in Telex-family layouts.
  Other inputs/layouts get a neutral score; dictionary protection still applies.
- Dictionary scope: alphabetic ASCII words up to 128 bytes with a vowel. This
  avoids treating the English abbreviation `dd` as a reason to prevent Telex `đ`.
- `ProtectEnglish` defaults on; `EnglishModel` defaults on and can be disabled
  independently for dictionary-only behavior. Missing dictionaries log a warning
  and leave the model/manual override usable.
- URL/email fields are passed through when the application supplies those Fcitx
  hints. Password/sensitive fields retain their existing bypass.
- No typing history is written to disk or sent to a service. Raw composition keys
  are cleared on reset/commit/configuration. Explicit corrections may remain in
  bounded input-context memory, as described below.

The preview now follows the selected language. When Fcitx/client auto-commit
runs, it sees the already selected English word rather than the old Vietnamese
preview. Native Wayland delivery can still discard a pending preedit during
focus changes; the measured fallback and failures are in LIMITS.md. Backspace removes one displayed English key when
the raw candidate is visible; Vietnamese compositions retain composed-character
Backspace. Returning to Vietnamese after an English Backspace rebuilds the
alternative from the remaining keys.

With `LearnCorrections` (default on), Shift+Backspace also remembers the chosen
language for that exact spelling in the current input context. For example, an
explicit correction can teach the low-score identifier `nginx`, or make `as`
prefer Vietnamese `á` next time. At most 256 entries, each with at most 128 bytes
of raw keys, are retained; new entries evict an existing entry at the limit. This
is bounded feedback adaptation layered over the fixed ML classifier, not online
retraining of the model weights. It stays in memory, is isolated between input
contexts, and is cleared when settings change or the context is destroyed.
Disable `LearnCorrections` to keep every decision independent.

`PreeditOnly` (default off) forbids all surrounding-text deletions. Enable it for
applications that fail the faster-typing stress test; it retains prediction and
feedback learning. Format controls, combining marks, ideographic letters, and
malformed UTF-8 no longer count as evidence for a safe surrounding-text edit.

## Training and reproducibility

Training code: `tools/train_english.py` (Python 3 + NumPy, training only).
Synthetic Vietnamese generator: `tools/vietnamese-corpus/main.go` (Go 1.18+).
Model and validation manifest: `engine/models/english-v1.bin` and
`engine/models/english-v1.json`.

English data is the alphabetic base-word set from the system's SCOWL-derived
size-60 `en_US.dic`. Hunspell affixes are not expanded. Vietnamese examples are
synthetic Telex spellings generated from the vendored MIT bamboo spelling tables,
filtered through the core's syllable validity check. These are spelling-rule
examples, not a Vietnamese natural-language corpus.

There are 56,655 training examples. Hash splits allocate approximately 80% to
training, 10% to validation, and 10% to testing. Tone variants of a Vietnamese
syllable stay together so the same syllable family does not leak across splits.
237 spellings that occur in both classes are excluded from ML evaluation because
the spelling alone cannot supply a single correct label. Dictionary English-first
policy and the manual override handle such cases explicitly.

Features use unsigned FNV-1a hashes of boundary-marked character 2/3/4-grams
into 4096 buckets, normalized by the square root of feature count. Training uses
class-balanced logistic loss, L2 regularization, deterministic seed 7, and 30 SGD
epochs. The threshold uses validation data with a conservative floor of 0.8 and
a 99.9th-percentile Vietnamese score guard. The reported test metrics use the
exported float32 weights. Scores are **not calibrated probabilities of intent**.

Rebuild using the same input hashes to reproduce this model (NumPy/platform
versions may affect final floating-point details):

```sh
go run ./tools/vietnamese-corpus > /tmp/bami-vietnamese.tsv
python3 tools/train_english.py \
  --english /usr/share/hunspell/en_US.dic \
  --vietnamese /tmp/bami-vietnamese.tsv \
  --output engine/models
```

The manifest records source SHA-256 hashes, the model hash, split sizes, threshold,
and cross-language inference probes. English attribution and the full source
notice are retained in `THIRD_PARTY_NOTICES.md`; the dictionary itself is not
bundled. Normal builds use the embedded artifact and require no training tools.

## Measured results

| Check | Result |
| --- | --- |
| Validation English detection | 79.78% of 4,432 English examples |
| Validation Vietnamese false English | 1 of 2,627 examples (0.038%) |
| Test English detection | 79.20% of 4,352 English examples |
| Test Vietnamese false English | 0 of 2,751 examples |
| Pure scorer benchmark, Linux arm64 host | 119.2 ns/op, 0 B/op, 0 allocations/op |

These are development dataset results, not end-user accuracy or a zero-error
guarantee. The latency measures the scorer for `typescript`, excluding C ABI,
dictionary loading/checking, application event delivery, and text replacement.
For example, `typescript` scores 0.8301 and `vieetj` 0.0669; `nginx` scores only
0.1166, demonstrating a real unsupported-English case. The override remains
necessary. English dictionary matches are an independent signal and are not
included in the classifier accuracy figures.

Go tests compare exported-model inference against trainer probes, verify the
model hash and threshold, check unsupported inputs/layouts, and assert zero
allocations. Composer tests cover English protection, manual override in both
directions, disabled/missing dictionaries, ML-only correction, dictionary-only
mode, XIM/no-delete behavior, stale edits, normal flush, and IM switching. C ABI
tests check raw-string ownership, scores, and invalid handles. The real Fcitx
instance test exercises Shift+Backspace routing and structured field bypass.
Ubuntu 22.04/Fcitx 5.0.14/Go 1.18 and Debian 13/Fcitx 5.1.12 builds pass CTest.

## Linux modules and other approaches checked

| Component | Evidence and decision |
| --- | --- |
| Fcitx spell addon | Its 5.0.14 public API provides dictionary availability and hints, not exact per-word membership. Its Enchant backend calls `suggest`; treating the first suggestion as proof would be incorrect. Use the direct Enchant membership API. |
| Enchant/Hunspell | Already present on the owner's system. `enchant_dict_check` distinguishes a recognized word from a misspelling/error and supports system/personal dictionaries. Reused via dynamic linking. |
| Kernel `uinput`, evdev/libinput | Provide device events/injection, not a language model or application text semantics. They cannot resolve identical bilingual key sequences. No privileged input daemon added. |
| Separate local ML service | A service would use the same text evidence while adding IPC and lifecycle handling. The small trained model fits in the existing Go core and is faster to integrate in process. |
| fastText language identification | Useful to investigate for future sentence context. The official models target 176-language identification rather than selecting an interpretation of isolated Telex input. The 126 MB/917 KB models add a different runtime/artifact; the newer NLLB model also has different licensing. Not adopted for this word-level task. |
| IBus Bamboo / Fcitx alternatives | Changing frameworks does not add missing intent. Do not run multiple input frameworks over the same typing stream or copy another engine's GPL implementation into Bami's MIT code. |

Sources checked:

- [Fcitx 5.0.14 spell public API](https://github.com/fcitx/fcitx5/blob/5.0.14/src/modules/spell/spell_public.h)
  and [Enchant backend](https://github.com/fcitx/fcitx5/blob/5.0.14/src/modules/spell/spell-enchant.cpp).
- [Enchant membership API](https://github.com/rrthomas/enchant/blob/main/lib/enchant.h)
  and [project](https://github.com/rrthomas/enchant).
- [Linux uinput documentation](https://www.kernel.org/doc/html/latest/input/uinput.html).
- [fastText language identification documentation](https://github.com/facebookresearch/fastText/blob/main/docs/language-identification.md).
- [SCOWL/ESDB provenance](https://wordlist.aspell.net/)
  and [copyright](https://github.com/en-wl/wordlist/blob/v2/Copyright).
- [Fcitx field capabilities](https://github.com/fcitx/fcitx5/blob/5.0.14/src/lib/fcitx-utils/capabilityflags.h).

## Limits that remain

The same keys can intentionally mean English `as` or Vietnamese `á`. More context
can improve a prediction but cannot make all names, code, loanwords, and mixed
sentences unambiguous. The dictionary version and personal words also affect
English-first decisions; for example, one tested distro dictionary recognized
`aa`, while another did not. There is no claim that either interpretation is
universally correct.

Displaying the selected candidate while typing fixes the auto-commit language
mismatch. Some native Wayland focus changes still discard pending preedit. A late
or mismatched surrounding-text update can also
prevent a safe replacement; manual override consumes the shortcut without forcing
a delete in that case. Use `PreeditOnly=True` to avoid that protocol dependency.
Those frontend constraints are independent of classifier confidence.

Native GTK4 and Qt6 timing was exercised against a headless labwc compositor;
see [LIMITS.md](LIMITS.md) for the exact results, including an edit-mode stress
failure. Browsers, other compositors, packaged Fedora/Arch builds, prediction
quality on the owner's writing, and deployment to the live desktop remain unverified. Future ML work should collect an explicit, consented evaluation
corpus with names, identifiers, mixed-language text, edited words, and hard
collisions before claiming production accuracy. No automatic collection was added.
