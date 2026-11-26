#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "tsf_popup.hpp"
#include "core/pinyin_correction.hpp"
#include <algorithm>
#include <string>
#include <cwchar>
namespace nativeime::win {
bool ReadNativeEditCaret(HWND edit,RECT& rect) noexcept {
    if(!edit||GetFocus()!=edit||!IsWindowVisible(edit))return false;
    wchar_t klass[16]{};
    if(!GetClassNameW(edit,klass,16)||_wcsicmp(klass,L"Edit")!=0)return false;
    GUITHREADINFO gui{sizeof(gui)};
    if(!GetGUIThreadInfo(GetCurrentThreadId(),&gui)||gui.hwndCaret!=edit)return false;
    POINT p[2]={{gui.rcCaret.left,gui.rcCaret.top},{gui.rcCaret.right,gui.rcCaret.bottom}};
    if(!ClientToScreen(edit,&p[0])||!ClientToScreen(edit,&p[1]))return false;
    rect={p[0].x,p[0].y,p[1].x,p[1].y};
    return rect.bottom>rect.top&&rect.right>=rect.left;
}
bool ReadCaretRect(ITfContext* context,TfEditCookie cookie,ITfRange* range,RECT& rect) noexcept {
    if(!context||!range)return false;
    ITfContextView* view=nullptr;
    if(FAILED(context->GetActiveView(&view))||!view)return false;
    BOOL clipped=TRUE;
    const HRESULT hr=view->GetTextExt(cookie,range,&rect,&clipped);
    view->Release();
    return hr==S_OK&&!clipped&&rect.bottom>rect.top&&rect.right>=rect.left;
}
void TsfPopup::Hide() noexcept {if(window_)ShowWindow(window_,SW_HIDE);}
void TsfPopup::Destroy() noexcept {
    if(window_)DestroyWindow(window_);
    window_=nullptr;owner_=nullptr;
}
bool TsfPopup::Show(HWND owner,const RECT& anchor,const core::InputSession& session) noexcept {
    try {
        if(!owner||!IsWindow(owner)||session.Preedit().empty()||session.Candidates().empty()){
            Hide();return false;
        }
        owner=GetAncestor(owner,GA_ROOT);
        if(!owner||!IsWindowVisible(owner)){Hide();return false;}
        if(window_&&(!IsWindow(window_)||owner_!=owner))Destroy();
        if(!window_){
            // A system window class avoids registering code that could outlive the TSF DLL.
            window_=CreateWindowExW(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,L"STATIC",L"",
                WS_POPUP|WS_BORDER|WS_DISABLED|SS_LEFTNOWORDWRAP|SS_NOPREFIX,
                0,0,0,0,owner,nullptr,GetModuleHandleW(nullptr),nullptr);
            if(!window_)return false;
            owner_=owner;
            SendMessageW(window_,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),FALSE);
        }
        std::wstring text=session.Preedit();
        for(std::size_t i=0;i<session.Candidates().size();++i){
            text+=L"\r\n";
            text+=(i==session.Selected()?L"> ":L"  ");
            text+=std::to_wstring(i+1)+L" "+session.Candidates()[i].text;
            const auto& tag=session.Candidates()[i].annotation;
            if(tag==core::CorrectionTag||tag==core::RawPinyinTag)text+=L"  ["+tag+L"]";
        }
        MONITORINFO mi{sizeof(mi)};
        if(!GetMonitorInfoW(MonitorFromRect(&anchor,MONITOR_DEFAULTTONEAREST),&mi)){Hide();return false;}
        HDC dc=GetDC(window_);
        if(!dc){Hide();return false;}
        HGDIOBJ prev=SelectObject(dc,GetStockObject(DEFAULT_GUI_FONT));
        RECT measure{};
        DrawTextW(dc,text.c_str(),-1,&measure,DT_CALCRECT|DT_NOPREFIX);
        if(prev)SelectObject(dc,prev);
        ReleaseDC(window_,dc);
        const int workW=mi.rcWork.right-mi.rcWork.left,workH=mi.rcWork.bottom-mi.rcWork.top;
        const int width=std::min(workW,std::max(180,static_cast<int>(measure.right)+24));
        const int height=static_cast<int>(measure.bottom)+16;
        // Never display a clipped candidate list: numeric selection is gated by visibility.
        if(width<measure.right+4||height>workH){Hide();return false;}
        const int x=std::clamp<int>(anchor.left,mi.rcWork.left,mi.rcWork.right-width);
        int y=anchor.bottom+4;
        if(y+height>mi.rcWork.bottom)y=anchor.top-height-4;
        y=std::clamp<int>(y,mi.rcWork.top,mi.rcWork.bottom-height);
        if(!SetWindowTextW(window_,text.c_str())||
           !SetWindowPos(window_,HWND_TOP,x,y,width,height,SWP_NOACTIVATE|SWP_SHOWWINDOW)){
            Hide();return false;
        }
        return Visible();
    }catch(...){Hide();return false;}
}
}
