// SPDX-License-Identifier: MIT
#include "english.h"
#include <string_view>
#include <fcitx-utils/log.h>

namespace bami {
EnglishDictionary::~EnglishDictionary() {
    if (dict_) enchant_broker_free_dict(broker_, dict_);
    if (broker_) enchant_broker_free(broker_);
}

void EnglishDictionary::configure(const std::string &language) {
    if (language == language_) return;
    if (dict_) enchant_broker_free_dict(broker_, dict_);
    dict_ = nullptr;
    language_ = language;
    if (language.empty()) return;
    if (!broker_) broker_ = enchant_broker_init();
    if (broker_) dict_ = enchant_broker_request_dict(broker_, language.c_str());
    if (!dict_) FCITX_WARN() << "bami: English dictionary unavailable: " << language
                            << "; dictionary checks are unavailable";
}

bool EnglishDictionary::contains(const std::string &word) const {
    // shortcut: alphabetic words up to 128 bytes with a vowel; extend for token-aware completion.
    // Consonant-only abbreviations such as dd must remain Vietnamese typing keys.
    if (!dict_ || word.empty() || word.size() > 128) return false;
    bool vowel = false;
    for (unsigned char c : word) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) return false;
        if (std::string_view("aeiouyAEIOUY").find(c) != std::string::npos) vowel = true;
    }
    return vowel && enchant_dict_check(dict_, word.c_str(), word.size()) == 0;
}
}
