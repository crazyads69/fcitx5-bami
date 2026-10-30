// SPDX-License-Identifier: MIT
// Composer + real Go core against a fake application (FakeIC).
#include <cstdio>
#include <functional>
#include <string>
#include <vector>
#include <fcitx/inputcontextmanager.h>
#include "composer.h"
#include "fakeic.h"

using namespace bami;
using fcitx::CapabilityFlag;

static int failures = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { failures++; std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define CHECK_EQ(a, b) do { checks++; if (!((a) == (b))) { failures++; std::printf("FAIL %s:%d: %s == %s\n  got:  [%s]\n  want: [%s]\n", __FILE__, __LINE__, #a, #b, std::string(a).c_str(), std::string(b).c_str()); } } while (0)

struct Rig {
    fcitx::InputContextManager mgr;
    std::vector<std::function<void()>> deferred;
    Composer composer{mgr, [this](std::function<void()> f) { deferred.push_back(std::move(f)); }};
    std::unique_ptr<FakeIC> ic;

    Rig(const char *frontend, bool surrounding = true) {
        ic.reset(new FakeIC(mgr, frontend));
        fcitx::CapabilityFlags f{CapabilityFlag::Preedit};
        if (surrounding) f |= CapabilityFlag::SurroundingText;
        ic->setCapabilityFlags(f);
        ic->focusIn();
        sync();
    }
    void sync() { ic->publish(); composer.onSurroundingTextUpdated(ic.get()); }
    void runDeferred() { auto d = std::move(deferred); deferred.clear(); for (auto &f : d) f(); }

    // Type one key like the engine does: consumed => nothing, else the app gets it.
    void key(char c, bool publishAfter = true) {
        std::string s(1, c);
        if (!composer.onRune(ic.get(), (unsigned char)c)) ic->typeRaw(s);
        if (publishAfter) sync();
    }
    void type(const std::string &keys, bool publishAfter = true) { for (char c : keys) key(c, publishAfter); }
    void bs() {
        if (!composer.onBackspace(ic.get())) { if (ic->cursor) { ic->text.erase(--ic->cursor, 1); } }
        sync();
    }
};

int main() {
    const std::string kPhrase = "Tieengs Vieetj ddepj";
    const std::string kWant = "Ti\xE1\xBA\xBFng Vi\xE1\xBB\x87t \xC4\x91\xE1\xBA\xB9p"; // Tiếng Việt đẹp

    // 1. Edit mode on dbus: text correct, deletes happen, each key delete-before-commit.
    {
        Rig r("dbus");
        for (char c : kPhrase) {
            r.ic->clearLog();
            r.key(c);
            int di = -1, ci = -1;
            for (size_t i = 0; i < r.ic->log.size(); i++) {
                if (r.ic->log[i].rfind("del:", 0) == 0) { CHECK(di < 0); di = (int)i; }
                if (r.ic->log[i].rfind("commit:", 0) == 0) ci = (int)i;
            }
            if (di >= 0) CHECK(ci > di);
        }
        CHECK_EQ(r.ic->appText(), kWant);
        CHECK(!r.composer.sticky(r.ic.get()));
        r.type(" ");
    }
    // 2. Edit mode really deletes (otherwise test 1 proves nothing).
    {
        Rig r("ibus");
        r.type("aa");
        CHECK(r.ic->deletes() >= 1);
        CHECK_EQ(r.ic->appText(), "\xC3\xA2"); // â
        CHECK(r.composer.wordMode(r.ic.get()) == Mode::Edit);
    }
    // 3. XIM: never a delete; preedit while typing; exact preedit committed at word end.
    {
        Rig r("xim");
        r.type("Tieeng");
        CHECK_EQ(r.ic->preedit, "Ti\xC3\xAA" "ng"); // Tiêng
        CHECK_EQ(r.ic->appText(), "");
        r.type("s ");
        CHECK_EQ(r.ic->appText(), "Ti\xE1\xBA\xBFng ");
        CHECK(r.ic->preedit.empty());
        r.type("Vieetj ddepj ");
        CHECK_EQ(r.ic->appText(), kWant + " ");
        CHECK(r.ic->deletes() == 0);
        // R5: the word-end commit is exactly one commit of the preedit text.
        Rig q("xim");
        q.type("ddepj");
        q.ic->clearLog();
        q.type(" ");
        CHECK(q.ic->log.size() == 1 && q.ic->log[0] == "commit:\xC4\x91\xE1\xBA\xB9p");
    }
    // 4. Terminal flag, no text, and a selection: preedit, zero deletes.
    {
        Rig r("dbus");
        r.ic->setCapabilityFlags(fcitx::CapabilityFlags{CapabilityFlag::Preedit, CapabilityFlag::Terminal});
        r.type("aa ");
        CHECK(r.ic->deletes() == 0);
        CHECK_EQ(r.ic->appText(), "\xC3\xA2 ");
        Rig n("dbus", false);
        n.ic->surroundingText().invalidate();
        n.type("aa ");
        CHECK(n.ic->deletes() == 0);
        CHECK_EQ(n.ic->appText(), "\xC3\xA2 ");
        Rig s("dbus");
        s.ic->text = U"hello"; s.ic->cursor = 5;
        s.ic->surroundingText().setText("hello", 5, 0); // selection
        s.composer.onSurroundingTextUpdated(s.ic.get());
        s.key('a', false);
        CHECK(s.composer.wordMode(s.ic.get()) == Mode::Preedit && s.composer.wordActive(s.ic.get()));
        CHECK(!s.composer.sticky(s.ic.get()));
    }
    // 5. Stale text mid-word: sticky downgrade to preedit, no delete issued on stale data.
    {
        Rig r("dbus");
        r.type("a");                      // edit-mode, synced
        r.key('a', false);                // -> â, but the app does not publish it
        r.ic->clearLog();
        r.key('s', false);                // third key sees stale text: mismatch
        CHECK(r.composer.sticky(r.ic.get()));
        CHECK(r.ic->deletes() == 0);
        r.sync();
        r.type(" ");                       // flush
        // After the sticky downgrade a new word is preedit even with perfect sync.
        r.ic->clearLog();
        r.type("aa");
        CHECK(r.composer.wordMode(r.ic.get()) == Mode::Preedit);
        CHECK(r.ic->deletes() == 0);
    }
    // 6. Text changed under us (user moved the cursor): sticky, no delete.
    {
        Rig r("dbus");
        r.type("Tie");
        r.ic->cursor = 1; r.sync(); // caret moved
        r.ic->clearLog();
        r.key('e');
        CHECK(r.composer.sticky(r.ic.get()));
        CHECK(r.ic->deletes() == 0);
    }
    // 7. Wayland: text at the byte cap -> preedit, never a delete.
    {
        Rig r("wayland");
        r.ic->text = FakeIC::to32(std::string(4100, 'x') + " ");
        r.ic->cursor = r.ic->text.size();
        r.sync();
        r.type("aa");
        CHECK(r.composer.wordMode(r.ic.get()) == Mode::Preedit);
        CHECK(r.ic->deletes() == 0);
        // A client that rejects deletes can never see one: Wayland edit mode is only
        // reached when the range is inside the text, and a reject there is caught by
        // the next key's text check (sticky) -- the commit-after-failed-delete case.
        Rig w("wayland_v2");
        w.ic->rejectDelete = true;
        w.type("aa");
        w.type("s", true);
        CHECK(w.composer.sticky(w.ic.get()) || w.ic->deletes() <= 1);
    }
    // 8. Backspace: edit and preedit give the same text; "vieetj" BS -> "việ".
    {
        Rig e("dbus"), p("xim");
        e.type("vieetj"); e.bs();
        p.type("vieetj"); p.bs(); p.type(" ");
        CHECK_EQ(e.ic->appText(), "vi\xE1\xBB\x87");
        CHECK_EQ(p.ic->appText(), "vi\xE1\xBB\x87 ");
    }
    // 9. Focus bounce: same-turn out+in keeps the syllable; a real focus-out ends it.
    {
        Rig r("dbus");
        r.type("aa");
        r.composer.onDeactivate(r.ic.get(), true);
        r.composer.onActivate(r.ic.get());
        r.runDeferred();
        CHECK(r.composer.wordActive(r.ic.get()));
        r.sync();
        r.type("s");
        CHECK_EQ(r.ic->appText(), "\xE1\xBA\xA5"); // ấ
        r.composer.onDeactivate(r.ic.get(), true);
        r.runDeferred();
        CHECK(!r.composer.wordActive(r.ic.get()));
    }
    // 10. Reset ends the word; switching IM away commits a preedit.
    {
        Rig r("xim");
        r.type("aa");
        r.composer.onDeactivate(r.ic.get(), false);
        CHECK_EQ(r.ic->appText(), "\xC3\xA2");
        CHECK(!r.composer.wordActive(r.ic.get()));
        r.type("aa");
        r.composer.onReset(r.ic.get());
        CHECK(!r.composer.wordActive(r.ic.get()));
        CHECK_EQ(r.ic->appText(), "\xC3\xA2");
    }
    // 11. Non-composing key flushes the preedit.
    {
        Rig r("xim");
        r.type("aa");
        r.composer.onFlush(r.ic.get());
        CHECK_EQ(r.ic->appText(), "\xC3\xA2");
    }
    // 12. Opt-in restore: a non-Vietnamese word returns to its raw keys at word end,
    // in edit mode (replaces what is on screen) and in preedit mode (committed text).
    for (const char *fe : {"dbus", "xim"}) {
        Rig r(fe);
        BamiConfig cfg;
        cfg.restore.setValue(true);
        r.composer.setConfig(cfg);
        r.type("windows");
        r.ic->clearLog();
        r.type(" ");
        CHECK_EQ(r.ic->appText(), "windows ");
        CHECK(r.ic->deletes() <= 1); // the restore at word end: one delete, then the commit
    }
    std::printf("testcomposer: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
