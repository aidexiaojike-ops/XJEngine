#include "Asset/XJAssetRef.h"

#include <charconv>
#include <string_view>

namespace XJ
{
    namespace
    {
        bool ParseUInt64Strict(std::string_view text, uint64_t& outValue)
        {
            if (text.empty())
                return false;

            const char* begin = text.data();
            const char* end = begin + text.size();
            auto [ptr, ec] = std::from_chars(begin, end, outValue, 10);
            return ec == std::errc{} && ptr == end;
        }

        bool ParseIntStrict(std::string_view text, int& outValue)
        {
            if (text.empty())
                return false;

            const char* begin = text.data();
            const char* end = begin + text.size();
            auto [ptr, ec] = std::from_chars(begin, end, outValue, 10);
            return ec == std::errc{} && ptr == end;
        }
    }

    std::string XJAssetRef::ToUri() const//把资产序列化
    {
        if (!IsValid())
            return {};
        return "asset://" + std::to_string(static_cast<int>(Type)) + "/" + std::to_string(Handle);//序列化的格式
    }

    XJAssetRef XJAssetRef::FromUri(const std::string& uri, XJAssetType expectedType)//读取序列 反序列化
    {
        XJAssetRef ref;
        try
        {
            if (uri.rfind("asset://", 0) == 0)
            {
                const std::string_view payload(uri.data() + 8, uri.size() - 8);
                const size_t slash = payload.find('/');

                if (slash == std::string_view::npos)
                {
                    // Legacy format: asset://123. Keep reading old scene/material files.
                    uint64_t handle = 0;
                    if (!ParseUInt64Strict(payload, handle))
                        return ref;

                    ref.Handle = handle;
                    ref.Type = expectedType;
                }
                else
                {
                    int typeValue = 0;
                    uint64_t handle = 0;
                    if (!ParseIntStrict(payload.substr(0, slash), typeValue) ||
                        !ParseUInt64Strict(payload.substr(slash + 1), handle))
                    {
                        return ref;
                    }

                    ref.Type = static_cast<XJAssetType>(typeValue);
                    ref.Handle = handle;
                }
            }
            else
            {
                auto pos = uri.find(':');
                if (pos == std::string::npos || pos == 0 || pos == uri.size() - 1)
                    return ref;

                int typeValue = 0;
                uint64_t handle = 0;
                if (!ParseIntStrict(uri.substr(0, pos), typeValue) ||
                    !ParseUInt64Strict(uri.substr(pos + 1), handle))
                {
                    return ref;
                }

                ref.Type = static_cast<XJAssetType>(typeValue);
                ref.Handle = handle;
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
