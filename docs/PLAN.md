# fcitx5-bami: verified plan (2026-10-30)

Source: PRD (claude.ai doc "fcitx5-bami — PRD", rev 14) checked by two rounds of research agents
against fcitx5, fcitx5-gtk/qt, KWin, labwc, GTK, Qt, Chromium, Mutter, bamboo-core,
fcitx5-lotus, fcitx5-unikey. Unverified items are marked.

## PRD corrections
1. `ProcessKeyEventBatch` is a D-Bus frontend method, not an engine API; exists in 5.0.21 and master. Drop the "5.0.14 lacks it" argument.
2. Library installs to `${CMAKE_INSTALL_LIBDIR}/fcitx5` (Debian: /usr/lib/<triplet>/fcitx5).
3. fcitx5-lotus has no Go/cgo (C++ port of bamboo-core). The Go-runtime conflict is with fcitx5-bamboo only. Drop lotus from Conflicts.
4. fcitx5-lotus is active (v6.0.3, 2026-10-28) and already ships a "Surrounding Text" mode (scan <=15 chars, deleteSurroundingText + commit, no verification, no per-frontend rules; still has timer paths). The PRD problem table must say bami differs by: per-frontend rules, xim exclusion, post-commit verification with sticky preedit downgrade, zero timers.
5. "ibus-lotus archived 17 Jan 2026": UNVERIFIED, repo not found. Remove or source it. "200 ms ack timeout" is slack added to a computed deadline (lotus-state.cpp:35,564).
6. Commit ordering: 5.0.14 sends immediately; 5.0.15-5.1.5 send delete immediately, queue commit; 5.1.6+ batch both. "At most one delete per key, issued first" is safe everywhere.
7. KWin is input-method v1 only (no `wayland_v2`) and sends delete and commit as separate `done`s; labwc is one transaction. Wayland byte offsets are converted from the client's CACHED text.
8. GTK and Qt always underline preedit on native Wayland (not "may").
9. Distro Fcitx: Ubuntu 22.04 5.0.14, 24.04 5.1.7, 26.04 5.1.19; Debian 12 5.0.21, 13 5.1.12. Upstream fcitx5-bamboo needs 5.1.13, so bami must set a low floor and avoid newer APIs (UNVERIFIED: ClientUnfocusCommit/KeyEventOrderFix in 5.0.14).
10. fcitx5-unikey already has PreeditStyle=None (no-underline preedit). F10 is not novel.

## Strategy-rule changes
- Require a SurroundingTextUpdated after the last commit on EVERY frontend (GTK refreshes text on an idle callback; Qt/GTK/Chromium convert bytes from their own cache). Also check cursor == recorded cursor + commit delta, and a word boundary before the syllable.
- Do not gate on the SurroundingText capability flag alone: fcitx5-qt drops it after each key (fcitx5-qt #91).
- Wayland: a failed delete still lets the commit through (Fcitx delegates return silently; Chromium too) = duplicated text. Delete only when bami proves the conversion succeeds.
- R4 conflicts with focus bounce (Chromium X11, Brave GNOME, lotus #486/#610): treat deactivate+activate in one key batch as a bounce, no timer.
- Terminals (VTE, kitty, Alacritty) never give surrounding text -> preedit. Address-bar autocomplete selection -> preedit.
- Coalesce delete+commit in one key handler (lotus #594).

## Engine decision
Reuse bamboo-core (MIT, Go, dependency-free), vendored with notice. Do not copy lotus/unikey (GPL/LGPL).
Patch: new-style Backspace after "việt" yields "vịê" (tone must stay on ê); add Simple Telex; lone-w policy; auto-restore (caller must call RestoreLastWord).
Backspace = remove last composed char (bamboo/unikey semantics), NOT pop raw key. deleteN = runes.
PRD must pick the Telex variant (lone `w`, brackets).

## Build/test/CI
- go.mod floor `go 1.18` (Ubuntu 22.04 1.18, Debian 12 1.19). No slices/maps/min/max/clear. No `toolchain` line. Keep deps zero or vendor.
- CMake: copy fcitx5-bamboo (custom_command go build -buildmode=c-archive -trimpath, IMPORTED target, add_fcitx5_addon).
- Tests: Fcitx test frontend ships in fcitx5-modules(-dev); model on fcitx5-unikey/test/testunikey.cpp. Own FakeIC overrides frontend(), commitStringImpl, deleteSurroundingTextImpl, forwardKeyImpl, updatePreeditImpl and calls destroy() in dtor. Add one regression test per lotus bug class.
- CI: Go job (done, .github/workflows/ci.yml); addon job in debian:12 + ubuntu:24.04 containers with libfcitx5{core,config,utils}-dev fcitx5-modules-dev fcitx5-modules fcitx5-data (UNTESTED). Xvfb+xdotool+xterm e2e = nightly, non-gating. labwc+wtype = spike.
- Packaging: Debian packaging/debian + dh cmake with ctest; Fedora spec; AUR PKGBUILD; Nix flake. No systemd/udev/postinst.

## Milestones (proposed; the PRD body was empty)
- M0 build proof: Go c-archive loads in Fcitx on x86_64+aarch64. (scaffold exists: engine/, bridge/)
- M1 engine + FakeIC: bamboo-core vendored + patches, golden corpus, FakeIC asserts R1-R9 on 5 frontends.
- M2 XFCE/X11: xterm(xim), GTK, Qt; 0 wrong chars at 10/25 cps.
- M3 MATE, LXQt-X11, browsers, LibreOffice; AUR package; F13 diagnostics.
- M4 Wayland (labwc, KDE, GNOME); deb/COPR; v1.0.

## Open decisions for the owner
1. Reuse bamboo-core (MIT) or rewrite? (recommended: reuse)
2. Which Telex variant is "Telex" (lone w -> ư?) and is Simple Telex needed in v1?
3. Declare Ubuntu 22.04 (Fcitx 5.0.14) supported or best-effort?
4. Repo license (MIT/LGPL vs GPL)? Avoid lotus code and its GPL dictionary.

## Round 3 results and decisions (2026-10-30)
Decisions (owner): reuse bamboo-core and fix its defects; support all Telex variants; Ubuntu 22.04 (Fcitx 5.0.14) is SUPPORTED; license MIT.

Done in repo
- engine/third_party/bamboo-core: bamboo-core b2e49a2 + 3 patches (patches/): Backspace tone refresh (vieetj,BS -> việ), lone-w undo (www), opt-in EautoRestore. Tests pass. Word-list corpus test dropped (list of unknown license).
- LICENSE (MIT), THIRD_PARTY_NOTICES.md (vendored dir keeps the full upstream MIT text).
- bridge/: fixed Go export params named `new` (breaks the cgo header in C++ / gcc 11).

Verified on Ubuntu 22.04 (podman, aarch64): addon + Go 1.18 c-archive builds against Fcitx 5.0.14 packages. All needed APIs exist incl. ClientUnfocusCommit, KeyEventOrderFix. `frontendName()` is MISSING in 5.0.14: use strcmp(ic->frontend(), ...). x86_64 not run.
Verified e2e (Xvfb+fcitx5+unikey+xterm, xdotool 10/25 cps): "Tiếng Việt đẹp" correct. X11/XIM only.
labwc 0.9.6 headless exposes input-method-v2 + text-input-v3. Wayfire 0.10.1 with input-method-v1 plugin exposes input-method-v1 + text-input-v3, NOT v2 (so Fcitx uses `wayland` v1 there).
ibus-lotus archive claim: no evidence, remove from PRD.

Telex variants: one rule table + gates (lone w: off/after-letter/everywhere; brackets: same; Quick Telex opt-in). MS layout needs its own variant (brackets swapped). Variants: Telex, Telex 2, Telex W, Simple Telex 1/2, VNI, VIQR, MS, Telex+VNI(+VIQR). Schema draft: scratchpad/telex/schema/schema.go.

Not fixable in the engine: valid-looking English (test1.c -> tét1.c) needs a frontend dictionary check; EautoRestore can wrongly restore valid loanwords missing from spelling tables.

Still TODO
1. CMake + C++ addon (M0); CI addon job in debian:12/ubuntu:22.04/ubuntu:24.04 containers must also compile a C++ include of the cgo header.
2. Engine wrapper in engine/ over bamboo-core (Telex variant table, Backspace, Diff).
3. FakeIC tests R1-R9; focus-bounce rule without timers.
4. Edit the PRD doc with the corrections above (not done; ask owner).
