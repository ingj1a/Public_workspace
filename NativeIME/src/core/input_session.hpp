#pragma once
#include "core/pinyin_engine.hpp"
#include <cstddef>
#include <functional>
#include <utility>

namespace nativeime::core {
enum class Key { Letter, Space, Enter, Backspace, Escape, Previous, Next, First, Last, Digit, Other };
struct KeyEvent {
    Key key = Key::Other;
    wchar_t character = 0;
    bool shortcut = false;
};
struct InputResult {
    bool consumed = false;
    std::wstring commit;
};
class InputSession {
public:
    using QueryProvider = std::function<QueryResult(const std::wstring&)>;
    explicit InputSession(QueryProvider query = {}) : query_(std::move(query)) {}
    InputResult Handle(KeyEvent event);
    void SetChinese(bool enabled);
    void Reset() noexcept;
    void ClearCandidates() noexcept { candidates_.clear(); selected_=0; }
    bool AcceptCandidates(const std::wstring& input, std::vector<Candidate> candidates);
    bool Chinese() const noexcept { return chinese_; }
    const std::wstring& Preedit() const noexcept { return preedit_; }
    const std::vector<Candidate>& Candidates() const noexcept { return candidates_; }
    std::size_t Selected() const noexcept { return selected_; }
    static constexpr std::size_t MaxPreedit = 64;
private:
    void Refresh();
    InputResult Commit(std::size_t index, bool appendSpace);
    PinyinEngine engine_;
    QueryProvider query_;
    bool chinese_ = true;
    std::wstring preedit_;
    std::vector<Candidate> candidates_;
    std::size_t selected_ = 0;
};
}
