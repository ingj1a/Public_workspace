#include "core/rime_provider.hpp"
#include "core/input_session.hpp"
#include <filesystem>
#include <iostream>
int main(int argc,char** argv){
#ifdef _WIN32
    if(argc!=4) return 1;
    const auto module=std::filesystem::u8path(argv[3]).wstring();
#else
    if(argc!=3) return 1;
    const std::wstring module;
#endif
    try{
        std::filesystem::create_directories(argv[2]);
        nativeime::core::RimeProvider rime(argv[1],argv[2],"luna_pinyin",module);
        auto q=rime.Query(L"nihao");
        bool found=false;
        for(auto& c:q.candidates) if(c.text==L"你好") found=true;
        if(!found){std::cerr<<"Expected real candidate not returned\n";return 2;}
        nativeime::core::InputSession session([&](const std::wstring& input){return rime.Query(input);});
        for(auto ch:std::wstring(L"nihao")) session.Handle({nativeime::core::Key::Letter,ch});
        if(session.Candidates().empty()||session.Handle({nativeime::core::Key::Space}).commit.empty()) return 3;
        std::cout<<"Real Rime candidate + session smoke passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 4;}
}
