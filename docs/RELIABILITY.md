# Reliability review — 2026-10-31

Scope: one desktop user typing through the Go engine, C ABI, Fcitx addon,
composition strategy, and packaging/build paths. This is a targeted source and
regression-test review, not proof that every defect or application incompatibility
has been eliminated. Existing untracked end-to-end experiments and SELinux files
were left untouched.

## Reproduced and fixed

| Problem | Trigger and previous result | Change |
| --- | --- | --- |
| Backspace committed deleted text | XIM/preedit: type `aa`, Backspace. The old `â` was committed instead of removed. | An empty result from a consumed Backspace clears the word without committing it. |
| Settings changes lost text | Type `aa`, change Telex to VNI, type `e6`. The displayed `â` could disappear in either composition mode. | Finish the visible word before configuring the core, and clear the old composition baseline. Preedit text is committed exactly as displayed. |
| Restore depended on the word-ending key | With restore enabled, `windows` followed by Return/arrow/shortcut kept `windớ`, while Space restored `windows`. | Flush uses the same commit and restoration path as word-ending characters. Edit-mode restoration still requires fresh, matching surrounding text. |
| Invalid cursor accepted outside Wayland | A cached cursor beyond the end of the text could pass the edit strategy on DBus/IBus/other frontends. | Check cursor bounds on every frontend; a mid-word violation makes the context stay in preedit. |
| Production build required test frontend | `BUILD_TESTING=OFF` still required the Fcitx test module. | Find the test frontend only when tests are enabled. |
| CI did not exercise the addon | Go-only CI missed composition, C++ compilation, header, and linking failures. | Add Release build, CTest, and staged installation jobs for Ubuntu 22.04/24.04 and Debian 12. |

The new composer tests failed on the old implementation (six failed assertions)
and passed after the fixes. Cursor tests cover DBus, IBus, and other frontends.
A new C++ C ABI test checks returned-string ownership, null output pointers,
configuration, reset, and recovery from freed/null handles. These checks also run
with Release builds, where standard C assertions would otherwise be disabled.

## Verification

- `go vet ./...`, fresh `go test -count=1 ./...`, and
  `go test -race -count=1 ./...` passed on the host (Linux arm64).
- Ubuntu 22.04 container, Go 1.18.1, GCC 11, Fcitx 5.0.14: addon builds and all
  four CTest programs pass. A tests-disabled build and staged installation pass.
- Debian 13 container, GCC 14, Fcitx 5.1.12, host Go toolchain mounted into the
  container: Release addon build and all four CTest programs pass.
- The new GitHub Actions matrix has been written but has not run on GitHub here.
- Structural indexing reported partial parsing in C++ macros/default parameters;
  the affected source was read directly and caller searches verified in source.

## Remaining limits and concrete next work

1. **The application matrix is only partly verified.** Follow-up tests exercised
   GTK4/Qt6 under headless labwc, found native focus/timing failures, and verified
   a Fcitx module/preedit fallback. See LIMITS.md. Browsers, office applications,
   other compositors, and X11 still need their own timing tests. The running
   desktop addon was not replaced.
2. **Surrounding-text edits cannot be made transactional by this addon.** Fcitx's
   delete API has no success result. A client can reject a delete yet accept a
   later commit; the next surrounding-text check detects the mismatch only after
   that event. Freshness, range checks, and sticky preedit reduce the risk but do
   not establish a guarantee against a broken client. XIM and terminals retain
   preedit. Do not remove these guards to gain edit-mode coverage.
3. **Sticky downgrade and first-word preedit are deliberate safety policies.**
   Missing updates, a mismatch, or a missing initial surrounding-text update
   restrict composition. Restoring edit mode automatically needs verified real
   frontend behavior and a clear recovery rule, rather than a timer guess.
4. **English/Vietnamese ambiguity now has an explicit policy.** Local dictionary
   and ML protection prefer English at word boundaries; Shift+Backspace chooses
   the other interpretation. Both are implemented and tested. Genuine shared
   spellings still need user intent. See [ENGLISH.md](ENGLISH.md) for the research,
   model evaluation, and focus-out restrictions.
5. **VIQR single-character escaping and MS shifted tones are implemented.**
   Layout golden tests cover escaping, reset/Backspace state, and `% ^ & * (`
   equivalence to digits. This does not implement VIQR stream-mode directives or
   every physical Windows keyboard/AltGr behavior.
6. **Preedit rendering belongs partly to the toolkit.** The underline preference
   cannot force every native Wayland toolkit to render text identically.
7. **The bamboo runtime conflict is only partly enforceable in-process.** Packages
   conflict with fcitx5-bamboo and startup checks refuse an already loaded bamboo.
   A later load of another Go-based addon cannot be prevented from this factory.
   Keep only one enabled; do not claim the startup check resolves every load order.
8. **Packaging coverage is incomplete.** Native Fedora/Arch packages and the full
   x86_64 distribution matrix were not built here. The local builds are arm64;
   the new CI matrix supplies x86_64 addon checks when it runs.

## Upstream evidence

- [Fcitx 5.0.14 SurroundingText implementation](https://github.com/fcitx/fcitx5/blob/5.0.14/src/lib/fcitx/surroundingtext.cpp):
  `setText` stores the supplied cursor and marks text valid without checking its
  range. This supports retaining an independent strategy range check.
- [Fcitx input-context API](https://github.com/fcitx/fcitx5/blob/master/src/lib/fcitx/inputcontext.h):
  surrounding-text deletion is a request with a void return, not an acknowledged
  edit transaction.
- [Wayland text-input-v3 protocol source](https://gitlab.freedesktop.org/wayland/wayland-protocols/-/blob/main/unstable/text-input/text-input-unstable-v3.xml):
  surrounding text is UTF-8, bounded to 4000 bytes, and uses byte cursor/anchor
  offsets at the protocol boundary. Bami's strategy works in Fcitx code points;
  frontend conversion and cached client state still matter.

## Follow-up deep review

The follow-up fixes live language preview/focus-out behavior, learns explicit
corrections in bounded input-context memory, adds a preedit-only safety option,
validates UTF-8 and Unicode word boundaries before deletion, and makes the C++
replacement diff linear rather than quadratic. See [LIMITS.md](LIMITS.md) for
the full disposition and real-widget validation results.
