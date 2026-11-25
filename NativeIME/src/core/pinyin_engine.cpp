#include "core/pinyin_engine.hpp"

#include <cwctype>
#include <unordered_map>

namespace nativeime::core {
namespace {
const std::unordered_map<std::wstring, std::vector<Candidate>>& SeedLexicon() {
    static const std::unordered_map<std::wstring, std::vector<Candidate>> lexicon{
        {L"nihao", {{L"你好", 100, L"常用问候"}, {L"拟好", 20, L""}}},
        {L"zhongguo", {{L"中国", 100, L"国家名称"}}},
        {L"shijie", {{L"世界", 100, L"常用词"}}},
        {L"ceshi", {{L"测试", 100, L"常用词"}}},
        {L"tupian", {{L"图片", 100, L"设计常用词"}}}
    };
    return lexicon;
}
}

std::wstring PinyinEngine::Normalize(const std::wstring& input) {
    std::wstring output;
    output.reserve(input.size());
    for (wchar_t ch : input) {
        // Pinyin syllables are ASCII. Keep normalization deterministic across
        // Windows locale settings; punctuation and non-ASCII chars are ignored.
        if (ch >= L'A' && ch <= L'Z') {
            output.push_back(static_cast<wchar_t>(ch - L'A' + L'a'));
        } else if (ch >= L'a' && ch <= L'z') {
            output.push_back(ch);
        }
    }
    return output;
}

QueryResult PinyinEngine::Query(const std::wstring& input) const {
    QueryResult result;
    result.normalized_input = Normalize(input);
    if (result.normalized_input.empty()) {
        return result;
    }

    const auto& lexicon = SeedLexicon();
    const auto it = lexicon.find(result.normalized_input);
    if (it != lexicon.end()) {
        result.candidates = it->second;
    }
    return result;
}

} // namespace nativeime::core
