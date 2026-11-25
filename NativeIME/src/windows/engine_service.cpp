#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "core/rime_provider.hpp"
#include "core/engine_protocol.hpp"
#include <string>
#include <vector>
#include <iostream>
namespace {
bool ReadExact(HANDLE h,void* p,DWORD n){auto b=static_cast<unsigned char*>(p);while(n){DWORD got=0;if(!ReadFile(h,b,n,&got,nullptr)||!got)return false;b+=got;n-=got;}return true;}
bool WriteExact(HANDLE h,const void* p,DWORD n){auto b=static_cast<const unsigned char*>(p);while(n){DWORD sent=0;if(!WriteFile(h,b,n,&sent,nullptr)||!sent)return false;b+=sent;n-=sent;}return true;}
std::wstring Arg(int argc,wchar_t** argv,const wchar_t* key){for(int i=1;i+1<argc;++i)if(_wcsicmp(argv[i],key)==0)return argv[i+1];return {};}
bool Serve(HANDLE pipe,nativeime::core::RimeProvider& rime){
    for(;;){std::uint32_t n=0;if(!ReadExact(pipe,&n,sizeof(n)))return false;if(n<12||n>nativeime::core::ipc::MaxFrame)return false;
        std::vector<std::uint8_t> frame(n);if(!ReadExact(pipe,frame.data(),n))return false;
        nativeime::core::ipc::Request req;nativeime::core::ipc::Response resp;
        if(nativeime::core::ipc::DecodeRequest(frame.data(),frame.size(),req)){resp.ok=true;resp.candidates=rime.Query(req.pinyin).candidates;}
        auto out=nativeime::core::ipc::EncodeResponse(resp);const auto len=static_cast<std::uint32_t>(out.size());
        if(!WriteExact(pipe,&len,sizeof(len))||!WriteExact(pipe,out.data(),len))return false;
    }
}
}
int wmain(int argc,wchar_t** argv){
    try{
        auto shared=Arg(argc,argv,L"--shared"),user=Arg(argc,argv,L"--user"),schema=Arg(argc,argv,L"--schema"),dll=Arg(argc,argv,L"--rime-dll");
        if(shared.empty()||user.empty()||schema.empty()||dll.empty())return 2;
        std::filesystem::create_directories(user);
        nativeime::core::RimeProvider rime(std::string(shared.begin(),shared.end()),std::string(user.begin(),user.end()),std::string(schema.begin(),schema.end()),dll);
        constexpr wchar_t pipeName[]=L"\\\\.\\pipe\\NativeIME.Engine.v1";
        for(;;){
            HANDLE pipe=CreateNamedPipeW(pipeName,PIPE_ACCESS_DUPLEX,PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,nativeime::core::ipc::MaxFrame+4,nativeime::core::ipc::MaxFrame+4,0,nullptr);
            if(pipe==INVALID_HANDLE_VALUE)return 3;
            BOOL connected=ConnectNamedPipe(pipe,nullptr)?TRUE:(GetLastError()==ERROR_PIPE_CONNECTED);
            if(connected)Serve(pipe,rime);
            FlushFileBuffers(pipe);DisconnectNamedPipe(pipe);CloseHandle(pipe);
        }
    }catch(...){return 4;}
}
