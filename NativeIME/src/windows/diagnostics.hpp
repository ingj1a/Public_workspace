#pragma once
// Fixed, numeric-only diagnostic schema. No text, key codes, document paths,
// process names, window titles, clipboard data or free-form event fields.
#include <windows.h>
#include <array>
#include <string>
#include "pipe_security.hpp"
namespace nativeime::win::diag {
enum class Event : unsigned { Activation, Composition, Commit, Cancel, EditFailure,
    CandidateShown, CandidateUnavailable, StaleReply, IpcOk, IpcFailure,
    IpcMilliseconds, SupervisorRestart, CandidateLatencyCount, CandidateLatencyTotal,
    LatencyLe10, LatencyLe20, LatencyLe30, LatencyLe50, LatencyLe80, LatencyLe150, LatencyGt150, ModeToEnglish, ModeToChinese, Count };
constexpr unsigned Count=static_cast<unsigned>(Event::Count);
inline constexpr const char* Names[Count]={"activation","composition","commit","cancel",
    "edit_failure","candidate_shown","candidate_unavailable","stale_reply","ipc_ok",
    "ipc_failure","ipc_total_ms","supervisor_restart","candidate_latency_count","candidate_latency_total_ms",
    "candidate_le_10_ms","candidate_11_20_ms","candidate_21_30_ms","candidate_31_50_ms","candidate_51_80_ms","candidate_81_150_ms","candidate_gt_150_ms","mode_to_english","mode_to_chinese"};
constexpr DWORD Magic=0x32474944;
constexpr ULONGLONG MaxDuration=30ull*60*1000;
struct Data {
    DWORD magic,enabled;
    ULONGLONG expires;
    unsigned long long values[Count];
};
using Snapshot=std::array<unsigned long long,Count>;
class Channel {
public:
    Channel()=default;
    ~Channel(){Close();}
    Channel(const Channel&)=delete;Channel& operator=(const Channel&)=delete;
    bool Open(bool create) noexcept {
        if(data_)return true;
        try{
            if(name_.empty()){
                auto sid=LogonSid();if(sid.empty())return false;
                name_=L"Local\\NativeIME.Diagnostics.v2."+sid;
                sddl_=L"D:P(A;;GA;;;"+sid+L")";
            }
            auto lockName=name_+L".lock";
            if(create){
                PSECURITY_DESCRIPTOR sd=nullptr;
                if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl_.c_str(),SDDL_REVISION_1,&sd,nullptr))return false;
                SECURITY_ATTRIBUTES sa{sizeof(sa),sd,FALSE};
                mutex_=CreateMutexW(&sa,FALSE,lockName.c_str());
                mapping_=CreateFileMappingW(INVALID_HANDLE_VALUE,&sa,PAGE_READWRITE,0,sizeof(Data),name_.c_str());
                LocalFree(sd);
            }else{
                mutex_=OpenMutexW(SYNCHRONIZE|MUTEX_MODIFY_STATE,FALSE,lockName.c_str());
                mapping_=OpenFileMappingW(FILE_MAP_WRITE,FALSE,name_.c_str());
            }
            if(!mutex_||!mapping_){Close();return false;}
            data_=static_cast<Data*>(MapViewOfFile(mapping_,FILE_MAP_WRITE,0,0,sizeof(Data)));
            if(!data_){Close();return false;}
            return true;
        }catch(...){Close();return false;}
    }
    bool Lock(DWORD timeout=0) noexcept {
        if(!mutex_)return false;
        auto status=WaitForSingleObject(mutex_,timeout);
        return status==WAIT_OBJECT_0||status==WAIT_ABANDONED;
    }
    void Unlock() noexcept {ReleaseMutex(mutex_);}
    Data* Get() noexcept {return data_;}
    void Close() noexcept {
        if(data_)UnmapViewOfFile(data_);
        if(mapping_)CloseHandle(mapping_);
        if(mutex_)CloseHandle(mutex_);
        data_=nullptr;mapping_=nullptr;mutex_=nullptr;
    }
private:
    std::wstring name_,sddl_;
    HANDLE mutex_=nullptr,mapping_=nullptr;
    Data* data_=nullptr;
};
class Writer {
public:
    void Record(Event e,unsigned long long amount=1) noexcept {
        if(static_cast<unsigned>(e)>=Count||amount>10000)return;
        Update([&](Data& d){Add(d,e,amount);});
    }
    void RecordIpc(bool ok,ULONGLONG ms) noexcept {
        Update([&](Data& d){Add(d,ok?Event::IpcOk:Event::IpcFailure,1);Add(d,Event::IpcMilliseconds,ms>10000?10000:ms);});
    }
    void RecordCandidateLatency(ULONGLONG ms) noexcept {
        Update([&](Data& d){
            Add(d,Event::CandidateShown,1);
            Add(d,Event::CandidateLatencyCount,1);
            Add(d,Event::CandidateLatencyTotal,ms>10000?10000:ms);
            Add(d,ms<=10?Event::LatencyLe10:ms<=20?Event::LatencyLe20:ms<=30?Event::LatencyLe30:
                ms<=50?Event::LatencyLe50:ms<=80?Event::LatencyLe80:ms<=150?Event::LatencyLe150:Event::LatencyGt150,1);
        });
    }
private:
    static void Add(Data& d,Event e,unsigned long long n) noexcept {
        auto& v=d.values[static_cast<unsigned>(e)];if(v<=~0ull-n)v+=n;
    }
    template<class F> void Update(F&& fn) noexcept {
        const auto now=GetTickCount64();
        if(!channel_.Get()){
            if(now<retry_)return;
            retry_=now+1000;
            if(!channel_.Open(false))return;
        }
        if(!channel_.Lock())return;
        auto* d=channel_.Get();
        const bool active=d->magic==Magic&&d->enabled&&now<d->expires;
        if(active)fn(*d);
        channel_.Unlock();
        if(!active){channel_.Close();retry_=now+1000;}
    }
    Channel channel_;
    ULONGLONG retry_=0;
};
class Capture {
public:
    ~Capture(){Stop();}
    bool Start() noexcept {
        if(owned_)return false;
        if(!channel_.Open(true)||!channel_.Lock(100))return false;
        auto* d=channel_.Get();
        const auto now=GetTickCount64();
        if(d->magic==Magic&&d->enabled&&now<d->expires){channel_.Unlock();return false;}
        *d={};d->magic=Magic;d->enabled=1;d->expires=now+MaxDuration;
        owned_=true;started_=now;end_=0;last_={};
        channel_.Unlock();return true;
    }
    bool Active() const noexcept {return owned_&&GetTickCount64()-started_<MaxDuration;}
    bool Stop() noexcept {
        if(!owned_)return true;
        if(!channel_.Lock(100))return false;
        auto* d=channel_.Get();
        // Only the session we started can be stopped or read.
        if(d->magic==Magic&&d->expires==started_+MaxDuration){
            d->enabled=0;
            for(unsigned i=0;i<Count;++i)last_[i]=d->values[i];
        }
        end_=GetTickCount64();owned_=false;channel_.Unlock();channel_.Close();return true;
    }
    Snapshot Read() noexcept {
        if(!owned_||!channel_.Lock(0))return last_;
        auto* d=channel_.Get();
        if(d->magic==Magic&&d->expires==started_+MaxDuration)
            for(unsigned i=0;i<Count;++i)last_[i]=d->values[i];
        channel_.Unlock();return last_;
    }
    ULONGLONG Duration() const noexcept {
        if(!started_)return 0;
        auto elapsed=(end_?end_:GetTickCount64())-started_;
        return elapsed>MaxDuration?MaxDuration:elapsed;
    }
    bool Clear() noexcept {if(owned_&&!Stop())return false;last_={};started_=end_=0;return true;}
private:
    Channel channel_;Snapshot last_{};
    bool owned_=false;ULONGLONG started_=0,end_=0;
};
}
