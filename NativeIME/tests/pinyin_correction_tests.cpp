#include "core/pinyin_correction.hpp"
#include "core/quality_probe.hpp"
#include "core/input_session.hpp"
using namespace nativeime::core;
int main(){
    for(const auto& rule:CorrectionRules){
        if(FindCorrection(rule.typo)!=&rule||FindCorrection(rule.spelling))return 1;
        QueryResult exact{std::wstring(rule.spelling),{{std::wstring(rule.expected),100,L""}}};
        QueryResult original{std::wstring(rule.typo),{{L"原首选",100,L""},{L"原次选",99,L""}}};
        if(!AddCorrection(original,rule,exact)||original.candidates[0].text!=L"原首选"||original.candidates[1].text!=L"原次选"||
           original.candidates[2].text!=rule.expected||original.candidates[2].annotation!=CorrectionTag)return 2;
        if(AddCorrection(original,rule,exact))return 3;
        QueryResult empty{std::wstring(rule.typo),{}};
        QueryResult wrong{std::wstring(rule.spelling),{{L"无关",100,L""}}};
        if(AddCorrection(empty,rule,wrong)||!empty.candidates.empty())return 4;
        if(!AddCorrection(empty,rule,exact)||empty.candidates.size()!=2||empty.candidates[0].text!=rule.typo||empty.candidates[0].annotation!=RawPinyinTag)return 5;
        InputSession s([&](const std::wstring& p){return p==rule.typo?empty:QueryResult{p,{}};});
        for(auto ch:rule.typo)s.Handle({Key::Letter,ch});
        if(s.Handle({Key::Space}).commit!=std::wstring(rule.typo)+L" ")return 6;
        for(auto ch:rule.typo)s.Handle({Key::Letter,ch});
        if(s.Handle({Key::Digit,L'2'}).commit!=rule.expected)return 7;
    }
    if(FindCorrection(L"qwerty")||FindCorrection(L"nihao")||FindCorrection(L"niho"))return 8;
    auto negative=RunQualityProbe([](const std::wstring&,std::vector<Candidate>&){return false;});
    if(negative.Passed()||negative.queryFailures!=negative.exactTotal+negative.correctionTotal+negative.negativeTotal)return 9;
    return 0;
}
