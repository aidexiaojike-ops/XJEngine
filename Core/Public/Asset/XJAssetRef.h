#ifndef XJ_ASSET_REF_H
#define XJ_ASSET_REF_H

#include "Asset/XJAsset.h"
#include <string>
//统一资产引用格式。Scene 里只存 handle，不存绝对路径。
//序列化
namespace XJ
{
    struct XJAssetRef
    {
        /* data */
        XJAssetHandle Handle = 0;
        XJAssetType Type = XJAssetType::None;

        bool IsValid() const{return Handle != 0 && Type != XJAssetType::None;}

        std::string ToUri() const;//将资源转换为URI字符串，可能是用于序列化或加载。
        //静态方法，从URI创建资源引用，可选指定期望类型。
        static XJAssetRef FromUri(const std::string& uri, XJAssetType expectedType = XJAssetType::None);

    };
    
}



#endif