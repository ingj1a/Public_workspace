#pragma once
#include <windows.h>
namespace nativeime::win {
HWND CreateSetupPage(HWND parent,HINSTANCE instance);
bool SetupPageBusy() noexcept;
void SetSetupEngineBusy(bool busy) noexcept;
}
