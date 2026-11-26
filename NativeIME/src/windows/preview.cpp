// Native Win32 development harness, not a system input method.
#include <windows.h>
#include <string>
#include <algorithm>
#include <windowsx.h>
#include "core/input_session.hpp"
using namespace nativeime::core;
namespace {
InputSession session;
HWND editor, preedit, candidates, modeButton, clearButton;
WNDPROC originalEdit;
HFONT font;
HWND popup;
void Insert(const std::wstring& text);
void Render();
LRESULT CALLBACK PopupProc(HWND hwnd,UINT msg,WPARAM w,LPARAM l) {
    if(msg==WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if(msg==WM_LBUTTONUP) {
        const int index=(GET_Y_LPARAM(l)-34)/32;
        if(GET_Y_LPARAM(l)>=34 && index>=0 && index<9 && static_cast<std::size_t>(index)<session.Candidates().size()) {
            auto r=session.Handle({Key::Digit,static_cast<wchar_t>(L'1'+index)});
            Insert(r.commit); Render();
        }
        return 0;
    }
    if(msg==WM_PAINT) {
        PAINTSTRUCT ps{}; HDC dc=BeginPaint(hwnd,&ps); RECT r{};GetClientRect(hwnd,&r);
        FillRect(dc,&r,GetSysColorBrush(COLOR_WINDOW)); SetBkMode(dc,TRANSPARENT);
        SelectObject(dc,font);SetTextColor(dc,GetSysColor(COLOR_WINDOWTEXT));
        RECT header{12,6,r.right-12,32}; DrawTextW(dc,session.Preedit().c_str(),-1,&header,DT_SINGLELINE|DT_END_ELLIPSIS|DT_NOPREFIX);
        for(std::size_t i=0;i<session.Candidates().size();++i){
            RECT row{4,34+static_cast<LONG>(i)*32,r.right-4,66+static_cast<LONG>(i)*32};
            if(i==session.Selected()) FillRect(dc,&row,GetSysColorBrush(COLOR_HIGHLIGHT));
            SetTextColor(dc,GetSysColor(i==session.Selected()?COLOR_HIGHLIGHTTEXT:COLOR_WINDOWTEXT));
            auto label=std::to_wstring(i+1)+L"   "+session.Candidates()[i].text;
            row.left+=10;DrawTextW(dc,label.c_str(),-1,&row,DT_SINGLELINE|DT_VCENTER|DT_END_ELLIPSIS|DT_NOPREFIX);
        }
        EndPaint(hwnd,&ps);return 0;
    }
    return DefWindowProcW(hwnd,msg,w,l);
}
void UpdatePopup(){
    if(!popup)return;
    if(session.Preedit().empty()||GetFocus()!=editor){ShowWindow(popup,SW_HIDE);return;}
    POINT pt{};if(!GetCaretPos(&pt)){ShowWindow(popup,SW_HIDE);return;}
    ClientToScreen(editor,&pt);
    MONITORINFO mi{sizeof(mi)};if(!GetMonitorInfoW(MonitorFromPoint(pt,MONITOR_DEFAULTTONEAREST),&mi))return;
    const int width=std::min<int>(300,mi.rcWork.right-mi.rcWork.left);
    const int height=38+32*static_cast<int>(session.Candidates().size());
    const int x=std::clamp<int>(pt.x,mi.rcWork.left,std::max<int>(mi.rcWork.left,mi.rcWork.right-width));
    int y=pt.y+24;if(y+height>mi.rcWork.bottom)y=pt.y-height-4;
    y=std::max<int>(mi.rcWork.top,y);
    SetWindowPos(popup,HWND_TOP,x,y,width,height,SWP_NOACTIVATE|SWP_SHOWWINDOW);
    InvalidateRect(popup,nullptr,TRUE);
}
void Render() {
    SetWindowTextW(preedit, session.Preedit().empty()?L"Ready":session.Preedit().c_str());
    std::wstring line;
    for(std::size_t i=0;i<session.Candidates().size() && i<9;++i) {
        if(i) line+=L"    ";
        if(i==session.Selected()) line+=L"[";
        line+=std::to_wstring(i+1)+L" "+session.Candidates()[i].text;
        if(i==session.Selected()) line+=L"]";
    }
    SetWindowTextW(candidates,line.empty()?L"No candidates":line.c_str());
    SetWindowTextW(modeButton,session.Chinese()?L"Chinese / 中文":L"English / 英文");
    UpdatePopup();
}
void Insert(const std::wstring& text) {
    if(!text.empty()) SendMessageW(editor,EM_REPLACESEL,TRUE,reinterpret_cast<LPARAM>(text.c_str()));
}
LRESULT CALLBACK EditProc(HWND hwnd,UINT msg,WPARAM w,LPARAM l) {
    if(msg==WM_KILLFOCUS) { session.Reset(); Render(); }
    if(msg==WM_KEYDOWN) {
        bool shortcut=(GetKeyState(VK_CONTROL)&0x8000)||(GetKeyState(VK_MENU)&0x8000)||
            (GetKeyState(VK_LWIN)&0x8000)||(GetKeyState(VK_RWIN)&0x8000);
        Key key=Key::Other;
        if(w==VK_UP) key=Key::Previous;
        if(w==VK_DOWN) key=Key::Next;
        if(w==VK_ESCAPE) key=Key::Escape;
        if(key!=Key::Other && session.Handle({key,0,shortcut}).consumed) { Render(); return 0; }
    }
    if(msg==WM_CHAR) {
        const wchar_t ch=static_cast<wchar_t>(w);
        Key key=Key::Other;
        if((ch>=L'a'&&ch<=L'z')||(ch>=L'A'&&ch<=L'Z')) key=Key::Letter;
        else if(ch>=L'1'&&ch<=L'9') key=Key::Digit;
        else if(ch==L' ') key=Key::Space;
        else if(ch==L'\r') key=Key::Enter;
        else if(ch==L'\b') key=Key::Backspace;
        const bool shortcut=(GetKeyState(VK_CONTROL)&0x8000)||(GetKeyState(VK_MENU)&0x8000);
        auto result=session.Handle({key,ch,shortcut});
        if(result.consumed) { Insert(result.commit); Render(); return 0; }
        // Keep raw input in the document before punctuation / unsupported characters.
        if(!shortcut && !session.Preedit().empty() && ch>=32) {
            Insert(session.Preedit()); session.Reset(); Render();
        }
    }
    return CallWindowProcW(originalEdit,hwnd,msg,w,l);
}
HWND Child(HWND parent,const wchar_t* klass,const wchar_t* label,DWORD style,int id) {
    HWND h=CreateWindowExW(0,klass,label,WS_CHILD|WS_VISIBLE|style,0,0,0,0,parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
    if(h) SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
    return h;
}
HWND heading, helper;
LRESULT CALLBACK WndProc(HWND hwnd,UINT msg,WPARAM w,LPARAM l) {
    switch(msg) {
    case WM_CREATE: {
        font=static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
        heading=Child(hwnd,L"STATIC",L"NativeIME  /  Input lab",0,10);
        helper=Child(hwnd,L"STATIC",L"Type nihao, ceshi, tupian. Space / 1–9 to select; Esc to cancel.",0,11);
        modeButton=Child(hwnd,L"BUTTON",L"Chinese / 中文",BS_PUSHBUTTON|WS_TABSTOP,1);
        clearButton=Child(hwnd,L"BUTTON",L"Clear",BS_PUSHBUTTON|WS_TABSTOP,2);
        editor=Child(hwnd,L"EDIT",L"",WS_BORDER|ES_MULTILINE|ES_AUTOVSCROLL|WS_VSCROLL|WS_TABSTOP,3);
        preedit=Child(hwnd,L"STATIC",L"Ready",SS_LEFT,4);
        candidates=Child(hwnd,L"STATIC",L"No candidates",SS_LEFT,5);
        if(!editor||!preedit||!candidates||!modeButton||!clearButton||!heading||!helper) return -1;
        originalEdit=reinterpret_cast<WNDPROC>(SetWindowLongPtrW(editor,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(EditProc)));
        WNDCLASSW pc{};pc.lpfnWndProc=PopupProc;pc.hInstance=GetModuleHandleW(nullptr);pc.lpszClassName=L"NativeIME.Candidates";pc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
        RegisterClassW(&pc);
        popup=CreateWindowExW(WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,pc.lpszClassName,L"候选词",WS_POPUP|WS_BORDER,0,0,0,0,hwnd,nullptr,pc.hInstance,nullptr);
        SetFocus(editor); return 0;
    }
    case WM_SIZE: {
        const int width=LOWORD(l),height=HIWORD(l);
        if(!editor) return 0;
        const int body=width>48?width-48:1;
        MoveWindow(heading,24,16,body,24,TRUE);
        MoveWindow(helper,24,45,body,40,TRUE);
        MoveWindow(modeButton,24,88,160,32,TRUE);
        MoveWindow(clearButton,194,88,80,32,TRUE);
        MoveWindow(editor,24,132,body,height>242?height-242:1,TRUE);
        MoveWindow(preedit,24,height-100,body,30,TRUE);
        MoveWindow(candidates,24,height-66,body,48,TRUE);
        return 0;
    }
    case WM_GETMINMAXINFO:
        reinterpret_cast<MINMAXINFO*>(l)->ptMinTrackSize={520,360}; return 0;
    case WM_COMMAND:
        if(LOWORD(w)==1) { session.SetChinese(!session.Chinese()); Render(); SetFocus(editor); return 0; }
        if(LOWORD(w)==2) { session.Reset(); SetWindowTextW(editor,L""); Render(); SetFocus(editor); return 0; }
        break;
    case WM_DESTROY: if(popup)DestroyWindow(popup);popup=nullptr;PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd,msg,w,l);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
    WNDCLASSW wc{}; wc.lpfnWndProc=WndProc; wc.hInstance=instance;
    wc.lpszClassName=L"NativeIMEInputLab"; wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
    wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    if(!RegisterClassW(&wc)) return 1;
    HWND window=CreateWindowW(wc.lpszClassName,L"NativeIME — Input lab (development preview)",
        WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,720,420,nullptr,nullptr,instance,nullptr);
    if(!window) return 2;
    ShowWindow(window,show); UpdateWindow(window);
    MSG message{}; BOOL status;
    while((status=GetMessageW(&message,nullptr,0,0))>0) { TranslateMessage(&message); DispatchMessageW(&message); }
    return status<0?3:0;
}
