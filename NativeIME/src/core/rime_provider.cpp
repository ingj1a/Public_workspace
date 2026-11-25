#include "core/rime_provider.hpp"
#include <cstring>
#include <rime_api.h>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <atomic>
#include <codecvt>
#include <locale>
#include <stdexcept>
namespace nativeime::core {
namespace {
std::atomic<bool> active{false};
std::wstring Wide(const char* p) {
    if(!p) return {};
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> cv;
    return cv.from_bytes(p);
}
}
struct RimeProvider::Impl {
    RimeApi* api=nullptr;
    RimeSessionId session=0;
    bool initialized=false,claimed=false;
    std::string shared,user,schema;
#ifdef _WIN32
    HMODULE module=nullptr;
#endif
    ~Impl(){
        if(session && api) api->destroy_session(session);
        if(initialized && api) api->finalize();
#ifdef _WIN32
        if(module) FreeLibrary(module);
#endif
        if(claimed) active.store(false);
    }
};
RimeProvider::RimeProvider(std::string shared,std::string user,std::string schema,std::wstring modulePath):impl_(new Impl) {
    auto& p=*impl_;
    if(active.exchange(true)) throw std::runtime_error("Rime already active");
    p.claimed=true;
    p.shared=std::move(shared); p.user=std::move(user); p.schema=std::move(schema);
#ifdef _WIN32
    if(modulePath.empty()) throw std::runtime_error("An absolute rime.dll path is required on Windows");
    p.module=LoadLibraryExW(modulePath.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if(!p.module) throw std::runtime_error("LoadLibraryExW(rime.dll) failed: "+std::to_string(GetLastError()));
    using GetRimeApi = RimeApi* (__cdecl*)();
    auto getApi=reinterpret_cast<GetRimeApi>(GetProcAddress(p.module,"rime_get_api"));
    if(!getApi) throw std::runtime_error("rime_get_api export not found");
    p.api=getApi();
#else
    (void)modulePath;
    p.api=rime_get_api();
#endif
    if(!p.api) throw std::runtime_error("Rime API unavailable");
    RIME_STRUCT(RimeTraits,traits);
    traits.shared_data_dir=p.shared.c_str(); traits.user_data_dir=p.user.c_str();
    traits.distribution_name="NativeIME"; traits.distribution_code_name="nativeime";
    traits.distribution_version="0.1"; traits.app_name="rime.nativeime";
    traits.min_log_level=2; traits.log_dir="";
    p.api->setup(&traits); p.api->initialize(&traits); p.initialized=true;
    // Deployment is explicit construction work, not per-keystroke work.
    if(p.api->start_maintenance(False)) p.api->join_maintenance_thread();
    p.session=p.api->create_session();
    if(!p.session||!p.api->select_schema(p.session,p.schema.c_str()))
        throw std::runtime_error("Rime schema unavailable");
    p.api->set_option(p.session,"ascii_mode",False);
    p.api->set_option(p.session,"simplification",True);
}
RimeProvider::~RimeProvider()=default;
QueryResult RimeProvider::Query(const std::wstring& input){
    QueryResult out; out.normalized_input=PinyinEngine::Normalize(input);
    if(out.normalized_input.empty()||out.normalized_input.size()>64) return out;
    auto& p=*impl_; auto* api=p.api;
    api->clear_composition(p.session);
    for(wchar_t ch:out.normalized_input) {
        if(!api->process_key(p.session,static_cast<int>(ch),0)) {
            api->clear_composition(p.session); return out;
        }
        RIME_STRUCT(RimeCommit,commit);
        if(api->get_commit(p.session,&commit)) {
            api->free_commit(&commit); api->clear_composition(p.session); return out;
        }
    }
    RIME_STRUCT(RimeContext,ctx);
    if(api->get_context(p.session,&ctx)){
        try {
            if(ctx.menu.candidates){
                for(int i=0;i<std::min(ctx.menu.num_candidates,9);++i){
                    const auto& c=ctx.menu.candidates[i];
                    if(c.text && *c.text) out.candidates.push_back({Wide(c.text),100-i,Wide(c.comment)});
                }
            }
        }catch(...){api->free_context(&ctx);api->clear_composition(p.session);throw;}
        api->free_context(&ctx);
    }
    api->clear_composition(p.session);
    return out;
}
}
