#pragma once

#include <string>
#include <vector>

namespace nativeime::core {

struct Candidate {
    std::wstring text;
    int score = 0;
    std::wstring annotation;
};

struct QueryResult {
    std::wstring normalized_input;
    std::vector<Candidate> candidates;
};

// A small, deterministic interface boundary for the first milestone.
// The seed lexicon is only a test fixture, not a production dictionary.
class PinyinEngine final {
public:
    QueryResult Query(const std::wstring& input) const;
    static std::wstring Normalize(const std::wstring& input);
};

} // namespace nativeime::core
