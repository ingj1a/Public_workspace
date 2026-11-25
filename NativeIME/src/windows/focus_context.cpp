#include "focus_context.hpp"
#include <cwchar>
namespace nativeime::win {
core::App CurrentApplication() noexcept {
    wchar_t path[32768]{};
    const auto length=GetModuleFileNameW(nullptr,path,static_cast<DWORD>(sizeof(path)/sizeof(path[0])));
    if(!length||length>=sizeof(path)/sizeof(path[0])) return core::App::Unknown;
    return core::IdentifyApp(std::wstring_view(path,length));
}
FocusSnapshot InspectFocus(HWND focus,core::App app) noexcept {
    FocusSnapshot out; out.app=app;
    if(!focus||!IsWindow(focus))return out;
    DWORD pid=0;
    const DWORD tid=GetWindowThreadProcessId(focus,&pid);
    if(pid!=GetCurrentProcessId()||tid!=GetCurrentThreadId())return out;
    wchar_t klass[32]{};
    if(!GetClassNameW(focus,klass,32)||_wcsicmp(klass,L"Edit")!=0)return out;
    const auto style=GetWindowLongPtrW(focus,GWL_STYLE);
    if((style&ES_PASSWORD)||SendMessageW(focus,EM_GETPASSWORDCHAR,0,0)!=0){
        out.focus=core::Focus::Secret;return out;
    }
    if((style&ES_READONLY)||!IsWindowEnabled(focus))return out;
    out.focus=core::Focus::Text;out.writableEdit=true;
    return out;
}
}
