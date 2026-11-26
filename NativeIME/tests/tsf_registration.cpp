// Disposable Windows CI only. Never selects a default or active input profile.
#include <windows.h>
#include <msctf.h>
#include <objbase.h>
#include <iostream>
namespace {
const CLSID clsid={0x9d61a6d1,0x2c5e,0x4b6b,{0x9c,0x8a,0x7a,0x8e,0x1f,0x2d,0x1a,0x10}};
const GUID profile={0xd0b37c31,0x9e0b,0x4a68,{0x8c,0x5d,0x7a,0x3a,0x29,0xd6,0xe4,0x11}};
constexpr wchar_t key[]=L"CLSID\\{9D61A6D1-2C5E-4B6B-9C8A-7A8E1F2D1A10}";
constexpr LANGID language=MAKELANGID(LANG_CHINESE,SUBLANG_CHINESE_SIMPLIFIED);
using Reg=HRESULT(WINAPI*)();
bool Absent(){
    HKEY h=nullptr;
    LONG r=RegOpenKeyExW(HKEY_CLASSES_ROOT,key,0,KEY_READ,&h);
    if(h)RegCloseKey(h);
    return r==ERROR_FILE_NOT_FOUND||r==ERROR_PATH_NOT_FOUND;
}
bool ProfileExists(ITfInputProcessorProfiles* p){
    BSTR desc=nullptr;
    const HRESULT hr=p->GetLanguageProfileDescription(clsid,language,profile,&desc);
    const bool found=hr==S_OK&&desc&&SysStringLen(desc)>0;
    SysFreeString(desc);return found;
}
}
int wmain(int argc,wchar_t** argv){
    // Explicit opt-in even if someone runs this test binary outside CI.
    if(argc!=3||wcscmp(argv[2],L"--disposable-ci")!=0)return 1;
    HRESULT init=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(init))return 2;
    ITfInputProcessorProfiles* profiles=nullptr;
    if(FAILED(CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&profiles)))){
        CoUninitialize();return 3;
    }
    if(!Absent()||ProfileExists(profiles)){profiles->Release();CoUninitialize();return 4;}
    HMODULE module=LoadLibraryW(argv[1]);
    if(!module){profiles->Release();CoUninitialize();return 5;}
    auto reg=reinterpret_cast<Reg>(GetProcAddress(module,"DllRegisterServer"));
    auto unreg=reinterpret_cast<Reg>(GetProcAddress(module,"DllUnregisterServer"));
    int result=6;
    if(reg&&unreg){
        const HRESULT hr=reg();
        std::cout<<"Register HRESULT: 0x"<<std::hex<<static_cast<unsigned long>(hr)<<"\n";
        if(SUCCEEDED(hr)&&!Absent()&&ProfileExists(profiles)){
            ITfTextInputProcessorEx* tip=nullptr;
            HRESULT created=CoCreateInstance(clsid,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&tip));
            if(tip)tip->Release();
            if(SUCCEEDED(created))result=0;else result=7;
        }
        // Roll back even after partial registration. The preflight established
        // that this project-specific CLSID/profile did not previously exist.
        const HRESULT cleanup=unreg();
        if(FAILED(cleanup)||!Absent()||ProfileExists(profiles))result=8;
    }
    profiles->Release();FreeLibrary(module);CoUninitialize();
    if(!result)std::cout<<"Windows COM registration, TSF profile lookup, COM activation and removal passed; no keyboard/profile activation tested\n";
    return result;
}
