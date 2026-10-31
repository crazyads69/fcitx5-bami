// SPDX-License-Identifier: MIT
// The whole addon inside a real fcitx::Instance with the test frontend:
// types "Tieengs Vieetj ddepj" (Telex) and checks what is committed.
#include <functional>
#include <fcitx-config/configuration.h>
#include <fcitx-utils/capabilityflags.h>
#include <fcitx-utils/eventdispatcher.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/macros.h>
#include <fcitx-utils/testing.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputmethodgroup.h>
#include <fcitx/inputmethodmanager.h>
#include <fcitx/instance.h>
#include <fcitx/inputpanel.h>
#include <testfrontend_public.h>
#include "testdir.h"

using namespace fcitx;

static void typeKeys(AddonInstance *fe, const ICUUID &uuid, const std::string &keys) {
    for (char c : keys) {
        fe->call<ITestFrontend::keyEvent>(uuid, c == ' ' ? Key(FcitxKey_space) : Key(std::string(1, c)), false);
    }
}

int main() {
#ifdef BAMI_HAVE_TESTING_PATH
    setupTestingEnvironmentPath(TESTING_BINARY_DIR, {"src"}, {TESTING_BINARY_DIR "/test"});
#else
    setupTestingEnvironment(TESTING_BINARY_DIR, {"src"}, {TESTING_BINARY_DIR "/test"});
#endif
    char arg0[] = "testbami", arg1[] = "--disable=all", arg2[] = "--enable=testim,testfrontend,bami";
    char *argv[] = {arg0, arg1, arg2};
    Instance instance(FCITX_ARRAY_SIZE(argv), argv);
    instance.addonManager().registerDefaultLoader(nullptr);
    EventDispatcher dispatcher;
    dispatcher.attach(&instance.eventLoop());

    dispatcher.schedule([&instance]() {
        auto *bami = instance.addonManager().addon("bami", true);
        FCITX_ASSERT(bami);
        auto group = instance.inputMethodManager().currentGroup();
        group.inputMethodList().clear();
        group.inputMethodList().push_back(InputMethodGroupItem("keyboard-us"));
        group.inputMethodList().push_back(InputMethodGroupItem("bami"));
        group.setDefaultInputMethod("");
        instance.inputMethodManager().setGroup(group);
    });

    // Preedit path (the test frontend has no surrounding text): words are committed
    // exactly as previewed, at the word end. The space itself is passed on.
    dispatcher.schedule([&instance]() {
        auto *fe = instance.addonManager().addon("testfrontend");
        auto uuid = fe->call<ITestFrontend::createInputContext>("testapp");
        auto *ic = instance.inputContextManager().findByUUID(uuid);
        ic->focusIn();
        instance.setCurrentInputMethod(ic, "bami", false);
        fe->call<ITestFrontend::pushCommitExpectation>("Ti\xE1\xBA\xBFng");
        fe->call<ITestFrontend::pushCommitExpectation>("Vi\xE1\xBB\x87t");
        fe->call<ITestFrontend::pushCommitExpectation>("\xC4\x91\xE1\xBA\xB9p");
        typeKeys(fe, uuid, "Tieengs Vieetj ddepj ");
        fe->call<ITestFrontend::destroyInputContext>(uuid);
    });

    // Real key routing: Shift+Backspace overrides automatic English protection.
    dispatcher.schedule([&instance]() {
        auto *fe = instance.addonManager().addon("testfrontend");
        auto uuid = fe->call<ITestFrontend::createInputContext>("mixed-language");
        auto *ic = instance.inputContextManager().findByUUID(uuid);
        ic->focusIn();
        instance.setCurrentInputMethod(ic, "bami", false);
        fe->call<ITestFrontend::pushCommitExpectation>("as");
        typeKeys(fe, uuid, "as ");
        fe->call<ITestFrontend::pushCommitExpectation>("á");
        typeKeys(fe, uuid, "as");
        fe->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_BackSpace, KeyState::Shift), false);
        fe->call<ITestFrontend::destroyInputContext>(uuid);
    });

    // Fcitx's own focus-out commit must see the protected English preview.
    dispatcher.schedule([&instance]() {
        auto *fe = instance.addonManager().addon("testfrontend");
        auto uuid = fe->call<ITestFrontend::createInputContext>("focus-language");
        auto *ic = instance.inputContextManager().findByUUID(uuid);
        ic->setCapabilityFlags(CapabilityFlag::Preedit);
        ic->focusIn();
        instance.setCurrentInputMethod(ic, "bami", false);
        typeKeys(fe, uuid, "as");
        FCITX_ASSERT(ic->inputPanel().clientPreedit().toString() == "as");
        fe->call<ITestFrontend::pushCommitExpectation>("as");
        ic->focusOut();
        fe->call<ITestFrontend::destroyInputContext>(uuid);
    });

    // Panel composition survives a real Fcitx focus bounce without an early commit.
    dispatcher.schedule([&instance]() {
        auto *addon = instance.addonManager().addon("bami");
        RawConfig config;
        addon->getConfig()->save(config);
        config.setValueByPath("ClientPreedit", "False");
        config.setValueByPath("PreeditOnly", "True");
        addon->setConfig(config);
        auto *fe = instance.addonManager().addon("testfrontend");
        auto uuid = fe->call<ITestFrontend::createInputContext>("panel-bounce");
        auto *ic = instance.inputContextManager().findByUUID(uuid);
        ic->setCapabilityFlags(CapabilityFlag::Preedit);
        ic->focusIn();
        instance.setCurrentInputMethod(ic, "bami", false);
        typeKeys(fe, uuid, "Tieeng");
        FCITX_ASSERT(ic->inputPanel().clientPreedit().empty());
        ic->focusOut(); ic->focusIn();
        fe->call<ITestFrontend::pushCommitExpectation>("Tiếng");
        typeKeys(fe, uuid, "s ");
        fe->call<ITestFrontend::destroyInputContext>(uuid);
        config.setValueByPath("ClientPreedit", "True");
        config.setValueByPath("PreeditOnly", "False");
        addon->setConfig(config);
    });

    // Structured field hints: do not transform URL or email addresses.
    dispatcher.schedule([&instance]() {
        auto *fe = instance.addonManager().addon("testfrontend");
        auto uuid = fe->call<ITestFrontend::createInputContext>("address-field");
        auto *ic = instance.inputContextManager().findByUUID(uuid);
        ic->focusIn();
        instance.setCurrentInputMethod(ic, "bami", false);
        for (auto hint : {CapabilityFlag::Url, CapabilityFlag::Email, CapabilityFlag::Password,
                          CapabilityFlag::Sensitive}) {
            ic->setCapabilityFlags(hint);
            for (const char *letter : {"a", "s"}) {
                KeyEvent ev(ic, Key(letter), false);
                ic->keyEvent(ev);
            }
            FCITX_ASSERT(ic->inputPanel().clientPreedit().empty());
        }
        fe->call<ITestFrontend::destroyInputContext>(uuid);
    });

    // Edit path + focus bounce. The test frontend does not model an application, so
    // this script plays the application: it publishes the surrounding text itself.
    dispatcher.schedule([&instance, &dispatcher]() {
        auto *fe = instance.addonManager().addon("testfrontend");
        auto uuid = fe->call<ITestFrontend::createInputContext>("testapp2");
        auto *ic = instance.inputContextManager().findByUUID(uuid);
        ic->setCapabilityFlags(CapabilityFlag::SurroundingText);
        ic->focusIn();
        instance.setCurrentInputMethod(ic, "bami", false);
        auto publish = [ic](const std::string &t, unsigned c) {
            ic->surroundingText().setText(t, c, c);
            ic->updateSurroundingText();
        };
        publish("", 0);
        fe->call<ITestFrontend::pushCommitExpectation>("T"); // edit mode: committed at once
        typeKeys(fe, uuid, "T");
        publish("T", 1);
        // Bounce within one event-loop turn: the syllable survives, "i" is an edit.
        ic->focusOut();
        ic->focusIn();
        fe->call<ITestFrontend::pushCommitExpectation>("i");
        typeKeys(fe, uuid, "i");
        publish("Ti", 2);
        // Real focus loss: the deferred reset must run (next loop turn).
        ic->focusOut();
        // Two hops: the addon's own dispatcher (not ours) runs the deferred reset, and
        // separate event sources are not ordered inside one loop iteration.
        dispatcher.schedule([&instance, &dispatcher, uuid, ic]() { dispatcher.schedule([&instance, uuid, ic]() {
            auto *fe = instance.addonManager().addon("testfrontend");
            ic->focusIn();
            ic->surroundingText().setText("Ti", 2, 2);
            ic->updateSurroundingText();
            // New word after a letter -> preedit, so nothing is committed by this key.
            typeKeys(fe, uuid, "e");
            fe->call<ITestFrontend::destroyInputContext>(uuid);
        }); });
    });

    // Exit after enough loop turns for everything above (each hop is one turn).
    std::function<void(int)> hop = [&](int n) {
        dispatcher.schedule([&, n]() { n ? hop(n - 1) : instance.exit(); });
    };
    hop(6);
    instance.exec();
    return 0;
}
