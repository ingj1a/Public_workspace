#include "windows/engine_client.hpp"
#include "windows/pipe_security.hpp"
#include "windows/pipe_io.hpp"
#include "windows/async_engine.hpp"
#include "windows/tsf_popup.hpp"
#include "core/input_session.hpp"
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
// Real engine + production async client + native popup and EDIT.
// EM_REPLACESEL is not a TSF edit session.
bool NativeControlIntegration(const std::wstring& exe){
    HWND root=CreateWindowExW(0,L"STATIC",L"NativeIME integration test",WS_OVERLAPPEDWINDOW,
        40,40,500,300,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!root)return false;
    HWND edit=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_VISIBLE|ES_AUTOHSCROLL,
        12,12,400,32,root,nullptr,GetModuleHandleW(nullptr),nullptr);
    bool ok=false;
    if(edit){
        ShowWindow(root,SW_SHOWNOACTIVATE);
        core::InputSession session([](const std::wstring& p){return core::QueryResult{p,{}};});
        for(auto ch:std::wstring(L"nihao"))session.Handle({core::Key::Letter,ch});
        win::AsyncEngine async(edit,exe);
        for(unsigned long long generation=1;generation<=5&&!ok;++generation){
            async.Submit(generation,L"nihao");
            const auto end=GetTickCount64()+1000;
            while(GetTickCount64()<end){
                MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){
                    if(msg.message!=win::EngineReady){TranslateMessage(&msg);DispatchMessageW(&msg);}
                }
                auto reply=async.Take();
                if(!reply){Sleep(5);continue;}
                if(reply->generation!=generation)continue;
                if(!reply->response.ok)break;
                ok=session.AcceptCandidates(reply->input,std::move(reply->response.candidates));break;
            }
        }
        async.Stop();
        if(ok){
            const auto it=std::find_if(session.Candidates().begin(),session.Candidates().end(),[](const auto& c){return c.text==L"你好";});
            ok=it!=session.Candidates().end();
            if(ok){
                const auto index=std::distance(session.Candidates().begin(),it);
                POINT pt{20,40};ClientToScreen(edit,&pt);
                RECT anchor{pt.x,pt.y,pt.x+1,pt.y+16};
                win::TsfPopup popup;const HWND prior=GetFocus();
                ok=popup.Show(edit,anchor,session)&&popup.Visible()&&GetFocus()==prior;
                if(ok){
                    auto result=session.Handle({core::Key::Digit,static_cast<wchar_t>(L'1'+index)});
                    SendMessageW(edit,EM_REPLACESEL,TRUE,reinterpret_cast<LPARAM>(result.commit.c_str()));
                    wchar_t actual[64]{};GetWindowTextW(edit,actual,64);
                    popup.Hide();
                    ok=result.consumed&&std::wstring(actual)==L"你好"&&!popup.Visible();
                }
            }
        }
    }
    DestroyWindow(root);
    if(ok)std::cout<<"Real Rime async candidate, native popup and Win32 EDIT insertion passed (not TSF edit session)\n";
    return ok;
}
// Identify only a service belonging to the disposable test's own Job object.
HANDLE OwnedService(const Child& supervisor,DWORD& pid){
    pid=0;
    const auto name=win::PipeName(win::LogonSid());
    if(!WaitNamedPipeW(name.c_str(),1000))return nullptr;
    HANDLE pipe=CreateFileW(name.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED|SECURITY_SQOS_PRESENT|SECURITY_IDENTIFICATION,nullptr);
    if(pipe==INVALID_HANDLE_VALUE)return nullptr;
    ULONG actual=0;
    const BOOL identified=GetNamedPipeServerProcessId(pipe,&actual);
    CloseHandle(pipe);
    if(!identified||!actual)return nullptr;
    HANDLE process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE|PROCESS_TERMINATE,FALSE,actual);
    if(!process)return nullptr;
    BOOL owned=FALSE;
    if(!IsProcessInJob(process,supervisor.job,&owned)||!owned){CloseHandle(process);return nullptr;}
    pid=actual;return process;
}
bool SupervisorIntegration(const std::wstring& exe,const std::wstring& supervisor,const std::wstring& shared,
                           const std::wstring& user,const std::wstring& dll){
    win::EngineClient client(exe);
    Child c;
    if(!c.start(supervisor,shared,user,dll)||!Ready(client))return false;
    DWORD original=0;
    HANDLE first=OwnedService(c,original);
    if(!first)return false;
    // Terminate only the verified descendant of this test process.
    bool killed=TerminateProcess(first,42)&&WaitForSingleObject(first,5000)==WAIT_OBJECT_0;
    CloseHandle(first);
    if(!killed||!Ready(client))return false;
    DWORD replacement=0;
    HANDLE second=OwnedService(c,replacement);
    if(!second)return false;
    bool ok=replacement!=original&&SetEvent(c.stop)&&WaitForSingleObject(c.process,15000)==WAIT_OBJECT_0;
    DWORD status=1;
    ok=ok&&GetExitCodeProcess(c.process,&status)&&status==0&&WaitForSingleObject(second,5000)==WAIT_OBJECT_0;
    CloseHandle(second);
    core::ipc::Response r;
    ok=ok&&!client.Query(L"nihao",r,80);
    if(ok)std::cout<<"Supervisor: real service restarted automatically and both processes stopped cleanly\n";
    return ok;
}
int wmain(int argc,wchar_t** argv){
    if(argc!=6)return 1;
    const std::wstring exe=std::filesystem::absolute(argv[1]).wstring();
    win::EngineClient client(exe);core::ipc::Response r;
    if(client.Query(L"nihao",r,80)){std::cerr<<"Unexpected existing service\n";return 2;}
    {
        Child c;if(!c.start(exe,argv[2],argv[3],argv[4])||!Ready(client))return 3;
        if(!NativeControlIntegration(exe))return 14;
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
    if(!SupervisorIntegration(exe,std::filesystem::absolute(argv[5]).wstring(),argv[2],argv[3],argv[4]))return 15;
    return 0;
}
