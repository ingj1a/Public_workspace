#include "core/engine_protocol.hpp"
#include <codecvt>
#include <locale>
#include <limits>
namespace nativeime::core::ipc {
namespace {
void U16(std::vector<std::uint8_t>& b,std::uint16_t v){b.push_back(v&255);b.push_back((v>>8)&255);}
void U32(std::vector<std::uint8_t>& b,std::uint32_t v){for(int i=0;i<4;++i)b.push_back((v>>(i*8))&255);}
bool Get16(const std::uint8_t*& p,const std::uint8_t* e,std::uint16_t& v){if(e-p<2)return false;v=p[0]|(std::uint16_t(p[1])<<8);p+=2;return true;}
bool Get32(const std::uint8_t*& p,const std::uint8_t* e,std::uint32_t& v){if(e-p<4)return false;v=0;for(int i=0;i<4;++i)v|=std::uint32_t(p[i])<<(i*8);p+=4;return true;}
std::string Utf8(const std::wstring& s){std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> c;return c.to_bytes(s);}
std::wstring Wide(const std::string& s){std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> c;return c.from_bytes(s);}
}
std::vector<std::uint8_t> EncodeRequest(const Request& r){
    auto s=Utf8(r.pinyin);if(s.empty()||s.size()>64)throw std::invalid_argument("invalid pinyin length");
    std::vector<std::uint8_t>b;U32(b,Magic);U16(b,Version);U16(b,1);U32(b,static_cast<std::uint32_t>(s.size()));b.insert(b.end(),s.begin(),s.end());return b;
}
bool DecodeRequest(const std::uint8_t* data,std::size_t size,Request& r) noexcept{
    try{if(!data)return false;auto p=data;auto e=data+size;std::uint32_t magic=0,n=0;std::uint16_t ver=0,type=0;
    if(!Get32(p,e,magic)||!Get16(p,e,ver)||!Get16(p,e,type)||!Get32(p,e,n)||magic!=Magic||ver!=Version||type!=1||n==0||n>64||static_cast<std::size_t>(e-p)!=n)return false;
    r.pinyin=Wide(std::string(reinterpret_cast<const char*>(p),n));return !r.pinyin.empty()&&r.pinyin.size()<=64;}catch(...){return false;}
}
std::vector<std::uint8_t> EncodeResponse(const Response& r){
    std::vector<std::uint8_t>b;U32(b,Magic);U16(b,Version);U16(b,2);b.push_back(r.ok?1:0);const auto count=std::min<std::size_t>(r.candidates.size(),9);b.push_back(static_cast<std::uint8_t>(count));
    for(std::size_t i=0;i<count;++i){auto t=Utf8(r.candidates[i].text),c=Utf8(r.candidates[i].comment);if(t.size()>512)t.resize(512);if(c.size()>256)c.resize(256);U16(b,static_cast<std::uint16_t>(t.size()));U16(b,static_cast<std::uint16_t>(c.size()));b.insert(b.end(),t.begin(),t.end());b.insert(b.end(),c.begin(),c.end());}
    return b;
}
bool DecodeResponse(const std::uint8_t* data,std::size_t size,Response& r) noexcept{
    try{if(!data)return false;auto p=data;auto e=data+size;std::uint32_t magic=0;std::uint16_t ver=0,type=0;
    if(!Get32(p,e,magic)||!Get16(p,e,ver)||!Get16(p,e,type)||magic!=Magic||ver!=Version||type!=2||e-p<2)return false;
    r.ok=*p++==1;const auto count=*p++;if(count>9)return false;r.candidates.clear();
    for(unsigned i=0;i<count;++i){std::uint16_t tn=0,cn=0;if(!Get16(p,e,tn)||!Get16(p,e,cn)||tn==0||tn>512||cn>256||static_cast<std::size_t>(e-p)<tn+cn)return false;
    Candidate c;c.text=Wide(std::string(reinterpret_cast<const char*>(p),tn));p+=tn;c.comment=Wide(std::string(reinterpret_cast<const char*>(p),cn));p+=cn;r.candidates.push_back(std::move(c));}
    return p==e;}catch(...){return false;}
}
}
