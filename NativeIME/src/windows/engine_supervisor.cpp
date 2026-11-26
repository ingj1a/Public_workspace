// Explicitly launched per-user supervisor. No installation, startup entry,
// scheduled task, elevated process or TSF callback process creation.
#include <windows.h>
#include "pipe_security.hpp"
#include "diagnostics.hpp"
#include <filesystem>
#include <iostream>
namespace {
HANDLE stopping=nullptr;
BOOL WINAPI Handler(DWORD kind){
    if(kind==CTRL_C_EVENT||kind==CTRL_BREAK_EVENT){if(stopping)SetEvent(stopping);return TRUE;}return FALSE;
}
std::wstring Arg(int n,wchar_t** args,const wchar_t* key){
    for(int i=1;i+1<n;++i)if(_wcsicmp(args[i],key)==0)return args[i+1];return {};
}
struct Handle {
    HANDLE h=nullptr;
    explicit Handle(HANDLE value=nullptr):h(value){}
    ~Handle(){if(h&&h!=INVALID_HANDLE_VALUE)CloseHandle(h);}
    Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
};
bool SafeArg(const std::wstring& s){return !s.empty()&&s.find(L'"')==std::wstring::npos&&s.back()!=L'\\';}
}
int wmain(int argc,wchar_t** argv){
    try{
        auto shared=Arg(argc,argv,L"--shared"),user=Arg(argc,argv,L"--user"),dll=Arg(argc,argv,L"--rime-dll");
        if(!SafeArg(shared)||!SafeArg(user)||!SafeArg(dll))return 2;
        for(const auto& v:{shared,user,dll})if(!std::filesystem::path(v).is_absolute())return 2;
        const auto exe=(std::filesystem::path(nativeime::win::ModulePath(nullptr)).parent_path()/L"NativeIME.EngineService.exe").wstring();
        if(!std::filesystem::is_regular_file(exe))return 3;
        auto sid=nativeime::win::LogonSid();if(sid.empty())return 3;
        Handle mutex(CreateMutexW(nullptr,FALSE,(L"Local\\NativeIME.Supervisor.v1."+sid).c_str()));
        const auto mutexError=GetLastError();
        if(!mutex.h||mutexError==ERROR_ALREADY_EXISTS)return 4;
        auto inherited=Arg(argc,argv,L"--stop-handle");
        Handle stop(inherited.empty()?CreateEventW(nullptr,TRUE,FALSE,nullptr):reinterpret_cast<HANDLE>(static_cast<ULONG_PTR>(std::stoull(inherited))));
        DWORD flags=0;if(!stop.h||!GetHandleInformation(stop.h,&flags))return 5;
        // Do not pass the controller's event to the service.
        SetHandleInformation(stop.h,HANDLE_FLAG_INHERIT,0);
        stopping=stop.h;SetConsoleCtrlHandler(Handler,TRUE);
        int failures=0;nativeime::win::diag::Writer diagnostics;
        while(WaitForSingleObject(stop.h,0)==WAIT_TIMEOUT){
            SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};
            Handle childStop(CreateEventW(&sa,TRUE,FALSE,nullptr));
            Handle job(CreateJobObjectW(nullptr,nullptr));
            if(!childStop.h||!job.h)return 6;
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
            limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            if(!SetInformationJobObject(job.h,JobObjectExtendedLimitInformation,&limits,sizeof(limits)))return 6;
            std::wstring cmd=L"\""+exe+L"\" --shared \""+shared+L"\" --user \""+user+L"\" --schema luna_pinyin --rime-dll \""+dll+L"\" --stop-handle "+std::to_wstring(reinterpret_cast<ULONG_PTR>(childStop.h));
            STARTUPINFOEXW si{};si.StartupInfo.cb=sizeof(si);
            SIZE_T size=0;InitializeProcThreadAttributeList(nullptr,1,0,&size);
            if(!size)return 6;
            std::vector<unsigned char> buffer(size);
            si.lpAttributeList=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(buffer.data());
            if(!InitializeProcThreadAttributeList(si.lpAttributeList,1,0,&size))return 6;
            HANDLE inheritedHandles[]={childStop.h};
            BOOL prepared=UpdateProcThreadAttribute(si.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,inheritedHandles,sizeof(inheritedHandles),nullptr,nullptr);
            PROCESS_INFORMATION pi{};
            BOOL started=prepared&&CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,TRUE,
                EXTENDED_STARTUPINFO_PRESENT|CREATE_SUSPENDED|CREATE_NO_WINDOW,nullptr,nullptr,&si.StartupInfo,&pi);
            DeleteProcThreadAttributeList(si.lpAttributeList);
            if(!started)return 7;
            Handle process(pi.hProcess),thread(pi.hThread);
            if(!AssignProcessToJobObject(job.h,process.h)){TerminateProcess(process.h,98);return 7;}
            if(ResumeThread(thread.h)==static_cast<DWORD>(-1)){TerminateProcess(process.h,98);return 7;}
            const auto begin=GetTickCount64();
            HANDLE waits[]={stop.h,process.h};
            const auto wait=WaitForMultipleObjects(2,waits,FALSE,INFINITE);
            if(wait==WAIT_OBJECT_0){
                SetEvent(childStop.h);
                if(WaitForSingleObject(process.h,10000)!=WAIT_OBJECT_0){
                    TerminateProcess(process.h,99);WaitForSingleObject(process.h,5000);
                }
                break;
            }
            if(wait!=WAIT_OBJECT_0+1)return 8;
            DWORD code=1;GetExitCodeProcess(process.h,&code);
            if(code==0)break;
            // A second running service is not a crash to recover from.
            if(code==3)return 9;
            if(GetTickCount64()-begin>60000)failures=0;
            if(++failures>3)return 10;
            diagnostics.Record(nativeime::win::diag::Event::SupervisorRestart);
            const DWORD delay=500u<<(failures-1);
            std::cerr<<"Engine exit "<<code<<"; restarting after "<<delay<<" ms\n";
            if(WaitForSingleObject(stop.h,delay)!=WAIT_TIMEOUT)break;
        }
        SetConsoleCtrlHandler(Handler,FALSE);stopping=nullptr;return 0;
    }catch(...){return 11;}
}
