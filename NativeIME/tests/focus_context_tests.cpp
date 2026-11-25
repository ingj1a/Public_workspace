#include "windows/focus_context.hpp"
using namespace nativeime;
int main(){
    HWND parent=CreateWindowExW(0,L"STATIC",L"",WS_POPUP,0,0,300,200,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!parent)return 1;
    HWND plain=CreateWindowExW(0,L"EDIT",L"",WS_CHILD,0,0,100,20,parent,nullptr,GetModuleHandleW(nullptr),nullptr);
    HWND secret=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|ES_PASSWORD,0,0,100,20,parent,nullptr,GetModuleHandleW(nullptr),nullptr);
    HWND readonly=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|ES_READONLY,0,0,100,20,parent,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!plain||!secret||!readonly){DestroyWindow(parent);return 2;}
    bool ok=win::InspectFocus(plain,core::App::Blender).writableEdit
        &&win::InspectFocus(secret,core::App::Photoshop).focus==core::Focus::Secret
        &&!win::InspectFocus(readonly,core::App::VSCode).writableEdit
        &&win::InspectFocus(parent,core::App::FigmaDesktop).focus==core::Focus::Unknown
        &&!win::InspectFocus(nullptr,core::App::Unknown).writableEdit;
    EnableWindow(plain,FALSE);
    ok=ok&&!win::InspectFocus(plain,core::App::Unknown).writableEdit;
    DestroyWindow(parent);
    return ok?0:3;
}
