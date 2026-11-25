#include "engine_client.hpp"
#include <windows.h>
#include <vector>
namespace nativeime::win {
namespace {
bool Write(HANDLE h,const void* p,DWORD n){DWORD sent=0;return WriteFile(h,p,n,&sent,nullptr)&&sent==n;}
bool Read(HANDLE h,void* p,DWORD n){DWORD got=0;return ReadFile(h,p,n,&got,nullptr)&&got==n;}
}
bool EngineClient::Query(const std::wstring& pinyin,nativeime::core::ipc::Response& response,DWORD timeout) const noexcept{
    HANDLE pipe=INVALID_HANDLE_VALUE;
    try{
        if(!WaitNamedPipeW(pipeName_.c_str(),timeout))return false;
        pipe=CreateFileW(pipeName_.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(pipe==INVALID_HANDLE_VALUE)return false;
        auto request=nativeime::core::ipc::EncodeRequest({pinyin});
        DWORD length=static_cast<DWORD>(request.size());
        bool ok=Write(pipe,&length,sizeof(length))&&Write(pipe,request.data(),length);
        DWORD responseLength=0;
        ok=ok&&Read(pipe,&responseLength,sizeof(responseLength))&&responseLength>=10&&responseLength<=nativeime::core::ipc::MaxFrame;
        std::vector<std::uint8_t> bytes;if(ok){bytes.resize(responseLength);ok=Read(pipe,bytes.data(),responseLength);}
        if(ok)ok=nativeime::core::ipc::DecodeResponse(bytes.data(),bytes.size(),response);
        CloseHandle(pipe);return ok&&response.ok;
    }catch(...){if(pipe!=INVALID_HANDLE_VALUE)CloseHandle(pipe);return false;}
}
}
