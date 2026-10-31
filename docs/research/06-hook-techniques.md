# 06 - Hooking into other programs: what helps, what breaks the PRD

Date 2026-10-31. Can bami force reliable input in other programs without uinput, root helpers, fake keys or timers? Prototypes are in the session scratchpad under `scratchpad/hooks/{atspi,shim}`. Tests ran in a Debian 12 container (GTK 3.24, Chromium 156, fcitx5 5.0.21 plus the bami build).

## Short answer

No hook can invent surrounding text an app does not hold, and every injection path
(uinput, libei, XTEST, ptrace) either is forbidden by the PRD or only moves the
failure. The problem is not eliminated by hooking. It is made harmless by never
trusting an unverified edit: **preedit-first with verify-by-readback, per-app learned
mode on disk, launcher/environment fixes for known-bad toolkit paths, and upstream
fixes**. AT-SPI is a useful optional second witness, not a base.

## Ranked table

Scores 1 (worst) to 5 (best). "PRD" = compatible with: no uinput, no root helper, no
fake keys, no timers.

| # | Technique | Effect | Safety | PRD | Effort | Verdict |
|---|-----------|:-:|:-:|:-:|:-:|---------|
| 1 | In-channel two-phase commit and verify-by-readback (section 8) | 4 | 5 | 5 | 2 | Do first; mostly done in PLAN |
| 2 | Per-app learned mode on disk (preedit-only vs surrounding), keyed by program + frontend | 4 | 5 | 5 | 2 | Do first |
| 3 | Launchers, `.desktop` overrides, `environment.d` (section 6) | 4 | 5 | 5 | 1 | Do; ship as opt-in data files |
| 4 | Upstream fixes (section 7) | 5 | 5 | 5 | 4 | Do in parallel; slow |
| 5 | AT-SPI cross-check helper (section 2) | 3 | 4 | 4 | 3 | Optional validator, off by default |
| 6 | LD_PRELOAD shim on GTK set_surrounding (section 1) | 2 | 3 | 4 | 3 | Diagnostic tool only |
| 7 | GTK IM-module / Qt platforminputcontext wrapper | 3 | 2 | 4 | 5 | No: fragile ABI, distro-hostile |
| 8 | Wayland relay / protocol-filtering proxy | 2 | 2 | 4 | 5 | No |
| 9 | Portals (RemoteDesktop + libei) for key injection | 3 | 3 | 1 | 4 | No: is fake key events |
| 10 | X11 XTEST / XIM tricks | 2 | 3 | 1-3 | 2 | No (XTEST = fake keys); XIM already excluded |
| 11 | ptrace / LD_AUDIT / seccomp injection | 2 | 1 | 2 | 5 | No: Flatpak, Yama, anti-cheat, distro policy |
| 12 | uinput / kernel | 5 | 1 | 0 | - | Forbidden (also see docs/SESSION.md) |

## Evidence by technique

### 1. LD_PRELOAD shims (prototype B)

Findings from `shim.c` (about 40 lines, built with plain gcc, loaded into gedit under
fcitx5+bami):

- GTK path: fcitx's GTK module does not import `gtk_im_context_get_surrounding`. The
  toolkit hands text over via `gtk_im_context_set_surrounding`, which the preload
  intercepts (80 calls for typing 20 characters). Clean run: 0 invalid, average
  vetting cost 0.9 microseconds per call.
- Fault injection (cursor off by one, as a buggy toolkit would send): 32 of 80 calls
  were flagged, and GTK emitted `assertion 'cursor_index >= 0 && cursor_index <= len'`
  Criticals without the shim's repair. With repair (clamp to UTF-8 boundary) all 32
  were fixed (average 23 microseconds, dominated by logging). Typed output
  `Tiếng Việt đẹp` was correct in all four runs.
- Limits found: `dlsym(RTLD_NEXT)` returns NULL for libraries the IM module
  `dlopen`s locally; my first version segfaulted gedit on exactly that. Qt cannot be
  done this way because `QInputMethodQueryEvent`/`inputMethodQuery` are C++ virtuals
  (vtable calls skip the PLT). Chromium-on-Wayland talks text-input-v3 directly, so
  there is no GTK/Qt symbol to interpose. Flatpak/Snap apps do not see the preload, and
  `LD_PRELOAD` is dropped for setuid and many sandboxed launches.
- A shim cannot create text the app never exposes (terminals, Chromium canvases); it only rejects bad data, which bami already does. Use as a diagnostic, per app via launcher; a global preload is distro-unacceptable.

### 2. AT-SPI cross-check (prototype A)

`probe.py` finds the focused object that implements `org.a11y.atspi.Text` and reads
caret offset, character count and the 8 characters before the caret. Typed
`xin chao` into each app (X11, container, a11y bus enabled):

| App | Result | Walk to find focus | Per-read p50 / p95 |
|-----|--------|-------------------|--------------------|
| GTK3 Gtk.Entry | caret 8, count 8, text exact | 3.9 ms | 0.08 / 0.11 ms |
| gedit (GtkSourceView) | exact | 69.9 ms | 0.22 / 1.71 ms |
| Chromium 156, `--force-renderer-accessibility`, textarea | exact | 40.8 ms | 0.13 / 0.19 ms |
| xterm | no AT-SPI object | - | - |
| Firefox ESR (GNOME_ACCESSIBILITY=1) | no focused text object found; not diagnosed | - | - |
| Qt5/Qt6 | not run (no test app in image) | - | - |

Reading is cheap once the object is known, and offsets are in characters (not bytes),
and independent of the IME channel. Weaknesses:

- Terminals and Electron without a flag expose nothing; Chromium needs the flag or an enabled a11y bus (performance cost).
- The query is a synchronous D-Bus call; it must be asynchronous or it can reenter/deadlock with fcitx.
- No ordering guarantee between the Wayland/fcitx commit and the AT-SPI view, so a
  mismatch after a commit is ambiguous without a timer. Safe use: compare only when the
  next surrounding-text update arrives (needs no timer), to learn "this app lies".
- Needs the user a11y bus; keep it an optional helper feeding only the on-disk reliability record.

### 3. Wayland-level options

- Protocol relay (wl_proxy/filtering text-input-v3): must sit between every client
  and compositor, needs `WAYLAND_DISPLAY` redirection per app, and breaks sandbox
  socket policy. It cannot add information the client never sent. Rejected.
- Compositor bugs are the real Wayland failures: sway
  [#9191](https://github.com/swaywm/sway/issues/9191) (stale `active_features` drops
  surrounding text for Chromium), sway
  [#8276](https://github.com/swaywm/sway/issues/8276), labwc
  [#3749](https://github.com/labwc/labwc/issues/3749) (surrounding_text not forwarded
  to input-method v2). Fix upstream rather than around.
- Portals: `RemoteDesktop.ConnectToEIS` (libei) gives non-root key injection after a
  user consent dialog (GNOME/KDE). It is exactly fake key events with a permission
  prompt, so it violates the PRD; it also sends keycodes, not text. KDE `fake_input`
  grant has the same shape, see fcitx5
  [#1692](https://github.com/fcitx/fcitx5/issues/1692). Not adopted.

### 4-5. X11, ptrace/LD_AUDIT/seccomp

XIM is excluded; XTEST/xdotool are fake keys (used only to drive tests in my container). Injection needs ptrace permission (Yama), fails in Flatpak/Snap, trips anti-cheat/EDR. Rejected.

### 6. Launchers, `.desktop`, environment.d

Safest and already effective in the repo's own testing (docs/LIMITS.md): per-app
`GTK_IM_MODULE=fcitx` / `QT_IM_MODULE=fcitx` with PreeditOnly passed the 100 cps run.
Ship opt-in drop-ins: `~/.config/environment.d/*.conf` for global defaults,
`~/.local/share/applications/<app>.desktop` overrides (Chromium/Electron
`--enable-wayland-ime --wayland-text-input-version=3`, VS Code
`argv.json`, see vscode [#237061](https://github.com/microsoft/vscode/issues/237061)).
Generate them from the learned-mode file, never overwrite vendor files.

### 7. Upstream bugs to file or patch

| Project | Existing issue | Ask |
|---|---|---|
| fcitx5-qt | [#91](https://github.com/fcitx/fcitx5-qt/issues/91) open: `update()` without a query clears surrounding text | patch: only clear when a query ran |
| fcitx5 | [#1529](https://github.com/fcitx/fcitx5/issues/1529): wayland_v2 reuses stale surrounding text across ICs; [#1676](https://github.com/fcitx/fcitx5/issues/1676) stale capability flags | confirm fixed in released versions |
| sway | [#9191](https://github.com/swaywm/sway/issues/9191), [#9193](https://github.com/swaywm/sway/issues/9193) | review patch |
| labwc | [#3749](https://github.com/labwc/labwc/issues/3749) closed, [#2170](https://github.com/labwc/labwc/issues/2170) Chromium blink | verify fix version in the matrix |
| Electron/Chromium | [electron #53175](https://github.com/electron/electron/issues/53175) preedit rectangle; [vscode #315649](https://github.com/microsoft/vscode/issues/315649) | cursor-rect and surrounding gaps |
| fcitx5-lotus (reference) | [#490](https://github.com/LotusInputMethod/fcitx5-lotus/issues/490), [#610](https://github.com/LotusInputMethod/fcitx5-lotus/issues/610) | same class of bugs; our regression tests mirror them |

### 8. In-channel protocol tricks (no hooking)

All of these keep the PRD intact:

1. **Readback as the confirmation.** After `delete_surrounding_text` + `commit_string`
   + `commit(serial)` (text-input-v3: client replies with a new `surrounding_text`
   then `done(serial)` matching the commit count), accept the edit only if the next
   surrounding text shows the expected suffix. Event-driven, no timer.
2. **Serial discipline.** Ignore surrounding text whose `done` serial is older than
   the last commit; treat deactivate+activate in one batch as a bounce (PLAN R4).
3. **Downgrade, never retry.** On a mismatch, flip that IC to preedit-only and write
   the verdict to the learned-reliability file; no delay loops.
4. **Capability probing.** Treat missing `SurroundingText` capability, content type
   password/terminal and empty-but-valid text as "do not edit". Text-input content hints
   (`ZWP_TEXT_INPUT_V3_CONTENT_HINT_*`) only describe the field; they cannot force
   behaviour, so use them as a risk signal, not a lever.
5. **Per-app learned reliability on disk**, keyed by program, frontend and toolkit
   version: counts of verified/failed edits, decayed, with a floor so a single glitch
   does not permanently downgrade. This replaces per-app delays.

## Recommended shortest path

1. Finish the verify-by-readback state machine and the on-disk learned-mode file (items 1, 2). A failed verification then leaves correct preedit text, never a wrong delete.
2. Ship generated opt-in launcher/environment drop-ins for the top apps (Chromium family, VS Code, Electron apps, Firefox, kitty/alacritty/wezterm, VTE terminals, LibreOffice, Telegram).
3. Upstream: fcitx5-qt #91 patch; confirm sway/labwc forwarding fixes in the test matrix.
4. Later: AT-SPI witness as a separate user helper that only writes the learned file.

## Not done

Firefox and Qt not measured via AT-SPI; X11 only; one run per app, so latency is indicative. No relay, libei or ptrace prototype; those verdicts rest on constraints and sources.
