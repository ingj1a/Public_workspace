#include <windows.h>
#include <commdlg.h>
#include <filesystem>
#include <fstream>
#include <future>
#include <algorithm>
#include <sstream>
#include <vector>
#include <iomanip>
#include <locale>
#include <optional>
#include <psapi.h>
#include "core/zip_store.hpp"
#include "core/quality_probe.hpp"
#include "diagnostics.hpp"
#include "diagnostics_page.hpp"
#include "engine_client.hpp"
#ifndef NATIVEIME_BUILD_ID
#define NATIVEIME_BUILD_ID "local"
#endif
namespace {
using namespace nativeime::win;
namespace d=diag;
enum {Start=101,Stop,Export,Clear,Check,Desktop};
d::Capture capture;
HWND root=nullptr,status=nullptr,details=nullptr,checkStatus=nullptr;
HWND startButton=nullptr,stopButton=nullptr,exportButton=nullptr,clearButton=nullptr,checkButton=nullptr,desktopButton=nullptr;
struct ProbeResult { int code=1; ULONGLONG ms=0; nativeime::core::QualityResult quality; };
std::future<ProbeResult> probe;
nativeime::core::QualityResult qualityResult;
HANDLE verifier=nullptr;
int engineResult=-1,desktopResult=-1,preflightResult=-1;
ULONGLONG engineMs=0;
struct Perf {
    ULONGLONG elapsed=0;
    std::optional<double> cpu,engineCpu;
    SIZE_T workingSet=0;
    std::optional<SIZE_T> enginePeakCommit;
};
HANDLE engineJob=nullptr;
std::optional<ULONGLONG> previousJobCpu;
ULONGLONG previousJobTick=0;
std::vector<Perf> samples;
ULONGLONG captureStarted=0,lastSample=0,lastCpu=0;
bool ReadCpu(ULONGLONG& value){
 FILETIME c{},e{},k{},u{}; if(!GetProcessTimes(GetCurrentProcess(),&c,&e,&k,&u))return false;
 ULARGE_INTEGER a{},b{};a.LowPart=k.dwLowDateTime;a.HighPart=k.dwHighDateTime;b.LowPart=u.dwLowDateTime;b.HighPart=u.dwHighDateTime;value=a.QuadPart+b.QuadPart;return true;
}
void SamplePerformance(){
 if(!capture.Active())return;const auto now=GetTickCount64();if(lastSample&&now-lastSample<1000)return;
 PROCESS_MEMORY_COUNTERS_EX mem{};mem.cb=sizeof(mem);ULONGLONG cpu=0;
 if(!GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&mem),sizeof(mem))||!ReadCpu(cpu))return;
 Perf sample;sample.elapsed=now-captureStarted;sample.workingSet=mem.WorkingSetSize;
 if(lastSample&&now>lastSample&&cpu>=lastCpu)sample.cpu=100.0*double(cpu-lastCpu)/10000.0/double(now-lastSample);
 if(engineJob){
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION a{};
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    if(QueryInformationJobObject(engineJob,JobObjectBasicAccountingInformation,&a,sizeof(a),nullptr)){
        const auto total=static_cast<ULONGLONG>(a.TotalUserTime.QuadPart+a.TotalKernelTime.QuadPart);
        if(previousJobCpu&&now>previousJobTick&&total>=*previousJobCpu)
            sample.engineCpu=100.0*double(total-*previousJobCpu)/10000.0/double(now-previousJobTick);
        previousJobCpu=total;previousJobTick=now;
    }else{previousJobCpu.reset();previousJobTick=0;}
    if(QueryInformationJobObject(engineJob,JobObjectExtendedLimitInformation,&limits,sizeof(limits),nullptr))
        sample.enginePeakCommit=limits.PeakJobMemoryUsed;
 }else{previousJobCpu.reset();previousJobTick=0;}
 samples.push_back(sample);if(samples.size()>1800)samples.erase(samples.begin(),samples.begin()+300);
 lastSample=now;lastCpu=cpu;
}
std::wstring Base(){return std::filesystem::path(ModulePath(nullptr)).parent_path().wstring();}
const wchar_t* Result(int code){return code<0?L"未测试":code==0?L"通过":code==77?L"前置条件不足":L"失败";}
HWND Control(const wchar_t* klass,const wchar_t* label,DWORD style,int id,int x,int y,int w,int h){
    HWND out=CreateWindowExW(0,klass,label,WS_CHILD|WS_VISIBLE|style,x,y,w,h,root,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
    if(out)SendMessageW(out,WM_SETFONT,reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)),TRUE);
    return out;
}
void Refresh(){
    bool active=capture.Active();
    SetWindowTextW(status,active?L"正在采集（最多30分钟）":L"未采集 · 数据仅保存在本机内存");
    auto values=capture.Read();
    std::wostringstream text;
    text<<L"TSF 激活 "<<values[0]<<L"     组合输入 "<<values[1]<<L"     提交 "<<values[2]<<L"     取消 "<<values[3]
        <<L"\r\n编辑失败 "<<values[4]<<L"     候选显示 "<<values[5]<<L"     候选不可用 "<<values[6]
        <<L"\r\n过期响应 "<<values[7]<<L"     IPC 成功 "<<values[8]<<L"     IPC 失败 "<<values[9]
        <<L"\r\nIPC 平均耗时 ";
    if(values[8]+values[9])text<<static_cast<double>(values[10])/(values[8]+values[9])<<L" ms";
    else text<<L"—";
    text<<L"     引擎重启 "<<values[11]<<L"\r\n候选延迟采样 "<<values[12]<<L"     中/英文切换 "<<values[22]<<L" / "<<values[21];
    SetWindowTextW(details,text.str().c_str());
    std::wstring checks=L"真实引擎："+std::wstring(Result(engineResult))+L"    桌面前置："+Result(preflightResult)+L"    TSF 实测："+Result(desktopResult);
    if(qualityResult.exactTotal)checks+=L"\r\n固定语料：精确 "+std::to_wstring(qualityResult.exactHits)+L"/"+std::to_wstring(qualityResult.exactTotal)+L"  纠错 "+std::to_wstring(qualityResult.correctionHits)+L"/"+std::to_wstring(qualityResult.correctionTotal)+L"  反例 "+std::to_wstring(qualityResult.negativePass)+L"/"+std::to_wstring(qualityResult.negativeTotal);
    if(probe.valid()||verifier)checks+=L"  （检测中）";
    SetWindowTextW(checkStatus,checks.c_str());
    EnableWindow(startButton,!active);EnableWindow(stopButton,active);
    EnableWindow(exportButton,!active&&!probe.valid()&&!verifier);
    EnableWindow(clearButton,!active&&!probe.valid()&&!verifier);
    EnableWindow(checkButton,!probe.valid()&&!verifier);
    EnableWindow(desktopButton,!probe.valid()&&!verifier);
}
bool LaunchVerifier(bool real){
    std::wstring exe=Base()+L"\\NativeIME.TsfDesktopE2E.exe";
    std::wstring cmd=L"\""+exe+L"\" "+(real?L"--isolated-desktop":L"--preflight-only");
    STARTUPINFOW si{sizeof(si)};PROCESS_INFORMATION pi{};
    if(!CreateProcessW(exe.c_str(),cmd.data(),nullptr,nullptr,FALSE,real?0:CREATE_NO_WINDOW,nullptr,nullptr,&si,&pi))return false;
    CloseHandle(pi.hThread);verifier=pi.hProcess;return true;
}
bool realVerifier=false;
void SelfCheck(){
    if(probe.valid()||verifier)return;
    engineResult=preflightResult=-1;engineMs=0;qualityResult={};
    auto exe=Base()+L"\\NativeIME.EngineService.exe";
    probe=std::async(std::launch::async,[exe]{
        EngineClient client(exe);nativeime::core::ipc::Response response;
        auto start=GetTickCount64();
        bool ok=client.Query(L"nihao",response,500)&&std::any_of(response.candidates.begin(),response.candidates.end(),
            [](const auto& c){return c.text==L"\u4f60\u597d";});
        // Only fixed test text is queried. Duration is encoded as a bounded result.
        ProbeResult result;result.code=ok?0:1;result.ms=GetTickCount64()-start;
        if(ok)result.quality=nativeime::core::RunQualityProbe([&](const std::wstring& p,std::vector<nativeime::core::Candidate>& candidates){
            nativeime::core::ipc::Response r;if(!client.Query(p,r,500))return false;
            candidates=std::move(r.candidates);return true;
        });
        return result;
    });
    realVerifier=false;
    if(!LaunchVerifier(false))preflightResult=1;
}
std::string PerformanceCsv(){
 std::ostringstream out;out.imbue(std::locale::classic());
 out<<"elapsed_ms,ui_cpu_one_core_pct,ui_working_set_bytes,engine_job_cpu_one_core_pct,engine_job_peak_commit_bytes\n";
 for(const auto& s:samples){
     out<<s.elapsed<<","<<std::fixed<<std::setprecision(2);
     if(s.cpu)out<<*s.cpu;
     out<<","<<s.workingSet<<",";
     if(s.engineCpu)out<<*s.engineCpu;
     out<<",";if(s.enginePeakCommit)out<<*s.enginePeakCommit;
     out<<"\n";
 }
 return out.str();
}
std::string Report(){
    auto v=capture.Read();std::ostringstream out;
    out<<"{\n  \"schema\":2,\n  \"build\":\"" NATIVEIME_BUILD_ID "\",\n  \"platform\":\"windows-x64\",\n"
        <<"  \"duration_ms\":"<<capture.Duration()<<",\n  \"capture\":\"aggregate_only\",\n  \"events\":{";
    for(unsigned i=0;i<d::Count;++i){if(i)out<<',';out<<"\n    \""<<d::Names[i]<<"\":"<<v[i];}
    out<<"\n  },\n  \"self_test\":{\"engine\":"<<engineResult<<",\"engine_probe_ms\":"<<engineMs
       <<",\"desktop_preflight\":"<<preflightResult<<",\"tsf_desktop\":"<<desktopResult
       <<"},\n  \"fixed_quality_probe\":{\"exact_hits\":"<<qualityResult.exactHits<<",\"exact_total\":"<<qualityResult.exactTotal
       <<",\"correction_hits\":"<<qualityResult.correctionHits<<",\"correction_total\":"<<qualityResult.correctionTotal
       <<",\"negative_pass\":"<<qualityResult.negativePass<<",\"negative_total\":"<<qualityResult.negativeTotal<<",\"query_failures\":"<<qualityResult.queryFailures<<",\"correction_miss_mask\":"<<qualityResult.correctionMissMask
       <<"},\n  \"notes\":\"-1 not run; 0 pass; 77 blocked; other codes fail. Candidate latency measures submission to popup Show success, not compositor paint. Engine job CPU includes owned session, supervisor and engine descendants; peak commit is not working set. Blank samples unavailable. Host TSF resource usage excluded. No user text collected.\"\n}\n";
    return out.str();
}
bool WriteReport(const std::filesystem::path& path){
    const auto zip=nativeime::core::StoreZip({{"summary.json",Report()},{"performance.csv",PerformanceCsv()}});
    std::ofstream file(path,std::ios::binary|std::ios::trunc);
    file.write(zip.data(),static_cast<std::streamsize>(zip.size()));file.close();
    return static_cast<bool>(file);
}
void Save(){
    if(capture.Active()||!capture.Stop())return;
    SYSTEMTIME t{};GetLocalTime(&t);
    wchar_t path[MAX_PATH]{};
    swprintf_s(path,L"NativeIME-Diagnostics-%04u%02u%02u-%02u%02u%02u.zip",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);
    OPENFILENAMEW ofn{sizeof(ofn)};ofn.hwndOwner=root;ofn.lpstrFilter=L"Diagnostic ZIP (*.zip)\0*.zip\0\0";
    ofn.lpstrFile=path;ofn.nMaxFile=MAX_PATH;ofn.lpstrDefExt=L"zip";ofn.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    if(!GetSaveFileNameW(&ofn))return;
    try{
        const bool file=WriteReport(path);
        MessageBoxW(root,file?L"已导出脱敏 ZIP 报告。包含摘要与性能采样，请检查后再分享。":L"报告保存失败。",L"NativeIME",MB_OK|(file?MB_ICONINFORMATION:MB_ICONERROR));
    }catch(...){MessageBoxW(root,L"报告生成失败。",L"NativeIME",MB_OK|MB_ICONERROR);}
}
LRESULT CALLBACK Proc(HWND h,UINT msg,WPARAM w,LPARAM l){
    try{
        switch(msg){
        case WM_CREATE:
            root=h;
            Control(L"STATIC",L"诊断与测试",0,0,24,18,630,26);
            status=Control(L"STATIC",L"",0,0,24,52,630,24);
            startButton=Control(L"BUTTON",L"开始采集",BS_PUSHBUTTON|WS_TABSTOP,Start,24,88,140,32);
            stopButton=Control(L"BUTTON",L"停止采集",BS_PUSHBUTTON|WS_TABSTOP,Stop,174,88,140,32);
            exportButton=Control(L"BUTTON",L"导出 ZIP 报告",BS_PUSHBUTTON|WS_TABSTOP,Export,324,88,150,32);
            clearButton=Control(L"BUTTON",L"清空",BS_PUSHBUTTON|WS_TABSTOP,Clear,484,88,110,32);
            details=Control(L"STATIC",L"",0,0,24,144,630,110);
            checkButton=Control(L"BUTTON",L"一键自检",BS_PUSHBUTTON|WS_TABSTOP,Check,24,270,140,32);
            desktopButton=Control(L"BUTTON",L"隔离桌面 TSF 实测",BS_PUSHBUTTON|WS_TABSTOP,Desktop,174,270,200,32);
            checkStatus=Control(L"STATIC",L"",0,0,24,320,630,44);
            Control(L"STATIC",L"记录聚合事件、进程 CPU 与内存采样，不记录输入文字、拼音、按键、密码或窗口标题。\r\n自检不修改系统配置；TSF 实测仅限已准备的隔离 Windows 测试环境。",0,0,24,380,630,62);
            SetTimer(h,1,250,nullptr);Refresh();return 0;
        case WM_COMMAND:
            switch(LOWORD(w)){
            case Start:if(!capture.Start())MessageBoxW(h,L"无法开始：可能已有采集会话。",L"NativeIME",MB_OK);else{samples.clear();previousJobCpu.reset();previousJobTick=0;captureStarted=lastSample=GetTickCount64();if(!ReadCpu(lastCpu))lastSample=0;}break;
            case Stop:if(!capture.Stop())MessageBoxW(h,L"暂时无法停止，请重试。",L"NativeIME",MB_OK);break;
            case Clear:if(capture.Clear()){engineResult=desktopResult=preflightResult=-1;engineMs=0;qualityResult={};samples.clear();previousJobCpu.reset();previousJobTick=0;captureStarted=lastSample=lastCpu=0;}break;
            case Export:Save();break;
            case Check:SelfCheck();break;
            case Desktop:
                if(!probe.valid()&&!verifier&&MessageBoxW(h,L"仅在隔离 Windows 测试环境中执行。需要已启用实验 TSF、简体中文输入语言及运行中的引擎。继续？",L"NativeIME",MB_YESNO|MB_ICONWARNING)==IDYES){
                    desktopResult=-1;realVerifier=true;if(!LaunchVerifier(true))desktopResult=1;
                }break;
            }
            Refresh();return 0;
        case WM_TIMER:
            if(capture.Active())SamplePerformance();else capture.Stop();
            if(probe.valid()&&probe.wait_for(std::chrono::seconds(0))==std::future_status::ready){
                auto r=probe.get();engineResult=r.code;engineMs=r.ms;qualityResult=r.quality;
            }
            if(verifier&&WaitForSingleObject(verifier,0)==WAIT_OBJECT_0){
                DWORD code=1;GetExitCodeProcess(verifier,&code);CloseHandle(verifier);verifier=nullptr;
                (realVerifier?desktopResult:preflightResult)=static_cast<int>(code);
            }
            Refresh();return 0;
        case WM_CLOSE:return 0; // lifetime belongs to the main application
        case WM_DESTROY:KillTimer(h,1);capture.Stop();root=nullptr;return 0;
        }
    }catch(...){MessageBoxW(h,L"诊断操作失败。",L"NativeIME",MB_OK|MB_ICONERROR);return 0;}
    return DefWindowProcW(h,msg,w,l);
}
}

namespace nativeime::win {
HWND CreateDiagnosticsPage(HWND parent,HINSTANCE instance){
    WNDCLASSW wc{};wc.lpfnWndProc=Proc;wc.hInstance=instance;wc.lpszClassName=L"NativeIME.DiagnosticsPage";
    wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hbrBackground=GetSysColorBrush(COLOR_WINDOW);
    if(!RegisterClassW(&wc)&&GetLastError()!=ERROR_CLASS_ALREADY_EXISTS)return nullptr;
    return CreateWindowExW(WS_EX_CONTROLPARENT,wc.lpszClassName,L"",WS_CHILD|WS_CLIPCHILDREN,
        0,0,680,480,parent,nullptr,instance,nullptr);
}
void SetDiagnosticsEngineJob(HANDLE job) noexcept {
    engineJob=job;previousJobCpu.reset();previousJobTick=0;
}
bool WriteDiagnosticsSmokeReport() noexcept {
    // Explicit CI mode uses the production serializer with empty capture.
    // Refuse to overwrite an existing file.
    try{
        const auto path=std::filesystem::current_path()/L"nativeime-report-smoke.zip";
        if(std::filesystem::exists(path))return false;
        return WriteReport(path);
    }catch(...){return false;}
}
bool CloseDiagnosticsPage() noexcept {
    if(verifier||probe.valid())return false;
    return capture.Stop();
}
}
