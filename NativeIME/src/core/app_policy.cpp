#include "core/app_policy.hpp"
namespace nativeime::core {
namespace {
bool Equal(std::wstring_view a,std::wstring_view b) noexcept {
    if(a.size()!=b.size()) return false;
    for(std::size_t i=0;i<a.size();++i){
        wchar_t c=a[i];
        if(c>=L'A'&&c<=L'Z') c+=L'a'-L'A';
        if(c!=b[i]) return false;
    }
    return true;
}
}
App IdentifyApp(std::wstring_view exe) noexcept {
    const auto i=exe.find_last_of(L"\\/");
    if(i!=std::wstring_view::npos) exe.remove_prefix(i+1);
    if(Equal(exe,L"photoshop.exe")) return App::Photoshop;
    if(Equal(exe,L"blender.exe")) return App::Blender;
    if(Equal(exe,L"figma.exe")) return App::FigmaDesktop;
    if(Equal(exe,L"code.exe")||Equal(exe,L"code - insiders.exe")) return App::VSCode;
    return App::Unknown;
}
Decision ResolveApp(App app,Focus focus,Override user,Mode last) noexcept {
    Domain domain=Domain::General;
    if(app==App::Photoshop||app==App::FigmaDesktop) domain=Domain::Design;
    if(app==App::Blender) domain=Domain::Rendering;
    if(app==App::VSCode) domain=Domain::Development;
    if(focus==Focus::Secret||user==Override::Disabled) return {Mode::Disabled,domain};
    if(user==Override::Chinese) return {Mode::Chinese,domain};
    if(user==Override::English) return {Mode::English,domain};
    if(app==App::Unknown||focus==Focus::Unknown) return {last,domain};
    if(focus==Focus::Tool||focus==Focus::Code) return {Mode::English,domain};
    if(focus==Focus::Text) return {Mode::Chinese,domain};
    return {last,domain};
}
}
