// SPDX-License-Identifier: MIT
#include "composer.h"
#include <algorithm>
#include <cstring>
#include <fcitx-utils/log.h>
#include <fcitx/inputpanel.h>
#include <fcitx/text.h>
#include <fcitx/userinterface.h>
extern "C" {
#include "bami-core.h"
}

namespace bami {

namespace {
// Owns a string returned by the Go core.
struct GoStr {
    char *p = nullptr;
    ~GoStr() {
        if (p) BamiFreeString(p);
    }
    GoStr() = default;
    GoStr(const GoStr &) = delete;
    GoStr &operator=(const GoStr &) = delete;
    std::string str() const { return p ? p : ""; }
};

// Longest common code-point prefix; delete the rest of `from`, insert the rest of `to`.
void diffText(const std::string &from, const std::string &to, int &deleteN, std::string &insert) {
    std::string a = from, b = to;
    size_t common = 0, n = std::min(utf8Length(a), utf8Length(b));
    while (common < n && utf8Prefix(a, common + 1) == utf8Prefix(b, common + 1)) common++;
    deleteN = static_cast<int>(utf8Length(a) - common);
    insert = b.substr(utf8Prefix(b, common).size());
}
} // namespace

struct IcState : fcitx::InputContextProperty {
    uintptr_t h = 0;
    unsigned appliedGen = 0;
    // Current word.
    bool active = false;
    Mode mode = Mode::Preedit;
    std::string shown;   // edit mode: the word text that is in the application
    std::string preedit; // preedit mode: the preedit currently displayed
    // Verification bookkeeping.
    bool updated = false;
    bool haveExpected = false;
    unsigned expected = 0;
    bool sticky = false;
    Reason lastReason = Reason::Ok;
    unsigned bounceToken = 0; // non-zero: a deferred focus-out reset is pending

    IcState(const CoreParams &p) : h(BamiNew(p.variant, p.toneStyle, p.flags)) {}
    ~IcState() override {
        if (h) BamiFree(h);
    }
};

struct Composer::Impl {
    Defer defer;
    BamiConfig cfg;
    unsigned gen = 1;
    unsigned nextToken = 0;
    bool errLogged = false;
    fcitx::InputContextManager &manager;
    fcitx::FactoryFor<IcState> factory;

    Impl(fcitx::InputContextManager &m, Defer df)
        : defer(std::move(df)), manager(m), factory([this](fcitx::InputContext &) {
              static bool inited = (BamiInit(), true);
              (void)inited;
              return new IcState(coreParams(cfg));
          }) {
        manager.registerProperty("bamiState", &factory);
    }

    IcState *st(fcitx::InputContext *ic) {
        auto *s = ic->propertyFor(&factory);
        if (s->appliedGen != gen) {
            auto p = coreParams(cfg);
            BamiConfigure(s->h, p.variant, p.toneStyle, p.flags);
            s->appliedGen = gen;
        }
        return s;
    }

    void goError(const char *what) {
        if (!errLogged) {
            errLogged = true;
            FCITX_WARN() << "bami: Go core call failed (" << what << "); passing keys through";
        }
    }

    void showPreedit(fcitx::InputContext *ic, const std::string &text) {
        fcitx::Text t(text, *cfg.underlinePreedit ? fcitx::TextFormatFlag::Underline
                                                  : fcitx::TextFormatFlag::NoFlag);
        if (!text.empty()) t.setCursor(text.size());
        if (ic->capabilityFlags().test(fcitx::CapabilityFlag::Preedit)) {
            ic->inputPanel().setClientPreedit(t);
        } else {
            ic->inputPanel().setPreedit(t);
        }
        ic->updatePreedit();
        ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
    }

    // Forget the word in both Bami and the state; text already in the app stays.
    void endWord(fcitx::InputContext *ic, IcState *s) {
        BamiReset(s->h);
        s->active = false;
        s->shown.clear();
        if (!s->preedit.empty()) {
            s->preedit.clear();
            showPreedit(ic, {});
        }
        s->haveExpected = false;
        s->mode = Mode::Preedit;
    }

    StrategyInput inputFor(fcitx::InputContext *ic, IcState *s) {
        StrategyInput in;
        in.frontend = frontendKindFromName(ic->frontend());
        auto cf = ic->capabilityFlags();
        in.terminal = cf.test(fcitx::CapabilityFlag::Terminal);
        in.surroundingFlag = cf.test(fcitx::CapabilityFlag::SurroundingText);
        const auto &st = ic->surroundingText();
        in.textValid = st.isValid();
        if (in.textValid) {
            in.text = st.text();
            in.cursor = st.cursor();
            in.anchor = st.anchor();
        }
        in.shown = s->shown;
        in.updatedSinceEdit = s->updated;
        in.haveExpectedCursor = s->haveExpected;
        in.expectedCursor = s->expected;
        in.sticky = s->sticky;
        return in;
    }

    // Decide how to handle this keystroke. Returns the mode for the word; when an
    // edit-mode word loses its footing, abandons it (the text typed so far stays in
    // the application as plain text) and restarts in preedit mode.
    Mode choose(fcitx::InputContext *ic, IcState *s) {
        if (s->active && s->mode == Mode::Preedit) return Mode::Preedit;
        Decision d = decide(inputFor(ic, s));
        s->lastReason = d.reason;
        if (d.makeSticky) s->sticky = true;
        if (d.mode == Mode::Edit) return Mode::Edit;
        if (s->active) endWord(ic, s); // edit-mode word downgraded
        return Mode::Preedit;
    }

    // Apply the core's answer. Returns false if the key must be passed through.
    // `final` is the text BamiCommit() returned when the key ended the word (rc == 0).
    bool apply(fcitx::InputContext *ic, IcState *s, Mode mode, int rc, int deleteN,
               std::string insert, std::string pre, const std::string &final = {}) {
        if (rc == 0) pre.clear(); // the word is over, whatever the core reports as preedit
        if (mode == Mode::Edit) {
            // Word end with a final form (e.g. restored raw keys): replace what is shown.
            if (rc == 0 && !final.empty() && final != s->shown) diffText(s->shown, final, deleteN, insert);
            // One delete at most, never longer than what we put there; issued first.
            if (deleteN < 0 || static_cast<size_t>(deleteN) > utf8Length(s->shown)) {
                goError("deleteN out of range");
                endWord(ic, s);
                return false;
            }
            const auto &sx = ic->surroundingText();
            unsigned cur = sx.isValid() ? sx.cursor() : 0;
            if (deleteN > 0) ic->deleteSurroundingText(-deleteN, static_cast<unsigned>(deleteN));
            if (!insert.empty()) ic->commitString(insert);
            if (deleteN > 0 || !insert.empty()) {
                s->haveExpected = true;
                s->expected = cur - static_cast<unsigned>(deleteN) + static_cast<unsigned>(utf8Length(insert));
                s->updated = false;
            }
            s->shown = pre;
            s->active = !pre.empty();
            s->mode = Mode::Edit;
            if (!s->active) s->haveExpected = false;
            if (rc == 0) {
                s->updated = false; // the forwarded key edits the text too
                s->haveExpected = false;
                return false;
            }
            return true;
        }
        // Preedit mode.
        if (!pre.empty()) {
            s->active = true;
            s->mode = Mode::Preedit;
            s->preedit = pre;
            showPreedit(ic, pre);
            return rc != 0;
        }
        // Word ended: commit exactly what the user saw (R5). Only the opt-in restore
        // option may commit something else (the restored raw keys).
        std::string text = s->preedit;
        if (!final.empty() && (*cfg.restore || text.empty())) text = final;
        if (text.empty()) text = insert;
        s->preedit.clear();
        s->active = false;
        s->mode = Mode::Preedit;
        showPreedit(ic, {});
        if (!text.empty()) ic->commitString(text);
        if (rc == 0) {
            s->updated = false;
            return false;
        }
        return true;
    }
};

Composer::Composer(fcitx::InputContextManager &m, Defer defer) : d(new Impl(m, std::move(defer))) {}
Composer::~Composer() = default;

void Composer::setConfig(const BamiConfig &cfg) {
    fcitx::RawConfig raw;
    cfg.save(raw);
    d->cfg.load(raw);
    d->gen++;
}

bool Composer::onRune(fcitx::InputContext *ic, uint32_t rune) {
    auto *s = d->st(ic);
    s->bounceToken = 0;
    Mode mode = d->choose(ic, s);
    BamiAckShown(s->h, const_cast<char *>(mode == Mode::Edit ? s->shown.c_str() : ""));
    int deleteN = 0;
    GoStr ins, pre;
    int rc = BamiKey(s->h, rune, &deleteN, &ins.p, &pre.p);
    if (rc < 0) {
        d->goError("BamiKey");
        d->endWord(ic, s);
        return false;
    }
    std::string final;
    if (rc == 0 && s->active) {
        GoStr c;
        c.p = BamiCommit(s->h); // the word ended: the core hands back its final form and resets
        final = c.str();
    }
    return d->apply(ic, s, mode, rc, deleteN, ins.str(), pre.str(), final);
}

bool Composer::onBackspace(fcitx::InputContext *ic) {
    auto *s = d->st(ic);
    if (!s->active) return false;
    Mode mode = d->choose(ic, s);
    if (!s->active) return false; // edit-mode word abandoned: let the app handle Backspace
    BamiAckShown(s->h, const_cast<char *>(mode == Mode::Edit ? s->shown.c_str() : ""));
    int deleteN = 0;
    GoStr ins, pre;
    int rc = BamiBackspace(s->h, &deleteN, &ins.p, &pre.p);
    if (rc < 0) {
        d->goError("BamiBackspace");
        d->endWord(ic, s);
        return false;
    }
    return d->apply(ic, s, mode, rc, deleteN, ins.str(), pre.str());
}

void Composer::onFlush(fcitx::InputContext *ic) {
    auto *s = d->st(ic);
    if (s->active && s->mode == Mode::Preedit && !s->preedit.empty()) {
        std::string text = s->preedit;
        s->preedit.clear();
        d->showPreedit(ic, {});
        ic->commitString(text);
    }
    BamiReset(s->h);
    s->active = false;
    s->shown.clear();
    s->preedit.clear();
    s->haveExpected = false;
    s->updated = false; // the forwarded key edits the text
    s->mode = Mode::Preedit;
}

void Composer::onReset(fcitx::InputContext *ic) {
    auto *s = d->st(ic);
    s->bounceToken = 0;
    d->endWord(ic, s);
}

void Composer::onActivate(fcitx::InputContext *ic) {
    auto *s = d->st(ic);
    s->bounceToken = 0; // focus came back in the same turn: keep the syllable
}

void Composer::onDeactivate(fcitx::InputContext *ic, bool focusOut) {
    auto *s = d->st(ic);
    if (!s->active) return;
    if (focusOut && s->mode == Mode::Edit && d->defer) {
        unsigned token = ++d->nextToken;
        s->bounceToken = token;
        auto ref = ic->watch();
        d->defer([this, ref, token]() {
            auto *c = ref.get();
            if (!c) return;
            auto *st = d->st(c);
            if (st->bounceToken == token) {
                st->bounceToken = 0;
                d->endWord(c, st);
            }
        });
        return;
    }
    if (!focusOut && s->mode == Mode::Preedit && !s->preedit.empty()) {
        // Switching away from bami: Fcitx does not commit the preedit for us.
        std::string text = s->preedit;
        s->preedit.clear();
        d->showPreedit(ic, {});
        ic->commitString(text);
    }
    d->endWord(ic, s);
}

void Composer::onSurroundingTextUpdated(fcitx::InputContext *ic) { d->st(ic)->updated = true; }

bool Composer::sticky(fcitx::InputContext *ic) { return d->st(ic)->sticky; }
Mode Composer::wordMode(fcitx::InputContext *ic) {
    auto *s = d->st(ic);
    return s->active ? s->mode : Mode::Preedit;
}
bool Composer::wordActive(fcitx::InputContext *ic) { return d->st(ic)->active; }
Reason Composer::lastReason(fcitx::InputContext *ic) { return d->st(ic)->lastReason; }

} // namespace bami
