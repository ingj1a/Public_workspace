#pragma once
#include <windows.h>
#include "core/engine_protocol.hpp"
namespace nativeime::win {
class EngineClient {
public:
    bool Query(const std::wstring& pinyin,core::ipc::Response& response,DWORD timeoutMs=80) const noexcept;
};
}
