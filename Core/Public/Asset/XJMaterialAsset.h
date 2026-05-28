#ifndef XJ_MATERIAL_ASSET_H
#define XJ_MATERIAL_ASSET_H

#include "Asset/XJAsset.h"
#include <glm/glm.hpp>


namespace XJ
{
    class XJMaterialAsset : public XJAsset
    {
        public:
              // 贴图引用（句柄，指向 XJTextureAsset）
                XJAssetHandle AlbedoTexture   = 0;
                XJAssetHandle NormalTexture   = 0;      
                XJAssetHandle MetallicRoughnessTexture = 0; 

                // PBR 参数
                glm::vec4 BaseColorFactor  = glm::vec4(1.0f);
                float MetallicFactor       = 1.0f;      
                float RoughnessFactor      = 1.0f;       

    };

}

#endif
