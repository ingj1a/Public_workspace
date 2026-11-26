#pragma once
#include <windows.h>
#include <msctf.h>
#include "core/input_session.hpp"
namespace nativeime::win {
// Keyboard-only native popup. Never activate or steal host focus.
class TsfPopup final {
public:
    ~TsfPopup(){Destroy();}
    TsfPopup()=default;
    TsfPopup(const TsfPopup&)=delete;
    TsfPopup& operator=(const TsfPopup&)=delete;
    bool Show(HWND owner, const RECT& anchor, const core::InputSession& session) noexcept;
    void Hide() noexcept;
    void Destroy() noexcept;
    bool Visible() const noexcept {return window_&&IsWindowVisible(window_);}
private:
    HWND window_=nullptr;
    HWND owner_=nullptr;
};
bool ReadNativeEditCaret(HWND edit,RECT& rect) noexcept;
bool ReadCaretRect(ITfContext* context,TfEditCookie cookie,ITfRange* collapsed,RECT& rect) noexcept;
}
