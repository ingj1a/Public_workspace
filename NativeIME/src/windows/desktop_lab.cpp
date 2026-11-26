// Ordinary native text-control host. No interception, IME simulation or registration.
#include <windows.h>
namespace {
HWND normal=nullptr,secret=nullptr,readonlyBox=nullptr,other=nullptr;
HWND Label(HWND parent,const wchar_t* text,int y){
    return CreateWindowExW(0,L"STATIC",text,WS_CHILD|WS_VISIBLE,20,y,560,22,parent,nullptr,GetModuleHandleW(nullptr),nullptr);
}
HWND Edit(HWND parent,int y,DWORD style,const wchar_t* text=L""){
    return CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",text,WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL|style,
        20,y,560,30,parent,nullptr,GetModuleHandleW(nullptr),nullptr);
}
LRESULT CALLBACK Proc(HWND hwnd,UINT msg,WPARAM w,LPARAM l){
    switch(msg){
    case WM_CREATE:
        Label(hwnd,L"Select NativeIME manually. Type nihao, wait for candidates, select 你好.",14);
        normal=Edit(hwnd,42,0);
        Label(hwnd,L"Password field: must not show candidates.",90);secret=Edit(hwnd,114,ES_PASSWORD);
        Label(hwnd,L"Read-only field: must not consume text.",162);readonlyBox=Edit(hwnd,186,ES_READONLY,L"Read only");
        Label(hwnd,L"Second editable field: test focus changes and cancellation.",234);other=Edit(hwnd,258,0);
        if(!normal||!secret||!readonlyBox||!other)return -1;
        SetFocus(normal);return 0;
    case WM_DESTROY:PostQuitMessage(0);return 0;
    }
    return DefWindowProcW(hwnd,msg,w,l);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show){
    WNDCLASSW wc{};wc.lpfnWndProc=Proc;wc.hInstance=instance;wc.lpszClassName=L"NativeIME.DesktopLab";
    wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=GetSysColorBrush(COLOR_WINDOW);
    if(!RegisterClassW(&wc))return 1;
    HWND window=CreateWindowW(wc.lpszClassName,L"NativeIME - Windows TSF verification host",
        WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,640,370,nullptr,nullptr,instance,nullptr);
    if(!window)return 2;
    ShowWindow(window,show);UpdateWindow(window);
    MSG msg{};BOOL status;
    while((status=GetMessageW(&msg,nullptr,0,0))>0){
        if(msg.message==WM_KEYDOWN&&msg.wParam==VK_TAB){
            HWND order[]={normal,secret,readonlyBox,other};
            for(int i=0;i<4;++i)if(GetFocus()==order[i]){SetFocus(order[(i+((GetKeyState(VK_SHIFT)&0x8000)?3:1))%4]);break;}
            continue;
        }
        TranslateMessage(&msg);DispatchMessageW(&msg);
    }
    return status<0?3:0;
}
