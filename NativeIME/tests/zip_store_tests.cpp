#include "core/zip_store.hpp"
#include <fstream>
int main(int argc,char** argv){
    using namespace nativeime::core;
    if(Crc32("123456789")!=0xcbf43926u)return 1;
    if(argc!=2)return 2;
    auto z=StoreZip({{"summary.json","{\"schema\":2}\n"},{"performance.csv","elapsed_ms,cpu_pct\n0,\n"}});
    if(z.size()<100||z.substr(0,4)!=std::string("PK\x03\x04",4))return 3;
    try{StoreZip({{"../bad","x"}});return 4;}catch(const std::invalid_argument&){}
    try{StoreZip({{"same","x"},{"same","y"}});return 5;}catch(const std::invalid_argument&){}
    std::ofstream out(argv[1],std::ios::binary|std::ios::trunc);out.write(z.data(),static_cast<std::streamsize>(z.size()));out.close();
    return out?0:6;
}
