// SPDX-License-Identifier: MIT
#include "strategy.h"
#include <cstring>

namespace bami {

FrontendKind frontendKindFromName(const char *n) {
    if (!n) return FrontendKind::Other;
    if (!strcmp(n, "xim")) return FrontendKind::Xim;
    if (!strcmp(n, "dbus")) return FrontendKind::DBus;
    if (!strcmp(n, "ibus")) return FrontendKind::IBus;
    if (!strcmp(n, "wayland")) return FrontendKind::Wayland;
    if (!strcmp(n, "wayland_v2")) return FrontendKind::WaylandV2;
    return FrontendKind::Other;
}

namespace {
// Byte length of the UTF-8 sequence that starts with lead byte c (1 for junk).
size_t seqLen(unsigned char c) {
    if (c < 0x80) return 1;
    if ((c >> 5) == 0x6) return 2;
    if ((c >> 4) == 0xE) return 3;
    if ((c >> 3) == 0x1E) return 4;
    return 1;
}
char32_t decodeAt(const std::string &s, size_t i, size_t n) {
    unsigned char c = s[i];
    if (n == 1) return c;
    char32_t cp = n == 2 ? c & 0x1F : n == 3 ? c & 0x0F : c & 0x07;
    for (size_t k = 1; k < n && i + k < s.size(); k++) cp = (cp << 6) | (s[i + k] & 0x3F);
    return cp;
}
} // namespace

size_t utf8Length(const std::string &s) {
    size_t n = 0;
    for (size_t i = 0; i < s.size(); i += seqLen(s[i])) n++;
    return n;
}

std::string utf8Prefix(const std::string &s, size_t cps) {
    size_t i = 0;
    while (cps-- && i < s.size()) i += seqLen(s[i]);
    return s.substr(0, i > s.size() ? s.size() : i);
}

bool endsWith(const std::string &s, const std::string &suf) {
    return s.size() >= suf.size() && s.compare(s.size() - suf.size(), suf.size(), suf) == 0;
}

// The character that ends at byteEnd is a boundary if there is none (start of
// text) or it is ASCII space/punctuation/control or a Unicode space/punctuation
// block. Letters (including Vietnamese) and digits are not boundaries.
bool isWordBoundaryBefore(const std::string &t, size_t end) {
    if (end == 0) return true;
    size_t i = end - 1;
    while (i > 0 && (static_cast<unsigned char>(t[i]) & 0xC0) == 0x80) i--;
    char32_t cp = decodeAt(t, i, seqLen(t[i]));
    if (cp < 0x80) {
        bool alnum = (cp >= '0' && cp <= '9') || (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z');
        return !alnum;
    }
    return cp == 0xA0 || (cp >= 0x2000 && cp <= 0x206F) || cp == 0x3000 || (cp >= 0x3001 && cp <= 0x303F);
}

namespace {
Decision preedit(Reason r, bool sticky = false) { return {Mode::Preedit, r, sticky}; }
} // namespace

Decision decide(const StrategyInput &in) {
    // 1. Unconditional preedit contexts. XIM first: it must never delete, whatever
    // else is true, because XIM has no delete-surrounding request at all.
    if (in.frontend == FrontendKind::Xim) return preedit(Reason::Xim);
    if (in.terminal) return preedit(Reason::Terminal);
    if (in.sticky) return preedit(Reason::Sticky);

    // 2. Capability. The flag alone is not enough (fcitx5-qt drops it after each key),
    // and without valid text nothing below can be verified.
    if (!in.surroundingFlag && !in.textValid) return preedit(Reason::NoSurrounding);
    if (!in.textValid) return preedit(Reason::InvalidText);
    if (in.anchor != in.cursor) return preedit(Reason::Selection);

    const bool midWord = !in.shown.empty();
    const bool wayland = in.frontend == FrontendKind::Wayland || in.frontend == FrontendKind::WaylandV2;

    // 3. Freshness: apps update the cache asynchronously (GTK on an idle callback).
    // At a word start a late update only costs this one word; mid-word it means the
    // text we are about to delete from may not be what we think it is.
    if (!in.updatedSinceEdit) return preedit(Reason::Stale, midWord);

    // 4. Wayland converts our offsets to bytes from the client's CACHED text, and a
    // rejected delete does not stop the following commit (duplicated text). Only
    // delete when the whole range is provably inside the cached text.
    if (wayland) {
        if (in.text.size() >= kWaylandMaxText) return preedit(Reason::Range);
        if (utf8Length(in.text) < in.cursor) return preedit(Reason::Range);
    }

    // 5. Verify against the text before the cursor.
    const std::string before = utf8Prefix(in.text, in.cursor);
    if (midWord) {
        if (!endsWith(before, in.shown)) return preedit(Reason::TextMismatch, true);
        if (!in.haveExpectedCursor || in.cursor != in.expectedCursor)
            return preedit(Reason::CursorMismatch, true);
        if (!isWordBoundaryBefore(before, before.size() - in.shown.size()))
            return preedit(Reason::NotBoundary, true);
    } else if (!isWordBoundaryBefore(before, before.size())) {
        // Word start inside/after a letter: stay conservative for this word only.
        return preedit(Reason::NotBoundary);
    }
    return {Mode::Edit, Reason::Ok, false};
}

} // namespace bami
