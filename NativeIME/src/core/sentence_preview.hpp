#pragma once
#include "core/pinyin_engine.hpp"
#include <algorithm>
#include <string_view>
namespace nativeime::core {
inline constexpr std::wstring_view SentenceTag=L"整句预览";
// A Rime commit-text preview is an engine suggestion, not a prediction of
// words after commit. Only offer an entirely Han-character preview, and never
// promote it over an already displayed primary candidate.
inline bool AddSentencePreview(QueryResult& q,std::wstring_view preview){
    if(preview.size()<4||preview.size()>64||q.normalized_input.size()<8)return false;
    if(!std::all_of(preview.begin(),preview.end(),[](wchar_t c){return c>=0x3400&&c<=0x9fff;}))return false;
    if(std::any_of(q.candidates.begin(),q.candidates.end(),[&](const auto& c){return c.text==preview;}))return false;
    q.candidates.insert(q.candidates.begin()+std::min<std::size_t>(2,q.candidates.size()),
        {std::wstring(preview),85,std::wstring(SentenceTag)});
    if(q.candidates.size()>9)q.candidates.resize(9);
    return true;
}
}
