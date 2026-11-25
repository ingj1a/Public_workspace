#include "core/input_session.hpp"
#include <iostream>
#include <cstdlib>
using namespace nativeime::core;
int failures = 0;
void check(bool ok, const char* name) {
    if (!ok) { std::cerr << name << '\n'; ++failures; }
}
void type(InputSession& s, const wchar_t* text) {
    for (; *text; ++text) check(s.Handle({Key::Letter, *text}).consumed, "letter consumed");
}
int main() {
    InputSession s;
    check(!s.Handle({Key::Space}).consumed, "idle space passes");
    check(!s.Handle({Key::Backspace}).consumed, "idle backspace passes");
    type(s,L"nihao");
    check(s.Preedit()==L"nihao" && s.Candidates().size()==2, "composition and candidates");
    check(!s.Handle({Key::Letter,L's',true}).consumed && s.Preedit()==L"nihao", "shortcut preserves preedit");
    check(!s.Handle({Key::Digit,L'9'}).consumed && s.Preedit()==L"nihao", "invalid candidate passes");
    check(s.Handle({Key::Next}).consumed && s.Selected()==1, "next");
    check(s.Handle({Key::Previous}).consumed && s.Selected()==0, "previous");
    check(s.Handle({Key::Digit,L'2'}).commit==L"拟好", "second candidate");
    check(s.Preedit().empty() && !s.Handle({Key::Enter}).consumed, "no duplicate commit");
    type(s,L"nihaox"); s.Handle({Key::Backspace});
    check(s.Handle({Key::Space}).commit==L"你好", "backspace then space");
    type(s,L"qwerty");
    check(s.Handle({Key::Space}).commit==L"qwerty ", "unknown preserves raw with space");
    type(s,L"Qwerty");
    check(s.Handle({Key::Enter}).commit==L"Qwerty", "unknown preserves raw case");
    type(s,L"nihao"); s.Handle({Key::Escape});
    check(s.Preedit().empty(), "cancel");
    type(s,L"nihao"); s.SetChinese(false);
    check(s.Preedit().empty() && !s.Handle({Key::Letter,L'a'}).consumed, "English pass through");
    s.SetChinese(true); type(s,L"ceshi");
    check(s.Handle({Key::Enter}).commit==L"测试", "return to Chinese");
    for (std::size_t i=0;i<InputSession::MaxPreedit;++i) s.Handle({Key::Letter,L'x'});
    auto overflow=s.Handle({Key::Letter,L'y'});
    check(overflow.commit==std::wstring(InputSession::MaxPreedit,L'x') && s.Preedit()==L"y", "bounded buffer preserves raw");
    s.Reset(); check(s.Preedit().empty() && s.Candidates().empty(), "reset clears state");
    return failures ? EXIT_FAILURE : EXIT_SUCCESS;
}
