// SPDX-License-Identifier: MIT
// FakeIC: an InputContext that also plays the application. It owns a text
// buffer + cursor, applies delete/commit like a real client, records every
// operation, and exposes the buffer as surrounding text on demand.
#ifndef BAMI_FAKEIC_H
#define BAMI_FAKEIC_H

#include <string>
#include <vector>
#include <fcitx/inputcontext.h>
#include <fcitx/inputpanel.h>
#include "strategy.h"

class FakeIC : public fcitx::InputContext {
public:
    FakeIC(fcitx::InputContextManager &m, std::string frontend)
        : fcitx::InputContext(m, "fake"), frontend_(std::move(frontend)) {}
    ~FakeIC() override { destroy(); }

    const char *frontend() const override { return frontend_.c_str(); }

    // --- application model (code point indexed) ---
    std::u32string text;
    size_t cursor = 0;
    bool rejectDelete = false; // a client that ignores deleteSurroundingText
    std::vector<std::string> log; // "del:<n>" / "commit:<s>" / "preedit:<s>"
    std::string preedit;

    static std::u32string to32(const std::string &s) {
        std::u32string r;
        for (size_t i = 0; i < s.size();) {
            unsigned char c = s[i];
            size_t n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : 4;
            char32_t cp = n == 1 ? c : n == 2 ? c & 31 : n == 3 ? c & 15 : c & 7;
            for (size_t k = 1; k < n; k++) cp = (cp << 6) | (s[i + k] & 63);
            r.push_back(cp);
            i += n;
        }
        return r;
    }
    static std::string to8(const std::u32string &s) {
        std::string r;
        for (char32_t c : s) {
            if (c < 0x80) r += char(c);
            else if (c < 0x800) { r += char(0xC0 | c >> 6); r += char(0x80 | (c & 63)); }
            else if (c < 0x10000) { r += char(0xE0 | c >> 12); r += char(0x80 | ((c >> 6) & 63)); r += char(0x80 | (c & 63)); }
            else { r += char(0xF0 | c >> 18); r += char(0x80 | ((c >> 12) & 63)); r += char(0x80 | ((c >> 6) & 63)); r += char(0x80 | (c & 63)); }
        }
        return r;
    }
    std::string appText() const { return to8(text); }
    void typeRaw(const std::string &s) { // key passed through to the app
        auto t = to32(s);
        text.insert(cursor, t);
        cursor += t.size();
    }
    // Publish the app's text as surrounding text (what a frontend does after a change).
    void publish() {
        surroundingText().setText(to8(text), cursor, cursor);
    }
    int deletes() const { int n = 0; for (auto &l : log) if (l.rfind("del:", 0) == 0) n++; return n; }
    void clearLog() { log.clear(); }

protected:
    void commitStringImpl(const std::string &s) override {
        log.push_back("commit:" + s);
        typeRaw(s);
    }
    void deleteSurroundingTextImpl(int offset, unsigned size) override {
        log.push_back("del:" + std::to_string(size) + "@" + std::to_string(offset));
        if (rejectDelete) return;
        long start = long(cursor) + offset;
        if (start < 0 || start + long(size) > long(text.size())) return;
        text.erase(start, size);
        cursor = start;
    }
    void forwardKeyImpl(const fcitx::ForwardKeyEvent &) override {}
    void updatePreeditImpl() override {
        preedit = inputPanel().clientPreedit().toString();
        if (preedit.empty()) preedit = inputPanel().preedit().toString();
    }

private:
    std::string frontend_;
};

#endif
