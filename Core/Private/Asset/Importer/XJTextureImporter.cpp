#include "Asset/Importer/XJTextureImporter.h"
#include <stb_image.h>
#include "spdlog/spdlog.h"

namespace XJ
{
    std::shared_ptr<XJTextureAsset> XJTextureImporter::ImportTexture(const std::string& path)
    {
        int width, height, channels;
        //path.c_str() 路径 将 std::string 转换为 const char*，这是 stbi_load 需要的参数类型
        stbi_uc* kData = stbi_load(path.c_str(), &width, &height, &channels, STBI_rgb_alpha);// 加载图片数据 强制 RGBA8
        if (!kData)
        {
            spdlog::error("Failed to load texture: {}", path);
            return nullptr;
        }

        std::shared_ptr<XJTextureAsset> kTextureAsset = std::make_shared<XJTextureAsset>();
        kTextureAsset->Width = static_cast<uint32_t>(width);
        kTextureAsset->Height = static_cast<uint32_t>(height);
        kTextureAsset->Pixels.assign(kData, kData + (width * height * 4)); // RGBA 格式，每个像素占 4 字节

        stbi_image_free(kData);// 释放加载的图像数据
        return kTextureAsset;
    }
}
