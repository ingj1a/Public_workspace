#include "core/app_policy.hpp"
#include "core/input_session.hpp"
#include <cstdlib>
using namespace nativeime::core;
int main(){
    if(IdentifyApp(L"C:\\Apps\\PHOTOSHOP.EXE")!=App::Photoshop) return 1;
    if(IdentifyApp(L"blender.exe")!=App::Blender||IdentifyApp(L"Figma.exe")!=App::FigmaDesktop||IdentifyApp(L"Code.exe")!=App::VSCode) return 2;
    if(IdentifyApp(L"chrome.exe")!=App::Unknown) return 3;
    if(ResolveApp(App::Blender,Focus::Tool,Override::Auto,Mode::Chinese).mode!=Mode::English) return 4;
    if(ResolveApp(App::Photoshop,Focus::Text,Override::Auto,Mode::English).mode!=Mode::Chinese) return 5;
    if(ResolveApp(App::VSCode,Focus::Unknown,Override::Auto,Mode::Chinese).mode!=Mode::Chinese) return 6;
    if(ResolveApp(App::FigmaDesktop,Focus::Tool,Override::Chinese,Mode::English).mode!=Mode::Chinese) return 7;
    if(ResolveApp(App::VSCode,Focus::Secret,Override::Chinese,Mode::Chinese).mode!=Mode::Disabled) return 8;
    if(ResolveApp(App::Blender,Focus::Text,Override::Auto,Mode::English).domain!=Domain::Rendering) return 9;
    InputSession s([](const std::wstring& p){return QueryResult{p,{{L"custom",100,L""}}};});
    s.Handle({Key::Letter,L'a'});
    if(s.Handle({Key::Space}).commit!=L"custom") return 10;
    return EXIT_SUCCESS;
}
