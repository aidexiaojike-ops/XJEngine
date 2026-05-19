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

    class XJMeshAsset : public XJAsset
    {
        public:

            std::vector<Vertex> mVertices;//顶点数据，包含位置、法线、UV等信息

            std::vector<uint32_t> mIndices;//索引数据，uint32_t 是常用的索引类型，可以根据需要改为 uint16_t 或其他类型
    };

}

#endif
