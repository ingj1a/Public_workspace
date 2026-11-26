// Explicitly opt-in, disposable interactive Windows desktop only.
// No registration, global input injection, default-profile change or text injection.
// The actual installed experimental TSF service must handle every positive test key.
#include <windows.h>
#include <msctf.h>
#include <wrl/client.h>
#include <iostream>
#include <string>
#include <cwchar>
#include "windows/tsf_edit.hpp"
using Microsoft::WRL::ComPtr;
namespace {
const CLSID kTip={0x9d61a6d1,0x2c5e,0x4b6b,{0x9c,0x8a,0x7a,0x8e,0x1f,0x2d,0x1a,0x10}};
const GUID kProfile={0xd0b37c31,0x9e0b,0x4a68,{0x8c,0x5d,0x7a,0x3a,0x29,0xd6,0xe4,0x11}};
constexpr LANGID kLanguage=MAKELANGID(LANG_CHINESE,SUBLANG_CHINESE_SIMPLIFIED);
constexpr int kBlocked=77;
struct Apartment {
    HRESULT hr=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    ~Apartment(){if(SUCCEEDED(hr))CoUninitialize();}
};
struct Window {
    HWND h=nullptr;
    ~Window(){if(h)DestroyWindow(h);}
};
struct ThreadManager {
    ComPtr<ITfThreadMgr> mgr;
    TfClientId client=TF_CLIENTID_NULL;
    ~ThreadManager(){if(mgr&&client!=TF_CLIENTID_NULL)mgr->Deactivate();}
};
bool Ours(const TF_INPUTPROCESSORPROFILE& p){
    return p.dwProfileType==TF_PROFILETYPE_INPUTPROCESSOR&&p.clsid==kTip&&p.guidProfile==kProfile&&p.langid==kLanguage;
}
struct Profile {
    ComPtr<ITfInputProcessorProfileMgr> mgr;
    TF_INPUTPROCESSORPROFILE previous{};
    bool restore=false;
    bool Active(){
        TF_INPUTPROCESSORPROFILE current{};
        return mgr&&mgr->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD,&current)==S_OK&&Ours(current);
    }
    bool Select(){
        if(mgr->GetActiveProfile(GUID_TFCAT_TIP_KEYBOARD,&previous)!=S_OK)return false;
        if(Ours(previous))return true;
        // Do not enable profiles globally or defer activation until a language switch.
        // Record even a partial activation attempt for cleanup.
        restore=true;
        return mgr->ActivateProfile(TF_PROFILETYPE_INPUTPROCESSOR,kLanguage,kTip,kProfile,nullptr,TF_IPPMF_FORPROCESS)==S_OK;
    }
    bool Restore(){
        if(!restore)return true;
        restore=false;
        return mgr->ActivateProfile(previous.dwProfileType,previous.langid,previous.clsid,
            previous.guidProfile,previous.hkl,TF_IPPMF_FORPROCESS)==S_OK;
    }
    ~Profile(){if(!Restore())std::cerr<<"Process profile restore failed\n";}
};
bool Pump(DWORD milliseconds){
    const auto deadline=GetTickCount64()+milliseconds;
    do{
        MSG msg{};
        while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){
            if(msg.message==WM_QUIT)return false;
            TranslateMessage(&msg);DispatchMessageW(&msg);
        }
        if(GetTickCount64()>=deadline)break;
        Sleep(5);
    }while(true);
    return true;
}
std::wstring Text(HWND h){
    const int length=GetWindowTextLengthW(h);
    if(length<0||length>4096)return {};
    std::wstring text(static_cast<std::size_t>(length)+1,L'\0');
    const int copied=GetWindowTextW(h,text.data(),length+1);
    text.resize(copied>0?static_cast<std::size_t>(copied):0);
    return text;
}
struct PopupSearch { HWND owner; std::wstring preedit; HWND popup=nullptr; int index=0; };
BOOL CALLBACK FindPopup(HWND h,LPARAM raw){
    auto& s=*reinterpret_cast<PopupSearch*>(raw);
    if(!IsWindowVisible(h)||GetWindow(h,GW_OWNER)!=s.owner||
       !(GetWindowLongPtrW(h,GWL_EXSTYLE)&WS_EX_NOACTIVATE))return TRUE;
    wchar_t klass[32]{};
    if(!GetClassNameW(h,klass,32)||_wcsicmp(klass,L"Static")!=0)return TRUE;
    auto t=Text(h);
    if(t.rfind(s.preedit+L"\r\n",0)!=0)return TRUE;
    s.popup=h;
    for(int i=1;i<=9;++i){
        const auto suffix=std::to_wstring(i)+L" \u4f60\u597d";
        if((t+L"\r\n").find(L"\r\n> "+suffix+L"\r\n")!=std::wstring::npos||
           (t+L"\r\n").find(L"\r\n  "+suffix+L"\r\n")!=std::wstring::npos){s.index=i;break;}
    }
    return FALSE;
}
PopupSearch Popup(HWND root,const std::wstring& pinyin){
    PopupSearch s{root,pinyin};
    EnumThreadWindows(GetCurrentThreadId(),FindPopup,reinterpret_cast<LPARAM>(&s));return s;
}
bool OwnFocus(HWND root,HWND edit){
    return IsWindow(root)&&IsWindow(edit)&&GetForegroundWindow()==root&&GetFocus()==edit;
}
bool CleanModifiers(){
    for(int key:{VK_SHIFT,VK_CONTROL,VK_MENU,VK_LWIN,VK_RWIN})
        if(GetKeyState(key)&0x8000)return false;
    return !(GetKeyState(VK_CAPITAL)&1);
}
struct Route {
    HWND root;
    Profile& profile;
    ThreadManager& thread;
    ComPtr<ITfKeystrokeMgr> keys;
    bool Init(){return SUCCEEDED(thread.mgr.As(&keys));}
    bool Context(ComPtr<ITfContext>& context){
        ComPtr<ITfDocumentMgr> doc;
        return thread.mgr->GetFocus(doc.GetAddressOf())==S_OK&&doc&&
            doc->GetTop(context.GetAddressOf())==S_OK&&context;
    }
    bool Key(HWND edit,WPARAM key,bool expected){
        if(!OwnFocus(root,edit)||!profile.Active()||!CleanModifiers())return false;
        ComPtr<ITfContext> context;
        if(!Context(context))return false;
        LPARAM param=1|(static_cast<LPARAM>(MapVirtualKeyW(static_cast<UINT>(key),MAPVK_VK_TO_VSC))<<16);
        BOOL eaten=FALSE;
        if(keys->TestKeyDown(key,param,&eaten)!=S_OK||static_cast<bool>(eaten)!=expected)return false;
        if(!expected)return true; // do not send a simulated fallback character.
        eaten=FALSE;
        if(keys->KeyDown(key,param,&eaten)!=S_OK||!eaten)return false;
        BOOL up=FALSE;
        const LPARAM released=param|static_cast<LPARAM>(0xC0000000u);
        if(keys->TestKeyUp(key,released,&up)==S_OK&&up)keys->KeyUp(key,released,&up);
        return Pump(20)&&OwnFocus(root,edit);
    }
    bool WaitReady(HWND edit){
        const auto deadline=GetTickCount64()+5000;
        do{
            if(!OwnFocus(root,edit)||!profile.Active()||!CleanModifiers())return false;
            BOOL eaten=FALSE;
            if(keys->TestKeyDown('N',1,&eaten)==S_OK&&eaten)return true;
            if(!Pump(50))return false;
        }while(GetTickCount64()<deadline);
        return false;
    }
    bool Pinyin(HWND edit){
        for(wchar_t ch:std::wstring(L"NIHAO"))if(!Key(edit,ch,true))return false;
        return true;
    }
    // A real TSF edit-session observation, not an EM_REPLACESEL insertion.
    bool ReadContext(const std::wstring& expected){
        ComPtr<ITfContext> context;
        if(!Context(context))return false;
        HRESULT hr=nativeime::win::RunEdit(context.Get(),thread.client,[&](TfEditCookie cookie){
            ComPtr<ITfRange> start,end;
            HRESULT result=context->GetStart(cookie,start.GetAddressOf());
            if(result!=S_OK)return result;
            result=context->GetEnd(cookie,end.GetAddressOf());
            if(result!=S_OK)return result;
            result=start->ShiftEndToRange(cookie,end.Get(),TF_ANCHOR_END);
            if(result!=S_OK)return result;
            wchar_t text[128]{};ULONG count=0;
            result=start->GetText(cookie,0,text,127,&count);
            if(result!=S_OK)return result;
            return std::wstring(text,count)==expected?S_OK:E_FAIL;
        });
        return hr==S_OK;
    }
};
int Fail(int code,const char* why){std::cerr<<"FAIL "<<code<<": "<<why<<"\n";return code;}
int Blocked(const char* why){std::cerr<<"BLOCKED: "<<why<<"\n";return kBlocked;}
int Preflight(){
    HDESK desktop=OpenInputDesktop(0,FALSE,DESKTOP_READOBJECTS);
    if(!desktop)return Blocked("input desktop unavailable");
    CloseDesktop(desktop);
    Apartment com;if(FAILED(com.hr))return Fail(2,"COM initialization");
    ComPtr<ITfInputProcessorProfiles> profiles;
    if(FAILED(CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(profiles.GetAddressOf()))))return Fail(4,"profile manager");
    LANGID language=0;BOOL enabled=FALSE;
    if(profiles->GetCurrentLanguage(&language)!=S_OK||language!=kLanguage)return Blocked("Simplified Chinese input language not active");
    if(profiles->IsEnabledLanguageProfile(kTip,kLanguage,kProfile,&enabled)!=S_OK||!enabled)return Blocked("NativeIME profile not enabled");
    std::cout<<"Desktop preconditions present (not an E2E pass)\n";return 0;
}
int Verify(){
    Apartment com;if(FAILED(com.hr))return Fail(2,"COM initialization");
    ThreadManager thread;
    if(FAILED(CoCreateInstance(CLSID_TF_ThreadMgr,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(thread.mgr.GetAddressOf())))||
       thread.mgr->Activate(&thread.client)!=S_OK)return Fail(3,"real TSF thread manager activation");
    ComPtr<ITfInputProcessorProfiles> profiles;
    Profile profile;
    if(FAILED(CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(profiles.GetAddressOf())))||
       FAILED(profiles.As(&profile.mgr)))return Fail(4,"profile manager");
    LANGID language=0;BOOL enabled=FALSE;
    if(profiles->GetCurrentLanguage(&language)!=S_OK||language!=kLanguage)
        return Blocked("select Simplified Chinese input language in the disposable VM first");
    if(profiles->IsEnabledLanguageProfile(kTip,kLanguage,kProfile,&enabled)!=S_OK||!enabled)
        return Blocked("register and enable the experimental NativeIME profile in the VM first");
    Window root;
    root.h=CreateWindowExW(0,L"STATIC",L"NativeIME isolated TSF desktop verification",
        WS_OVERLAPPEDWINDOW,80,80,560,240,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!root.h)return Fail(5,"create host");
    const auto makeEdit=[&](int y,DWORD style){
        return CreateWindowExW(WS_EX_CLIENTEDGE,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL|style,
            12,y,500,30,root.h,nullptr,GetModuleHandleW(nullptr),nullptr);
    };
    HWND edit=makeEdit(20,0),password=makeEdit(70,ES_PASSWORD),other=makeEdit(120,0);
    if(!edit||!password||!other)return Fail(6,"create EDIT controls");
    ShowWindow(root.h,SW_SHOW);SetForegroundWindow(root.h);SetFocus(edit);
    if(!Pump(150)||!OwnFocus(root.h,edit)||!CleanModifiers())return Blocked("interactive foreground desktop and clean modifiers required");
    if(!profile.Select())return Blocked("process-scoped profile activation failed; check registered profile and language");
    if(!Pump(200)||!profile.Active())return Fail(7,"NativeIME not active after process-scoped activation");
    Route route{root.h,profile,thread};
    if(!route.Init())return Fail(8,"TSF keystroke manager");
    ComPtr<ITfContext> context;
    if(!route.Context(context))return Fail(9,"standard EDIT has no focused TSF context");
    context.Reset();
    if(!route.WaitReady(edit))return Fail(10,"service never accepted the first Pinyin key; verify engine is running");
    const std::wstring chinese=L"\u4f60\u597d";
    for(int round=0;round<2;++round){
        std::wstring prior=round?chinese:L"";
        if(!route.Pinyin(edit)||Text(edit)!=prior+L"nihao")return Fail(11,"inline Pinyin or duplicate/lost key");
        PopupSearch s{root.h,L"nihao"};
        const auto end=GetTickCount64()+3000;
        do{
            if(!Pump(25)||!OwnFocus(root.h,edit)||!profile.Active())return Fail(12,"focus lost before candidate");
            s=Popup(root.h,L"nihao");
        }while(!s.index&&GetTickCount64()<end);
        if(!s.popup||!s.index)return Fail(13,"actual NativeIME popup lacks expected Chinese candidate");
        if(!route.Key(edit,L'0'+s.index,true)||Text(edit)!=prior+chinese||
           !route.ReadContext(prior+chinese)||Popup(root.h,L"nihao").popup)
            return Fail(14,"candidate selection, real TSF context or popup dismissal");
        std::cout<<"Round "<<round+1<<": actual TSF composition and exact Chinese commit passed\n";
    }
    if(!route.Pinyin(edit)||!route.Key(edit,VK_BACK,true)||Text(edit)!=chinese+chinese+L"niha"||
       !route.Key(edit,VK_ESCAPE,true)||Text(edit)!=chinese+chinese)
        return Fail(15,"backspace / cancel preservation");
    if(!route.Key(edit,VK_F2,false))return Fail(16,"unhandled host key was claimed");
    SetFocus(password);if(!Pump(100)||!route.Key(password,'N',false)||Popup(root.h,L"nihao").popup)
        return Fail(17,"password control key or candidate protection");
    SetFocus(other);if(!Pump(100)||!route.WaitReady(other)||!route.Pinyin(other))
        return Fail(18,"second text control");
    SetFocus(edit);if(!Pump(150)||Popup(root.h,L"nihao").popup)
        return Fail(19,"stale popup after focus switch");
    if(Text(edit)!=chinese+chinese)return Fail(20,"focus switch corrupted first control");
    if(!profile.Restore())return Fail(21,"restore original process profile");
    std::cout<<"PASS: registered active NativeIME, TSF keyboard routing, real candidates, two exact commits, edit session readback, cancel and focus checks.\n";
    return 0;
}
}
int wmain(int argc,wchar_t** argv){
    if(argc==2&&wcscmp(argv[1],L"--preflight-only")==0)return Preflight();
    if(argc!=2||wcscmp(argv[1],L"--isolated-desktop")!=0)
        return Blocked("requires explicit --isolated-desktop on a disposable interactive Windows VM");
    return Verify();
}
