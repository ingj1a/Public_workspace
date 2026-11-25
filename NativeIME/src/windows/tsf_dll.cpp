#include <windows.h>
#include <msctf.h>
#include <objbase.h>
#include <strsafe.h>

#include <atomic>
#include <new>
#include <utility>
#include "tsf_edit.hpp"
#ifdef NATIVEIME_EXPERIMENTAL_TSF
#include "core/input_session.hpp"
#include "focus_context.hpp"
#include "tsf_popup.hpp"
#include "async_engine.hpp"
#include <memory>
#include <textstor.h>
#include <cwchar>
#endif

namespace {
HINSTANCE g_instance = nullptr;
std::atomic<long> g_objects{0};
std::atomic<long> g_locks{0};

constexpr wchar_t kClsidText[] = L"{9D61A6D1-2C5E-4B6B-9C8A-7A8E1F2D1A10}";
const CLSID CLSID_NativeIME = {0x9d61a6d1, 0x2c5e, 0x4b6b, {0x9c,0x8a,0x7a,0x8e,0x1f,0x2d,0x1a,0x10}};
const GUID GUID_NativeIME_Profile = {0xd0b37c31, 0x9e0b, 0x4a68, {0x8c,0x5d,0x7a,0x3a,0x29,0xd6,0xe4,0x11}};

class NativeTextService final : public ITfTextInputProcessorEx, public ITfKeyEventSink, public ITfCompositionSink, public ITfThreadMgrEventSink {
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
        else if (riid == IID_ITfCompositionSink)
            *object = static_cast<ITfCompositionSink*>(this);
        else if (riid == IID_ITfThreadMgrEventSink)
            *object = static_cast<ITfThreadMgrEventSink*>(this);
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
    STDMETHODIMP ActivateEx(ITfThreadMgr* manager, TfClientId clientId, DWORD flags) override {
#ifdef NATIVEIME_EXPERIMENTAL_TSF
        secure_ = (flags & TF_TMAE_SECUREMODE) != 0;
#else
        (void)flags;
#endif
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
#ifdef NATIVEIME_EXPERIMENTAL_TSF
        if(SUCCEEDED(hr)){
            app_=nativeime::win::CurrentApplication();
            if(!secure_) StartEngine();
            ITfSource* source=nullptr;
            if(SUCCEEDED(threadManager_->QueryInterface(IID_PPV_ARGS(&source)))){
                source->AdviseSink(IID_ITfThreadMgrEventSink,static_cast<ITfThreadMgrEventSink*>(this),&focusCookie_);
                source->Release();
            }
        }
#endif
        if (FAILED(hr)) {
            threadManager_->Release();
            threadManager_ = nullptr;
            clientId_ = TF_CLIENTID_NULL;
        }
        return hr;
    }
    STDMETHODIMP Deactivate() override {
#ifdef NATIVEIME_EXPERIMENTAL_TSF
        StopEngine();
        popup_.Destroy();
        FinishRaw();
        if(threadManager_&&focusCookie_!=TF_INVALID_COOKIE){
            ITfSource* source=nullptr;
            if(SUCCEEDED(threadManager_->QueryInterface(IID_PPV_ARGS(&source)))){
                source->UnadviseSink(focusCookie_);
                source->Release();
            }
            focusCookie_=TF_INVALID_COOKIE;
        }
#endif
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

    STDMETHODIMP OnSetFocus(BOOL foreground) override {
#ifdef NATIVEIME_EXPERIMENTAL_TSF
        if (!foreground) { InvalidateQuery(); popup_.Hide(); FinishRaw(); }
#else
        (void)foreground;
#endif
        return S_OK;
    }
    STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr*) override {return S_OK;}
    STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr*) override {return S_OK;}
    STDMETHODIMP OnSetFocus(ITfDocumentMgr*,ITfDocumentMgr*) override {
#ifdef NATIVEIME_EXPERIMENTAL_TSF
        // Do not request a write lock during a document-focus notification.
        // Hide immediately; the next key/context termination handles cleanup.
        InvalidateQuery(); popup_.Hide();
#endif
        return S_OK;
    }
    STDMETHODIMP OnPushContext(ITfContext*) override {
#ifdef NATIVEIME_EXPERIMENTAL_TSF
        InvalidateQuery(); popup_.Hide();
#endif
        return S_OK;
    }
    STDMETHODIMP OnPopContext(ITfContext*) override {
#ifdef NATIVEIME_EXPERIMENTAL_TSF
        InvalidateQuery(); popup_.Hide();
#endif
        return S_OK;
    }
    STDMETHODIMP OnCompositionTerminated(TfEditCookie, ITfComposition* composition) override {
#ifdef NATIVEIME_EXPERIMENTAL_TSF
        if (composition == composition_) DropComposition();
#else
        (void)composition;
#endif
        return S_OK;
    }
    STDMETHODIMP OnTestKeyDown(ITfContext* context, WPARAM key, LPARAM, BOOL* eaten) override {
        if (!eaten) return E_POINTER;
        *eaten = FALSE;
#ifdef NATIVEIME_EXPERIMENTAL_TSF
        // Test callback is read-only: never advance composition twice.
        if (Eligible(context) && (!composition_ || context == context_)) {
            auto event = Translate(key);
            *eaten = Wants(event);
        }
#else
        (void)context; (void)key;
#endif
        return S_OK;
    }
    STDMETHODIMP OnKeyDown(ITfContext* context, WPARAM key, LPARAM, BOOL* eaten) override {
        if (!eaten) return E_POINTER;
        *eaten = FALSE;
#ifdef NATIVEIME_EXPERIMENTAL_TSF
        try {
            if (!Eligible(context) || (composition_ && context != context_)) {
                FinishRaw();
                return S_OK;
            }
            auto event = Translate(key);
            if (!Wants(event)) {
                FinishRaw();
                return S_OK;
            }
            auto next = session_;
            auto result = next.Handle(event);
            anchorValid_=false;
            if (!result.consumed) return S_OK;
            const HRESULT hr = nativeime::win::RunEdit(context, clientId_, [&](TfEditCookie cookie) {
                return Apply(context, cookie, next, result);
            });
            if (hr == S_OK) {
                session_ = std::move(next);
                *eaten = TRUE;
                if(event.key==Key::Letter||event.key==Key::Backspace){
                    InvalidateQuery();
                    if(engine_&&composition_&&!session_.Preedit().empty())engine_->Submit(generation_,session_.Preedit());
                }
                if(anchorValid_&&composition_&&focusCookie_!=TF_INVALID_COOKIE&&!blocked_)
                    popup_.Show(GetFocus(),anchor_,session_);
                else popup_.Hide();
            } else { popup_.Hide(); }
        } catch (...) { popup_.Hide(); /* Never allow a C++ exception to escape a COM callback. */ }
#else
        (void)context; (void)key;
#endif
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
#ifdef NATIVEIME_EXPERIMENTAL_TSF
    using Key = nativeime::core::Key;
    using KeyEvent = nativeime::core::KeyEvent;
    using Session = nativeime::core::InputSession;
    using Result = nativeime::core::InputResult;
    Session session_{[](const std::wstring& input){return nativeime::core::QueryResult{input,{}};}};
    std::unique_ptr<nativeime::win::AsyncEngine> engine_;
    HWND engineWindow_=nullptr;
    unsigned long long generation_=0;
    bool windowClassOwned_=false;
    wchar_t engineClass_[96]{};
    void InvalidateQuery() noexcept {++generation_;if(engine_)engine_->Cancel();}
    static LRESULT CALLBACK EngineWndProc(HWND hwnd,UINT msg,WPARAM w,LPARAM l){
        if(msg==WM_NCCREATE){auto* cs=reinterpret_cast<CREATESTRUCTW*>(l);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(cs->lpCreateParams));}
        auto* self=reinterpret_cast<NativeTextService*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
        if(msg==nativeime::win::EngineReady&&self){self->OnEngineReply();return 0;}
        return DefWindowProcW(hwnd,msg,w,l);
    }
    void StartEngine() noexcept {
        try{
            if(engine_)return;
            StringCchPrintfW(engineClass_,_countof(engineClass_),L"NativeIME.EngineReply.%p",static_cast<void*>(this));
            WNDCLASSW wc{};wc.lpfnWndProc=EngineWndProc;wc.hInstance=g_instance;wc.lpszClassName=engineClass_;
            if(!RegisterClassW(&wc))return;windowClassOwned_=true;
            engineWindow_=CreateWindowExW(0,engineClass_,L"",0,0,0,0,0,HWND_MESSAGE,nullptr,g_instance,this);
            if(engineWindow_)engine_=std::make_unique<nativeime::win::AsyncEngine>(engineWindow_);
            else StopEngine();
        }catch(...){StopEngine();}
    }
    void StopEngine() noexcept {
        InvalidateQuery();engine_.reset();
        if(engineWindow_){DestroyWindow(engineWindow_);engineWindow_=nullptr;}
        if(windowClassOwned_){UnregisterClassW(engineClass_,g_instance);windowClassOwned_=false;}
    }
    void OnEngineReply() noexcept {
        try{
            if(!engine_)return;auto reply=engine_->Take();
            if(!reply||reply->generation!=generation_||!composition_||!context_||!Eligible(context_))return;
            if(!reply->response.ok){popup_.Hide();return;}
            if(!session_.AcceptCandidates(reply->input,std::move(reply->response.candidates)))return;
            if(anchorValid_&&focusCookie_!=TF_INVALID_COOKIE&&!blocked_)popup_.Show(GetFocus(),anchor_,session_);
        }catch(...){popup_.Hide();}
    }
    ITfComposition* composition_ = nullptr;
    ITfContext* context_ = nullptr;
    bool secure_ = false;
    bool blocked_ = false;
    DWORD focusCookie_=TF_INVALID_COOKIE;
    nativeime::core::App app_=nativeime::core::App::Unknown;
    nativeime::win::TsfPopup popup_;
    RECT anchor_{};
    bool anchorValid_=false;
    // Deliberate allow-list for this engineering candidate: only a standard
    // focused Win32 EDIT control, in this process, not password/read-only.
    // Unknown custom controls and secure contexts pass through.
    bool Eligible(ITfContext* context) const noexcept {
        if (!context || !threadManager_ || secure_ || blocked_ || !engine_) return false;
        const auto snapshot=nativeime::win::InspectFocus(GetFocus(),app_);
        if(!snapshot.writableEdit)return false;
        const auto policy=nativeime::core::ResolveApp(snapshot.app,snapshot.focus,
            nativeime::core::Override::Auto,nativeime::core::Mode::Chinese);
        if(policy.mode!=nativeime::core::Mode::Chinese)return false;
        TF_STATUS status{};
        return SUCCEEDED(context->GetStatus(&status)) && !(status.dwDynamicFlags & TF_SD_READONLY);
    }
    static KeyEvent Translate(WPARAM key) noexcept {
        if ((GetKeyState(VK_CONTROL)&0x8000)||(GetKeyState(VK_MENU)&0x8000)||
            (GetKeyState(VK_LWIN)&0x8000)||(GetKeyState(VK_RWIN)&0x8000)||
            (GetKeyState(VK_SHIFT)&0x8000)||(GetKeyState(VK_CAPITAL)&1))
            return {Key::Other,0,true};
        if(key>='A' && key<='Z') return {Key::Letter,static_cast<wchar_t>(key-'A'+L'a')};
        if(key==VK_SPACE) return {Key::Space};
        if(key==VK_RETURN) return {Key::Enter};
        if(key==VK_BACK) return {Key::Backspace};
        if(key==VK_ESCAPE) return {Key::Escape};
        if(key>='1'&&key<='9')return {Key::Digit,static_cast<wchar_t>(key)};
        if(key==VK_UP)return {Key::Previous};
        if(key==VK_DOWN)return {Key::Next};
        return {};
    }
    bool Wants(KeyEvent event) const noexcept {
        if(event.shortcut) return false;
        if(event.key==Key::Letter) return session_.Preedit().size()<Session::MaxPreedit;
        if(event.key==Key::Digit)return popup_.Visible()&&event.character>=L'1'&&event.character<=L'9'&&static_cast<std::size_t>(event.character-L'1')<session_.Candidates().size();
        if(event.key==Key::Previous||event.key==Key::Next)return popup_.Visible()&&!session_.Candidates().empty();
        return !session_.Preedit().empty() && (event.key==Key::Space||event.key==Key::Enter||
            event.key==Key::Backspace||event.key==Key::Escape);
    }
    void DropComposition() noexcept {
        auto* old=composition_; composition_=nullptr;
        auto* ctx=context_; context_=nullptr;
        InvalidateQuery();session_.Reset();
        blocked_=false; popup_.Hide();
        if(old) old->Release();
        if(ctx) ctx->Release();
    }
    HRESULT End(TfEditCookie cookie) noexcept {
        if(!composition_) return S_OK;
        auto* comp=composition_;
        comp->AddRef();
        const HRESULT hr=comp->EndComposition(cookie);
        if(SUCCEEDED(hr) && composition_==comp) DropComposition();
        else if(FAILED(hr)&&composition_==comp) { blocked_=true; popup_.Hide(); }
        comp->Release();
        return hr;
    }
    void FinishRaw() noexcept {
        popup_.Hide();
        if(context_ && composition_) {
            auto* ctx=context_;
            ctx->AddRef();
            nativeime::win::RunEdit(ctx,clientId_,[&](TfEditCookie cookie){return End(cookie);});
            ctx->Release();
        }
    }
    HRESULT Begin(ITfContext* context,TfEditCookie cookie) noexcept {
        ITfInsertAtSelection* insert=nullptr;
        ITfContextComposition* manager=nullptr;
        ITfRange* range=nullptr;
        HRESULT hr=context->QueryInterface(IID_PPV_ARGS(&insert));
        if(SUCCEEDED(hr)) hr=insert->InsertTextAtSelection(cookie,TF_IAS_QUERYONLY,L"",0,&range);
        BOOL empty=FALSE;
        if(SUCCEEDED(hr)&&range) hr=range->IsEmpty(cookie,&empty);
        if(SUCCEEDED(hr)&&!empty) hr=E_ACCESSDENIED; // never destroy existing selection on cancel
        if(SUCCEEDED(hr)) hr=context->QueryInterface(IID_PPV_ARGS(&manager));
        ITfComposition* created=nullptr;
        if(SUCCEEDED(hr)) hr=manager->StartComposition(cookie,range,this,&created);
        if(SUCCEEDED(hr)&&created) {
            composition_=created; context_=context; context_->AddRef();
        } else if(SUCCEEDED(hr)) hr=E_FAIL;
        if(range) range->Release();
        if(manager) manager->Release();
        if(insert) insert->Release();
        return hr;
    }
    HRESULT Apply(ITfContext* context,TfEditCookie cookie,const Session& next,const Result& result) noexcept {
        const bool starting=!composition_;
        if(starting) {
            const HRESULT hr=Begin(context,cookie);
            if(FAILED(hr)) return hr;
        }
        // Inline preedit and candidate list share the same bounded session.
        const auto& text=result.commit.empty()?next.Preedit():result.commit;
        ITfRange* range=nullptr;
        HRESULT hr=composition_->GetRange(&range);
        if(SUCCEEDED(hr)) hr=range->SetText(cookie,0,text.c_str(),static_cast<LONG>(text.size()));
        if(FAILED(hr)) {
            if(range) range->Release();
            if(starting) End(cookie);
            return hr;
        }
        // After a successful text mutation never re-deliver the triggering key.
        // Selection/end failure is left for focus cleanup, not a duplicate insertion.
        if(SUCCEEDED(range->Collapse(cookie,TF_ANCHOR_END))) {
            TF_SELECTION sel{}; sel.range=range; sel.style.ase=TF_AE_NONE;
            if(SUCCEEDED(context->SetSelection(cookie,1,&sel))&&!next.Preedit().empty()&&result.commit.empty())
                anchorValid_=nativeime::win::ReadCaretRect(context,cookie,range,anchor_);
        }
        range->Release();
        if(next.Preedit().empty() || !result.commit.empty()) End(cookie);
        return S_OK;
    }
#endif
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
