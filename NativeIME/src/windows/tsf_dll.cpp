#include <windows.h>
#include <msctf.h>
#include <objbase.h>
#include <strsafe.h>

#include <atomic>
#include <new>

namespace {
HINSTANCE g_instance = nullptr;
std::atomic<long> g_objects{0};
std::atomic<long> g_locks{0};

constexpr wchar_t kClsidText[] = L"{9D61A6D1-2C5E-4B6B-9C8A-7A8E1F2D1A10}";
const CLSID CLSID_NativeIME = {0x9d61a6d1, 0x2c5e, 0x4b6b, {0x9c,0x8a,0x7a,0x8e,0x1f,0x2d,0x1a,0x10}};
const GUID GUID_NativeIME_Profile = {0xd0b37c31, 0x9e0b, 0x4a68, {0x8c,0x5d,0x7a,0x3a,0x29,0xd6,0xe4,0x11}};

class NativeTextService final : public ITfTextInputProcessorEx, public ITfKeyEventSink {
public:
    NativeTextService() { ++g_objects; }
    ~NativeTextService() { Deactivate(); --g_objects; }

    STDMETHODIMP QueryInterface(REFIID riid, void** object) override {
        if (!object) return E_POINTER;
        *object = nullptr;
        if (riid == IID_IUnknown || riid == IID_ITfTextInputProcessor || riid == IID_ITfTextInputProcessorEx)
            *object = static_cast<ITfTextInputProcessorEx*>(this);
        else if (riid == IID_ITfKeyEventSink)
            *object = static_cast<ITfKeyEventSink*>(this);
        else
            return E_NOINTERFACE;
        AddRef();
        return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&refs_)); }
    STDMETHODIMP_(ULONG) Release() override {
        const ULONG count = static_cast<ULONG>(InterlockedDecrement(&refs_));
        if (!count) delete this;
        return count;
    }

    STDMETHODIMP Activate(ITfThreadMgr* manager, TfClientId clientId) override {
        return ActivateEx(manager, clientId, 0);
    }
    STDMETHODIMP ActivateEx(ITfThreadMgr* manager, TfClientId clientId, DWORD) override {
        if (!manager) return E_INVALIDARG;
        if (threadManager_) return S_OK;
        threadManager_ = manager;
        threadManager_->AddRef();
        clientId_ = clientId;

        ITfKeystrokeMgr* keys = nullptr;
        HRESULT hr = threadManager_->QueryInterface(IID_PPV_ARGS(&keys));
        if (SUCCEEDED(hr)) {
            hr = keys->AdviseKeyEventSink(clientId_, static_cast<ITfKeyEventSink*>(this), TRUE);
            keys->Release();
        }
        if (FAILED(hr)) {
            threadManager_->Release();
            threadManager_ = nullptr;
            clientId_ = TF_CLIENTID_NULL;
        }
        return hr;
    }
    STDMETHODIMP Deactivate() override {
        if (!threadManager_) return S_OK;
        ITfKeystrokeMgr* keys = nullptr;
        if (SUCCEEDED(threadManager_->QueryInterface(IID_PPV_ARGS(&keys)))) {
            keys->UnadviseKeyEventSink(clientId_);
            keys->Release();
        }
        threadManager_->Release();
        threadManager_ = nullptr;
        clientId_ = TF_CLIENTID_NULL;
        return S_OK;
    }

    STDMETHODIMP OnSetFocus(BOOL) override { return S_OK; }
    STDMETHODIMP OnTestKeyDown(ITfContext*, WPARAM, LPARAM, BOOL* eaten) override {
        if (!eaten) return E_POINTER;
        *eaten = FALSE; // Phase 0 probe: never consume a host application's keys.
        return S_OK;
    }
    STDMETHODIMP OnKeyDown(ITfContext*, WPARAM, LPARAM, BOOL* eaten) override {
        if (!eaten) return E_POINTER;
        *eaten = FALSE;
        return S_OK;
    }
    STDMETHODIMP OnTestKeyUp(ITfContext*, WPARAM, LPARAM, BOOL* eaten) override {
        if (!eaten) return E_POINTER;
        *eaten = FALSE;
        return S_OK;
    }
    STDMETHODIMP OnKeyUp(ITfContext*, WPARAM, LPARAM, BOOL* eaten) override {
        if (!eaten) return E_POINTER;
        *eaten = FALSE;
        return S_OK;
    }
    STDMETHODIMP OnPreservedKey(ITfContext*, REFGUID, BOOL* eaten) override {
        if (!eaten) return E_POINTER;
        *eaten = FALSE;
        return S_OK;
    }

private:
    LONG refs_ = 1;
    ITfThreadMgr* threadManager_ = nullptr;
    TfClientId clientId_ = TF_CLIENTID_NULL;
};

class NativeClassFactory final : public IClassFactory {
public:
    NativeClassFactory() { ++g_objects; }
    ~NativeClassFactory() { --g_objects; }
    STDMETHODIMP QueryInterface(REFIID riid, void** object) override {
        if (!object) return E_POINTER;
        *object = nullptr;
        if (riid != IID_IUnknown && riid != IID_IClassFactory) return E_NOINTERFACE;
        *object = static_cast<IClassFactory*>(this);
        AddRef();
        return S_OK;
    }
    STDMETHODIMP_(ULONG) AddRef() override { return static_cast<ULONG>(InterlockedIncrement(&refs_)); }
    STDMETHODIMP_(ULONG) Release() override {
        const ULONG count = static_cast<ULONG>(InterlockedDecrement(&refs_));
        if (!count) delete this;
        return count;
    }
    STDMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** object) override {
        if (outer) return CLASS_E_NOAGGREGATION;
        auto* service = new (std::nothrow) NativeTextService();
        if (!service) return E_OUTOFMEMORY;
        const HRESULT hr = service->QueryInterface(riid, object);
        service->Release();
        return hr;
    }
    STDMETHODIMP LockServer(BOOL lock) override {
        if (lock) ++g_locks; else --g_locks;
        return S_OK;
    }
private:
    LONG refs_ = 1;
};

HRESULT SetRegistryString(HKEY key, const wchar_t* name, const wchar_t* value) {
    return RegSetValueExW(key, name, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(value),
        static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t))) == ERROR_SUCCESS ? S_OK : E_FAIL;
}

HRESULT RegisterComServer() {
    wchar_t modulePath[MAX_PATH]{};
    if (!GetModuleFileNameW(g_instance, modulePath, MAX_PATH)) return HRESULT_FROM_WIN32(GetLastError());

    wchar_t keyPath[128]{};
    HRESULT hr = StringCchPrintfW(keyPath, _countof(keyPath), L"CLSID\\%s", kClsidText);
    if (FAILED(hr)) return hr;

    HKEY clsidKey = nullptr;
    if (RegCreateKeyExW(HKEY_CLASSES_ROOT, keyPath, 0, nullptr, 0, KEY_WRITE, nullptr, &clsidKey, nullptr) != ERROR_SUCCESS)
        return E_ACCESSDENIED;
    SetRegistryString(clsidKey, nullptr, L"NativeIME Text Service");
    HKEY inprocKey = nullptr;
    LONG status = RegCreateKeyExW(clsidKey, L"InprocServer32", 0, nullptr, 0, KEY_WRITE, nullptr, &inprocKey, nullptr);
    if (status == ERROR_SUCCESS) {
        SetRegistryString(inprocKey, nullptr, modulePath);
        SetRegistryString(inprocKey, L"ThreadingModel", L"Apartment");
        RegCloseKey(inprocKey);
    }
    RegCloseKey(clsidKey);
    return status == ERROR_SUCCESS ? S_OK : HRESULT_FROM_WIN32(status);
}

HRESULT RegisterProfile() {
    ITfInputProcessorProfiles* profiles = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&profiles));
    if (FAILED(hr)) return hr;
    hr = profiles->Register(CLSID_NativeIME);
    if (SUCCEEDED(hr)) {
        hr = profiles->AddLanguageProfile(CLSID_NativeIME, MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED),
            GUID_NativeIME_Profile, L"NativeIME", static_cast<ULONG>(wcslen(L"NativeIME")),
            nullptr, 0, 0);
    }
    profiles->Release();
    if (FAILED(hr)) return hr;

    ITfCategoryMgr* categories = nullptr;
    hr = CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&categories));
    if (SUCCEEDED(hr)) {
        hr = categories->RegisterCategory(CLSID_NativeIME, GUID_TFCAT_TIP_KEYBOARD, CLSID_NativeIME);
        categories->Release();
    }
    return hr;
}
}

STDAPI DllGetClassObject(REFCLSID clsid, REFIID iid, void** object) {
    if (clsid != CLSID_NativeIME) return CLASS_E_CLASSNOTAVAILABLE;
    auto* factory = new (std::nothrow) NativeClassFactory();
    if (!factory) return E_OUTOFMEMORY;
    const HRESULT hr = factory->QueryInterface(iid, object);
    factory->Release();
    return hr;
}

STDAPI DllCanUnloadNow() {
    return (g_objects.load() == 0 && g_locks.load() == 0) ? S_OK : S_FALSE;
}

STDAPI DllRegisterServer() {
    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const HRESULT registry = RegisterComServer();
    const HRESULT profile = SUCCEEDED(registry) ? RegisterProfile() : registry;
    if (SUCCEEDED(init)) CoUninitialize();
    return FAILED(registry) ? registry : profile;
}

STDAPI DllUnregisterServer() {
    const HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ITfInputProcessorProfiles* profiles = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
                                   IID_PPV_ARGS(&profiles)))) {
        profiles->RemoveLanguageProfile(CLSID_NativeIME,
            MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_SIMPLIFIED), GUID_NativeIME_Profile);
        profiles->Unregister(CLSID_NativeIME);
        profiles->Release();
    }
    ITfCategoryMgr* categories = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&categories)))) {
        categories->UnregisterCategory(CLSID_NativeIME, GUID_TFCAT_TIP_KEYBOARD, CLSID_NativeIME);
        categories->Release();
    }
    wchar_t keyPath[128]{};
    StringCchPrintfW(keyPath, _countof(keyPath), L"CLSID\\%s", kClsidText);
    RegDeleteTreeW(HKEY_CLASSES_ROOT, keyPath);
    if (SUCCEEDED(init)) CoUninitialize();
    return S_OK;
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_instance = instance;
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}
