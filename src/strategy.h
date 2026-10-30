// SPDX-License-Identifier: MIT
//
// Per-keystroke choice between "edit" (delete + commit in the application) and
// "preedit" (show the syllable as client preedit, commit once at word end).
//
// decide() is a PURE function: no Fcitx types, no I/O, no state. All state it
// needs is passed in StrategyInput, so every row of the rule table is unit
// tested in test/teststrategy.cpp. The rules come from docs/PLAN.md.
#ifndef BAMI_STRATEGY_H
#define BAMI_STRATEGY_H

#include <string>

namespace bami {

enum class FrontendKind {
    Xim,       // "xim": never edit, the client cannot undo a wrong delete
    DBus,      // "dbus": GTK/Qt/Electron via fcitx5-gtk / fcitx5-qt
    IBus,      // "ibus"
    Wayland,   // "wayland" (input-method v1; KWin, Wayfire plugin)
    WaylandV2, // "wayland_v2" (labwc, sway, Mutter-less compositors)
    Other,     // "fcitx4", unknown, test frontends
};

// strcmp() on the name returned by InputContext::frontend(); nullptr is Other.
FrontendKind frontendKindFromName(const char *name);

enum class Mode { Preedit, Edit };

enum class Reason {
    Ok,           // Edit allowed
    Xim,          // frontend is XIM
    Terminal,     // Terminal capability flag: surrounding text never exists
    Sticky,       // an earlier mismatch marked this context unreliable
    NoSurrounding,// neither the capability flag nor valid text
    InvalidText,  // flag set but the cached text is invalid (fcitx5-qt drops it)
    Selection,    // anchor != cursor (address-bar autocomplete, user selection)
    Stale,        // no SurroundingTextUpdated since our last edit/forwarded key
    TextMismatch, // text before the cursor does not end with what we committed
    CursorMismatch, // cursor != recorded cursor + our delta
    NotBoundary,  // character before the syllable is not a word boundary
    Range,        // Wayland: delete cannot be proven to convert correctly
};

struct StrategyInput {
    FrontendKind frontend = FrontendKind::Other;
    bool terminal = false;           // CapabilityFlag::Terminal
    bool surroundingFlag = false;    // CapabilityFlag::SurroundingText
    bool textValid = false;          // ic->surroundingText().isValid()
    std::string text;                // cached surrounding text, UTF-8
    unsigned cursor = 0;             // in code points (Fcitx unit)
    unsigned anchor = 0;
    // Text bami has put in the application for the current word. Empty means
    // "word start": nothing to delete, so the verification below is lighter.
    std::string shown;
    bool updatedSinceEdit = false;   // SurroundingTextUpdated seen since last edit
    bool haveExpectedCursor = false; // recorded cursor + delta is known
    unsigned expectedCursor = 0;
    bool sticky = false;             // context already marked unreliable
};

struct Decision {
    Mode mode = Mode::Preedit;
    Reason reason = Reason::Ok;
    // True when the mismatch proves the application's text is not what bami
    // believes: the caller must stop editing for this context for good.
    bool makeSticky = false;
};

Decision decide(const StrategyInput &in);

// Helpers exposed for tests / composer.
size_t utf8Length(const std::string &s);            // code points
std::string utf8Prefix(const std::string &s, size_t codePoints);
bool endsWith(const std::string &s, const std::string &suffix);
bool isWordBoundaryBefore(const std::string &text, size_t byteEnd);

// Wayland text-input v3 caps surrounding text at 4000 bytes; a text at or over
// the cap may be a truncated window whose cursor offsets we cannot trust.
constexpr size_t kWaylandMaxText = 4000;

} // namespace bami

#endif
