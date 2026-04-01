#include <Graphic/XJVulkanAddModelAssets.h>
#include <Graphic/XJVulkanDevice.h>
#include <tiny_obj_loader.h>
#include <Edit/Mathinclude.h>

namespace XJ
{
    /*VulkanAddModelAssets::VulkanAddModelAssets(const std::string& MODEL_PATH, std::vector<Vertex>& vertices, std::vector<uint32_t>& indices)
    {
        //将模型加载到库的数据结构中
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;

        if(!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, MODEL_PATH.c_str()))
        {
            throw std::runtime_error(warn + err);
        }

        std::unordered_map<ade::Vertex, uint32_t> uniqueVertices{};

        for(const auto& shape : shapes)
        {
            for(const auto& index : shape.mesh.indices)
            {//顶点
                ade::Vertex vertex{};

                vertex.pos = 
                {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
                };

                vertex.texCoord = 
                {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };
                vertex.color = {1.0f, 1.0f, 1.0f};

                if(uniqueVertices.count(vertex) == 0)
                {
                    uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                    vertices.push_back(vertex);
                }

                indices.push_back(uniqueVertices[vertex]);
            }
        }
        
    }   */
    XJVulkanAddModelAssets::~XJVulkanAddModelAssets()
    {
    
        
    }
}