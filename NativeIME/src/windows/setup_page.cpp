#include "setup_page.hpp"
#include "input_preferences.hpp"
#include "lab_environment.hpp"
#include <shellapi.h>
#include <filesystem>
#include <string>
namespace nativeime::win {
namespace {
enum {Prepare=301,Install,Remove,Save,Shortcut,Edges};
HWND page=nullptr,state=nullptr,prepare=nullptr,install=nullptr,removeButton=nullptr,shortcut=nullptr,edges=nullptr;
HANDLE process=nullptr;
bool engineBusy=false;
DWORD lastCode=0;bool ran=false;
HWND Control(const wchar_t* cls,const wchar_t* text,DWORD style,int id,int x,int y,int w,int h){
    auto c=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,page,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
    if(c)SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);
    return c;
}
std::filesystem::path Base(){return std::filesystem::path(ModulePath(nullptr)).parent_path();}
bool Present(const wchar_t* file){std::error_code ec;return std::filesystem::is_regular_file(Base()/file,ec);}
void Refresh(){
    if(process&&WaitForSingleObject(process,0)==WAIT_OBJECT_0){
        lastCode=1;GetExitCodeProcess(process,&lastCode);CloseHandle(process);process=nullptr;ran=true;
    }
    const bool available=!process&&!engineBusy;
    const bool manifest=Present(L"alpha-manifest.json")&&Present(L"Alpha-Components.ps1");
    EnableWindow(prepare,available&&manifest&&Present(L"Prepare-Alpha.ps1")&&!Present(L"prepared.json"));
    EnableWindow(install,available&&manifest&&Present(L"Install-Alpha.ps1")&&Present(L"prepared.json"));
    EnableWindow(removeButton,available&&Present(L"Install-Alpha.ps1"));
    std::wstring status;
    if(process)status=L"部署操作正在运行，请完成后再退出。";
    else if(engineBusy)status=L"请先停止主程序引擎会话。";
    else if(ran)status=L"部署进程退出码："+std::to_wstring(lastCode)+L"。请返回常规页检测环境。";
    else if(!manifest)status=L"请从原始解压的 Alpha 测试包执行安装或注销。";
    else status=L"已识别 Alpha 测试包。仅在已建立快照的隔离 Windows VM 中使用。";
    SetWindowTextW(state,status.c_str());
}
bool Launch(int action){
    if(process||engineBusy)return false;
    const bool prep=action==Prepare;
    const auto base=Base();
    const auto script=(base/(prep?L"Prepare-Alpha.ps1":L"Install-Alpha.ps1")).wstring();
    if(!Present(prep?L"Prepare-Alpha.ps1":L"Install-Alpha.ps1")||script.find(L'"')!=std::wstring::npos)return false;
    if(MessageBoxW(page,prep?L"确认正在隔离 Windows VM 中，且已核验项目测试包来源？将联网获取固定依赖。":
        L"确认已核验测试包来源及脚本内容，并正在隔离 Windows VM 中？此操作需要管理员确认。注销前请切换至其他输入法并关闭已安装的 NativeIME。",
        L"Alpha 测试部署",MB_YESNO|MB_ICONWARNING)!=IDYES)return true;
    wchar_t system[MAX_PATH]{};if(!GetSystemDirectoryW(system,MAX_PATH))return false;
    auto powershell=(std::filesystem::path(system)/L"WindowsPowerShell"/L"v1.0"/L"powershell.exe").wstring();
    std::wstring args=L"-NoProfile -File \""+script+L"\" -IsolatedVm";
    if(!prep)args+=action==Install?L" -Action Install":L" -Action Remove";
    const auto dir=base.wstring();
    SHELLEXECUTEINFOW e{sizeof(e)};e.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_NOASYNC;e.hwnd=page;
    e.lpVerb=prep?L"open":L"runas";e.lpFile=powershell.c_str();e.lpParameters=args.c_str();e.lpDirectory=dir.c_str();e.nShow=SW_SHOWNORMAL;
    if(!ShellExecuteExW(&e)||!e.hProcess)return false;
    process=e.hProcess;ran=false;return true;
}
LRESULT CALLBACK Proc(HWND h,UINT msg,WPARAM w,LPARAM l){
    try{
        switch(msg){
        case WM_CREATE:{
            page=h;
            Control(L"STATIC",L"输入设置",0,0,24,18,620,26);
            shortcut=Control(L"BUTTON",L"启用 Ctrl + Shift + F12 中英文切换",BS_AUTOCHECKBOX|WS_TABSTOP,Shortcut,24,55,600,30);
            edges=Control(L"BUTTON",L"候选窗口显示时，Home / End 选择首末候选",BS_AUTOCHECKBOX|WS_TABSTOP,Edges,24,91,600,30);
            const auto p=LoadInputPreferences();
            SendMessageW(shortcut,BM_SETCHECK,p.modeShortcut?BST_CHECKED:BST_UNCHECKED,0);
            SendMessageW(edges,BM_SETCHECK,p.candidateEdges?BST_CHECKED:BST_UNCHECKED,0);
            Control(L"BUTTON",L"保存输入设置",BS_PUSHBUTTON|WS_TABSTOP,Save,24,133,140,32);
            Control(L"STATIC",L"设置只在下一次输入服务激活时生效；请切换输入法或重启测试应用。",0,0,24,176,620,42);
            Control(L"STATIC",L"Alpha 测试部署",0,0,24,232,620,26);
            prepare=Control(L"BUTTON",L"准备运行库",BS_PUSHBUTTON|WS_TABSTOP,Prepare,24,270,140,34);
            install=Control(L"BUTTON",L"安装 / 注册",BS_PUSHBUTTON|WS_TABSTOP,Install,174,270,140,34);
            removeButton=Control(L"BUTTON",L"注销 / 回退",BS_PUSHBUTTON|WS_TABSTOP,Remove,324,270,140,34);
            state=Control(L"STATIC",L"",0,0,24,320,620,55);
            Control(L"STATIC",L"使用已核验的原始测试包。准备及安装通过现有工程脚本执行，不绕过执行策略，不自动修改默认输入法。",0,0,24,390,620,62);
            if(!shortcut||!edges||!prepare||!install||!removeButton||!state)return -1;
            SetTimer(h,1,500,nullptr);Refresh();return 0;
        }
        case WM_TIMER:Refresh();return 0;
        case WM_COMMAND:
            switch(LOWORD(w)){
            case Save:{
                core::InputPreferences p{SendMessageW(shortcut,BM_GETCHECK,0,0)==BST_CHECKED,SendMessageW(edges,BM_GETCHECK,0,0)==BST_CHECKED};
                MessageBoxW(h,SaveInputPreferences(p)?L"已保存。下一次输入服务激活时生效。":L"保存失败。",L"NativeIME",MB_OK);break;
            }
            case Prepare:case Install:case Remove:
                if(!Launch(LOWORD(w)))MessageBoxW(h,L"未启动部署操作。请检查测试包、权限和脚本执行策略。",L"NativeIME",MB_OK);break;
            }
            Refresh();return 0;
        case WM_DESTROY:KillTimer(h,1);if(process){CloseHandle(process);process=nullptr;}page=nullptr;return 0;
        }
    }catch(...){return msg==WM_CREATE?-1:0;}
    return DefWindowProcW(h,msg,w,l);
}
}
HWND CreateSetupPage(HWND parent,HINSTANCE instance){
    WNDCLASSW wc{};wc.lpfnWndProc=Proc;wc.hInstance=instance;wc.lpszClassName=L"NativeIME.SetupPage";
    wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=GetSysColorBrush(COLOR_WINDOW);
    if(!RegisterClassW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return nullptr;
    return CreateWindowExW(WS_EX_CONTROLPARENT,wc.lpszClassName,L"",WS_CHILD|WS_CLIPCHILDREN,0,58,680,480,parent,nullptr,instance,nullptr);
}
bool SetupPageBusy() noexcept{return process!=nullptr;}
void SetSetupEngineBusy(bool busy) noexcept{engineBusy=busy;}
}
