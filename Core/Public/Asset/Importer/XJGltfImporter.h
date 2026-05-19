#ifndef XJ_GLTF_IMPORTER_H
#define XJ_GLTF_IMPORTER_H

#include "Asset/XJMeshAsset.h"
#include <memory>

namespace XJ
{
    class XJGltfImporter
    {
        public:
            static std::shared_ptr<XJMeshAsset> ImportMesh(const std::string& path);
    };
}

#endif