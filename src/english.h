// SPDX-License-Identifier: MIT
#ifndef BAMI_ENGLISH_H
#define BAMI_ENGLISH_H
#include <string>
#include <enchant.h>

namespace bami {
class EnglishDictionary {
public:
    EnglishDictionary() = default;
    ~EnglishDictionary();
    EnglishDictionary(const EnglishDictionary &) = delete;
    EnglishDictionary &operator=(const EnglishDictionary &) = delete;
    void configure(const std::string &language);
    bool contains(const std::string &word) const;
private:
    EnchantBroker *broker_ = nullptr;
    EnchantDict *dict_ = nullptr;
    std::string language_;
};
}
#endif
