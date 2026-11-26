#pragma once
#include "engine_client.hpp"
#include "diagnostics.hpp"
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <optional>
namespace nativeime::win {
constexpr UINT EngineReady=WM_APP+71;
struct EngineReply { unsigned long long generation; std::wstring input; core::ipc::Response response; ULONGLONG submitted=0; };
// One in-flight request and one latest pending request. Readiness is established
// by a fixed non-user-data query on the worker, never by blocking a TSF callback.
class AsyncEngine {
public:
    explicit AsyncEngine(HWND target,std::wstring expectedImage):target_(target),client_(std::move(expectedImage)),worker_([this]{Loop();}){}
    ~AsyncEngine(){Stop();}
    bool Ready() const noexcept {return ready_.load(std::memory_order_acquire);}
    void RequestProbe(){
        const auto now=GetTickCount64();
        {std::lock_guard<std::mutex> lock(mutex_);
         if(stopping_||Ready()||now<nextProbe_)return;
         nextProbe_=now+250;probeRequested_=true;}
        cv_.notify_one();
    }
    void Submit(unsigned long long generation,const std::wstring& input){
        {std::lock_guard<std::mutex> lock(mutex_);if(stopping_)return;pending_=EngineReply{generation,input,{},GetTickCount64()};}
        cv_.notify_one();
    }
    void Cancel(){std::lock_guard<std::mutex> lock(mutex_);pending_.reset();reply_.reset();}
    std::optional<EngineReply> Take(){std::lock_guard<std::mutex> lock(mutex_);auto r=std::move(reply_);reply_.reset();return r;}
    void Stop(){
        {std::lock_guard<std::mutex> lock(mutex_);stopping_=true;pending_.reset();probeRequested_=false;}
        cv_.notify_one();if(worker_.joinable())worker_.join();
        ready_.store(false,std::memory_order_release);
    }
private:
    void Loop() noexcept{
        diag::Writer diagnostics;
        for(;;){
            std::optional<EngineReply> work;
            {std::unique_lock<std::mutex> lock(mutex_);
             cv_.wait(lock,[this]{return stopping_||pending_.has_value()||probeRequested_;});
             if(stopping_)return;
             work=std::move(pending_);pending_.reset();probeRequested_=false;
             nextProbe_=GetTickCount64()+250;}
            if(!work){
                core::ipc::Response probe;
                ready_.store(client_.Query(L"nihao",probe,80),std::memory_order_release);
                continue;
            }
            const auto start=GetTickCount64();
            const bool ok=client_.Query(work->input,work->response,80);
            diagnostics.RecordIpc(ok,GetTickCount64()-start);
            ready_.store(ok,std::memory_order_release);
            if(!ok)work->response={};
            {std::lock_guard<std::mutex> lock(mutex_);if(stopping_)return;reply_=std::move(work);}
            if(target_)PostMessageW(target_,EngineReady,0,0);
        }
    }
    HWND target_;EngineClient client_;std::mutex mutex_;std::condition_variable cv_;
    bool stopping_=false,probeRequested_=true;
    ULONGLONG nextProbe_=0;
    std::atomic<bool> ready_{false};
    std::optional<EngineReply> pending_,reply_;std::thread worker_;
};
}
