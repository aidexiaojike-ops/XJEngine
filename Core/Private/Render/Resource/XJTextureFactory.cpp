#include "Render/Resource/XJTextureFactory.h"
#include "spdlog/spdlog.h"


namespace XJ
{
    std::shared_ptr<XJTexture> XJTextureFactory::CreateTextureFromAsset(const XJTextureAsset& asset)
    {
        if (asset.Width == 0 || asset.Height == 0)
        {
            spdlog::error(
                "XJTextureFactory: invalid texture extent {}x{}",
                asset.Width,
                asset.Height);
            return nullptr;
        }

        const size_t expectedSize =
            static_cast<size_t>(asset.Width) * static_cast<size_t>(asset.Height) * 4;

        if (asset.Pixels.size() != expectedSize)
        {
            spdlog::error(
                "XJTextureFactory: invalid pixel data size, expected={}, actual={}, extent={}x{}",
                expectedSize,
                asset.Pixels.size(),
                asset.Width,
                asset.Height);
            return nullptr;
        }

        auto texture = std::make_shared<XJTexture>(
            asset.Width,
            asset.Height,
            reinterpret_cast<RGBAColor*>(const_cast<uint8_t*>(asset.Pixels.data())));

        if (!texture || !texture->IsValid())
        {
            spdlog::error("XJTextureFactory: texture creation failed.");
            return nullptr;
        }

        return texture;
    }
}