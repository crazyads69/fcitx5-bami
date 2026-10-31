// SPDX-License-Identifier: MIT
#ifndef BAMI_CONFIG_H
#define BAMI_CONFIG_H

#include <fcitx-config/configuration.h>
#include <fcitx-config/enum.h>
#include <fcitx-utils/i18n.h>

namespace bami {

// Integer values are part of the C contract with the Go bridge (BamiNew /
// BamiConfigure): keep them in sync with the header comment of bridge/bami.go.
enum class Variant { Telex, Telex2, TelexW, SimpleTelex1, SimpleTelex2, VNI, VIQR, MS, TelexVNI };
FCITX_CONFIG_ENUM_NAME_WITH_I18N(Variant, N_("Telex"), N_("Telex 2"), N_("Telex W"),
                                 N_("Simple Telex 1"), N_("Simple Telex 2"), N_("VNI"),
                                 N_("VIQR"), N_("MS layout"), N_("Telex + VNI"));

enum class ToneStyle { Old, Modern };
FCITX_CONFIG_ENUM_NAME_WITH_I18N(ToneStyle, N_("Old (hòa, thủy)"), N_("Modern (hoà, thuỷ)"));

enum class Gate { VariantDefault, Off, AfterLetter, Everywhere }; // = bridge gate values
FCITX_CONFIG_ENUM_NAME_WITH_I18N(Gate, N_("Variant default"), N_("Off"), N_("After a letter"),
                                 N_("Everywhere"));

FCITX_CONFIGURATION(
    BamiConfig,
    fcitx::OptionWithAnnotation<Variant, VariantI18NAnnotation> variant{
        this, "Variant", _("Typing method"), Variant::Telex};
    fcitx::OptionWithAnnotation<ToneStyle, ToneStyleI18NAnnotation> toneStyle{
        this, "ToneStyle", _("Tone mark placement"), ToneStyle::Old};
    fcitx::OptionWithAnnotation<Gate, GateI18NAnnotation> loneW{
        this, "LoneW", _("Lone w types ư"), Gate::VariantDefault};
    fcitx::OptionWithAnnotation<Gate, GateI18NAnnotation> brackets{
        this, "Brackets", _("[ ] type ơ ư"), Gate::VariantDefault};
    fcitx::Option<bool> quickTelex{this, "QuickTelex", _("Quick Telex (cc→ch, gg→gi ...)"), false};
    fcitx::Option<bool> freeTone{this, "FreeTone", _("Free tone mark placement"), true};
    fcitx::Option<bool> spellCheck{this, "SpellCheck", _("Only transform possible Vietnamese syllables"), false};
    fcitx::Option<bool> ddAnywhere{this, "DdAnywhere", _("dd always types đ"), false};
    fcitx::Option<bool> clientPreedit{this, "ClientPreedit",
        _("Show composition inside the application (off: Fcitx panel)"), true};
    fcitx::Option<bool> preeditOnly{this, "PreeditOnly",
        _("Use preedit only (never delete application text)"), false};
    fcitx::Option<bool> protectEnglish{this, "ProtectEnglish",
                                      _("Keep recognized English words unchanged"), true};
    fcitx::Option<bool> englishModel{this, "EnglishModel",
                                    _("Use local English prediction model (Telex)"), true};
    fcitx::Option<bool> learnCorrections{this, "LearnCorrections",
        _("Remember language corrections in this input context (memory only)"), true};
    fcitx::Option<std::string> englishDictionary{this, "EnglishDictionary",
                                               _("English dictionary"), "en_US"};
    fcitx::Option<bool> restore{this, "RestoreNonVietnamese",
                                _("Restore raw keys of non-Vietnamese words at word end"), false};
    fcitx::Option<bool> underlinePreedit{this, "UnderlinePreedit",
                                         _("Underline the preedit text"), false};);

// Bit layout of BamiNew/BamiConfigure `flags` (bridge/bami.go): gates are 0 = variant
// default, 1 = off, 2 = after a letter, 3 = everywhere (same numbers as enum Gate).
enum : unsigned {
    kFlagFreeTone = 1u << 0,
    kFlagSpellCheck = 1u << 1,
    kFlagRestore = 1u << 2,
    kFlagDdAnywhere = 1u << 3,
    kFlagQuick = 1u << 4,
    kLoneWShift = 5,
    kBracketShift = 7,
};

struct CoreParams {
    int variant;
    int toneStyle;
    unsigned flags;
};

inline CoreParams coreParams(const BamiConfig &c) {
    unsigned f = 0;
    if (*c.freeTone) f |= kFlagFreeTone;
    if (*c.spellCheck) f |= kFlagSpellCheck;
    if (*c.ddAnywhere) f |= kFlagDdAnywhere;
    if (*c.restore) f |= kFlagRestore;
    if (*c.quickTelex) f |= kFlagQuick;
    f |= static_cast<unsigned>(*c.loneW) << kLoneWShift;
    f |= static_cast<unsigned>(*c.brackets) << kBracketShift;
    return {static_cast<int>(*c.variant), static_cast<int>(*c.toneStyle), f};
}

} // namespace bami

#endif
