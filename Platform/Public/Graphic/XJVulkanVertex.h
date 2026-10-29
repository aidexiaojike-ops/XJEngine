#ifndef XJ_VULKAN_VERTEX_H
#define XJ_VULKAN_VERTEX_H

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

// GPU layouts require packed GLM types; do not enable aligned gentypes.
static_assert(sizeof(glm::vec3) == 12, "XJEngine requires packed glm::vec3 (12 bytes)");

namespace XJ
{
    struct XJVulkanVertex
    {
        glm::vec3 position{};
        glm::vec2 texcoord0{};
        glm::vec3 tangent{};
        glm::vec3 normal{};
    };
}

#endif
