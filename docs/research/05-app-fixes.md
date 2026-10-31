# Per-application fixes for a reliable text channel

Question: what can a user do, without root, uinput or key grabbing, so other
programs give bami surrounding text (edit mode) instead of preedit (underline)?

Method. RAN rows ran bami (diagnostic build, logs `BAMI choose ... mode=`) in Podman:
Xvfb + openbox on Debian 12 (fcitx5 5.0.21) and headless labwc on Debian 13 (fcitx5
5.1.12, GTK 4.18, Qt 6.8.2). Typed `Tieengs Vieetj ddepj quas ` via xdotool/wtype,
read the buffer back. 1 run per cell, so rare flakes are invisible. The host is
KDE/Wayland (KWin), which I did **not** test; fcitx's wiki says KWin text-input-v3 has
quirks. Harness: `scratchpad/fix/` (not in repo). Mode legend: **edit** =
delete-surrounding + commit; **pre** = preedit (underline on Wayland).

## Matrix

| App / toolkit | Setting tried | Result | Run? | Evidence |
|---|---|---|---|---|
| GTK3 X11 (gedit) | `GTK_IM_MODULE=fcitx`, or unset | edit, text correct | RAN | log |
| GTK3 X11 | `GTK_IM_MODULE=xim` | pre (XIM has no surrounding) | RAN | log |
| GTK3 X11 | `gtk-im-context-simple` | no IME at all, raw Latin | RAN | log |
| GTK4 Wayland (labwc) | unset or `GTK_IM_MODULE=wayland` (text-input-v3) | edit, correct (1 of 3 runs showed `Tiêngs`, a wrong char) | RAN | [wiki](https://fcitx-im.org/wiki/Using_Fcitx_5_on_Wayland) |
| GTK4 Wayland | `GTK_IM_MODULE=fcitx` | edit, correct | RAN | wiki |
| Qt6 Wayland (6.8.2) | unset / `QT_IM_MODULE=wayland` | edit, correct | RAN | wiki |
| Qt6 Wayland | `QT_IM_MODULE=fcitx` | edit, but 1 pre fallback (InvalidText) and first char lost once | RAN | wiki |
| Qt5 X11 (kate) | `QT_IM_MODULE=fcitx` | edit, correct | RAN | log |
| Qt5 X11 | unset, or `QT_IM_MODULE=xim` | **no Vietnamese at all** (Qt5 xim plugin absent) | RAN | log |
| Qt5 Wayland | KWin: leave unset (text-input-v2); others `QT_IM_MODULE=fcitx` | not tested | NOT-RAN | wiki |
| Firefox X11 | `GTK_IM_MODULE=fcitx` or unset | mostly edit; 1-3 pre fallbacks per run (Stale/Sticky), text correct | RAN | [IMContextWrapper](https://raw.githubusercontent.com/mozilla/gecko-dev/master/widget/gtk/IMContextWrapper.cpp) handles retrieve/delete-surrounding |
| Firefox X11 | `GTK_IM_MODULE=xim` | pre | RAN | log |
| Firefox Wayland | unset or `=fcitx`, `MOZ_ENABLE_WAYLAND=1` | same mixed edit/pre; first char lost in both runs (possibly harness timing) | RAN | same |
| Chromium X11 | `GTK_IM_MODULE=fcitx`, unset, `--gtk-version=4` | pre only: GTK path exposes no SurroundingText flag (`term=--P`) | RAN | log |
| Chromium X11 | `GTK_IM_MODULE=xim` | pre | RAN | log |
| Chromium Wayland | `--ozone-platform=wayland` plus any of `--enable-wayland-ime`, `--wayland-text-input-version=3`, `--gtk-version=4`, or none | typing works; **pre in all 4** (flag set, text invalid) | RAN | [Chromium source](https://raw.githubusercontent.com/chromium/chromium/main/ui/ozone/platform/wayland/host/wayland_input_method_context.cc) trims and sends surrounding, yet fcitx saw none |
| Electron (VS Code, Slack, Discord, Obsidian, Zalo) | `ELECTRON_OZONE_PLATFORM_HINT=wayland` (Electron 28+) + `--enable-wayland-ime` | same engine as Chromium, so expect pre | NOT-RAN | [PR](https://github.com/electron/electron/issues/39792), [Fedora thread](https://discussion.fedoraproject.org/t/the-fcitx5-chinese-input-method-cannot-be-used-in-the-electron-software/136683) |
| LibreOffice X11 | `SAL_USE_VCLPLUGIN=gtk3` + `GTK_IM_MODULE=fcitx` | edit, correct (3 pre fallbacks) | RAN | log |
| LibreOffice X11 | `qt5` plugin + `QT_IM_MODULE=fcitx` | pre | RAN | log |
| LibreOffice X11 | `gen` (XIM) | pre; **`Việt` came out `việt`** (shift lost, 1 run) | RAN | log |
| Java Swing X11 (JDK 17) | `XMODIFIERS=@im=fcitx` | pre via XIM, correct | RAN | log |
| JetBrains | 2026.1 native Wayland toolkit (`-Dawt.toolkit.name=XToolkit` reverts to XIM) | unknown | NOT-RAN | [JetBrains](https://www.omgubuntu.co.uk/2026/02/intellij-native-wayland-support-2026-1) |
| xterm | `XMODIFIERS=@im=fcitx` | pre (XIM) | RAN | log |
| VTE (xfce4-terminal, GNOME Terminal) | `GTK_IM_MODULE=fcitx` | pre, no S flag; text correct | RAN | [VTE source](https://gitlab.gnome.org/GNOME/vte/-/raw/master/src/vte.cc): `im_retrieve_surrounding` returns false, bug 726191 |
| foot | text-input-v3 | pre, `Terminal` flag | RAN | [foot ime.c](https://codeberg.org/dnkl/foot/raw/branch/master/ime.c): "we never call set_surrounding_text()" |
| kitty | text-input-v3 | no `set_surrounding_text` in source | NOT-RAN | [kitty](https://raw.githubusercontent.com/kovidgoyal/kitty/master/glfw/wl_text_input.c) |
| WezTerm, Alacritty, Ghostty | v3 / GTK IM | no `set_surrounding` found in WezTerm or winit (Alacritty) source; Ghostty not checked past VTE-style stubs | NOT-RAN | [wezterm](https://raw.githubusercontent.com/wez/wezterm/main/window/src/os/wayland/inputhandler.rs) |
| Wine/Proton | `winewayland.drv` or XIM | only handles delete-surrounding, never sets text, so pre | NOT-RAN | [wine](https://gitlab.winehq.org/wine/wine/-/raw/master/dlls/winewayland.drv/wayland_text_input.c) |
| SDL games / Steam | `SDL_IM_MODULE=fcitx` | SDL Wayland code has no surrounding support, so pre | NOT-RAN | wiki, SDL source grep |
| Flatpak | `flatpak override --user --env=GTK_IM_MODULE=fcitx --env=QT_IM_MODULE=fcitx --env=XMODIFIERS=@im=fcitx`; Wayland GTK/Qt use the compositor socket | unverified; reports of override alone failing | NOT-RAN | [OBS thread](https://obsproject.com/forum/threads/flatpak-obs-fcitx5-does-not-working-xwayland.194824) |
| Snap, Android emulators | none found | unknown | NOT-RAN | none |

Reason codes differ between the two images' builds (the older X11 image numbers
them differently); I classified by capability flags (`S` = surrounding text).

## What it means

1. **Fixable by configuration:** GTK3/4 and Qt6 on Wayland (do nothing, or set nothing:
   leave `GTK_IM_MODULE`/`QT_IM_MODULE` unset or `wayland`), GTK on X11
   (`GTK_IM_MODULE=fcitx`), Qt5 on X11 (`QT_IM_MODULE=fcitx` is mandatory; without it
   Qt5 gets nothing), LibreOffice via the gtk3 VCL plugin (`SAL_USE_VCLPLUGIN=gtk3`).
2. **Fixable only partly:** Firefox edits most words but falls back a few times per
   sentence. Treat as "good, not perfect".
3. **Not fixable by user-space config; app limitation:** every Chromium/Electron
   variant tested (flags change whether IME works, not whether surrounding text
   arrives), all VTE terminals, foot, kitty, WezTerm, Alacritty, XIM clients
   (xterm, Java, LibreOffice `gen`), Wine, SDL. Bami must stay on preedit here.
   Underline on Wayland remains; the panel profile (`PreeditOnly=True`,
   `ClientPreedit=False`, see `docs/SESSION.md`) removes the inline underline at the
   cost of a panel.
4. **Make PRD claims narrower:** "edit mode where the application exposes surrounding
   text" (GTK, Qt, Firefox, LibreOffice-gtk3); "preedit or panel for Chromium family,
   terminals, XIM, Wine, SDL."
5. First-character loss appeared in 3 of 12 Wayland runs (qt6 fcitx,
   Firefox x2). Probably typing before the input context focused. Not chased; needs a
   re-test with a longer settle delay before it counts against any app.

## Tooling

**bami-doctor** (prototype, Go stdlib only, `scratchpad/doctor/`, 3 unit tests pass:
plans per session type, apply/revert leaves foreign files, DebugInfo parsing).
- Detects session type, desktop, `fcitx5 --version`, IM env vars, and live contexts from
  `gdbus ... org.fcitx.Fcitx.Controller1.DebugInfo`. Verified real format:
  `IC [id] program:NAME frontend:wayland_v2 cap:72 focus:1`; `cap & 64` is
  SurroundingText. It flags XIM and focused contexts lacking it.
- `--apply [--dry-run]` writes `~/.config/environment.d/60-bami.conf` (X11:
  GTK/QT/XMODIFIERS/SDL=fcitx; KDE Wayland: XMODIFIERS+SDL only; other Wayland adds
  `QT_IM_MODULE=fcitx`) and Chromium-family `*-flags.conf` on Wayland, plus a global
  `flatpak override --user --env`. Files carry a marker; `--revert` deletes only
  marked files and unsets recorded flatpak env. Existing user files are never touched.
- Honest limits: flags files are read only by distro wrappers (Arch/Fedora, not
  plain Debian); the Chromium flags did not buy edit mode in my test; `.desktop`
  overrides and per-app Flatpak IDs are not implemented; the plan table is hard-coded,
  not yet generated from this matrix.

**Profile list (b).** Derive from the RAN rows: one record per app with `env`, `flags`,
`expected_mode`, and `verified` (RAN/NOT-RAN); doctor prints expected mode per detected
program so users learn which apps will underline before they file bugs.
