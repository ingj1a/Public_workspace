#pragma once
#include "core/pinyin_engine.hpp"
#include <algorithm>
#include <string_view>
namespace nativeime::core {
// Original, small curated exact-spelling vocabulary. Not a predictive model.
// General Rime candidates keep their first two positions, and duplicate
// candidates keep their original position. No fuzzy matching or auto-commit.
struct ProfessionalTerm { std::wstring_view pinyin,text,domain; };
inline constexpr ProfessionalTerm ProfessionalTerms[]={
    {L"mengban",L"蒙版",L"设计"},{L"jianqiemengban",L"剪切蒙版",L"设计"},
    {L"tucengmengban",L"图层蒙版",L"设计"},{L"shiliangmengban",L"矢量蒙版",L"设计"},
    {L"zhinengduixiang",L"智能对象",L"设计"},{L"hunshe",L"混色",L"设计"},
    {L"xiangsuronghe",L"像素融合",L"设计"},{L"sexiangbaohedu",L"色相饱和度",L"设计"},
    {L"diaozhengceng",L"调整层",L"设计"},{L"quxiandiaozheng",L"曲线调整",L"设计"},
    {L"shilianglujing",L"矢量路径",L"设计"},{L"jiemiansheji",L"界面设计",L"设计"},
    {L"zidongbuju",L"自动布局",L"设计"},{L"zujianbianti",L"组件变体",L"设计"},
    {L"shangpinzhutu",L"商品主图",L"电商"},{L"xiangqingye",L"详情页",L"电商"},
    {L"shangpinjingxiu",L"商品精修",L"电商"},{L"toumingdise",L"透明底色",L"电商"},
    {L"guangxianzhuizong",L"光线追踪",L"三维"},{L"quanjuguangzhao",L"全局光照",L"三维"},
    {L"faxiantietu",L"法线贴图",L"三维"},{L"weiyitietu",L"位移贴图",L"三维"},
    {L"huanjingzhebi",L"环境遮蔽",L"三维"},{L"gugebangding",L"骨骼绑定",L"三维"},
    {L"wulixuanran",L"物理渲染",L"三维"},{L"duobianxingwangge",L"多边形网格",L"三维"},
    {L"guanjianzhen",L"关键帧",L"影视"},{L"yundongmohu",L"运动模糊",L"影视"},
    {L"shendutietu",L"深度贴图",L"影视"},{L"beijingtihuan",L"背景替换",L"影视"},
    {L"yibutongxin",L"异步通信",L"开发"},{L"kuajincheng",L"跨进程",L"开发"},
    {L"huiguiceshi",L"回归测试",L"开发"},{L"danyuanceshi",L"单元测试",L"开发"},
    {L"yichanghuifu",L"异常恢复",L"开发"},{L"yilaisuoding",L"依赖锁定",L"开发"}
};
inline bool AddProfessionalCandidate(QueryResult& result){
    for(const auto& term:ProfessionalTerms){
        if(result.normalized_input!=term.pinyin)continue;
        if(std::any_of(result.candidates.begin(),result.candidates.end(),[&](const Candidate& c){return c.text==term.text;}))return false;
        const auto index=std::min<std::size_t>(2,result.candidates.size());
        result.candidates.insert(result.candidates.begin()+index,{std::wstring(term.text),80,std::wstring(term.domain)});
        if(result.candidates.size()>9)result.candidates.resize(9);
        return true;
    }
    return false;
}
}
