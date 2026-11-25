#include "core/engine_protocol.hpp"
#include <iostream>
using namespace nativeime::core;
int main(){
    ipc::Request q{L"nihao"},decoded;
    auto request=ipc::EncodeRequest(q);
    if(!ipc::DecodeRequest(request.data(),request.size(),decoded)||decoded.pinyin!=q.pinyin)return 1;
    ipc::Response r;r.ok=true;r.candidates={{L"你好",100,L"问候"},{L"拟好",80,L""}};
    auto response=ipc::EncodeResponse(r);ipc::Response out;
    if(!ipc::DecodeResponse(response.data(),response.size(),out)||!out.ok||out.candidates.size()!=2||out.candidates[0].text!=L"你好")return 2;
    response.pop_back();if(ipc::DecodeResponse(response.data(),response.size(),out))return 3;
    auto invalid=request;invalid[0]=0;if(ipc::DecodeRequest(invalid.data(),invalid.size(),decoded))return 4;
    ipc::Response huge;huge.ok=true;for(int i=0;i<9;++i)huge.candidates.push_back({std::wstring(160,L'你'),0,std::wstring(80,L'好')});
    auto bounded=ipc::EncodeResponse(huge);if(bounded.size()>ipc::MaxFrame||!ipc::DecodeResponse(bounded.data(),bounded.size(),out))return 5;
    auto bad=request;bad.back()=0;if(ipc::DecodeRequest(bad.data(),bad.size(),decoded))return 6;
    return 0;
}
