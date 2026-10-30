// SPDX-License-Identifier: MIT
//
// Composer: glue between one Fcitx InputContext and the Go core. It owns the
// per-context state (cgo handle, current word, verification bookkeeping) and
// applies the Strategy decision. It has no Instance dependency, so tests drive
// it with a fake InputContext.
//
// Contract with the Go core (bami-core.h), as used here:
//  * BamiAckShown(h, s) tells the core which text of the current word is really
//    in the application ("" in preedit mode); the next BamiKey/BamiBackspace
//    returns deleteN/insert relative to that text, and `preedit` is the whole
//    composed word.
//  * A key that ends the word (space, punctuation) returns 0 and leaves the
//    preedit empty; any deleteN/insert it returns is applied before the key is
//    passed through.
//  * In preedit mode the text committed at word end is exactly the preedit
//    bami last showed (rule R5), never a recomputed string.
//
// Focus bounce (no timer): deactivate() on a focus-out in edit mode does not
// reset at once; it asks `defer` to run the reset on the NEXT event-loop
// iteration. A focus-in for the same context in the same iteration (Chromium X11,
// Brave on GNOME bounce focus out/in while typing) calls activate(), which
// cancels the pending reset, so the syllable survives. A real focus loss has no
// activate before the deferred reset runs, so the word ends. Preedit mode never
// survives a bounce: Fcitx itself commits the client preedit on focus-out.
#ifndef BAMI_COMPOSER_H
#define BAMI_COMPOSER_H

#include <cstdint>
#include <functional>
#include <memory>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputcontextproperty.h>
#include "config.h"
#include "strategy.h"

namespace bami {

struct IcState; // per input context, defined in composer.cpp

class Composer {
public:
    using Defer = std::function<void(std::function<void()>)>;

    Composer(fcitx::InputContextManager &manager, Defer defer);
    ~Composer();

    // Takes effect on each context at its next key.
    void setConfig(const BamiConfig &cfg);

    // Key entry points. Return true when the key was consumed.
    bool onRune(fcitx::InputContext *ic, uint32_t rune);
    bool onBackspace(fcitx::InputContext *ic);
    // A key that is not part of composing (Return, arrows, Ctrl+x ...): commit the
    // preedit if any, end the word. The key itself is passed through by the caller.
    void onFlush(fcitx::InputContext *ic);

    void onReset(fcitx::InputContext *ic);
    void onActivate(fcitx::InputContext *ic);
    // focusOut: event was InputContextFocusOut (Fcitx already committed the preedit).
    void onDeactivate(fcitx::InputContext *ic, bool focusOut);
    void onSurroundingTextUpdated(fcitx::InputContext *ic);

    // Introspection (tests, diagnostics).
    bool sticky(fcitx::InputContext *ic);
    Mode wordMode(fcitx::InputContext *ic); // Preedit also when no word is active
    bool wordActive(fcitx::InputContext *ic);
    Reason lastReason(fcitx::InputContext *ic);

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace bami

#endif
