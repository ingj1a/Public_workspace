#pragma once
#include "core/pinyin_correction.hpp"
#include <cstddef>
#include <string_view>
namespace nativeime::core {
// Only fixed synthetic text; the exported result contains counts, not inputs.
struct QualityCase { std::wstring_view pinyin,expected; };
inline constexpr QualityCase ExactCases[]={
    {L"nihao",L"你好"},{L"jianqiemengban",L"剪切蒙版"},
    {L"faxiantietu",L"法线贴图"},{L"yibutongxin",L"异步通信"}
};
inline constexpr std::wstring_view NegativeCases[]={L"qwerty",L"zzzzzz",L"nihao"};
struct QualityResult {
    unsigned exactTotal=0,exactHits=0,correctionTotal=0,correctionHits=0,
        negativeTotal=0,negativePass=0,queryFailures=0;
    bool Passed()const noexcept {
        return exactTotal&&correctionTotal&&negativeTotal&&!queryFailures&&
            exactHits==exactTotal&&correctionHits==correctionTotal&&negativePass==negativeTotal;
    }
};
// Query contract: return true on successful response, populate candidates.
template<class Query> QualityResult RunQualityProbe(Query query){
    QualityResult r;
    auto check=[&](std::wstring_view input,std::wstring_view expected,bool negative){
        std::vector<Candidate> candidates;
        if(!query(std::wstring(input),candidates)){++r.queryFailures;return false;}
        if(negative)return std::none_of(candidates.begin(),candidates.end(),[](const auto& c){return c.annotation==CorrectionTag;});
        return std::any_of(candidates.begin(),candidates.end(),[&](const auto& c){return c.text==expected;});
    };
    for(const auto& c:ExactCases){++r.exactTotal;if(check(c.pinyin,c.expected,false))++r.exactHits;}
    for(const auto& c:CorrectionRules){++r.correctionTotal;if(check(c.typo,c.expected,false))++r.correctionHits;}
    for(auto p:NegativeCases){++r.negativeTotal;if(check(p,L"",true))++r.negativePass;}
    return r;
}
}
