#ifndef XJ_TEXTURE_FACTORY_H
#define XJ_TEXTURE_FACTORY_H

#include "Asset/XJTextureAsset.h"
#include "Render/Resource/XJTexture.h"
#include <memory>

namespace XJ
{
    class XJTextureFactory
    {
        public: 
            static std::shared_ptr<XJTexture> CreateTextureFromAsset(const XJTextureAsset& asset);
    };
}

#endif