#include "core/input_session.hpp"
#include "core/pinyin_correction.hpp"
#include <algorithm>

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
    candidates.erase(std::remove_if(candidates.begin(),candidates.end(),[](const Candidate& c){return c.text.empty();}),candidates.end());
    if(candidates.size()>9)candidates.resize(9);
    for(auto& candidate:candidates)if(candidate.annotation==RawPinyinTag)candidate.text=preedit_;
    candidates_=std::move(candidates);selected_=0;return true;
}
void InputSession::Refresh() {
    ClearCandidates();
    if(preedit_.empty())return;
    AcceptCandidates(preedit_,(query_ ? query_(preedit_) : engine_.Query(preedit_)).candidates);
}
InputResult InputSession::Commit(std::size_t index, bool appendSpace) {
    if (preedit_.empty()) return {};
    if (!candidates_.empty() && index >= candidates_.size()) return {};
    const bool raw=candidates_.empty()||candidates_[index].annotation==RawPinyinTag;
    InputResult result{true,raw?preedit_:candidates_[index].text};
    if(appendSpace&&raw)result.commit+=L' ';
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
    case Key::First:
        if(candidates_.empty())return {};
        selected_=0;return {true,{}};
    case Key::Last:
        if(candidates_.empty())return {};
        selected_=candidates_.size()-1;return {true,{}};
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
