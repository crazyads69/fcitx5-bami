# fcitx5-bami

A Vietnamese input method for [Fcitx 5](https://fcitx-im.org): Telex (several
variants), VNI, VIQR and the MS layout. MIT licensed. The typing engine is Go
(vendored [bamboo-core](engine/third_party/bamboo-core), patched) linked into a
small C++ addon. English protection uses the system Enchant dictionary and a
small embedded local model; no text is sent to a service.

What makes it different from other Fcitx Vietnamese IMs is how it puts text in
the application. For each keystroke it picks one of two strategies:

* **edit**: delete the previous syllable with `deleteSurroundingText`, then
  commit the new one (no underline, works like a keyboard layout);
* **preedit**: show the syllable as underline-less client preedit and commit it once
  at the word end.

Edit is used only when the application's surrounding text proves it is safe;
anything doubtful downgrades to preedit and stays there. The rules live in
[`src/strategy.cpp`](src/strategy.cpp), a pure function with a table test.

| Situation | Strategy |
|---|---|
| frontend `xim` | preedit, always (never deletes) |
| `Terminal` capability (VTE, kitty, Alacritty) | preedit |
| password / sensitive field | bami is bypassed |
| no valid surrounding text, selection (address-bar autocomplete) | preedit |
| `dbus`, `ibus`, `wayland`, `wayland_v2`, other | edit if all checks below pass |

Checks before an edit: a `SurroundingTextUpdated` arrived since our last edit or
forwarded key; the text before the cursor ends with the syllable we committed;
the cursor equals the recorded cursor plus our delta; the character before the
syllable is a word boundary. Wayland additionally requires the text to be below
the 4000-byte protocol cap, because a rejected delete does not stop the following
commit. One failed check mid-word marks the context unreliable for good (preedit).
At most one delete per key, issued before the commit. There are no timers.

## Build

Needs Fcitx 5.0.14 or newer (Ubuntu 22.04 and later, Debian 12 and later), CMake
3.22, a C++17 compiler, Go 1.18+, Enchant 2 and an English dictionary.

```sh
# Debian / Ubuntu
sudo apt install build-essential cmake extra-cmake-modules pkg-config golang-go \
  libfcitx5core-dev libfcitx5config-dev libfcitx5utils-dev libenchant-2-dev hunspell-en-us \
  fcitx5-modules-dev fcitx5-modules fcitx5-data
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
sudo cmake --install build        # /usr/local by default; use -DCMAKE_INSTALL_PREFIX=/usr for packages
```

The build runs `go build -buildmode=c-archive -trimpath` (offline, no modules to
download) and links the archive into `libbami.so`; the Go symbols are hidden from the
process-wide symbol table. Files installed: `<libdir>/fcitx5/libbami.so`,
`<share>/fcitx5/addon/bami.conf`, `<share>/fcitx5/inputmethod/bami.conf`.

Packaging recipes are in `packaging/`: Debian (`cp -r packaging/debian . && dpkg-buildpackage -us -uc`),
RPM (`fcitx5-bami.spec`), Arch (`arch/fcitx5-bami`, `arch/fcitx5-bami-git`). All run
the tests in their check phase; none ships a udev rule or enables the optional session initializer automatically.

## Use

Restart Fcitx (`fcitx5 -r`), open `fcitx5-configtool`, add **Bami** to the input
method list. Options (Typing method, tone placement, lone `w`, brackets, Quick
Telex, restore non-Vietnamese words, underline the preedit) are on its configure
page; changes apply on the next context event, finishing the current word first
so already typed text is preserved.

Environment variables for the application side (set in your session, e.g.
`~/.config/environment.d/fcitx.conf` or `/etc/environment`):

| Desktop / toolkit | What to set |
|---|---|
| X11 (any WM): XIM and legacy apps | `XMODIFIERS=@im=fcitx` |
| X11 GTK / Qt apps | `GTK_IM_MODULE=fcitx`, `QT_IM_MODULE=fcitx` (needs fcitx5-frontend-gtk / -qt) |
| GNOME on Wayland | none for GTK (text-input-v3); `QT_IM_MODULE=fcitx` for Qt; install the Fcitx 5 GNOME extension or Kimpanel for the panel |
| KDE Plasma on Wayland | System Settings > Virtual Keyboard > Fcitx 5; do not set `GTK_IM_MODULE` / `QT_IM_MODULE` |
| labwc, sway, Wayfire | start `fcitx5` from the session; no GTK/Qt variables on Wayland-native apps; keep `XMODIFIERS` for XWayland |

These are the common settings, not a tested matrix: see "Not verified".

## Mixed English and Vietnamese

English protection is enabled by default. While composing and when committing, Bami prefers the original
keys when the configured English dictionary recognizes them. For Telex layouts,
a local character-ngram model can also recognize English words missing from the
dictionary. Predictions update the displayed candidate as you type. Python
and NumPy are only needed to retrain the bundled model.

Before ending the current word, press **Shift+Backspace** to commit the other
interpretation, without inserting a space. For example:

| Keys | Normal ending | Shift+Backspace |
| --- | --- | --- |
| `as` | `as` | `á` |
| `vieetj` | `việt` | `vieetj` |
| `typescript` | `typescript` | composed Vietnamese interpretation |

Options: **Keep recognized English words unchanged** (`ProtectEnglish`),
**Use local English prediction model (Telex)** (`EnglishModel`), and
**English dictionary** (`EnglishDictionary`, default `en_US`; e.g. `en_GB`).
Disable the model for dictionary-only behavior, or disable English protection
for the original Vietnamese-first behavior. An unavailable dictionary logs a
warning; the local model and manual override remain available.

The preview follows the selected language, so auto-commit sees that choice.
Some native Wayland clients still discard pending preedit on focus changes;
[the tested frontend fallback](docs/LIMITS.md) addresses the observed cases.
Backspace removes one displayed English key or one composed Vietnamese character.
`LearnCorrections` remembers explicit overrides in bounded memory for the current
input context; settings changes clear them. Disable it for independent decisions.
Enable **Use preedit only** (`PreeditOnly=True`) to forbid application-text deletes
in clients with unreliable updates. An unsafe surrounding-text edit is never
forced, even by the override shortcut.
Apps that advertise URL/email fields are passed through without composing.
Unknown identifiers and genuine bilingual collisions still need an explicit
choice. See [the model and Linux integration research](docs/ENGLISH.md).

## Do not run it with fcitx5-bamboo

Both embed a Go runtime. bami refuses to start when `fcitx5-bamboo` is already
loaded (checks the addon name `bamboo` and, with `dlopen(RTLD_NOLOAD)`, a mapped
`libbamboo.so` exporting `EngineProcessKeyEvent`) and logs an error. If bamboo is
loaded after bami it cannot be stopped from here: uninstall or disable one of the
two. The distro packages declare `Conflicts: fcitx5-bamboo`.

## Composition modes and remaining limits

The [panel profile and session initializer](docs/SESSION.md) address the original
five limitations with `PreeditOnly=True` and `ClientPreedit=False`. Composition
appears in Fcitx's panel, needs no surrounding text, does not create application
preedit underlines, and resumes when the same input context regains focus. The
first word works without an initial surrounding-text update. Commit the word
before closing the application; reset/context destruction can cancel it.

For inline/edit mode:

* Safe deletion still requires a fresh, matching surrounding-text update. A
  startup service or ML prediction cannot supply that acknowledgement.
* A temporary stale downgrade can recover after the previous edit is verified
  and a fresh word boundary passes all checks. Confirmed mismatches remain
  downgraded until an explicit input-method switch or context destruction.
* Client preedit styling and focus-out commits belong partly to the toolkit.
  Use panel composition to avoid those client-preedit behaviors.
* English protection can choose the wrong language on a new ambiguous word;
  Shift+Backspace overrides it and can teach subsequent uses in that context.
* If the Go core reports an error, Bami preserves visible preedit where possible,
  logs once, and passes the key through.

## Not verified

GTK4/Qt6 widgets were exercised under headless labwc; the stress tests exposed
native Wayland timing/focus failures and verified an application-specific Fcitx
module/preedit fallback. See [docs/LIMITS.md](docs/LIMITS.md) for exact results.
Chromium, LibreOffice, KWin, GNOME, and Fedora/Arch packages remain unverified.
Unit tests alone cannot reproduce real frontend timing.

## Tests

`ctest` runs five programs: `teststrategy` (every row of the strategy table, no
Fcitx runtime), `testcomposer` (a fake application drives the real Go core: order of
delete and commit, xim never deletes, sticky downgrade, selection, stale text,
terminal, Wayland range, backspace, focus bounce) and `testbami` (the addon in a real
`fcitx::Instance`: `Tieengs Vieetj ddepj` becomes `Tiếng Việt đẹp`) and `testbridge` (C API string ownership, null output
pointers, invalid/freed handles, and C++ header compatibility). `testsessioninit`
checks the bounded, read-only session initializer.

The reliability review, fixes, verification results, and remaining work are in
[`docs/RELIABILITY.md`](docs/RELIABILITY.md).
