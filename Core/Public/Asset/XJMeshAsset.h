#ifndef XJ_MESH_ASSET_H
#define XJ_MESH_ASSET_H

#include "Asset/XJAsset.h"

#include <cstdint>
#include <vector>
#include <glm/glm.hpp>


namespace XJ
{
    struct Vertex
    {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec3 Tangent;  
        glm::vec2 UV;
    };
    // CPU 资产层保存的 glTF primitive 信息。
    // 顶点和索引仍然合并存储，primitive 只保存自己的索引范围。
    struct XJMeshPrimitive
    {
        uint32_t FirstIndex = 0;
        uint32_t IndexCount = 0;

        // 引擎内部材质槽。第一版使用有效 primitive 的顺序。
        uint32_t MaterialSlot = 0;

        // glTF 原始 material index，仅用于以后自动导入材质。
        // -1 表示该 primitive 没有指定 glTF material。
        int32_t SourceMaterialIndex = -1;

        bool IsValid(uint32_t totalIndexCount) const
        {
            if(IndexCount == 0)
                return false;
            
            if(FirstIndex > totalIndexCount)
                return false;

            return IndexCount <= totalIndexCount - FirstIndex;
        }
    };

    class XJMeshAsset : public XJAsset
    {
        public:

            std::vector<Vertex> mVertices;//顶点数据，包含位置、法线、UV等信息
            std::vector<uint32_t> mIndices;//索引数据，uint32_t 是常用的索引类型，可以根据需要改为 uint16_t 或其他类型
            std::vector<XJMeshPrimitive> mPrimitives;
    };

}

#endif
