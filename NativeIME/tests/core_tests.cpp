#include "core/pinyin_engine.hpp"

#include <cstdlib>
#include <iostream>

using nativeime::core::PinyinEngine;

namespace {
int failures = 0;
void Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\\n';
        ++failures;
    }
}
}

int main() {
    const PinyinEngine engine;
    const auto nihao = engine.Query(L"NiHao");
    Expect(nihao.normalized_input == L"nihao", "input should normalize to lowercase");
    Expect(!nihao.candidates.empty(), "known Pinyin should return candidates");
    Expect(nihao.candidates.front().text == L"你好", "top seed candidate should be 你好");

    const auto punctuation = engine.Query(L"ni-hao!");
    Expect(punctuation.normalized_input == L"nihao", "ASCII separators should be ignored");

    const auto mixed = engine.Query(L"你Ni好Hao");
    Expect(mixed.normalized_input == L"nihao", "non-ASCII characters should not enter Pinyin key");

    const auto empty = engine.Query(L"123!?");
    Expect(empty.normalized_input.empty(), "non-letter input should normalize to empty");
    Expect(empty.candidates.empty(), "empty query should not return candidates");

    const auto unknown = engine.Query(L"qwerty");
    Expect(unknown.candidates.empty(), "unknown Pinyin should return no candidates");

    if (failures == 0) {
        std::cout << "All core contract tests passed.\\n";
        return EXIT_SUCCESS;
    }
    std::cerr << failures << " test(s) failed.\\n";
    return EXIT_FAILURE;
}
