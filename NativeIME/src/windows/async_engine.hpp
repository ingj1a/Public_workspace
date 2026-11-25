#pragma once
#include "engine_client.hpp"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <optional>
namespace nativeime::win {
constexpr UINT EngineReady=WM_APP+71;
struct EngineReply { unsigned long long generation; std::wstring input; core::ipc::Response response; };
// One in-flight request and one latest pending request; no unbounded queue.
class AsyncEngine {
public:
    explicit AsyncEngine(HWND target):target_(target),worker_([this]{Loop();}){}
    ~AsyncEngine(){Stop();}
    void Submit(unsigned long long generation,const std::wstring& input){
        {std::lock_guard<std::mutex> lock(mutex_);pending_=EngineReply{generation,input,{}};}
        cv_.notify_one();
    }
    void Cancel(){std::lock_guard<std::mutex> lock(mutex_);pending_.reset();reply_.reset();}
    std::optional<EngineReply> Take(){std::lock_guard<std::mutex> lock(mutex_);auto r=std::move(reply_);reply_.reset();return r;}
    void Stop(){
        {std::lock_guard<std::mutex> lock(mutex_);stopping_=true;pending_.reset();}
        cv_.notify_one();if(worker_.joinable())worker_.join();
    }
private:
    void Loop() noexcept{
        for(;;){
            std::optional<EngineReply> work;
            {std::unique_lock<std::mutex> lock(mutex_);cv_.wait(lock,[this]{return stopping_||pending_.has_value();});
             if(stopping_)return;work=std::move(pending_);pending_.reset();}
            EngineClient client;
            if(!client.Query(work->input,work->response,80))work->response={};
            {std::lock_guard<std::mutex> lock(mutex_);if(stopping_)return;reply_=std::move(work);}
            PostMessageW(target_,EngineReady,0,0);
        }
    }
    HWND target_;std::mutex mutex_;std::condition_variable cv_;
    bool stopping_=false;std::optional<EngineReply> pending_,reply_;std::thread worker_;
};
}
