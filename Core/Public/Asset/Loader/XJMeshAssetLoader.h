#ifndef XJ_MESH_ASSET_LOADER_H
#define XJ_MESH_ASSET_LOADER_H

#include "Asset/XJAsset.h"

#include <memory>
#include <unordered_map>

namespace XJ
{
    class XJAssetRegistry;
    class XJMesh;

    struct XJMeshAssetLoadContext//加载网格资产时的上下文信息
    {
        XJAssetRegistry* Registry = nullptr;
        std::unordered_map<XJAssetHandle, std::shared_ptr<XJMesh>>* MeshCache = nullptr;
    };

    class XJMeshAssetLoader
    {
        public:
            static std::shared_ptr<XJMesh> LoadMesh(XJAssetHandle handle, XJMeshAssetLoadContext& context);
    };
}
#endif