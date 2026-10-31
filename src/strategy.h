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
#include <string_view>

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
    ConfiguredPreedit, // user explicitly disabled surrounding-text edits
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
    Range,        // cursor outside cached text, or Wayland byte limit reached
};

struct StrategyInput {
    FrontendKind frontend = FrontendKind::Other;
    bool terminal = false;           // CapabilityFlag::Terminal
    bool surroundingFlag = false;    // CapabilityFlag::SurroundingText
    bool textValid = false;          // ic->surroundingText().isValid()
    std::string text;                // cached surrounding text, UTF-8 (tests)
    const std::string *textRef = nullptr; // preferred: points at the IC's own cache, no per-key copy
    unsigned cursor = 0;             // in code points (Fcitx unit)
    unsigned anchor = 0;
    // Text bami has put in the application for the current word. Empty means
    // "word start": nothing to delete, so the verification below is lighter.
    std::string shown;
    bool updatedSinceEdit = false;   // SurroundingTextUpdated seen since last edit
    bool haveExpectedCursor = false; // recorded cursor + delta is known
    unsigned expectedCursor = 0;
    bool sticky = false;             // unreliable until verified recovery or explicit IM switch
};

struct Decision {
    Mode mode = Mode::Preedit;
    Reason reason = Reason::Ok;
    // True when the mismatch proves the application's text is not what bami
    // believes: stop editing until the caller has evidence for recovery.
    bool makeSticky = false;
};

Decision decide(const StrategyInput &in);

// Helpers exposed for tests / composer.
size_t utf8Length(std::string_view s);            // code points
std::string utf8Prefix(std::string_view s, size_t codePoints);
bool endsWith(std::string_view s, std::string_view suffix);
bool isWordBoundaryBefore(std::string_view text, size_t byteEnd);

// Wayland text-input v3 caps surrounding text at 4000 bytes; a text at or over
// the cap may be a truncated window whose cursor offsets we cannot trust.
constexpr size_t kWaylandMaxText = 4000;

// Every key of an edit-mode word verifies the text before the cursor, which costs O(bytes).
// Qt caps its text at 4096 chars (<= 12 KiB); beyond this bound the word is composed in preedit.
constexpr size_t kMaxVerifyBytes = 32 * 1024;

} // namespace bami

#endif
