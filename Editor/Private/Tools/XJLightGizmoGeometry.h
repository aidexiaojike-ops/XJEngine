#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace XJ
{
    class XJLightComponent;
    class XJTransformComponent;
}

namespace XJ::LightGizmoGeometry
{
    struct LineGeometry
    {
        std::vector<glm::vec3> Vertices;
        std::vector<uint32_t> Indices;
    };

    LineGeometry BuildPointGeometry();
    LineGeometry BuildDirectionalGeometry();
    LineGeometry BuildSpotGeometry();

    bool TryBuildModel(const XJTransformComponent& transform, const XJLightComponent& light, glm::mat4& model);
}
