# Review of the user's zip (fcitx-bami.zip: telex/ lm/ ime/ capi/ cmd/)

Method: copied to a scratch dir, `GOFLAGS=-mod=readonly`, Go 1.26.10 linux/arm64 (8 cores), data downloaded by hand with the Makefile's four `curl -sfL` URLs (read first), base.gob built from them. Probe tests were added only to the scratch copy.

## 1. Measured results
- `go build ./...`, `go vet ./...`: clean. `go test -race -count=1 ./...` with `BAMI_MODEL` set: ime/lm/telex ok (9 test funcs total; ime tests skip without BAMI_MODEL, so plain `go test` proves little). `-buildmode=c-shared` builds libbami.so.
- Trainer: 0.43 s, 70 MB RSS; base.gob 2,609,514 B; 70,229 unigrams (46,717 English), 6,736 bigram contexts, 66,486 bigrams.
- Load: `LoadBase` 28.7 ms; `lm.New` (reindex: skeleton sort + classify every word) 174 ms; +20 MB heap. Startup is about 200 ms on this machine, paid synchronously in `bami_init`.
- `go test -bench`: Keystroke 12.9 ms per 16 keys (0.8 ms/key mean, 589 allocs, 2.2 MB per 16 keys); SingleLetter (key+backspace) 2.72 ms/op, 615 KB, 99 allocs.
- Latency probe, 39,000 keys of mixed text: p50 156 us, **p99 5.57 ms, max 21.5 ms**. First key of a word (single-letter prefix): p50 1.5 ms, max 3.0 ms. The 1 ms p99 target is **not met**. Cause: every key calls `Complete` (lm/model.go:280) which scans the index and sorts up to 400 scored candidates (`rank`, :309), plus `reindex`-style slice insertion in `Learn` (:408-413), plus GC from per-key maps.

## 2. Correctness findings
Telex (telex/engine.go, syllable.go). 300k random-string fuzz: 0 panics, no "valid but not viable" violations. Spot checks pass: vieetj, tieengs, truwowngf, hoaf/khoer/thuys (modern), nhwng, chuyeenj, class/string/next/window stay raw, `ss`/`ddd`/`aaa`/`ww` undo works.
- B1 `dd` alone stays "dd" (Decide: `IsValid("đ")` false, so raw). Fine for English "dd", but `ddd`-style words not in the LM are decided only by validity.
- B2 Only one Telex variant (`DefaultOptions`: modern tone, standalone w). No Simple Telex, VNI, brackets `[ ]`, no traditional-tone toggle except per session. No `z` handling beyond tone removal.
- B3 Backspace pops the raw key (engine.go:253), not the last composed char. After `vieetj`+BS you get `viee`->"viê" not bamboo's "việ". Differs from the repo's design (PLAN.md:29).
- B4 `IsViablePrefix` rejects `ie`/`oo`-style prefixes early (`viet` viable=false) so preedit flips from "viet" to raw while composing; no crash but visible flicker for `vie`->`viee`.
- B5 `isWordKey` is ASCII-only (ime/session.go:730): any non-ASCII or digit/punctuation ends the word and is committed by the session (`Key` always returns Commit+rune), so the engine owns punctuation and Shift/AltGr/dead-key input cannot pass through; there is no way to say "not handled" for printable keys.
- B6 `Select` capitalises by raw[0] only; `langOf` returns `lastLang` for shared words (keeps run) - reasonable.

Decide() (session.go:628-654): rules are sound (escape, composed==raw, invalid syllable -> raw, non-English raw -> VI, else LM compare with `StayProb`). Observed: car/best/bar -> English with no context, bets/bas/cas -> Vietnamese, `mix`->"mĩ", `sex`->"sẽ", `six`->"sĩ", `tex`->"tex"; i.e. English words whose Telex reading is a valid syllable (mix, six, sex) lose to Vietnamese unless a prior English word precedes them. The division `Prob/vw` and `/(1-vw)` (:647-648) relies on the base mixing constant being baked into Uni; it works only for base-only probabilities, and user-learned mass (p1 blends user counts) breaks the normalisation silently. `IsEnglish` is a flat set built from a 50k OpenSubtitles list: contains names, slang and misspellings (it contributes to false English).

LM math (lm/model.go).
- Interpolation constants are ad hoc: `0.6*pb+0.4*p`, `0.5*pt+0.5*p` (:213-218); no Kneser-Ney or backoff weights, not normalised (bigram given by `bctx[w]` can be 0, so p falls to 0.4*p1). Floors 1e-12.
- `p2` recomputes `sum(uctx)` on every call (:182) and `p3` `sum(ctx)` (:196): O(context size) per candidate, a major cost.
- User blend `b=min(0.5, n/(n+2000))`, bigram `n/(n+3)`: the user model dominates after 3 observations of a context, so one typo-sentence pollutes predictions.
- `Learn` lower-cases and `isWord` filters, but is called for every commit, including passwords (see 3).
- Thread-safety: Model locks correctly; `Session` is not thread safe (documented nowhere).

Persistence. `save` (:421-435): writes `path+".tmp"` then rename: atomic on same fs but **no fsync**, fixed tmp name (two processes race), no parent `MkdirAll`, mode 0666&umask = **0644 measured** (world-readable). `LoadUser` returns an error on a corrupt/empty file (measured: "unexpected EOF", "EOF") and `bami_init` then returns -2 and the whole engine fails to initialise; there is no quarantine-and-reset. Model size is unbounded (no pruning/cap).
- **`~` is not expanded**: capi passes `C.GoString` straight to `os.Open` (main.go:775-779). Measured: `LoadUser("~/x/user.gob")` returns nil error (looks like a missing file) and `SaveUser("~/x/user.gob")` fails with "open ~/x/user.gob.tmp: no such file". The README example (`~/.local/share/...`) therefore silently never persists unless the caller expands it.

C API (capi/main.go).
- Handles are small sequential ints in a global map (:760-801); stale or reused-after-free ids are tolerated (return 0) but never reused, fine. `with()` drops the global mutex before running (:811-823) and writes `st.out` unsynchronised: concurrent calls on one id race (fcitx calls from one thread, so low risk but untested; `-race` does not cover capi, no tests).
- Memory: `bami_commit/preedit/candidates` return `C.CString` copies that the caller must `bami_free`; three allocations per key, leaked if the addon forgets. `bami_free(NULL)` is safe (free(NULL)). The repo uses cgo.Handle and out-parameters instead.
- **No `recover()` anywhere**: a Go panic in any export (e.g. slice index in `Select`, nil `m.base` map) aborts the whole fcitx5 process. The repo's bridge recovers (bridge/bami.go:87,96).
- `bami_init` called twice replaces the model while existing sessions keep the old pointer: harmless but leaks; no unload.
- `bami_save` is explicit only: crash or SIGKILL loses the session's learning.

## 3. Security and privacy
- **Privacy (high)**: `Session.push` calls `Learn` for every committed word (session.go:699-711) with `Learn: true` by default. There is no password-field, `fcitx::CapabilityFlag::Password/Sensitive`, URL or incognito check; the frontend must not call bami_key, but the API gives no hint. Measured: typing `hunter two P4ssw0rd! zzz` stored `hunter, two, p, ssw, rd, zzz` in user.gob (letter runs, digits dropped but fragments kept) with bigram/trigram context, mode 0644, plaintext gob. No opt-out, no size cap, no wipe API, no encryption.
- Makefile `data` (lines 8-12): `curl -sfL` over HTTPS from raw.githubusercontent.com using `master`/`main` branch URLs: **no checksum pinning, no commit pin**, no `-o` temp then verify; upstream rewrite or force-push changes the model silently. Also `-f` hides HTTP errors only partially (an HTML error body with 200 would be parsed). Mitigation: pin commit SHAs and SHA256 (measured sha256 of today's files: en_50k 5351ff40..., vi_50k 3f5b112e..., Viet74K b83d4930..., en_2grams 9e2ae9e6...).
- Training code is benign (reads local files only); gob decoding of a hostile base.gob/user.gob can allocate arbitrarily (encoding/gob has no size limit on maps) - low risk since files are user-owned.

## 4. Data licenses (checked against upstream READMEs / GitHub license API)
- hermitdave/FrequencyWords: repo `MIT` for code, **CC-BY-SA-4.0 for content** (README lines 24-26); derived from OpenSubtitles (2018), itself of mixed/unclear subtitle copyright. A base.gob built from it is a derivative of CC-BY-SA data: needs attribution and **ShareAlike** (the data file would have to stay CC-BY-SA, and combining with MIT code is allowed only as separate works).
- duyet/vietnamese-wordlist Viet74K: **GPL-2.0** (repo licence; README: "GNU General Public License", credit Ho Ngoc Duc). Compound bigrams from it are derived data: redistributing base.gob built with it conflicts with an MIT distribution.
- orgtre/google-books-ngram-frequency: **CC BY 3.0** (README line 158; Google Books Ngram data is CC BY 3.0): attribution only, MIT-compatible.
- Verdict: **a base.gob built from these three cannot be shipped under MIT.** The Viet74K (GPL) and FrequencyWords vi/en (CC-BY-SA) inputs must be dropped or the model must ship as a separate, clearly-licensed data package. This matches the committed repo's earlier decision to drop the word-list corpus (PLAN.md:57). Not legal advice.

## 5. Comparison with the committed design
Zip does better: (a) session-level VI/EN decision using a language model with a run prior `StayProb`, where the repo uses a 16 KB hashed char-ngram classifier (threshold 0.8) plus Enchant (docs/ENGLISH.md) with no context; (b) candidate list: prefix completion over skeleton index and next-word prediction (no equivalent in repo); (c) `IsViablePrefix` shows raw keys while a word cannot be Vietnamese, with a clean preedit-only contract; (d) pure Go, zero deps, small tests.
Zip lacks: surrounding-text/delete strategy and per-frontend rules (src/strategy.cpp, docs/LIMITS.md), Fcitx addon, Telex variants (repo has 9: engine/variant.go, docs/ENGINE.md:7-24), bamboo-core patches (Backspace tone refresh, lone-w undo, EautoRestore), cgo.Handle and panic recovery, password-field bypass (docs/ENGLISH.md), CI/packaging, race tests on the C layer, Go floor 1.18 (zip needs go 1.22, which breaks Ubuntu 22.04's Go 1.18; PLAN.md:34).

## 6. Merge plan
1. Adopt only `lm/` as `engine/predict` (new package, no change to bamboo-core). Keep `telex/` and `ime/` out: the repo's engine is the Telex source of truth. Port `IsViablePrefix` idea only as a cross-check test.
2. Rewrite lm for Go 1.18 (no `min`/`slices`), precompute context sums, cache candidate score; cap `rank` to 64 candidates and use a trie on skeleton so p99 < 1 ms; build the index at build time into the model instead of `reindex` at load (174 ms). Add a benchmark gate (p99 < 1 ms, load < 50 ms) in CI.
3. Bridge additions (backward compatible): `BamiPredict(h, ctxJson, k, out**char)`, `BamiLearnCommit(h, word)`, `BamiPredictConfigure(h, flags)`, `BamiPredictSave()`; all with the existing recover wrapper and out-params; the Fcitx candidate list is optional and off by default. Feed `EnglishScore` with the LM probability in `Decide` as a second signal rather than replacing Enchant.
4. Data: do not ship the zip's base.gob. Build a model from permissively licensed or self-collected text (public-domain or CC0 Vietnamese corpus, CC BY Google ngrams with attribution in THIRD_PARTY_NOTICES.md). Training script pins commit SHAs and SHA256; the built model lives under `data/` with its own licence file. Revisit Viet74K/FrequencyWords only if the model is a separate CC-BY-SA/GPL package.
5. Privacy controls (must precede enabling learning): learning off by default; never call `Learn` when Fcitx reports Password/Sensitive/Email/URL capability or when the frontend's strategy is in pass-through; per-app deny list; `ClearUserModel` action; hard cap (e.g. 20k words, LRU/decay, min count before persist); file mode 0600 in `$XDG_DATA_HOME/fcitx5/bami/` created with `MkdirAll 0700`; atomic write with unique tmp + fsync + rename; corrupt file moves to `user.gob.bad` and engine starts empty; expand `~`/XDG in C++ (never in Go); optional encryption via a user-supplied secret-service key (stretch, not v1).
6. Tests: telex-vs-lm contract tests; golden VI/EN decisions (car/cả, mix, sex/six) with a labelled set; `-race` test on the bridge predict calls; corrupt/empty/huge user.gob fixtures; latency benchmark (p99, p99.9) on x86_64 and arm64; Fcitx FakeIC test asserting no learning in password fields; license check script failing the build if a data file lacks a recorded source/licence.
