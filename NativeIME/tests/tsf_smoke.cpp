// Loads the DLL directly. No registration, system settings, or keyboard hooks.
#include <windows.h>
#include <msctf.h>
#include <objbase.h>
#include <iostream>
int wmain(int argc, wchar_t** argv) {
    if (argc != 2) return 1;
    HMODULE dll = LoadLibraryW(argv[1]);
    if (!dll) return 2;
    using GetFactory = HRESULT (WINAPI*)(REFCLSID, REFIID, void**);
    using CanUnload = HRESULT (WINAPI*)();
    auto get = reinterpret_cast<GetFactory>(GetProcAddress(dll,"DllGetClassObject"));
    auto unload = reinterpret_cast<CanUnload>(GetProcAddress(dll,"DllCanUnloadNow"));
    if (!get || !unload || !GetProcAddress(dll,"DllRegisterServer") ||
        !GetProcAddress(dll,"DllUnregisterServer")) { FreeLibrary(dll); return 3; }
    const CLSID clsid = {0x9d61a6d1,0x2c5e,0x4b6b,{0x9c,0x8a,0x7a,0x8e,0x1f,0x2d,0x1a,0x10}};
    IClassFactory* factory=nullptr;
    ITfKeyEventSink* sink=nullptr;
    bool ok = SUCCEEDED(get(clsid,IID_IClassFactory,reinterpret_cast<void**>(&factory))) && factory;
    if (ok) ok = SUCCEEDED(factory->CreateInstance(nullptr,IID_ITfKeyEventSink,reinterpret_cast<void**>(&sink))) && sink;
    if (ok) {
        BOOL eaten=TRUE;
        ok = unload()==S_FALSE && SUCCEEDED(sink->OnTestKeyDown(nullptr,'A',0,&eaten)) && !eaten;
        eaten=TRUE;
        ok = ok && SUCCEEDED(sink->OnKeyDown(nullptr,'A',0,&eaten)) && !eaten;
        ITfTextInputProcessorEx* tip=nullptr;
        ok = ok && SUCCEEDED(sink->QueryInterface(IID_ITfTextInputProcessorEx,reinterpret_cast<void**>(&tip))) && tip;
        if(tip) tip->Release();
    }
    if(sink) sink->Release();
    if(factory) factory->Release();
    ok = ok && unload()==S_OK;
    FreeLibrary(dll);
    std::cout << (ok ? "TSF COM smoke passed\n" : "TSF COM smoke failed\n");
    return ok ? 0 : 4;
}
