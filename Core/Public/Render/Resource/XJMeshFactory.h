#ifndef XJ_MESH_FACTORY_H
#define XJ_MESH_FACTORY_H

#include "Asset/XJMeshAsset.h"
#include "Render/Resource/XJMesh.h"
#include <memory>
//XJMeshFactory 是 GPU Resource 创建器
namespace XJ
{
    class XJMeshFactory
    {
        public:
        
            static std::shared_ptr<XJMesh> CreateFromAsset(const XJMeshAsset& asset);//// 转换 Vertex（Asset 格式）→ XJVulkanVertex（GPU 格式）
    };
}



#endif