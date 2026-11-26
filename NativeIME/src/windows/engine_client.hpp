#pragma once
#include <windows.h>
#include <utility>
#include "core/engine_protocol.hpp"
namespace nativeime::win {
class EngineClient {
public:
    explicit EngineClient(std::wstring expectedImage):expectedImage_(std::move(expectedImage)){}
    bool Query(const std::wstring& pinyin,core::ipc::Response& response,DWORD timeoutMs=80) const noexcept;
private:
    std::wstring expectedImage_;
};
}
