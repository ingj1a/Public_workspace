#include "core/common_lexicon.hpp"
#include "core/sentence_preview.hpp"
using namespace nativeime::core;
int main(){
    for(const auto& term:CommonTerms){
        QueryResult q{std::wstring(term.pinyin),{{L"原首选",100,L""},{L"原次选",99,L""}}};
        if(!AddCommonCandidate(q)||q.candidates[2].text!=term.text||q.candidates[2].annotation!=L"常用"||
           q.candidates[0].text!=L"原首选"||AddCommonCandidate(q))return 1;
    }
    QueryResult q{L"wojintianhenkaixin",{{L"我",100,L""},{L"窝",99,L""}}};
    if(AddSentencePreview(q,L"我今天henkaixin")||AddSentencePreview(q,L"我今天")||!AddSentencePreview(q,L"我今天很开心"))return 2;
    if(q.candidates.size()!=3||q.candidates[2].text!=L"我今天很开心"||q.candidates[2].annotation!=SentenceTag||AddSentencePreview(q,L"我今天很开心"))return 3;
    QueryResult shortInput{L"nihao",{}};
    if(AddSentencePreview(shortInput,L"我今天很开心"))return 4;
    return 0;
}
