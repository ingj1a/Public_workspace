#pragma once
#include "core/engine_protocol.hpp"
#include <string>
namespace nativeime::win {
// Explicitly bounded synchronous IPC client. Call only from a worker thread,
// never from a TSF/COM keystroke callback.
class EngineClient {
public:
    explicit EngineClient(std::wstring pipeName=L"\\\\.\\pipe\\NativeIME.Engine.v1"):pipeName_(std::move(pipeName)){}
    bool Query(const std::wstring& pinyin,nativeime::core::ipc::Response& response,DWORD timeoutMs=80) const noexcept;
private:
    std::wstring pipeName_;
};
}
