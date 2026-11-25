#pragma once
#include "core/pinyin_engine.hpp"
#include <memory>
#include <string>
namespace nativeime::core {
// Single owner per process, invoked on its creating thread, outside TSF callbacks.
// No system Rime configuration is modified; supply an isolated user directory.
class RimeProvider {
public:
    RimeProvider(std::string sharedDir,std::string userDir,std::string schema);
    ~RimeProvider();
    RimeProvider(const RimeProvider&)=delete;
    RimeProvider& operator=(const RimeProvider&)=delete;
    QueryResult Query(const std::wstring& input);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
