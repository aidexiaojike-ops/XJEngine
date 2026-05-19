#include "Render/Resource/XJTextureFactory.h"
#include "spdlog/spdlog.h"


namespace XJ
{
    std::shared_ptr<XJTexture> XJTextureFactory::CreateTextureFromAsset(const XJTextureAsset& asset)
    {
       if (asset.Pixels.empty())
        {
            spdlog::error("XJTextureFactory: asset has no pixel data");
            return nullptr;
        }

        return std::make_shared<XJTexture>(
            asset.Width,
            asset.Height,
            reinterpret_cast<RGBAColor*>(const_cast<uint8_t*>(asset.Pixels.data())));
    }
}