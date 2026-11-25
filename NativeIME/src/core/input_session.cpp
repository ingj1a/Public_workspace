#include "core/input_session.hpp"

namespace nativeime::core {
void InputSession::Reset() noexcept {
    preedit_.clear();
    candidates_.clear();
    selected_ = 0;
}
void InputSession::SetChinese(bool enabled) {
    if (enabled != chinese_) Reset();
    chinese_ = enabled;
}
bool InputSession::AcceptCandidates(const std::wstring& input,std::vector<Candidate> candidates) {
    if(input.empty()||input!=preedit_||!chinese_)return false;
    if(candidates.size()>9)candidates.resize(9);
    candidates_=std::move(candidates);selected_=0;return true;
}
void InputSession::Refresh() {
    candidates_ = (query_ ? query_(preedit_) : engine_.Query(preedit_)).candidates;
    if (candidates_.size() > 9) candidates_.resize(9);
    selected_ = 0;
}
InputResult InputSession::Commit(std::size_t index, bool appendSpace) {
    if (preedit_.empty()) return {};
    if (!candidates_.empty() && index >= candidates_.size()) return {};
    InputResult result{true, candidates_.empty() ? preedit_ : candidates_[index].text};
    if (candidates_.empty() && appendSpace) result.commit += L' ';
    Reset();
    return result;
}
InputResult InputSession::Handle(KeyEvent e) {
    if (!chinese_ || e.shortcut) return {};
    switch (e.key) {
    case Key::Letter:
        if (!((e.character >= L'a' && e.character <= L'z') ||
              (e.character >= L'A' && e.character <= L'Z'))) return {};
        if (preedit_.size() >= MaxPreedit) {
            // Flush the raw preedit, not a guessed candidate, before continuing.
            InputResult result{true, preedit_};
            Reset();
            preedit_ += e.character;
            Refresh();
            return result;
        }
        preedit_ += e.character;
        Refresh();
        return {true, {}};
    case Key::Backspace:
        if (preedit_.empty()) return {};
        preedit_.pop_back();
        Refresh();
        return {true, {}};
    case Key::Escape:
        if (preedit_.empty()) return {};
        Reset();
        return {true, {}};
    case Key::Space: return Commit(selected_, true);
    case Key::Enter: return Commit(selected_, false);
    case Key::Digit:
        if (e.character < L'1' || e.character > L'9' ||
            static_cast<std::size_t>(e.character - L'1') >= candidates_.size()) return {};
        return Commit(static_cast<std::size_t>(e.character - L'1'), false);
    case Key::Previous:
        if (candidates_.empty()) return {};
        if (selected_ > 0) --selected_;
        return {true, {}};
    case Key::Next:
        if (candidates_.empty()) return {};
        if (selected_ + 1 < candidates_.size()) ++selected_;
        return {true, {}};
    default: return {};
    }
}
}
