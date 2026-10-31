// SPDX-License-Identifier: MIT
#include <dlfcn.h>
#include <fcitx-utils/capabilityflags.h>
#include <fcitx-utils/eventdispatcher.h>
#include <fcitx-config/iniparser.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/keysym.h>
#include <fcitx-utils/log.h>
#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>
#include "composer.h"

namespace bami {

namespace {

// fcitx5-bamboo is also a Go c-archive. Two Go runtimes in one process fight over
// signal handlers and thread state, so refuse to start when it is already loaded:
// either its addon instance exists, or libbamboo.so is mapped (RTLD_NOLOAD never
// loads it) and exports its Go entry point. If it is loaded AFTER bami we cannot
// stop it; the README tells users to keep only one of the two enabled.
bool otherGoRuntimeLoaded(fcitx::Instance *instance) {
    if (instance->addonManager().addon("bamboo")) return true;
    if (void *h = dlopen("libbamboo.so", RTLD_NOW | RTLD_NOLOAD)) {
        bool goLib = dlsym(h, "EngineProcessKeyEvent") != nullptr;
        dlclose(h);
        return goLib;
    }
    return false;
}

class BamiEngine : public fcitx::InputMethodEngineV3 {
public:
    explicit BamiEngine(fcitx::Instance *instance)
        : instance_(instance),
          composer_(instance->inputContextManager(),
                    [this](std::function<void()> f) { dispatcher_.schedule(std::move(f)); }) {
        // Instance::eventDispatcher() does not exist in Fcitx 5.0.14: own one.
        dispatcher_.attach(&instance->eventLoop());
        updateWatch_ = instance->watchEvent(
            fcitx::EventType::InputContextSurroundingTextUpdated, fcitx::EventWatcherPhase::Default,
            [this](fcitx::Event &e) {
                composer_.onSurroundingTextUpdated(static_cast<fcitx::InputContextEvent &>(e).inputContext());
            });
        reloadConfig();
    }

    void keyEvent(const fcitx::InputMethodEntry &, fcitx::KeyEvent &ev) override {
        auto *ic = ev.inputContext();
        if (ev.isRelease()) return;
        if (ic->capabilityFlags().test(fcitx::CapabilityFlag::PasswordOrSensitive)) {
            composer_.onReset(ic); // a word composed before the field turned sensitive must not reach it
            return;
        }
        if (ic->capabilityFlags().testAny(fcitx::CapabilityFlags{
                fcitx::CapabilityFlag::Url, fcitx::CapabilityFlag::Email})) {
            composer_.onFlush(ic);
            return;
        }
        const fcitx::Key key = ev.key();
        const fcitx::KeyStates mods = key.states() & fcitx::KeyStates{fcitx::KeyState::Ctrl, fcitx::KeyState::Alt,
                                                                       fcitx::KeyState::Super};
        if (key.isModifier()) return;
        if (mods) {
            composer_.onFlush(ic);
            return;
        }
        if (key.sym() == FcitxKey_BackSpace) {
            // Normalized Backspace drops Shift on older Fcitx; retain the raw modifier.
            bool consumed = ev.rawKey().states().test(fcitx::KeyState::Shift)
                                ? composer_.onAlternate(ic) : composer_.onBackspace(ic);
            if (consumed) ev.filterAndAccept();
            return;
        }
        uint32_t rune = fcitx::Key::keySymToUnicode(key.sym());
        if (rune >= 0x20 && rune != 0x7f) {
            if (composer_.onRune(ic, rune)) ev.filterAndAccept();
            return;
        }
        composer_.onFlush(ic); // Return, Tab, arrows, Escape ...
    }

    void activate(const fcitx::InputMethodEntry &, fcitx::InputContextEvent &ev) override {
        composer_.onActivate(ev.inputContext());
    }
    void deactivate(const fcitx::InputMethodEntry &, fcitx::InputContextEvent &ev) override {
        composer_.onDeactivate(ev.inputContext(), ev.type() == fcitx::EventType::InputContextFocusOut);
    }
    void reset(const fcitx::InputMethodEntry &, fcitx::InputContextEvent &ev) override {
        composer_.onReset(ev.inputContext());
    }

    const fcitx::Configuration *getConfig() const override { return &config_; }
    void setConfig(const fcitx::RawConfig &raw) override {
        config_.load(raw, true);
        fcitx::safeSaveAsIni(config_, kConfigPath);
        composer_.setConfig(config_);
    }
    void reloadConfig() override {
        fcitx::readAsIni(config_, kConfigPath);
        composer_.setConfig(config_);
    }

private:
    static constexpr const char *kConfigPath = "conf/bami.conf";
    fcitx::Instance *instance_;
    fcitx::EventDispatcher dispatcher_;
    BamiConfig config_;
    Composer composer_;
    std::unique_ptr<fcitx::HandlerTableEntry<fcitx::EventHandler>> updateWatch_;
};

class BamiFactory : public fcitx::AddonFactory {
public:
    fcitx::AddonInstance *create(fcitx::AddonManager *manager) override {
        if (otherGoRuntimeLoaded(manager->instance())) {
            FCITX_ERROR() << "bami: fcitx5-bamboo (another Go runtime) is loaded; refusing to start. "
                             "Disable one of them.";
            return nullptr;
        }
        return new BamiEngine(manager->instance());
    }
};

} // namespace
} // namespace bami

FCITX_ADDON_FACTORY(bami::BamiFactory)
