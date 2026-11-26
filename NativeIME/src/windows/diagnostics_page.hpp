#pragma once
#include <windows.h>
namespace nativeime::win {
// A persistent child page in NativeIME.exe. Navigation never destroys capture.
HWND CreateDiagnosticsPage(HWND parent,HINSTANCE instance);
bool CloseDiagnosticsPage() noexcept;
// Borrowed job owned by the main window, valid only on its UI thread.
void SetDiagnosticsEngineJob(HANDLE job) noexcept;
}
