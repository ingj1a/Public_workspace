#include <windows.h>
#include "core/rime_provider.hpp"
#include "core/engine_protocol.hpp"
#include "pipe_security.hpp"
#include "pipe_io.hpp"
#include <filesystem>
#include <iostream>
namespace {
using namespace nativeime;
std::wstring Arg(int argc,wchar_t** argv,const wchar_t* key){for(int i=1;i+1<argc;++i)if(_wcsicmp(argv[i],key)==0)return argv[i+1];return {};}
void Serve(HANDLE pipe,core::RimeProvider& rime){
    const auto deadline=GetTickCount64()+500;
    std::uint32_t n=0;if(!win::Transfer(pipe,&n,sizeof(n),false,deadline)||n<13||n>core::ipc::MaxFrame)return;
    std::vector<std::uint8_t> frame(n);if(!win::Transfer(pipe,frame.data(),n,false,deadline))return;
    core::ipc::Request req;core::ipc::Response resp;
    if(core::ipc::DecodeRequest(frame.data(),frame.size(),req)){
        try{resp.candidates=rime.Query(req.pinyin).candidates;resp.ok=true;}catch(...){resp={};}
    }
    auto out=core::ipc::EncodeResponse(resp);auto len=static_cast<std::uint32_t>(out.size());
    if(win::Transfer(pipe,&len,sizeof(len),true,deadline)&&win::Transfer(pipe,out.data(),len,true,deadline)){
        unsigned char ack=0;win::Transfer(pipe,&ack,1,false,deadline);
    }
}
}
int wmain(int argc,wchar_t** argv){
    try{
        auto shared=Arg(argc,argv,L"--shared"),user=Arg(argc,argv,L"--user"),schema=Arg(argc,argv,L"--schema"),dll=Arg(argc,argv,L"--rime-dll");
        if(shared.empty()||user.empty()||schema.empty()||dll.empty())return 2;
        std::filesystem::create_directories(user);
        auto sid=nativeime::win::LogonSid(),name=nativeime::win::PipeName(sid);
        if(name.empty())return 3;
        PSECURITY_DESCRIPTOR sd=nullptr;
        const auto sddl=L"D:P(A;;GA;;;"+sid+L")";
        if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl.c_str(),SDDL_REVISION_1,&sd,nullptr))return 3;
        SECURITY_ATTRIBUTES sa{sizeof(sa),sd,FALSE};
        HANDLE pipe=CreateNamedPipeW(name.c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED|FILE_FLAG_FIRST_PIPE_INSTANCE,
            PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,8192,8192,0,&sa);
        LocalFree(sd);
        if(pipe==INVALID_HANDLE_VALUE)return 3;
        nativeime::core::RimeProvider rime(std::filesystem::path(shared).u8string(),std::filesystem::path(user).u8string(),std::filesystem::path(schema).u8string(),dll);
        for(;;){
            OVERLAPPED ov{};ov.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);
            if(!ov.hEvent){CloseHandle(pipe);return 3;}
            BOOL connected=ConnectNamedPipe(pipe,&ov);
            if(!connected){
                const auto err=GetLastError();
                if(err==ERROR_PIPE_CONNECTED)connected=TRUE;
                else if(err==ERROR_IO_PENDING){DWORD bytes=0;connected=GetOverlappedResult(pipe,&ov,&bytes,TRUE);}
            }
            CloseHandle(ov.hEvent);
            if(connected)Serve(pipe,rime);
            DisconnectNamedPipe(pipe);
        }
    }catch(...){return 4;}
}
