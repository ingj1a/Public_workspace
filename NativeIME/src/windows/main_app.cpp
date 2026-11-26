// Native product shell: the diagnostics page is part of this process.
// The engine session remains a private child, never a second UI application.
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <filesystem>
#include <vector>
#include <string>
#include <future>
#include <chrono>
#include <algorithm>
#include "engine_client.hpp"
#include "lab_environment.hpp"
#include "diagnostics_page.hpp"
#include "pipe_security.hpp"
namespace {
using namespace nativeime::win;
enum {Home=201,Diagnostics,StartEngine,StopEngine,CheckSetup,LanguageSettings};
HWND root=nullptr,home=nullptr,diag=nullptr,homeNav=nullptr,diagNav=nullptr;
HWND engineState=nullptr,startButton=nullptr,stopButton=nullptr,testEdit=nullptr,setupState=nullptr;
HANDLE session=nullptr,stopEvent=nullptr,job=nullptr;
bool stopping=false;
DWORD lastExit=0;
bool everStarted=false,closing=false,engineReady=false;
ULONGLONG nextProbe=0;
std::future<bool> readiness;
bool HasSession(){return session!=nullptr;}
void CloseHandles(){
    if(session)CloseHandle(session);session=nullptr;
    if(stopEvent)CloseHandle(stopEvent);stopEvent=nullptr;
    SetDiagnosticsEngineJob(nullptr);
    if(job)CloseHandle(job);job=nullptr;
    stopping=false;engineReady=false;nextProbe=0;
}
HWND Control(HWND parent,const wchar_t* cls,const wchar_t* text,DWORD style,int id,int x,int y,int w,int h){
    HWND c=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
    if(c)SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);
    return c;
}
void ShowPage(bool diagnostic){
    ShowWindow(home,diagnostic?SW_HIDE:SW_SHOW);
    ShowWindow(diag,diagnostic?SW_SHOW:SW_HIDE);
    EnableWindow(homeNav,diagnostic);EnableWindow(diagNav,!diagnostic);
    SetFocus(diagnostic?homeNav:diagNav);
}
void RefreshSetup(){
    const auto s=InspectLabEnvironment();
    std::wstring text=L"运行库："+std::wstring(LabStatus(s.runtime))+L"  词库："+LabStatus(s.schema)+L"  引擎组件："+LabStatus(s.service)
        +L"\r\n本构建已注册："+LabStatus(s.registered)+L"  输入配置已启用："+LabStatus(s.enabled)+L"  简体中文："+LabStatus(s.chineseLanguage);
    SetWindowTextW(setupState,text.c_str());
}
void Refresh(){
    if(session&&WaitForSingleObject(session,0)==WAIT_OBJECT_0){
        lastExit=1;GetExitCodeProcess(session,&lastExit);CloseHandles();
    }
    if(readiness.valid()&&readiness.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        bool ok=readiness.get();engineReady=session&&!stopping&&ok;
    }
    if(session&&!stopping&&!readiness.valid()&&GetTickCount64()>=nextProbe){
        nextProbe=GetTickCount64()+(engineReady?3000:1000);
        const auto exe=(std::filesystem::path(ModulePath(nullptr)).parent_path()/L"NativeIME.EngineService.exe").wstring();
        readiness=std::async(std::launch::async,[exe]{
            EngineClient client(exe);nativeime::core::ipc::Response response;
            return client.Query(L"nihao",response,500)&&std::any_of(response.candidates.begin(),response.candidates.end(),[](const auto& c){return c.text==L"你好";});
        });
    }
    std::wstring text;
    if(stopping)text=L"正在停止引擎会话…";
    else if(session)text=engineReady?L"引擎已就绪 · 已确认真实词库候选。":L"引擎正在启动或恢复，暂不可用。";
    else if(everStarted&&lastExit)text=L"引擎会话退出，状态码 "+std::to_wstring(lastExit)+L"。";
    else text=L"引擎会话未启动。";
    SetWindowTextW(engineState,text.c_str());
    EnableWindow(startButton,!session&&!readiness.valid()&&!closing);EnableWindow(stopButton,session&&!stopping&&!closing);
    if(closing&&!session&&!readiness.valid())DestroyWindow(root);
}
bool Safe(const std::wstring& p){return !p.empty()&&p.find(L'"')==std::wstring::npos&&p.back()!=L'\\';}
std::wstring Q(const std::wstring& p){return L"\""+p+L"\"";}
bool StartSession(){
    if(session||readiness.valid()||closing)return false;
    const auto base=std::filesystem::path(ModulePath(nullptr)).parent_path();
    const auto exe=(base/L"NativeIME.Session.exe").wstring();
    const auto shared=(base/L"shared").wstring(),dll=(base/L"runtime"/L"rime.dll").wstring();
    if(!std::filesystem::is_regular_file(exe)||!std::filesystem::is_directory(shared)||!std::filesystem::is_regular_file(dll)){
        MessageBoxW(root,L"测试运行库尚未准备。需要同目录的 shared 词库、runtime\\rime.dll 和内部引擎组件。当前仍为开发构建。",L"NativeIME",MB_OK|MB_ICONINFORMATION);return false;
    }
    PWSTR local=nullptr;
    if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&local)))return false;
    std::wstring user;
    try{user=(std::filesystem::path(local)/L"NativeIME"/L"experimental-rime").wstring();}
    catch(...){CoTaskMemFree(local);throw;}
    CoTaskMemFree(local);
    if(!Safe(exe)||!Safe(shared)||!Safe(dll)||!Safe(user))return false;
    SECURITY_ATTRIBUTES sa{sizeof(sa),nullptr,TRUE};
    stopEvent=CreateEventW(&sa,TRUE,FALSE,nullptr);job=CreateJobObjectW(nullptr,nullptr);
    if(!stopEvent||!job){CloseHandles();return false;}
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
    info.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if(!SetInformationJobObject(job,JobObjectExtendedLimitInformation,&info,sizeof(info))){CloseHandles();return false;}
    STARTUPINFOEXW si{};si.StartupInfo.cb=sizeof(si);
    SIZE_T size=0;InitializeProcThreadAttributeList(nullptr,1,0,&size);
    if(!size){CloseHandles();return false;}
    std::vector<unsigned char> storage(size);
    si.lpAttributeList=reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(storage.data());
    if(!InitializeProcThreadAttributeList(si.lpAttributeList,1,0,&size)){CloseHandles();return false;}
    HANDLE allowed[]={stopEvent};
    BOOL prepared=UpdateProcThreadAttribute(si.lpAttributeList,0,PROC_THREAD_ATTRIBUTE_HANDLE_LIST,allowed,sizeof(allowed),nullptr,nullptr);
    auto cmd=Q(exe)+L" --run --shared "+Q(shared)+L" --user "+Q(user)+L" --rime-dll "+Q(dll)+L" --stop-handle "+std::to_wstring(reinterpret_cast<ULONG_PTR>(stopEvent));
    PROCESS_INFORMATION pi{};
    BOOL started=prepared&&CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,TRUE,
        CREATE_SUSPENDED|CREATE_NO_WINDOW|EXTENDED_STARTUPINFO_PRESENT,nullptr,nullptr,&si.StartupInfo,&pi);
    DeleteProcThreadAttributeList(si.lpAttributeList);
    if(!started){CloseHandles();return false;}
    session=pi.hProcess;
    bool ok=AssignProcessToJobObject(job,session)&&ResumeThread(pi.hThread)!=static_cast<DWORD>(-1);
    CloseHandle(pi.hThread);
    if(!ok){TerminateProcess(session,98);CloseHandles();return false;}
    SetDiagnosticsEngineJob(job);
    everStarted=true;lastExit=0;return true;
}
LRESULT CALLBACK HomeProc(HWND h,UINT msg,WPARAM w,LPARAM l){
    if(msg==WM_COMMAND){SendMessageW(GetParent(h),msg,w,l);return 0;}
    return DefWindowProcW(h,msg,w,l);
}
LRESULT CALLBACK Proc(HWND h,UINT msg,WPARAM w,LPARAM l){
    try{
        switch(msg){
        case WM_CREATE:{
            root=h;auto instance=GetModuleHandleW(nullptr);
            homeNav=Control(h,L"BUTTON",L"常规",BS_PUSHBUTTON|WS_TABSTOP,Home,24,14,110,34);
            diagNav=Control(h,L"BUTTON",L"诊断与测试",BS_PUSHBUTTON|WS_TABSTOP,Diagnostics,144,14,150,34);
            WNDCLASSW wc{};wc.lpfnWndProc=HomeProc;wc.hInstance=instance;wc.lpszClassName=L"NativeIME.HomePage";wc.hbrBackground=GetSysColorBrush(COLOR_WINDOW);
            if(!RegisterClassW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return -1;
            home=CreateWindowExW(WS_EX_CONTROLPARENT,wc.lpszClassName,L"",WS_CHILD|WS_CLIPCHILDREN,0,58,680,480,h,nullptr,instance,nullptr);
            diag=CreateDiagnosticsPage(h,instance);
            if(!home||!diag||!homeNav||!diagNav)return -1;
            SetWindowPos(diag,nullptr,0,58,680,480,SWP_NOZORDER|SWP_NOACTIVATE);
            Control(home,L"STATIC",L"NativeIME",0,0,24,20,630,30);
            Control(home,L"STATIC",L"原生 Windows 拼音输入法 · 实验开发版本",0,0,24,56,630,26);
            engineState=Control(home,L"STATIC",L"",0,0,24,105,630,60);
            startButton=Control(home,L"BUTTON",L"启动引擎",BS_PUSHBUTTON|WS_TABSTOP,StartEngine,24,180,140,34);
            stopButton=Control(home,L"BUTTON",L"停止引擎",BS_PUSHBUTTON|WS_TABSTOP,StopEngine,174,180,140,34);
            Control(home,L"STATIC",L"实验中英文切换：Ctrl + Shift + F12（仅普通文本框且无组合输入）。\r\n诊断与测试内置，采集默认关闭。",0,0,24,246,630,50);
            setupState=Control(home,L"STATIC",L"",0,0,24,302,630,45);
            Control(home,L"BUTTON",L"检测输入环境",BS_PUSHBUTTON|WS_TABSTOP,CheckSetup,24,348,140,28);
            Control(home,L"BUTTON",L"Windows 语言设置",BS_PUSHBUTTON|WS_TABSTOP,LanguageSettings,174,348,170,28);
            Control(home,L"STATIC",L"原生输入测试区（不会写入诊断报告）",0,0,24,388,630,24);
            testEdit=Control(home,L"EDIT",L"",WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,0,24,416,620,30);
            if(!engineState||!startButton||!stopButton||!testEdit||!setupState)return -1;
            RefreshSetup();
            SetTimer(h,1,250,nullptr);ShowPage(false);Refresh();return 0;
        }
        case WM_COMMAND:
            switch(LOWORD(w)){
            case Home:ShowPage(false);RefreshSetup();break;
            case CheckSetup:RefreshSetup();break;
            case LanguageSettings:if(reinterpret_cast<INT_PTR>(ShellExecuteW(h,L"open",L"ms-settings:regionlanguage",nullptr,nullptr,SW_SHOWNORMAL))<=32)MessageBoxW(h,L"请手动打开 Windows 设置 > 时间和语言。",L"NativeIME",MB_OK);break;
            case Diagnostics:ShowPage(true);break;
            case StartEngine:if(!StartSession())MessageBoxW(h,L"引擎未启动。请检查测试运行库与已有会话。",L"NativeIME",MB_OK);break;
            case StopEngine:if(session&&!stopping){SetEvent(stopEvent);stopping=true;}break;
            }
            Refresh();return 0;
        case WM_TIMER:Refresh();return 0;
        case WM_CLOSE:
            if(!CloseDiagnosticsPage()){MessageBoxW(h,L"诊断测试仍在运行或采集尚未停止，请完成后退出。",L"NativeIME",MB_OK);return 0;}
            closing=true;
            if(session&&!stopping){SetEvent(stopEvent);stopping=true;}
            Refresh();return 0;
        case WM_DESTROY:KillTimer(h,1);CloseHandles();PostQuitMessage(0);return 0;
        }
    }catch(...){if(!session)CloseHandles();MessageBoxW(h,L"操作失败。",L"NativeIME",MB_OK|MB_ICONERROR);return 0;}
    return DefWindowProcW(h,msg,w,l);
}
bool PageVisible(HWND h){return (GetWindowLongPtrW(h,GWL_STYLE)&WS_VISIBLE)!=0;}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR cmd,int show){
    const bool smoke=cmd&&wcscmp(cmd,L"--ui-smoke")==0;
    const bool report=cmd&&wcscmp(cmd,L"--report-smoke")==0;
    if(report)return WriteDiagnosticsSmokeReport()?0:5;
    WNDCLASSW wc{};wc.lpfnWndProc=Proc;wc.hInstance=instance;wc.lpszClassName=L"NativeIME.Main";
    wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=GetSysColorBrush(COLOR_WINDOW);
    if(!RegisterClassW(&wc))return 1;
    HWND h=CreateWindowW(wc.lpszClassName,L"NativeIME",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_CLIPCHILDREN,
        CW_USEDEFAULT,CW_USEDEFAULT,710,590,nullptr,nullptr,instance,nullptr);
    if(!h)return 2;
    if(smoke){
        // No engine launch, registration or visible desktop needed. Verify actual
        // child ownership and page navigation, not merely executable presence.
        DWORD pid=0;GetWindowThreadProcessId(diag,&pid);
        bool ok=GetParent(diag)==h&&pid==GetCurrentProcessId()&&PageVisible(home)&&!PageVisible(diag)&&GetParent(testEdit)==home;
        SendMessageW(h,WM_COMMAND,Diagnostics,0);ok=ok&&PageVisible(diag)&&!PageVisible(home);
        SendMessageW(h,WM_COMMAND,Home,0);ok=ok&&PageVisible(home)&&!PageVisible(diag)&&CloseDiagnosticsPage();
        DestroyWindow(h);return ok?0:4;
    }
    ShowWindow(h,show);UpdateWindow(h);
    MSG msg{};BOOL r;while((r=GetMessageW(&msg,nullptr,0,0))>0){
        if(!IsDialogMessageW(h,&msg)){TranslateMessage(&msg);DispatchMessageW(&msg);}
    }
    return r<0?3:0;
}
