#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <sddl.h>
#include <filesystem>
namespace nativeime::win {
// Scope the endpoint to the current Windows logon session (not just the account).
inline std::wstring LogonSid(HANDLE process=GetCurrentProcess()){
    HANDLE token=nullptr;
    if(!OpenProcessToken(process,TOKEN_QUERY,&token))return {};
    DWORD size=0;GetTokenInformation(token,TokenGroups,nullptr,0,&size);
    std::vector<unsigned char> buffer(size);
    std::wstring result;
    if(size&&GetTokenInformation(token,TokenGroups,buffer.data(),size,&size)){
        auto* groups=reinterpret_cast<TOKEN_GROUPS*>(buffer.data());
        for(DWORD i=0;i<groups->GroupCount;++i){
            if((groups->Groups[i].Attributes&SE_GROUP_LOGON_ID)!=SE_GROUP_LOGON_ID)continue;
            LPWSTR sid=nullptr;
            if(ConvertSidToStringSidW(groups->Groups[i].Sid,&sid)){result=sid;LocalFree(sid);}
            break;
        }
    }
    CloseHandle(token);return result;
}
inline std::wstring PipeName(const std::wstring& sid){
    return sid.empty()?std::wstring{}:L"\\\\.\\pipe\\NativeIME.Engine.v2."+sid;
}
inline std::wstring ModulePath(HMODULE module){
    std::wstring path(32768,L'\0');
    const DWORD n=GetModuleFileNameW(module,path.data(),static_cast<DWORD>(path.size()));
    if(!n||n>=path.size())return {};
    path.resize(n);return path;
}
inline bool VerifyServer(HANDLE pipe,const std::wstring& logon,const std::wstring& expected){
    if(expected.empty()||!std::filesystem::path(expected).is_absolute())return false;
    ULONG pid=0;
    if(!GetNamedPipeServerProcessId(pipe,&pid))return false;
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);
    if(!process)return false;
    wchar_t image[32768]{};DWORD length=32768;
    bool ok=false;
    try{
        if(LogonSid(process)==logon&&QueryFullProcessImageNameW(process,0,image,&length)){
            std::error_code ec;
            ok=std::filesystem::equivalent(std::filesystem::path(image),std::filesystem::path(expected),ec)&&!ec;
        }
    }catch(...){ok=false;}
    CloseHandle(process);return ok;
}
}
