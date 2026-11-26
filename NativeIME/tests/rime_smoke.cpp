#include "core/rime_provider.hpp"
#include "core/input_session.hpp"
#include "core/quality_probe.hpp"
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
        for(const auto& expected:std::vector<std::pair<std::wstring,std::wstring>>{
            {L"jianqiemengban",L"剪切蒙版"},{L"faxiantietu",L"法线贴图"},{L"yibutongxin",L"异步通信"}}){
            auto terms=rime.Query(expected.first);bool present=false;
            for(const auto& c:terms.candidates)if(c.text==expected.second)present=true;
            if(!present||terms.candidates.size()>9){std::cerr<<"Professional candidate missing\n";return 5;}
        }
        const auto quality=nativeime::core::RunQualityProbe([&](const std::wstring& p,std::vector<nativeime::core::Candidate>& candidates){candidates=rime.Query(p).candidates;return true;});
        std::cout<<"Fixed quality probe: exact "<<quality.exactHits<<"/"<<quality.exactTotal
                 <<", correction "<<quality.correctionHits<<"/"<<quality.correctionTotal
                 <<", negative "<<quality.negativePass<<"/"<<quality.negativeTotal<<"\n";
        if(!quality.Passed())return 6;
        std::cout<<"Real Rime candidate + session smoke passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 4;}
}
