#pragma once
#include "core/pinyin_engine.hpp"
#include <algorithm>
#include <string_view>
namespace nativeime::core {
// Small original exact-spelling compatibility vocabulary, independent of
// the typo table. Explicit source label; no claim it came from upstream Rime.
struct CommonTerm { std::wstring_view pinyin,text; };
inline constexpr CommonTerm CommonTerms[]={
    {L"nihao",L"你好"},{L"zhongguo",L"中国"},{L"shijie",L"世界"},{L"ceshi",L"测试"},
    {L"gongzuo",L"工作"},{L"xuexi",L"学习"},{L"shuru",L"输入"},{L"shurufa",L"输入法"}
};
inline bool AddCommonCandidate(QueryResult& q){
    for(const auto& term:CommonTerms){
        if(q.normalized_input!=term.pinyin)continue;
        if(std::any_of(q.candidates.begin(),q.candidates.end(),[&](const auto& c){return c.text==term.text;}))return false;
        q.candidates.insert(q.candidates.begin()+std::min<std::size_t>(2,q.candidates.size()),
            {std::wstring(term.text),75,L"常用"});
        if(q.candidates.size()>9)q.candidates.resize(9);
        return true;
    }
    return false;
}
}
