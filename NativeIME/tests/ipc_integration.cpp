#include "windows/engine_client.hpp"
#include "windows/pipe_security.hpp"
#include "windows/pipe_io.hpp"
#include <windows.h>
#include <algorithm>
#include <iostream>
#include <filesystem>
#include <thread>
using namespace nativeime;
struct Child {
    HANDLE stop=nullptr,process=nullptr,job=nullptr;
    ~Child(){if(process){SetEvent(stop);if(WaitForSingleObject(process,10000)!=WAIT_OBJECT_0)TerminateProcess(process,99);WaitForSingleObject(process,5000);CloseHandle(process);}if(job)CloseHandle(job);if(stop)CloseHandle(stop);}
    bool start(const std::wstring& exe,const std::wstring& shared,const std::wstring& user,const std::wstring& dll){
        SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};stop=CreateEventW(&sa,TRUE,FALSE,nullptr);
        job=CreateJobObjectW(nullptr,nullptr);if(!stop||!job)return false;
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if(!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&limits,sizeof(limits)))return false;
        std::wstring cmd=L"\""+exe+L"\" --shared \""+shared+L"\" --user \""+user+L"\" --schema luna_pinyin --rime-dll \""+dll+L"\" --stop-handle "+std::to_wstring(reinterpret_cast<ULONG_PTR>(stop));
        STARTUPINFOW si{sizeof(si)};PROCESS_INFORMATION pi{};
        if(!CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,TRUE,CREATE_SUSPENDED|CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi))return false;
        process=pi.hProcess;
        if(!AssignProcessToJobObject(job,process)){TerminateProcess(process,98);CloseHandle(pi.hThread);return false;}
        ResumeThread(pi.hThread);CloseHandle(pi.hThread);return true;
    }
};
bool Ready(const win::EngineClient& client){
    const auto deadline=GetTickCount64()+60000;
    while(GetTickCount64()<deadline){
        core::ipc::Response r;
        if(client.Query(L"nihao",r,1000)&&std::any_of(r.candidates.begin(),r.candidates.end(),[](const auto& c){return c.text==L"你好";}))return true;
        Sleep(100);
    }return false;
}
int wmain(int argc,wchar_t** argv){
    if(argc!=5)return 1;
    const std::wstring exe=std::filesystem::absolute(argv[1]).wstring();
    win::EngineClient client(exe);core::ipc::Response r;
    if(client.Query(L"nihao",r,80)){std::cerr<<"Unexpected existing service\n";return 2;}
    {
        Child c;if(!c.start(exe,argv[2],argv[3],argv[4])||!Ready(client))return 3;
        win::EngineClient wrong(win::ModulePath(nullptr));
        if(wrong.Query(L"nihao",r,1000))return 4;
        // A valid peer must recover after a client disconnects mid-frame.
        auto name=win::PipeName(win::LogonSid());
        if(!WaitNamedPipeW(name.c_str(),1000))return 5;
        HANDLE p=CreateFileW(name.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);
        if(p==INVALID_HANDLE_VALUE)return 6;
        DWORD invalid=0xffffffffu;win::Transfer(p,&invalid,sizeof(invalid),true,GetTickCount64()+200);
        CloseHandle(p);
        if(!Ready(client))return 7;
        // Force termination of only the child created by this test.
        TerminateProcess(c.process,42);WaitForSingleObject(c.process,5000);
        if(client.Query(L"nihao",r,80))return 8;
    }
    {
        Child c;if(!c.start(exe,argv[2],argv[3],argv[4])||!Ready(client))return 9;
        SetEvent(c.stop);
        if(WaitForSingleObject(c.process,10000)!=WAIT_OBJECT_0)return 10;
        DWORD code=1;GetExitCodeProcess(c.process,&code);if(code!=0)return 11;
    }
    // A same-image test endpoint that deliberately does not answer.
    auto name=win::PipeName(win::LogonSid());
    HANDLE pipe=CreateNamedPipeW(name.c_str(),PIPE_ACCESS_DUPLEX|FILE_FLAG_FIRST_PIPE_INSTANCE,PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,8192,8192,0,nullptr);
    if(pipe==INVALID_HANDLE_VALUE)return 12;
    std::thread stalled([&]{if(ConnectNamedPipe(pipe,nullptr)||GetLastError()==ERROR_PIPE_CONNECTED)Sleep(500);DisconnectNamedPipe(pipe);CloseHandle(pipe);});
    win::EngineClient self(win::ModulePath(nullptr));
    const auto begin=GetTickCount64();bool ok=self.Query(L"nihao",r,80);auto elapsed=GetTickCount64()-begin;
    stalled.join();
    if(ok||elapsed>400)return 13;
    std::cout<<"Real librime IPC, identity rejection, malformed request, crash, restart, stop and stalled read passed\n";
    return 0;
}
