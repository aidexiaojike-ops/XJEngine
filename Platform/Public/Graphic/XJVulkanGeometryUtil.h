#ifndef XJ_VULKAN_GEOMETRY_UTIL_H
#define XJ_VULKAN_GEOMETRY_UTIL_H

#include "Edit/Mathinclude.h"

namespace XJ
{
    struct XJVulkanVertex
    {
        glm::vec3 position; // 位置
        glm::vec2 texcoord0; // 纹理坐标
        glm::vec3 normal;
    };
    
    class XJVulkanGeometryUtil
    {
        private:
            /* data */
        public:
            XJVulkanGeometryUtil(/* args */);
            ~XJVulkanGeometryUtil();


        void CreateCube(float leftPlane, float rightPlane, float bottomPlane, float topPlane, float nearPlane, float farPlane,
                                    std::vector<XJVulkanVertex> &vertices, std::vector<uint32_t> &indices, const bool bUseTextcoords,
                                    const bool bUseNormals, const glm::mat4 &relativeMat);
    };
 
}


#endif