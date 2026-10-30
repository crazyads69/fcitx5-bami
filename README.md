# fcitx5-bami

A Vietnamese input method for [Fcitx 5](https://fcitx-im.org): Telex (several
variants), VNI, VIQR and the MS layout. MIT licensed. The typing engine is Go
(vendored [bamboo-core](engine/third_party/bamboo-core), patched) linked into a
small C++ addon.

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
3.22, a C++17 compiler and Go 1.18+.

```sh
# Debian / Ubuntu
sudo apt install build-essential cmake extra-cmake-modules pkg-config golang-go \
  libfcitx5core-dev libfcitx5config-dev libfcitx5utils-dev \
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
the tests in their check phase; none ships a postinst, udev rule or systemd unit.

## Use

Restart Fcitx (`fcitx5 -r`), open `fcitx5-configtool`, add **Bami** to the input
method list. Options (Typing method, tone placement, lone `w`, brackets, Quick
Telex, restore non-Vietnamese words, underline the preedit) are on its configure
page; changes apply from the next keystroke.

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

## Do not run it with fcitx5-bamboo

Both embed a Go runtime. bami refuses to start when `fcitx5-bamboo` is already
loaded (checks the addon name `bamboo` and, with `dlopen(RTLD_NOLOAD)`, a mapped
`libbamboo.so` exporting `EngineProcessKeyEvent`) and logs an error. If bamboo is
loaded after bami it cannot be stopped from here: uninstall or disable one of the
two. The distro packages declare `Conflicts: fcitx5-bamboo`.

## Honest limits

* Edit mode depends on the application sending surrounding text after every change.
  A late or missing update costs that word (preedit) or, mid-word, the rest of the
  composition (the typed prefix stays as plain text, no composition is lost on
  screen, but the tone/mark in progress is not applied). It never deletes on stale data.
* After a mismatch the context stays preedit until the input context is destroyed.
* Preedit mode underlines nothing by default, but toolkits decide: GTK and Qt on native
  Wayland always underline preedit.
* A focus bounce (focus-out and focus-in in the same event-loop turn, seen on
  Chromium/X11 and some GNOME apps) keeps an edit-mode syllable; in preedit mode Fcitx
  itself commits the preedit on focus-out, so the syllable ends there.
* The first word in a new input context is preedit until the application has sent
  surrounding text once.
* Valid-looking English that is also valid Vietnamese typing (`test1.c`) cannot be
  fixed by the engine; the restore option can wrongly restore loanwords.
* If the Go core reports an error, bami logs once and passes that key through.

## Not verified

Edit mode against real applications (GTK, Qt, Chromium, LibreOffice, Wayland
compositors) has not been exercised yet; tests use a fake application and the Fcitx
test frontend. Fedora and Arch recipes have not been built. The unit tests cannot
reproduce real frontend timing.

## Tests

`ctest` runs three programs: `teststrategy` (every row of the strategy table, no
Fcitx runtime), `testcomposer` (a fake application drives the real Go core: order of
delete and commit, xim never deletes, sticky downgrade, selection, stale text,
terminal, Wayland range, backspace, focus bounce) and `testbami` (the addon in a real
`fcitx::Instance`: `Tieengs Vieetj ddepj` becomes `Tiếng Việt đẹp`).
