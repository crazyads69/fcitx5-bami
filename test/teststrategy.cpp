// SPDX-License-Identifier: MIT
// Table test for bami::decide(): one row per rule in docs/PLAN.md.
#include <cstdio>
#include <cstdlib>
#include "strategy.h"

using namespace bami;

static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { failures++; std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

// A healthy mid-word situation: "Tie" is in the app, the cursor is right after it.
static StrategyInput healthy(FrontendKind f = FrontendKind::DBus) {
    StrategyInput in;
    in.frontend = f;
    in.surroundingFlag = true;
    in.textValid = true;
    in.text = "xin Tie";
    in.cursor = in.anchor = 7;
    in.shown = "Tie";
    in.updatedSinceEdit = true;
    in.haveExpectedCursor = true;
    in.expectedCursor = 7;
    return in;
}

int main() {
    // Frontend names.
    CHECK(frontendKindFromName("xim") == FrontendKind::Xim);
    CHECK(frontendKindFromName("dbus") == FrontendKind::DBus);
    CHECK(frontendKindFromName("ibus") == FrontendKind::IBus);
    CHECK(frontendKindFromName("wayland") == FrontendKind::Wayland);
    CHECK(frontendKindFromName("wayland_v2") == FrontendKind::WaylandV2);
    CHECK(frontendKindFromName("fcitx4") == FrontendKind::Other);
    CHECK(frontendKindFromName(nullptr) == FrontendKind::Other);

    // Edit allowed everywhere except XIM.
    for (auto f : {FrontendKind::DBus, FrontendKind::IBus, FrontendKind::Wayland, FrontendKind::WaylandV2,
                   FrontendKind::Other}) {
        auto d = decide(healthy(f));
        CHECK(d.mode == Mode::Edit);
        CHECK(!d.makeSticky);
    }
    // XIM never edits, even with perfect text, at word start or mid-word.
    {
        auto in = healthy(FrontendKind::Xim);
        CHECK(decide(in).mode == Mode::Preedit && decide(in).reason == Reason::Xim);
        in.shown.clear();
        CHECK(decide(in).mode == Mode::Preedit);
        in.sticky = true;
        CHECK(decide(in).reason == Reason::Xim);
    }
    // Terminal flag.
    { auto in = healthy(); in.terminal = true; CHECK(decide(in).reason == Reason::Terminal); }
    // Capability: nothing at all.
    { auto in = healthy(); in.surroundingFlag = false; in.textValid = false;
      CHECK(decide(in).reason == Reason::NoSurrounding && decide(in).mode == Mode::Preedit); }
    // Flag without text (fcitx5-qt drops the text): preedit.
    { auto in = healthy(); in.textValid = false; CHECK(decide(in).reason == Reason::InvalidText); }
    // Valid text without the flag is accepted.
    { auto in = healthy(); in.surroundingFlag = false; CHECK(decide(in).mode == Mode::Edit); }
    // Selection (address-bar autocomplete): preedit, not sticky.
    { auto in = healthy(); in.anchor = 3; auto d = decide(in);
      CHECK(d.mode == Mode::Preedit && d.reason == Reason::Selection && !d.makeSticky); }
    // Stale: mid-word is a mismatch (sticky); at word start only this word falls back.
    { auto in = healthy(); in.updatedSinceEdit = false; auto d = decide(in);
      CHECK(d.mode == Mode::Preedit && d.reason == Reason::Stale && d.makeSticky);
      in.shown.clear(); d = decide(in);
      CHECK(d.mode == Mode::Preedit && d.reason == Reason::Stale && !d.makeSticky); }
    // Text before cursor no longer ends with the syllable.
    { auto in = healthy(); in.text = "xin Tix"; auto d = decide(in);
      CHECK(d.mode == Mode::Preedit && d.reason == Reason::TextMismatch && d.makeSticky); }
    // Cursor moved.
    { auto in = healthy(); in.expectedCursor = 6; auto d = decide(in);
      CHECK(d.reason == Reason::CursorMismatch && d.makeSticky);
      in = healthy(); in.haveExpectedCursor = false;
      CHECK(decide(in).reason == Reason::CursorMismatch); }
    // Char before the syllable is a letter: not a boundary.
    { auto in = healthy(); in.text = "xinTie"; in.cursor = in.anchor = in.expectedCursor = 6;
      auto d = decide(in); CHECK(d.reason == Reason::NotBoundary && d.makeSticky);
      in.text = "xi\xE1\xBA\xBF" "Tie"; // ế before: Vietnamese letters are not boundaries
      in.cursor = in.anchor = in.expectedCursor = 6; CHECK(decide(in).reason == Reason::NotBoundary);
      in.text = "(Tie"; in.cursor = in.anchor = in.expectedCursor = 4; CHECK(decide(in).mode == Mode::Edit);
      in.text = "Tie"; in.cursor = in.anchor = in.expectedCursor = 3; CHECK(decide(in).mode == Mode::Edit); }
    // Cursor mid-text: only the part before the cursor counts.
    { auto in = healthy(); in.text = "xin Tie rest"; CHECK(decide(in).mode == Mode::Edit); }
    // Word start.
    { auto in = healthy(); in.shown.clear(); in.haveExpectedCursor = false;
      in.text = "xin "; in.cursor = in.anchor = 4;
      CHECK(decide(in).mode == Mode::Edit);
      in.text = "abc"; in.cursor = in.anchor = 3; auto d = decide(in);
      CHECK(d.mode == Mode::Preedit && d.reason == Reason::NotBoundary && !d.makeSticky);
      in.text = ""; in.cursor = in.anchor = 0; CHECK(decide(in).mode == Mode::Edit); }
    // Sticky.
    { auto in = healthy(); in.sticky = true; CHECK(decide(in).reason == Reason::Sticky); }
    // Wayland range validation: huge text, cursor beyond text.
    for (auto f : {FrontendKind::Wayland, FrontendKind::WaylandV2}) {
        auto in = healthy(f); in.text = std::string(kWaylandMaxText, 'a') + " Tie";
        in.cursor = in.anchor = in.expectedCursor = static_cast<unsigned>(in.text.size());
        auto d = decide(in); CHECK(d.mode == Mode::Preedit && d.reason == Reason::Range);
        in = healthy(f); in.cursor = in.anchor = in.expectedCursor = 99; d = decide(in);
        CHECK(d.reason == Reason::Range);
    }
    // Every frontend must reject a cursor outside its cached text.
    for (auto f : {FrontendKind::DBus, FrontendKind::IBus, FrontendKind::Other}) {
        auto in = healthy(f);
        in.cursor = in.anchor = in.expectedCursor = 99;
        CHECK(decide(in).reason == Reason::Range);
        in.shown.clear();
        CHECK(decide(in).mode == Mode::Preedit);
    }
    // The same long text is fine on dbus (only the Wayland byte conversion is the risk).
    { auto in = healthy(); in.text = std::string(kWaylandMaxText, 'a') + " Tie";
      in.cursor = in.anchor = in.expectedCursor = static_cast<unsigned>(in.text.size());
      CHECK(decide(in).mode == Mode::Edit); }
    // Malformed UTF-8 must never provide evidence for a surrounding-text delete.
    for (const std::string &bad : {std::string("\x80"), std::string("\xC0\xAF"),
         std::string("\xE0\x80\xAF"), std::string("\xED\xA0\x80"),
         std::string("\xF4\x90\x80\x80"), std::string("\xE1\xBA"), std::string(1, '\0')}) {
        auto in = healthy(); in.text = bad + " Tie";
        CHECK(decide(in).reason == Reason::InvalidText);
        CHECK(decide(in).makeSticky);
    }
    // Joining/format characters, ideographic letters, and combining marks are not punctuation.
    for (const std::string prefix : {"\xE2\x80\x8D", "\xE2\x80\xAA", "\xE3\x80\x85", "\xE3\x80\xAA"}) {
        auto in = healthy(); in.text = prefix + "Tie";
        in.cursor = in.anchor = in.expectedCursor = 4;
        CHECK(decide(in).reason == Reason::NotBoundary);
    }
    // Helpers.
    CHECK(utf8Length("Ti\xE1\xBA\xBFng") == 5);
    CHECK(utf8Prefix("Ti\xE1\xBA\xBFng", 3) == "Ti\xE1\xBA\xBF");
    CHECK(endsWith("abc", "bc") && !endsWith("c", "bc"));

    std::printf("teststrategy: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
