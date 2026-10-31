// SPDX-License-Identifier: MIT
// Exercise the public C ABI, including ownership and invalid handles.
#include <cstdio>
#include <cstring>
#include <initializer_list>
extern "C" {
#include "bami-core.h"
}

int main() {
    int failures = 0;
#define CHECK(c) do { if (!(c)) { ++failures; std::printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)
    BamiInit();
    auto h = BamiNew(0, 0, 7);
    CHECK(h != 0);
    for (unsigned char c : "tieengs") {
        if (!c) break;
        int deleted = -1;
        char *insert = nullptr, *preedit = nullptr;
        CHECK(BamiKey(h, c, &deleted, &insert, &preedit) == 1);
        CHECK(deleted >= 0 && insert && preedit);
        BamiFreeString(insert);
        BamiFreeString(preedit);
    }
    char *text = BamiRaw(h);
    CHECK(text && !std::strcmp(text, "tieengs"));
    BamiFreeString(text);
    text = BamiCommit(h);
    CHECK(text && !std::strcmp(text, "tiếng"));
    BamiFreeString(text);
    CHECK(BamiBackspace(h, nullptr, nullptr, nullptr) == 0);
    CHECK(BamiKey(h, 'a', nullptr, nullptr, nullptr) == 1);
    CHECK(BamiEnglishScore(h) >= 0 && BamiEnglishScore(h) <= 1);
    CHECK(BamiEnglishThreshold() > 0.5 && BamiEnglishThreshold() <= 1);
    BamiReset(h);
    text = BamiPreedit(h);
    CHECK(text && !*text);
    BamiFreeString(text);
    BamiConfigure(h, 5, 0, 7); // VNI
    CHECK(BamiKey(h, 'a', nullptr, nullptr, nullptr) == 1);
    CHECK(BamiKey(h, '6', nullptr, nullptr, nullptr) == 1);
    text = BamiCommit(h);
    CHECK(text && !std::strcmp(text, "â"));
    BamiFreeString(text);
    BamiFree(h);
    // Freed and null handles must be contained at the language boundary.
    for (auto invalid : {h, decltype(h)(0)}) {
        int deleted = 123;
        char sentinel = 0;
        char *insert = &sentinel, *preedit = &sentinel;
        CHECK(BamiKey(invalid, 'a', &deleted, &insert, &preedit) == -1);
        CHECK(deleted == 0 && !insert && !preedit);
        CHECK(BamiBackspace(invalid, nullptr, nullptr, nullptr) == -1);
        CHECK(BamiBackspaceRaw(invalid, nullptr, nullptr, nullptr) == -1);
        CHECK(BamiCommit(invalid) == nullptr);
        CHECK(BamiPreedit(invalid) == nullptr);
        CHECK(BamiRaw(invalid) == nullptr);
        CHECK(BamiEnglishScore(invalid) == -1);
        BamiReset(invalid);
        BamiConfigure(invalid, 0, 0, 7);
        BamiFree(invalid);
    }
    BamiFreeString(nullptr);
    return failures ? 1 : 0;
}
