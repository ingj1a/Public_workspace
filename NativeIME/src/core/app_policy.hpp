#pragma once
#include <string_view>
namespace nativeime::core {
enum class App { Unknown, Photoshop, Blender, FigmaDesktop, VSCode };
enum class Focus { Unknown, Text, Tool, Code, Secret };
enum class Mode { Chinese, English, Disabled };
enum class Override { Auto, Chinese, English, Disabled };
enum class Domain { General, Design, Rendering, Development };
struct Decision { Mode mode; Domain domain; };
App IdentifyApp(std::wstring_view executable) noexcept;
Decision ResolveApp(App app, Focus focus, Override user, Mode last) noexcept;
}
