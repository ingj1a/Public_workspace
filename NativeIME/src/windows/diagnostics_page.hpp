#pragma once
#include <windows.h>
namespace nativeime::win {
// A persistent child page in NativeIME.exe. Navigation never destroys capture.
HWND CreateDiagnosticsPage(HWND parent,HINSTANCE instance);
bool CloseDiagnosticsPage() noexcept;
}
