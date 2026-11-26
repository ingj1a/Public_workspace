// Explicit Windows native session entry point. No registry, startup tasks,
// profile enablement or process creation in TSF callbacks.
#include <windows.h>
#include "engine_client.hpp"
#include "pipe_security.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <vector>
namespace {
using namespace nativeime;
HANDLE stopping=nullptr;
BOOL WINAPI StopHandler(DWORD kind){
    if(kind==CTRL_C_EVENT||kind==CTRL_BREAK_EVENT){if(stopping)SetEvent(stopping);return TRUE;}return FALSE;
}
struct Handle {
    HANDLE value=nullptr;
    explicit Handle(HANDLE h=nullptr):value(h){}
    ~Handle(){if(value&&value!=INVALID_HANDLE_VALUE)CloseHandle(value);}
    Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
};
std::wstring Arg(int argc,wchar_t** argv,const wchar_t* key){
    for(int i=1;i+1<argc;++i)if(wcscmp(argv[i],key)==0)return argv[i+1];return {};
}
bool SafePath(const std::wstring& s){
    return !s.empty()&&s.find(L'"')==std::wstring::npos&&s.back()!=L'\\'&&std::filesystem::path(s).is_absolute();
}
std::wstring Quote(const std::wstring& s){return L"\""+s+L"\"";}
struct Session {
    Handle stop{nullptr},job{nullptr},supervisor{nullptr};
    ~Session(){Close();}
    bool Start(const std::wstring& exe,const std::wstring& shared,const std::wstring& user,const std::wstring& dll){
        SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};
        stop.value=CreateEventW(&sa,TRUE,FALSE,nullptr);
        job.value=CreateJobObjectW(nullptr,nullptr);
        if(!stop.value||!job.value)return false;
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
        info.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if(!SetInformationJobObject(job.value,JobObjectExtendedLimitInformation,&info,sizeof(info)))return false;
        STARTUPINFOEXW si{};si.StartupInfo.cb=sizeof(si);
        SIZE_T size=0;InitializeProcThreadAttributeList(nullptr,1,0,&size);
        if(!size)return false;
        std::vector<unsigned char> storage(size);
        si.lpAttributeList=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
        if(!InitializeProcThreadAttributeList(si.lpAttributeList,1,0,&size))return false;
        HANDLE inherited[]={stop.value};
        BOOL ok=UpdateProcThreadAttribute(si.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inherited,sizeof(inherited),nullptr,nullptr);
        auto cmd=Quote(exe)+L" --shared "+Quote(shared)+L" --user "+Quote(user)+L" --rime-dll "+Quote(dll)+L" --stop-handle "+std::to_wstring(reinterpret_cast<ULONG_PTR>(stop.value));
        PROCESS_INFORMATION pi{};
        ok=ok&&CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,TRUE,
            CREATE_SUSPENDED|CREATE_NO_WINDOW|EXTENDED_STARTUPINFO_PRESENT,nullptr,nullptr,&si.StartupInfo,&pi);
        DeleteProcThreadAttributeList(si.lpAttributeList);
        if(!ok)return false;
        supervisor.value=pi.hProcess;Handle thread(pi.hThread);
        if(!AssignProcessToJobObject(job.value,supervisor.value)||ResumeThread(thread.value)==static_cast<DWORD>(-1)){
            TerminateProcess(supervisor.value,98);return false;
        }
        return true;
    }
    bool Close() noexcept {
        if(!supervisor.value)return true;
        SetEvent(stop.value);
        bool clean=WaitForSingleObject(supervisor.value,15000)==WAIT_OBJECT_0;
        DWORD code=1;
        clean=clean&&GetExitCodeProcess(supervisor.value,&code)&&code==0;
        if(!clean)TerminateJobObject(job.value,99);
        WaitForSingleObject(supervisor.value,5000);
        CloseHandle(supervisor.value);supervisor.value=nullptr;
        return clean;
    }
};
bool Ready(const win::EngineClient& client){
    core::ipc::Response reply;
    return client.Query(L"nihao",reply,500)&&std::any_of(reply.candidates.begin(),reply.candidates.end(),
        [](const auto& c){return c.text==L"\u4f60\u597d";});
}
bool OwnService(const Session& session){
    const auto sid=win::LogonSid(),name=win::PipeName(sid);
    if(name.empty()||!WaitNamedPipeW(name.c_str(),500))return false;
    Handle pipe(CreateFileW(name.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED|SECURITY_SQOS_PRESENT|SECURITY_IDENTIFICATION,nullptr));
    if(pipe.value==INVALID_HANDLE_VALUE)return false;
    ULONG pid=0;if(!GetNamedPipeServerProcessId(pipe.value,&pid))return false;
    Handle process(OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid));
    BOOL owned=FALSE;
    return process.value&&IsProcessInJob(process.value,session.job.value,&owned)&&owned;
}
int Desktop(const std::wstring& exe,HANDLE cancel,HANDLE supervisor){
    auto cmd=Quote(exe)+L" --isolated-desktop";
    STARTUPINFOW si{sizeof(si)};PROCESS_INFORMATION pi{};
    if(!CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&si,&pi))return 10;
    Handle child(pi.hProcess),thread(pi.hThread);
    // Give the child a chance to restore its own process profile. Do not
    // forcibly terminate it on an ordinary cancellation or engine failure.
    HANDLE waits[]={child.value,cancel,supervisor};
    DWORD wait=WaitForMultipleObjects(3,waits,FALSE,INFINITE);
    if(wait!=WAIT_OBJECT_0&&WaitForSingleObject(child.value,60000)!=WAIT_OBJECT_0){
        std::cerr<<"Desktop verifier still running; close its own test window to finish cleanup.\n";
        WaitForSingleObject(child.value,INFINITE);
    }
    DWORD code=1;if(!GetExitCodeProcess(child.value,&code))return 11;
    return static_cast<int>(code);
}
}
int wmain(int argc,wchar_t** argv){
    try{
        if(argc!=8){std::cerr<<"Usage: NativeIME.Session.exe --engine-only-check|--run|--desktop-check --shared ABS --user ABS --rime-dll ABS\n";return 1;}
        std::wstring mode=argv[1],shared=Arg(argc,argv,L"--shared"),user=Arg(argc,argv,L"--user"),dll=Arg(argc,argv,L"--rime-dll");
        if(mode!=L"--engine-only-check"&&mode!=L"--run"&&mode!=L"--desktop-check")return 1;
        if(!SafePath(shared)||!SafePath(user)||!SafePath(dll)||!std::filesystem::is_directory(shared)||
           !std::filesystem::is_regular_file(dll))return 2;
        if(std::filesystem::weakly_canonical(shared)==std::filesystem::weakly_canonical(user))return 2;
        auto module=win::ModulePath(nullptr);if(module.empty())return 2;
        auto dir=std::filesystem::path(module).parent_path();
        auto supervisor=(dir/L"NativeIME.EngineSupervisor.exe").wstring(),engine=(dir/L"NativeIME.EngineService.exe").wstring(),
             desktop=(dir/L"NativeIME.TsfDesktopE2E.exe").wstring();
        if(!std::filesystem::is_regular_file(supervisor)||!std::filesystem::is_regular_file(engine)||
           (mode==L"--desktop-check"&&!std::filesystem::is_regular_file(desktop)))return 3;
        // Never adopt or terminate a pre-existing service.
        win::EngineClient client(engine);core::ipc::Response response;
        if(client.Query(L"nihao",response,500)){std::cerr<<"Existing service detected; not starting a second session.\n";return 4;}
        Handle cancel(CreateEventW(nullptr,TRUE,FALSE,nullptr));if(!cancel.value)return 5;
        stopping=cancel.value;SetConsoleCtrlHandler(StopHandler,TRUE);
        Session session;int result=6;
        if(session.Start(supervisor,shared,user,dll)){
            const auto deadline=GetTickCount64()+60000;
            while(GetTickCount64()<deadline&&WaitForSingleObject(cancel.value,0)==WAIT_TIMEOUT&&
                  WaitForSingleObject(session.supervisor.value,0)==WAIT_TIMEOUT){
                if(Ready(client)&&OwnService(session)){result=0;break;}
                Sleep(100);
            }
            if(result==0){
                std::cout<<"NativeIME session ready: real Rime candidate returned by owned engine\n";
                if(mode==L"--desktop-check")result=Desktop(desktop,cancel.value,session.supervisor.value);
                else if(mode==L"--run"){
                    HANDLE waits[]={cancel.value,session.supervisor.value};
                    if(WaitForMultipleObjects(2,waits,FALSE,INFINITE)!=WAIT_OBJECT_0)result=7;
                }
            }else std::cerr<<"Engine did not become ready within startup window\n";
        }
        if(!session.Close()&&result==0)result=8;
        // Only test-owned descendants are stopped. Verify that no ready peer remains.
        if(result==0&&client.Query(L"nihao",response,500))result=9;
        SetConsoleCtrlHandler(StopHandler,FALSE);stopping=nullptr;
        if(result==0)std::cout<<"NativeIME session stopped cleanly\n";
        return result;
    }catch(const std::exception& e){std::cerr<<e.what()<<"\n";return 12;}catch(...){return 12;}
}
