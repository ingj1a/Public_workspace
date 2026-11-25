#include "engine_client.hpp"
#include "pipe_security.hpp"
#include "pipe_io.hpp"
namespace nativeime::win {
bool EngineClient::Query(const std::wstring& pinyin,core::ipc::Response& response,DWORD timeout) const noexcept{
    HANDLE pipe=INVALID_HANDLE_VALUE;response={};
    try{
        if(!timeout||timeout>1000)return false;
        const auto deadline=GetTickCount64()+timeout;
        const auto sid=LogonSid(),name=PipeName(sid);
        if(name.empty())return false;
        auto request=core::ipc::EncodeRequest({pinyin});
        const auto now=GetTickCount64();
        if(now>=deadline||!WaitNamedPipeW(name.c_str(),static_cast<DWORD>(deadline-now)))return false;
        pipe=CreateFileW(name.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,
                         FILE_FLAG_OVERLAPPED|SECURITY_SQOS_PRESENT|SECURITY_IDENTIFICATION,nullptr);
        if(pipe==INVALID_HANDLE_VALUE)return false;
        DWORD n=static_cast<DWORD>(request.size()),len=0;
        bool ok=VerifyServer(pipe,sid)&&Transfer(pipe,&n,sizeof(n),true,deadline)&&Transfer(pipe,request.data(),n,true,deadline)
            &&Transfer(pipe,&len,sizeof(len),false,deadline)&&len>=10&&len<=core::ipc::MaxFrame;
        if(ok){std::vector<std::uint8_t> bytes(len);ok=Transfer(pipe,bytes.data(),len,false,deadline)
            &&core::ipc::DecodeResponse(bytes.data(),bytes.size(),response);}
        unsigned char ack=1;if(ok)Transfer(pipe,&ack,1,true,deadline);
        CloseHandle(pipe);return ok&&response.ok;
    }catch(...){if(pipe!=INVALID_HANDLE_VALUE)CloseHandle(pipe);return false;}
}
}
