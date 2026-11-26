#include "core/professional_lexicon.hpp"
using namespace nativeime::core;
int main(){
    for(const auto& term:ProfessionalTerms){
        if(term.pinyin.empty()||term.text.empty()||term.domain.empty())return 1;
        for(auto c:term.pinyin)if(c<L'a'||c>L'z')return 2;
        QueryResult q{std::wstring(term.pinyin),{{L"原首选",100,L""},{L"原次选",99,L""}}};
        if(!AddProfessionalCandidate(q)||q.candidates.size()!=3||q.candidates[0].text!=L"原首选"||q.candidates[1].text!=L"原次选"||q.candidates[2].text!=term.text)return 3;
        if(AddProfessionalCandidate(q)||q.candidates.size()!=3)return 4;
    }
    QueryResult general{L"nihao",{{L"你好",100,L""}}};if(AddProfessionalCandidate(general)||general.candidates.size()!=1)return 5;
    QueryResult partial{L"tucengmeng",{} };if(AddProfessionalCandidate(partial))return 6;
    QueryResult full{L"mengban",{}};for(int i=0;i<9;++i)full.candidates.push_back({std::to_wstring(i),100-i,L""});
    if(!AddProfessionalCandidate(full)||full.candidates.size()!=9||full.candidates[2].text!=L"蒙版")return 7;
    return 0;
}
