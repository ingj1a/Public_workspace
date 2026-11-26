#pragma once
#include <windows.h>
#include "core/app_policy.hpp"
namespace nativeime::win {
struct FocusSnapshot {
    core::App app=core::App::Unknown;
    core::Focus focus=core::Focus::Unknown;
    bool writableEdit=false;
};
// Only inspect the current thread's own HWND; never read document content.
FocusSnapshot InspectFocus(HWND focus, core::App app) noexcept;
core::App CurrentApplication() noexcept;
}
