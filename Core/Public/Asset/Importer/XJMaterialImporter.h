#ifndef XJ_MATERIAL_IMPORTER_H
#define XJ_MATERIAL_IMPORTER_H

#include "Asset/XJMaterialAsset.h"
#include <memory>

namespace XJ
{
    class XJMaterialImporter
    {
        public:
        
            static std::shared_ptr<XJMaterialAsset> ImportMaterial(const std::string& path);
    };
}

#endif