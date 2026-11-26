#pragma once
#include <windows.h>
#include <msctf.h>
#include <filesystem>
#include "pipe_security.hpp"
namespace nativeime::win {
// Read-only, no input content and no exported file paths.
// -1 means unavailable, 0 means false, 1 means true.
struct LabEnvironment {
    int runtime=0, schema=0, service=0, registered=0, enabled=-1, chineseLanguage=-1;
};
inline LabEnvironment InspectLabEnvironment() noexcept {
    LabEnvironment s;
    try{
        auto module=ModulePath(nullptr);
        if(module.empty())return s;
        auto base=std::filesystem::path(module).parent_path();
        std::error_code ec;
        s.runtime=std::filesystem::is_regular_file(base/L"runtime"/L"rime.dll",ec)?1:0;
        s.schema=std::filesystem::is_regular_file(base/L"shared"/L"luna_pinyin.schema.yaml",ec)?1:0;
        s.service=std::filesystem::is_regular_file(base/L"NativeIME.EngineService.exe",ec)&&
            std::filesystem::is_regular_file(base/L"NativeIME.EngineSupervisor.exe",ec)&&
            std::filesystem::is_regular_file(base/L"NativeIME.Session.exe",ec)?1:0;
        wchar_t path[32768]{};DWORD bytes=sizeof(path);
        if(RegGetValueW(HKEY_CLASSES_ROOT,L"CLSID\\{9D61A6D1-2C5E-4B6B-9C8A-7A8E1F2D1A10}\\InprocServer32",
            nullptr,RRF_RT_REG_SZ,nullptr,path,&bytes)==ERROR_SUCCESS){
            auto expected=(base/L"NativeIME.TSF.dll").wstring();
            s.registered=CompareStringOrdinal(path,-1,expected.c_str(),-1,TRUE)==CSTR_EQUAL?1:0;
        }
        const CLSID clsid={0x9d61a6d1,0x2c5e,0x4b6b,{0x9c,0x8a,0x7a,0x8e,0x1f,0x2d,0x1a,0x10}};
        const GUID profile={0xd0b37c31,0x9e0b,0x4a68,{0x8c,0x5d,0x7a,0x3a,0x29,0xd6,0xe4,0x11}};
        constexpr LANGID lang=MAKELANGID(LANG_CHINESE,SUBLANG_CHINESE_SIMPLIFIED);
        HRESULT init=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
        if(SUCCEEDED(init)||init==RPC_E_CHANGED_MODE){
            ITfInputProcessorProfiles* profiles=nullptr;
            if(SUCCEEDED(CoCreateInstance(CLSID_TF_InputProcessorProfiles,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&profiles)))){
                BOOL enabled=FALSE;LANGID current=0;
                if(SUCCEEDED(profiles->IsEnabledLanguageProfile(clsid,lang,profile,&enabled)))s.enabled=enabled?1:0;
                if(SUCCEEDED(profiles->GetCurrentLanguage(&current)))s.chineseLanguage=current==lang?1:0;
                profiles->Release();
            }
            if(SUCCEEDED(init))CoUninitialize();
        }
    }catch(...){}
    return s;
}
inline const wchar_t* LabStatus(int n) noexcept {return n==1?L"是":n==0?L"否":L"未知";}
}
