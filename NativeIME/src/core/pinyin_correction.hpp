#pragma once
#include "core/pinyin_engine.hpp"
#include <algorithm>
#include <string_view>
namespace nativeime::core {
// A small reviewed typo whitelist, not unrestricted fuzzy matching.
// Each suggestion must also be returned by a real lookup of the corrected spelling.
struct CorrectionRule { std::wstring_view typo,spelling,expected; };
inline constexpr CorrectionRule CorrectionRules[]={
    {L"nihoa",L"nihao",L"你好"},
    {L"zhongugo",L"zhongguo",L"中国"},
    {L"shijei",L"shijie",L"世界"},
    {L"cehsi",L"ceshi",L"测试"},
    {L"jianqiemengabn",L"jianqiemengban",L"剪切蒙版"},
    {L"tucengmengabn",L"tucengmengban",L"图层蒙版"},
    {L"faxiantiteu",L"faxiantietu",L"法线贴图"},
    {L"guangxianzhuiznog",L"guangxianzhuizong",L"光线追踪"},
    {L"yibutongixn",L"yibutongxin",L"异步通信"},
    {L"shangpinzhut",L"shangpinzhutu",L"商品主图"}
};
inline constexpr std::wstring_view CorrectionTag=L"纠错";
inline constexpr std::wstring_view RawPinyinTag=L"原拼音";
inline const CorrectionRule* FindCorrection(std::wstring_view input) noexcept {
    for(const auto& rule:CorrectionRules)if(input==rule.typo)return &rule;
    return nullptr;
}
inline bool AddCorrection(QueryResult& original,const CorrectionRule& rule,const QueryResult& verified){
    if(original.normalized_input!=rule.typo||verified.normalized_input!=rule.spelling)return false;
    if(!std::any_of(verified.candidates.begin(),verified.candidates.end(),[&](const auto& c){return c.text==rule.expected;}))return false;
    if(std::any_of(original.candidates.begin(),original.candidates.end(),[&](const auto& c){return c.text==rule.expected;}))return false;
    // A correction can never be the default suggestion when the base lookup fails.
    if(original.candidates.empty())original.candidates.push_back({original.normalized_input,0,std::wstring(RawPinyinTag)});
    auto index=std::min<std::size_t>(2,original.candidates.size());
    original.candidates.insert(original.candidates.begin()+index,{std::wstring(rule.expected),70,std::wstring(CorrectionTag)});
    if(original.candidates.size()>9)original.candidates.resize(9);
    return true;
}
}
