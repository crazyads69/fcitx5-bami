// SPDX-License-Identifier: MIT
#include "composer.h"
#include "english.h"
#include <algorithm>
#include <cstring>
#include <map>
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
    size_t common = 0, n = std::min(from.size(), to.size());
    while (common < n && from[common] == to[common]) common++;
    while (common && common < from.size() &&
           (static_cast<unsigned char>(from[common]) & 0xC0) == 0x80) common--;
    deleteN = 0;
    for (size_t i = common; i < from.size(); i++)
        if ((static_cast<unsigned char>(from[i]) & 0xC0) != 0x80) deleteN++;
    insert = to.substr(common);
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
    bool rawDisplay = false;
    std::map<std::string, bool> corrections; // raw keys -> prefer raw / Vietnamese
    // Verification bookkeeping.
    bool updated = false;
    bool haveExpected = false;
    unsigned expected = 0;
    bool sticky = false;
    bool transientStale = false;
    bool recoveryReady = false;
    std::string recoveryShown;
    unsigned recoveryCursor = 0;
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
    EnglishDictionary english;
    unsigned gen = 1;
    unsigned nextToken = 0;
    bool errLogged = false;
    fcitx::InputContextManager &manager;
    fcitx::FactoryFor<IcState> factory;

    Impl(fcitx::InputContextManager &m, Defer df)
        : defer(std::move(df)), manager(m), factory([this](fcitx::InputContext &) {
              return new IcState(coreParams(cfg));
          }) {
        static bool inited = (BamiInit(), true);
        (void)inited;
        manager.registerProperty("bamiState", &factory);
        english.configure(*cfg.englishDictionary);
    }

    IcState *st(fcitx::InputContext *ic) {
        auto *s = ic->propertyFor(&factory);
        if (s->appliedGen != gen) {
            // Finish the visible word before Configure resets the core's baseline.
            endWord(ic, s, true);
            s->corrections.clear();
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

    bool usesClientPreedit(fcitx::InputContext *ic) const {
        return *cfg.clientPreedit && ic->capabilityFlags().test(fcitx::CapabilityFlag::Preedit);
    }

    void showPreedit(fcitx::InputContext *ic, const std::string &text) {
        fcitx::Text t(text, *cfg.underlinePreedit ? fcitx::TextFormatFlag::Underline
                                                  : fcitx::TextFormatFlag::NoFlag);
        if (!text.empty()) t.setCursor(text.size());
        ic->inputPanel().setClientPreedit({});
        ic->inputPanel().setPreedit({});
        if (usesClientPreedit(ic)) {
            ic->inputPanel().setClientPreedit(t);
        } else {
            ic->inputPanel().setPreedit(t);
        }
        ic->updatePreedit();
        ic->updateUserInterface(fcitx::UserInterfaceComponent::InputPanel);
    }

    // Forget the word in both Bami and the state; text already in the app stays.
    void endWord(fcitx::InputContext *ic, IcState *s, bool preserve = false) {
        std::string text = preserve && s->mode == Mode::Preedit ? s->preedit : "";
        BamiReset(s->h);
        s->active = false;
        s->rawDisplay = false;
        s->shown.clear();
        if (!s->preedit.empty()) {
            s->preedit.clear();
            showPreedit(ic, {});
        }
        s->haveExpected = false;
        s->mode = Mode::Preedit;
        if (!text.empty()) {
            ic->commitString(text);
            s->updated = false;
        }
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
            in.textRef = &st.text();
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
        if (*cfg.preeditOnly) {
            s->lastReason = Reason::ConfiguredPreedit;
            return Mode::Preedit;
        }
        if (s->active && s->mode == Mode::Preedit) return Mode::Preedit;
        StrategyInput input = inputFor(ic, s);
        // Retry only a temporary missing update, at a fresh verified word boundary.
        if (s->sticky && s->transientStale && s->recoveryReady && !s->active && s->updated) {
            input.sticky = false;
            if (decide(input).mode == Mode::Edit) {
                s->sticky = false;
                s->transientStale = s->recoveryReady = false;
            } else input.sticky = true;
        }
        Decision d = decide(input);
        s->lastReason = d.reason;
        if (d.makeSticky) {
            s->sticky = true;
            s->transientStale = d.reason == Reason::Stale && s->haveExpected;
            s->recoveryReady = false;
            s->recoveryShown = s->transientStale ? s->shown : "";
            s->recoveryCursor = s->expected;
        }
        if (d.mode == Mode::Edit) return Mode::Edit;
        if (s->active) endWord(ic, s); // edit-mode word downgraded
        return Mode::Preedit;
    }

    std::string selectWord(IcState *s, const std::string &raw, const std::string &composed,
                           const std::string &normal) {
        if (!*cfg.protectEnglish || raw == composed) return normal;
        if (*cfg.learnCorrections) {
            auto found = s->corrections.find(raw);
            if (found != s->corrections.end()) return found->second ? raw : composed;
        }
        return english.contains(raw) || (*cfg.englishModel &&
               BamiEnglishScore(s->h) >= BamiEnglishThreshold()) ? raw : normal;
    }

    std::string commitWord(IcState *s, bool alternate = false) {
        GoStr raw, composed, final;
        raw.p = BamiRaw(s->h);
        composed.p = BamiPreedit(s->h);
        // Evaluate the model while its current word still exists.
        std::string selected = selectWord(s, raw.str(), composed.str(), composed.str());
        final.p = BamiCommit(s->h);
        if (!raw.p || !composed.p || !final.p) {
            goError("commitWord");
            return s->mode == Mode::Edit ? s->shown : s->preedit;
        }
        // Explicit learned Vietnamese choices also override automatic restoration.
        bool learned = *cfg.protectEnglish && *cfg.learnCorrections && s->corrections.count(raw.str());
        std::string normal = selected == raw.str() || learned ? selected : final.str();
        if (!alternate) return normal;
        std::string other = normal == raw.str() ? composed.str() : raw.str();
        if (*cfg.protectEnglish && *cfg.learnCorrections && raw.str() != composed.str() && raw.str().size() <= 128) {
            // Bound correction memory; it is cleared with this context or a settings change.
            if (s->corrections.size() >= 256 && !s->corrections.count(raw.str()))
                s->corrections.erase(s->corrections.begin());
            s->corrections[raw.str()] = other == raw.str();
        }
        return other;
    }

    // Apply the core's answer. Returns false if the key must be passed through.
    // `final` is the text BamiCommit() returned when the key ended the word (rc == 0).
    bool apply(fcitx::InputContext *ic, IcState *s, Mode mode, int rc, int deleteN,
               std::string insert, std::string pre, const std::string &final = {}) {
        if (rc == 0) pre.clear(); // the word is over, whatever the core reports as preedit
        else {
            s->rawDisplay = false;
            GoStr raw;
            if (*cfg.protectEnglish && !pre.empty()) raw.p = BamiRaw(s->h);
            if (raw.p) {
                std::string selected = selectWord(s, raw.str(), pre, pre);
                s->rawDisplay = selected == raw.str() && selected != pre;
                if (mode == Mode::Edit && selected != pre) diffText(s->shown, selected, deleteN, insert);
                pre = std::move(selected);
            }
        }
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
        // A consumed Backspace may empty the word; it must not commit the old preedit.
        if (rc != 0 && pre.empty()) {
            endWord(ic, s);
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
        // The final form includes the configured language policy or explicit override.
        std::string text = s->preedit;
        if (!final.empty()) text = final;
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
    d->english.configure(*cfg.protectEnglish ? *cfg.englishDictionary : "");
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
        d->endWord(ic, s, true);
        return false;
    }
    std::string final;
    if (rc == 0 && s->active) {
        final = d->commitWord(s);
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
    int rc = s->rawDisplay ? BamiBackspaceRaw(s->h, &deleteN, &ins.p, &pre.p)
                           : BamiBackspace(s->h, &deleteN, &ins.p, &pre.p);
    if (rc < 0) {
        d->goError("BamiBackspace");
        d->endWord(ic, s, true);
        return false;
    }
    return d->apply(ic, s, mode, rc, deleteN, ins.str(), pre.str());
}

bool Composer::onAlternate(fcitx::InputContext *ic) {
    auto *s = d->st(ic);
    if (!s->active) return false;
    s->bounceToken = 0;
    Mode mode = d->choose(ic, s);
    if (!s->active) return true; // unsafe edit: keep the text and swallow the shortcut
    d->apply(ic, s, mode, 0, 0, {}, {}, d->commitWord(s, true));
    return true;
}

void Composer::onFlush(fcitx::InputContext *ic) {
    auto *s = d->st(ic);
    if (s->active) {
        Mode mode = d->choose(ic, s);
        if (s->active) {
            d->apply(ic, s, mode, 0, 0, {}, {}, d->commitWord(s));
        }
    }
    d->endWord(ic, s);
    s->updated = false; // the forwarded key may edit the text
}

void Composer::onReset(fcitx::InputContext *ic) {
    auto *s = d->st(ic);
    s->bounceToken = 0;
    d->endWord(ic, s);
}

void Composer::onActivate(fcitx::InputContext *ic) {
    auto *s = d->st(ic);
    s->bounceToken = 0; // focus came back in the same turn: keep the syllable
    if (s->active && s->mode == Mode::Preedit && !d->usesClientPreedit(ic))
        d->showPreedit(ic, s->preedit);
}

void Composer::onDeactivate(fcitx::InputContext *ic, bool focusOut) {
    auto *s = d->st(ic);
    if (!s->active) {
        if (!focusOut) {
            s->sticky = s->transientStale = s->recoveryReady = false;
            s->recoveryShown.clear();
            s->updated = false;
        }
        return;
    }
    if (focusOut && s->mode == Mode::Preedit && !d->usesClientPreedit(ic)) {
        // Keep the word in its context; never send a delayed commit to an unfocused client.
        d->showPreedit(ic, {});
        return;
    }
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
    if (!focusOut) {
        onFlush(ic);
        s->sticky = s->transientStale = s->recoveryReady = false;
        s->recoveryShown.clear();
        return;
    }
    d->endWord(ic, s);
}

void Composer::onSurroundingTextUpdated(fcitx::InputContext *ic) {
    auto *s = d->st(ic);
    s->updated = true;
    if (s->sticky && s->transientStale && !s->recoveryReady) {
        auto check = d->inputFor(ic, s);
        check.sticky = false;
        check.shown = s->recoveryShown;
        check.haveExpectedCursor = true;
        check.expectedCursor = s->recoveryCursor;
        s->recoveryReady = decide(check).mode == Mode::Edit;
        if (!s->recoveryReady) s->transientStale = false;
        s->recoveryShown.clear();
    }
}

bool Composer::sticky(fcitx::InputContext *ic) { return d->st(ic)->sticky; }
Mode Composer::wordMode(fcitx::InputContext *ic) {
    auto *s = d->st(ic);
    return s->active ? s->mode : Mode::Preedit;
}
bool Composer::wordActive(fcitx::InputContext *ic) { return d->st(ic)->active; }
Reason Composer::lastReason(fcitx::InputContext *ic) { return d->st(ic)->lastReason; }

} // namespace bami
