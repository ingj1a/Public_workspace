#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <utility>
#include <stdexcept>
namespace nativeime::core {
// Tiny ZIP32 writer for bounded, generated ASCII-named reports; no external
// executables, compression library, filesystem enumeration or arbitrary paths.
inline std::uint32_t Crc32(const std::string& s){
    std::uint32_t crc=0xffffffffu;
    for(unsigned char c:s){crc^=c;for(int b=0;b<8;++b)crc=(crc>>1)^((crc&1)?0xedb88320u:0u);}
    return ~crc;
}
inline std::string StoreZip(const std::vector<std::pair<std::string,std::string>>& files){
    if(files.empty()||files.size()>32)throw std::invalid_argument("ZIP entry count");
    std::string out,central;
    auto u16=[](std::string& dst,std::uint32_t n){dst.push_back(static_cast<char>(n&255));dst.push_back(static_cast<char>((n>>8)&255));};
    auto u32=[&](std::string& dst,std::uint32_t n){u16(dst,n);u16(dst,n>>16);};
    std::vector<std::string> names;
    for(const auto& file:files){
        const auto& name=file.first;const auto& data=file.second;
        if(name.empty()||name.size()>128||name=="."||name==".."||data.size()>2*1024*1024)throw std::invalid_argument("ZIP entry");
        for(unsigned char c:name)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='_'||c=='-'))throw std::invalid_argument("ZIP name");
        for(const auto& old:names)if(old==name)throw std::invalid_argument("duplicate ZIP name");
        names.push_back(name);
        const auto offset=static_cast<std::uint32_t>(out.size()),crc=Crc32(data),n=static_cast<std::uint32_t>(data.size()),nl=static_cast<std::uint32_t>(name.size());
        u32(out,0x04034b50);u16(out,20);u16(out,0);u16(out,0);u16(out,0);u16(out,33);
        u32(out,crc);u32(out,n);u32(out,n);u16(out,nl);u16(out,0);out+=name;out+=data;
        u32(central,0x02014b50);u16(central,20);u16(central,20);u16(central,0);u16(central,0);
        u16(central,0);u16(central,33);u32(central,crc);u32(central,n);u32(central,n);
        u16(central,nl);u16(central,0);u16(central,0);u16(central,0);u16(central,0);u32(central,0);u32(central,offset);central+=name;
    }
    const auto offset=static_cast<std::uint32_t>(out.size());out+=central;
    u32(out,0x06054b50);u16(out,0);u16(out,0);u16(out,static_cast<std::uint32_t>(files.size()));u16(out,static_cast<std::uint32_t>(files.size()));
    u32(out,static_cast<std::uint32_t>(central.size()));u32(out,offset);u16(out,0);
    return out;
}
}
