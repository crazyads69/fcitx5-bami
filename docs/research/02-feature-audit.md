# Feature audit of fcitx5-bami (2026-10-31)

Basis: read-only inspection of the uncommitted tree, built in scratch copies. The PRD text (`prd_clean.txt`) was NOT
available, so F/R numbers are inferred from code comments (R5 = one commit at word end, R7 = Go panics contained,
F8 = Backspace, F10 = no-underline preedit, F13 = diagnostics) and from the task brief. Treat the ID column as
provisional and check it against the PRD.

## 1. Build and test evidence (uncommitted tree, arm64 only; x86_64 never run)

| Env | Go | go vet | go test -race | cmake build | ctest |
|---|---|---|---|---|---|
| ubuntu:22.04 (bami-jammy, Fcitx 5.0.14) | 1.18.1 | clean | engine and bamboo-core ok | ok | 5/5 pass |
| debian:12 (Fcitx 5.0.21) | 1.19.8 | clean | ok | ok | 5/5 pass |
| ubuntu:24.04 (Fcitx 5.1.7) | 1.22.2 | clean | ok | ok | 5/5 pass |
| debian:12 on commit 72e8415 (baseline) | 1.19.8 | clean | ok | ok | 3/3 pass |

ctest programs: teststrategy, testcomposer, testbami, testbridge, testsessioninit. No compile errors, no failing tests,
no race reports. `bridge` and `tools/vietnamese-corpus` have no Go tests (bridge is covered by testbridge.cpp).
The `jammy` image lacks `libenchant-2-dev`; the new hard dependency makes a bare 22.04 build fail until it is installed
(CI and README already list it).

## 2. Traceability (requirement to code to test)

C++ tests are sequential blocks in `main()`, so "test" names the block's leading comment.

| Req (inferred) | Implementation | Test | Status |
|---|---|---|---|
| F1 Telex family (9 variants, gates, Quick Telex) | `engine/variant.go`, `engine/engine.go`, `engine/third_party/bamboo-core` | `TestGoldenTelex*`, `TestGates`, `TestQuickTelex`, `TestCorpusGeneratedFromSpellingRules` | DONE |
| F2 VNI / VIQR / MS | `engine/variant.go`; VIQR escape at `engine/engine.go` (Key) | `TestGoldenVNI/VIQR/MS`, `TestVIQREscape`, `TestMSShiftedTones` | DONE (VIQR stream directives not done, per LIMITS.md) |
| F3 Tone placement old/new, free tone | `engine.go` Options.ToneStyle/FreeTone | `TestToneStyleAndFreeTone` | DONE |
| F4 Spell check, dd anywhere | `Options.SpellCheck/DdAnywhere` | `TestSpellCheckAndDdAnywhere` | DONE |
| F5 IME on/off toggle | none in bami; Fcitx's own trigger key | none | MISSING (relies on Fcitx; no bami-level hotkey or state indicator) |
| F6 Password/sensitive fields bypass | `src/bami.cpp:53`; Url/Email flush `:54-58` | testbami "Structured field hints" (URL/email); password has no test | PARTIAL (password path untested) |
| F7 Edit vs preedit strategy | `src/strategy.cpp:390-431` `decide()`; `src/composer.cpp` `choose()` | `teststrategy` rows; testcomposer blocks 1-7 | DONE |
| F8 Backspace / undo | `engine.go` Backspace and BackspaceRaw; `composer.cpp:427` | `TestBackspaceTable`, `TestCorpusBackspace`, testcomposer block 8; double-key undo `TestUndoAndEscape` | DONE (no cross-word undo; Shift+Backspace only toggles the current word's language) |
| F9 Cursor/arrow keys end word | `bami.cpp:79` flush then forward; `composer.cpp:455` | testcomposer block 11 | PARTIAL (arrows flush correctly; no in-word cursor editing, which is acceptable) |
| F10 No-underline preedit | `config.h:180` `UnderlinePreedit=false`; `composer.cpp:198` | testcomposer panel block | DONE (GTK/Qt Wayland force underline, see LIMITS.md) |
| F11 Macros / text expansion | none | none | MISSING |
| F12 Config UI / options | `src/config.h` (17 options), `bami.cpp:92-101` | testbami reads config; no UI test | DONE via Fcitx config UI (generated from `BamiConfig`); no per-app profile |
| F13 Diagnostics (`tools/bami-diag`) | none. Only `tools/fcitx5-bami-init` (warm-up) and the stale `test/e2e-x11/diag-log.patch` | none | MISSING |
| English protection (dictionary + local model + learned corrections) | `src/english.cpp`, `engine/englishscore.go`, `composer.cpp:285-319`; `engine/models/english-v1.bin` | `TestEnglishModelArtifact`, `TestEnglishScore`, testcomposer English blocks | DONE (model covers Telex/ASCII words only; limits in `docs/ENGLISH.md`) |
| R1 Never delete on XIM / terminal / no text / selection | `strategy.cpp:393-401` | `teststrategy`, testcomposer blocks 3-4 | DONE |
| R2 Freshness (update after last edit) | `strategy.cpp:410` | testcomposer block 5 | DONE |
| R3 Verify text, cursor, word boundary | `strategy.cpp:418-429` | testcomposer block 6, boundary rows | DONE |
| R4 Focus bounce, no timers | `composer.cpp:480-517`, `bami.cpp:42-46` | testcomposer block 9; testbami bounce (real Instance) | DONE |
| R5 One commit at word end | `composer.cpp:371-393` | testcomposer block 3 | DONE |
| R6 One delete per key, before commit | `composer.cpp:341-349` | testcomposer block 1 | DONE |
| R7 Go panics contained | `bridge/bami.go` (recover in every export) | `testbridge.cpp` (invalid and freed handles) | PARTIAL (no panic injection; `composer.cpp` `goError` path never exercised; embedded-model `panic` at package init `englishscore.go:17` is outside any recover) |
| R8 Sticky downgrade, no wrongly retried edits | `composer.cpp:256-283`, `onSurroundingTextUpdated` | testcomposer blocks 5-6, last block | DONE |
| R9 Wayland byte cap and failed-delete duplication | `strategy.cpp:415-416` | testcomposer block 7; GTK4/Qt6 labwc manual runs (LIMITS.md) | PARTIAL (logic done; the harness is not automated, 100 cps lost chars in edit mode) |
| Conflict with fcitx5-bamboo | `bami.cpp:24-32`; Debian control Conflicts | none (no automated test) | PARTIAL |
| Success metric: 0 wrong chars at 10/25 cps on X11 | Earlier manual run with unikey only (PLAN.md); bami X11 e2e runner is incomplete (see 3) | none in CI | PARTIAL |

## 3. Uncommitted inventory (all inside one dirty tree; no commits made)

The edits to `composer.cpp`, `strategy.cpp`, `config.h` mix the English, mid-word fallback and panel tasks and cannot be
split per file. Task labels: A engine gaps and English; B mid-word fallback and Go-error test; C X11 e2e; D Wayland e2e; E packaging, CI, conflict.

| Path | What | Task | State |
|---|---|---|---|
| `engine/engine.go`, `variant.go`, `golden_test.go` | Raw(), BackspaceRaw, VIQR escape, MS shifted tones, rune validation | A | COMPLETE (tests pass) |
| `engine/englishscore.go`, `_test.go`, `models/english-v1.{bin,json}` (16 KB) | embedded classifier | A | COMPLETE |
| `tools/train_english.py`, `tools/vietnamese-corpus/main.go` | model training and corpus generation | A | COMPLETE (script not run in CI; docs point at `/tmp` paths) |
| `bridge/bami.go` | BamiRaw, BackspaceRaw, EnglishScore, EnglishThreshold exports | A, B | COMPLETE |
| `src/english.{h,cpp}`, `config.h`, `composer.*`, `strategy.*`, `bami.cpp` | Enchant dictionary, language policy, learned corrections, sticky recovery, panel profile, PreeditOnly, URL/email/password handling | A, B | COMPLETE and green on 3 distros |
| `test/testbridge.cpp`, `testcomposer.cpp`, `testbami.cpp`, `teststrategy.cpp` | new tests | B | COMPLETE; the Go-error test covers the C ABI only |
| `data/bami-panel.conf`, `data/fcitx5-bami-init.service.in`, `tools/fcitx5-bami-init`, `test/testsessioninit.sh` | panel profile and opt-in warm-up service | A | COMPLETE (tested with a stubbed gdbus) |
| `docs/ENGLISH.md`, `LIMITS.md`, `RELIABILITY.md`, `SESSION.md`, `ENGINE.md`, `README.md`, `THIRD_PARTY_NOTICES.md` | docs | all | COMPLETE; LIMITS/SESSION/ENGLISH cite `/tmp/bami-*` scratch paths |
| `test/e2e-x11/{Containerfile,common.sh,session.sh}` | X11 e2e harness | C | BROKEN: `session.sh` sources `/e2e/apps/$APP.sh` and `/e2e/tests.sh`, but `apps/` and `findings/` are empty and `tests.sh` and `summarize.py` do not exist. `Containerfile` builds into `/tmp/build` |
| `test/e2e-x11/diag-log.patch` | debug logging patch for an older `composer.cpp` | C | stale, do not commit |
| `test/e2e-wayland/{Containerfile.debian13,clients/*.py}` | Debian 13 labwc image, GTK4/Qt6 clients | D | PARTIAL: no driver script, `findings/` empty; results exist only in LIMITS.md prose |
| `test/conflict/amd64-chroot.sh` | qemu amd64 chroot helper | E | PARTIAL (needs a user-supplied `$QEMU`; not wired into CI) |
| `.github/workflows/ci.yml`, `packaging/*`, `CMakeLists.txt`, `src|data|test/CMakeLists.txt` | addon CI job on 3 images, enchant, glib2 and hunspell deps, warm-up service files | E | COMPLETE locally; CI job never run on GitHub; Arch/RPM builds never run |

Do not commit: `test/e2e-x11/diag-log.patch`, empty `apps/` and `findings/` dirs, any `__pycache__` (the repo `.gitignore` does
not list it), and stray `/tmp/bami-*` path mentions unless reworded. No secrets, no large binaries and no build output found.
No TODO/FIXME markers in first-party code. (Note: I briefly created two `__pycache__` dirs via `py_compile` and deleted them.)

## 4. Gaps ranked by user impact

| # | Gap | Impact | Effort |
|---|---|---|---|
| 1 | No candidate or prediction window; the only suggestion is the English-vs-Vietnamese flip and Shift+Backspace has no on-screen hint | High: users cannot see or pick the alternative | M |
| 2 | Edit mode loses characters at 100 cps on Wayland GTK/Qt; no automated gate | High for fast typists; `PreeditOnly` is the workaround | M |
| 3 | Complete X11 e2e (apps, tests.sh) and run it in nightly CI; success metric is currently unproven for bami | High (release gate) | M |
| 4 | F13 `tools/bami-diag`: dump version, frontend, capability flags, last Reason per IC, Fcitx version, bamboo conflict | Medium-High: supportability | S |
| 5 | F5 toggle and on-off indicator inside bami (status area action, hotkey) | Medium | S |
| 6 | F11 macros | Medium (popular in Vietnamese IMs) | M |
| 7 | Per-application profiles (auto PreeditOnly for known-bad apps) and persistent corrections | Medium; corrections currently die with the IC | M |
| 8 | Test coverage holes: password field, CapsLock, numpad (`keySymToUnicode` on KP_ keys unverified), key repeat, Shift handling for uppercase tone keys | Medium: unverified, not known broken | S |
| 9 | Composer-level Go-error injection test; guard `panic` at `englishscore.go:17` (host crash if the model is corrupt) | Low-Medium | S |
| 10 | x86_64, Fedora, Arch, KWin, GNOME, browsers never built or run | Medium (release risk) | L |

Skipped or not verified: all numbers are from arm64 containers; no GUI, Wayland or X11 session was run by me; PRD wording unseen.
