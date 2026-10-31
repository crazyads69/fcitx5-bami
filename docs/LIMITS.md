# Remaining limits and verified improvements — 2026-10-31

This follows the source review and the first local ML implementation. It is not a
claim that every application, frontend, keyboard layout, and ambiguous word is
now error-free.

## Implemented in this follow-up

| Limit | Change and check |
| --- | --- |
| Language policy ran too late for automatic focus-out commits | Select English/Vietnamese while composing. The displayed candidate is now the candidate Fcitx/client auto-commit sees. A real Fcitx instance test verifies the server-side focus-out commit. Native frontend delivery restrictions remain below. |
| English Backspace would follow Vietnamese composition | Add `BackspaceRaw` through the Go/C API/composer. When English keys are displayed, Backspace removes one displayed key and rebuilds the Vietnamese alternative. Vietnamese Backspace remains composed-character deletion. Tests cover both modes and empty words. |
| Repeated ambiguous words and low-score identifiers | `LearnCorrections` remembers explicit Shift+Backspace choices in the current input context. Tests teach Vietnamese `as` → `á`, reverse that choice, and teach English `nginx`, which the fixed model misses. No automatic model-weight retraining or persistent typing collection. |
| Unbounded correction history would consume memory | Limit feedback to 256 entries and 128-byte keys, isolated between input contexts, cleared on settings changes/destruction; independent-decision mode is available. |
| Clients with late/missing surrounding text | `PreeditOnly=True` never asks the application to delete text. It retains local dictionary/ML selection and correction learning. A healthy-DBus fake application test verifies zero deletes. |
| Invalid text could justify an edit | Validate UTF-8 before deletion evidence: reject truncation, invalid continuations, overlong encodings, surrogates, out-of-range code points, and embedded NUL. Malformed mid-word text forces sticky preedit. |
| Some Unicode letters/format characters counted as punctuation | Exclude joining/format controls, CJK iteration letters and tone marks from word boundaries. Regression tests cover the affected ranges. |
| C++ replacement diff repeatedly scanned/copied the prefix | Use one byte-prefix scan, rewind to a UTF-8 character boundary, count only the removed suffix. This makes the helper linear. A long-prefix composer test verifies the replacement and single-delete rule. |
| Unnecessary prediction work | Skip dictionary/ML checks when raw and composed text already agree; skip preview raw-string calls when protection is disabled. The unchanged standalone model remains about 0.12 microseconds per tested prediction with zero allocations. |
| VIQR escaping absent | Add single-character backslash escaping, including escaped tone punctuation, escaped backslash, trailing backslash, Backspace and reset state. This is not the full VIQR stream-mode directive syntax. |
| MS shifted tones absent | Map `% ^ & * (` to the tone actions for `5 6 7 8 9`. Golden tests verify equivalence; no change to the vendored upstream tables. This is not a complete emulation of every Windows AltGr/key-state behavior. |
| Core errors could discard visible preedit | Preserve the displayed preedit on failed key/backspace calls using the same finish-visible-word path as configuration changes. Panic injection was not added; configuration preservation and C API invalid-handle containment remain tested. |

## Real-widget verification

Environment: isolated Debian 13 arm64 container, labwc headless compositor,
Fcitx 5.1.12, GTK4 TextView and Qt6 QPlainTextEdit. The existing workspace test
clients were used with temporary focus instrumentation. The desktop installation
was not replaced. Fixed test input combined Vietnamese, English Backspace,
Shift+Backspace, learned Vietnamese corrections, and an unfinished English word
followed by a real application focus change.

- The ordinary native Wayland typing sequence produced `Tiếng Việt đẹp a á á `
  in GTK4 and Qt6 during the initial smoke check.
- Native GTK4 edit-mode typing passed at 12.5 and 25 characters/second in the
  initial stress sequence. At 100 characters/second it produced `Tiêngs` instead
  of `Tiếng`: surrounding-text event timing remains a real constraint.
- Native Wayland focus tests sometimes omitted the pending final `test`, even
  with the selected English preview and preedit-only mode. Focus notifications
  confirmed that the original widget lost and regained focus. These experiments
  do not establish which toolkit/compositor component is at fault.
- With application-specific `GTK_IM_MODULE=fcitx` / `QT_IM_MODULE=fcitx` and
  `PreeditOnly=True`, both widgets passed the complete 100-character/second
  sequence, including the final focus-out commit, in the instrumented runs.
- An early run injected input before explicit focus-readiness checks and lost a
  first character. The harness now waits for active-window notification. This is
  not evidence that arbitrary application startup timing is solved.
- Ubuntu 22.04/Fcitx 5.0.14 and Debian 13/Fcitx 5.1.12 pass all four CTest
  programs. Go vet, fresh Go tests, and race tests pass.

Temporary experiment inputs, operation logs and snapshots are under
local scratch directories
. They are local development artifacts, not a portable CI
suite or a promise of performance on another machine.

## Practical setup for the observed failures

For an affected application, enable **Use preedit only** in Bami configuration.
Then, where appropriate for that desktop/application, test its Fcitx frontend:

```sh
GTK_IM_MODULE=fcitx your-gtk-application
QT_IM_MODULE=fcitx your-qt-application
```

The matching Fcitx GTK/Qt frontend package must be installed. These are
application-specific fallbacks. Do not automatically set them desktop-wide:
[Fcitx's Wayland guidance](https://fcitx-im.org/wiki/Using_Fcitx_5_on_Wayland)
recommends different setups by compositor and warns against global overrides
on KDE Plasma. KDE/KWin and GNOME were not tested here.

## Limits that require different evidence or components

| Remaining issue | Available route and actual limit |
| --- | --- |
| Identical keys with different intended languages | Dictionary + ML + explicit feedback handle many cases. First-use collisions still need context or a user choice. The existing score is not an intent probability. A sentence model needs a licensed, representative mixed-language evaluation corpus before it can be trusted to change more text. |
| Feedback disappears when an input context is destroyed | Deliberate memory-only policy. A persistent personal dictionary/rule file could retain preferences, but would need explicit storage/UI semantics. This change does not collect a writing corpus. |
| Novel names, identifiers and mixed scripts | The model remains a Telex/ASCII word classifier trained on dictionary and synthetic spelling examples. Explicit corrections help within a context; model coverage and holdout figures are unchanged. No blanket real-world accuracy claim. |
| Native Wayland drops preedit or delivers updates late | Use the tested Fcitx module/preedit fallback where supported, or fix the specific frontend/toolkit/compositor. A kernel input module cannot supply missing text transaction semantics. |
| No transactional acknowledgement for delete + commit | Keep freshness/range/boundary guards. Preedit-only avoids that API dependency. A higher ML score cannot justify a stale delete. |
| XIM and terminals cannot safely delete prior text | Preedit remains the supported strategy. Kernel key injection/backspace emulation cannot guarantee Unicode, selections, or cursor behavior across applications. |
| Toolkit forces preedit styling | Rendering is controlled by GTK/Qt and native text-input behavior. Disabling preedit underline in the addon cannot override every client renderer. A client patch or different supported frontend is required. |
| Multiple embedded Go runtimes | Keep the bamboo conflict checks/package conflicts. If simultaneous Go-based addons become a requirement, isolate Bami's Go core in a child process with a bounded local IPC contract and failure recovery. That architecture would address runtime isolation, not language ambiguity; it was not implemented here. |
| Distribution/browser/compositor matrix | Native Fedora/Arch packages, x86_64 builds, Chromium/Electron, Firefox, LibreOffice, KWin and GNOME still require their own tests. Existing CI configuration does not prove those environments have run successfully. |

## Source checks

- [Fcitx 5.0.14 focus-out ordering](https://github.com/fcitx/fcitx5/blob/5.0.14/src/lib/fcitx/instance.cpp):
  server-side preedit commit precedes engine deactivation when the client does
  not claim `ClientUnfocusCommit`.
- [VIQR specification, RFC 1456](https://datatracker.ietf.org/doc/html/rfc1456)
  and [VIQR convention documentation](https://vietstd.sourceforge.net/document/viqr.htm).
- [Unicode CLDR Windows Vietnamese layout](https://www.unicode.org/cldr/cldr-aux/charts/25/keyboards/layouts/vi.html):
  tone positions are retained in the shifted layout.
- [Linux uinput documentation](https://docs.kernel.org/input/uinput.html):
  device event emulation is below application text/language semantics.

Further research on dictionary libraries, local services, language-identification
models and data licensing is in [ENGLISH.md](ENGLISH.md).

## Follow-up: panel profile and initialization service

The five original composition limits now have an opt-in panel profile, verified
stale-update recovery, and a D-Bus runtime warm-up helper/user service. See
[SESSION.md](SESSION.md) for behavior, tradeoffs, installation and tests. Panel
mode also passed native GTK4/Qt6 first-word focus-loss/resume at 100 characters
per second. The earlier client-preedit failures above remain valid historical
results for that presentation mode.
