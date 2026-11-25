// Native Win32 development harness, not a system input method.
#include <windows.h>
#include <string>
#include "core/input_session.hpp"
using namespace nativeime::core;
namespace {
InputSession session;
HWND editor, preedit, candidates, modeButton, clearButton;
WNDPROC originalEdit;
HFONT font;
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
    case WM_CREATE:
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
        SetFocus(editor); return 0;
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
    case WM_DESTROY: PostQuitMessage(0); return 0;
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
