# The five composition limits and session initialization

The panel profile provides a concrete alternative for the five limits reported
in README. Enable **Use preedit only** and disable **Show composition inside the
application** in Bami settings:

```ini
PreeditOnly=True
ClientPreedit=False
```

The installed reference profile is
`<share>/fcitx5/bami/profiles/bami-panel.conf`. Apply the two settings through the
configuration UI to preserve your existing layout/dictionary preferences.

## What changes for each limit

| Reported limit | Implemented behavior |
| --- | --- |
| Late/missing surrounding updates interrupt composition | Panel/preedit-only composition never calls delete-surrounding and keeps the word in the core until commitment. It does not need surrounding-text updates. Optional edit mode still requires fresh acknowledgements. |
| Mismatch downgrades a context forever | A temporary stale update may recover after a later update proves the last edit succeeded, and a subsequent fresh word boundary passes all checks. A confirmed failed/mismatched edit stays downgraded; explicitly switching away from and back to Bami resets the verdict. No stale cache is marked fresh by the switch. |
| Native Wayland underlines preedit | Panel mode sends empty client preedit and renders composition in Fcitx's own input panel. The application has no pending inline preedit to underline. A working Fcitx UI (classic UI/Kimpanel as appropriate) is required. |
| Preedit ends on a focus bounce | Panel mode retains the unfinished word in its input context, hides the panel while unfocused, and restores it on return. It sends no delayed commit into an unfocused field. This also survives a longer focus loss when the same input context returns. |
| First word waits for initial surrounding text | The first word can be composed in the panel immediately, even with invalid/missing surrounding text. It is committed at a word boundary. Immediate inline editing still requires application text/cursor evidence. |

The presentation tradeoff is deliberate: the unfinished word appears in the
Fcitx panel; committed words appear in the application. This does not promise
inline text after every key with no surrounding-text support. An explicit input
reset, context destruction, or closing the application can cancel an uncommitted
panel word; commit before closing. Existing client-preedit mode keeps the
frontend's automatic focus-out commit behavior.

Correction learning and the local ML model also work in panel mode. ML is not
used to decide whether a stale text deletion is safe or whether a focus-out will
be followed by focus-in. Those are protocol/lifecycle facts, not language labels.

## Optional D-Bus runtime warm-up

`fcitx5-bami-init` waits for `org.fcitx.Fcitx5` on the current user's session bus,
then calls:

```
/controller
org.fcitx.Fcitx.Controller1.GetConfig
fcitx://config/addon/bami
```

The configuration request loads the addon. Bami now initializes its Go runtime
at composer construction, before any per-client state is needed. Its embedded
model and dictionary loader are initialized as part of that startup. An absent
dictionary can still produce its ordinary warning; successful initialization is
not proof that an optional dictionary exists or that an application has supplied
surrounding text.

The helper does not launch/restart Fcitx, select an input method, change config,
read application text, or start another input-method framework. In particular,
KWin must retain its compositor-managed Fcitx startup and Wayland socket.
`BAMI_INIT_TIMEOUT` limits each D-Bus wait/call to 1–30 seconds (default 10).
Missing bus/addon failures return nonzero with an actionable message. GLib's
`gdbus` command is required.

After installing, warm up once with:

```sh
fcitx5-bami-init
```

The optional `fcitx5-bami-init.service` is installed in the systemd user-unit
search path. It uses `Type=exec`, so waiting for Fcitx does not block completion
of the graphical-session target. It is not enabled automatically, including in
the Debian recipe. To opt in after installation:

```sh
systemctl --user daemon-reload
systemctl --user enable --now fcitx5-bami-init.service
```

On desktops without a systemd user manager, the same helper can be called from
the desktop's existing session startup. It does not replace the compositor's
input-method startup mechanism. This development session did not enable the
service or change the live desktop installation.

## Verification

- Ubuntu 22.04/Fcitx 5.0.14 and Debian 13/Fcitx 5.1.12: all five CTest programs
  pass, including the helper's success, timeout, failure, and read-only-call checks.
- Real Fcitx instance: panel composition survives focus-out/focus-in without an
  early commit, then `Tieeng` + resumed `s` commits `Tiếng` once.
- Native Wayland, headless labwc, GTK4 TextView and Qt6 QPlainTextEdit: both pass
  the 100-character/second panel test, including an actual focus loss/return while
  the first word is unfinished, English Backspace, and learned corrections.
- The helper successfully initializes Bami through a real isolated session bus
  before the first test application starts.
- systemd unit syntax verification passes; staged installation contains the
  helper, unit, and profile. No live service activation was performed.
- Regression tests distinguish a delayed successful edit from a rejected delete
  hidden by a stale cache; the latter does not automatically recover.

The native panel experiment logs were local and are not kept.
These tests do not establish compatibility with every application, UI renderer,
or compositor. Earlier inline/client-preedit failures remain recorded in
[LIMITS.md](LIMITS.md).

## Why other modules cannot transparently remove all five limits

The public Fcitx D-Bus controller can initialize/configure an addon; it cannot
manufacture a client's missing surrounding-text acknowledgement. A service that
observes key events cannot guarantee the application's current cursor, selected
text, or acceptance of a delete. Kernel `uinput`/evdev operate below that text
state. A larger ML model adds language guesses, not transaction acknowledgement.

Fcitx's core and some clients commit preedit before the engine's focus-out hook.
By that point, a later service or engine cannot safely undo the commit in every
application. Retaining panel composition avoids that auto-commit path instead of
trying to reverse it after the client changes focus.

Sources:

- [Fcitx D-Bus controller source](https://github.com/fcitx/fcitx5/blob/5.0.14/src/modules/dbus/dbusmodule.cpp)
  for addon configuration loading and controller method signatures.
- [Fcitx focus event contract](https://github.com/fcitx/fcitx5/blob/5.0.14/src/lib/fcitx/event.h)
  for focus-out commit ordering.
- [Qt Fcitx frontend](https://github.com/fcitx/fcitx5-qt/blob/master/qt5/platforminputcontext/qfcitxplatforminputcontext.cpp)
  for client-side preedit commit before a focus-object change.
- [Fcitx Wayland setup guidance](https://fcitx-im.org/wiki/Using_Fcitx_5_on_Wayland)
  for compositor-specific startup and frontend choices.
- [Debian's user-unit helper](https://manpages.debian.org/bookworm/debhelper/dh_installsystemduser.1.en.html)
  for opting out of package-time service enablement.
