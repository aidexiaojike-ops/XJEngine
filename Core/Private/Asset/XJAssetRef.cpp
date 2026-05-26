#include "Asset/XJAssetRef.h"

namespace XJ
{
    std::string XJAssetRef::ToUri() const//把资产序列化
    {
        if (!IsValid())
            return {};
        return "asset://" + std::to_string(Handle);//序列化的格式
    }

    XJAssetRef XJAssetRef::FromUri(const std::string& uri, XJAssetType expectedType)//读取序列 反序列化
    {
        XJAssetRef ref;
        try
        {
            if (uri.rfind("asset://", 0) == 0)
            {
                ref.Handle = std::stoull(uri.substr(8));
                ref.Type = expectedType;
            }
            else
            {
                auto pos = uri.find(':');
                if (pos == std::string::npos || pos == 0 || pos == uri.size() - 1)
                    return ref;

                ref.Type = static_cast<XJAssetType>(std::stoi(uri.substr(0, pos)));
                ref.Handle = std::stoull(uri.substr(pos + 1));
            }
        }
        catch (const std::exception&)
        {
            return XJAssetRef();
        }

        if (expectedType != XJAssetType::None && ref.Type != expectedType)
            return XJAssetRef();

        return ref;
    }
}
