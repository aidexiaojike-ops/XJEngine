#ifndef XJ_TEXTURE_IMPORTER_H
#define XJ_TEXTURE_IMPORTER_H

#include "Asset/XJTextureAsset.h"
#include <memory>

namespace XJ
{
    class XJTextureImporter
    {
        public:
        
            static std::shared_ptr<XJTextureAsset> ImportTexture(const std::string& path);
    };
}

#endif