#include "Tools/XJLightGizmoGeometry.h"

#include "ECS/Component/XJLightComponent.h"
#include "ECS/Component/XJTransformComponent.h"
#include "Render/XJLightSceneUtils.h"

#include <algorithm>
#include <cmath>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace XJ::LightGizmoGeometry
{
    namespace
    {
        constexpr uint32_t kCircleSegments = 20;
        constexpr float kPointRadius = 0.3f;
        constexpr float kDirectionalLength = 0.8f;
        constexpr float kSpotLength = 0.8f;

        void AddLine(LineGeometry& geometry, const glm::vec3& start, const glm::vec3& end)
        {
            const auto index = static_cast<uint32_t>(geometry.Vertices.size());
            geometry.Vertices.push_back(start);
            geometry.Vertices.push_back(end);
            geometry.Indices.push_back(index);
            geometry.Indices.push_back(index + 1);
        }

        void AddCircle(
            LineGeometry& geometry,
            int fixedAxis,
            float fixedValue,
            float radius)
        {
            for (uint32_t segment = 0; segment < kCircleSegments; ++segment)
            {
                const float a0 = glm::two_pi<float>() * static_cast<float>(segment) / static_cast<float>(kCircleSegments);
                const float a1 = glm::two_pi<float>() * static_cast<float>(segment + 1) / static_cast<float>(kCircleSegments);
                glm::vec3 p0(0.0f);
                glm::vec3 p1(0.0f);
                p0[fixedAxis] = fixedValue;
                p1[fixedAxis] = fixedValue;

                const int axisA = (fixedAxis + 1) % 3;
                const int axisB = (fixedAxis + 2) % 3;
                p0[axisA] = std::cos(a0) * radius;
                p0[axisB] = std::sin(a0) * radius;
                p1[axisA] = std::cos(a1) * radius;
                p1[axisB] = std::sin(a1) * radius;
                AddLine(geometry, p0, p1);
            }
        }

        glm::mat4 BuildDirectionRotation(const glm::vec3& direction)
        {
            const glm::vec3 xAxis = glm::normalize(direction);
            const glm::vec3 referenceUp = std::abs(glm::dot(xAxis, glm::vec3(0.0f, 1.0f, 0.0f))) > 0.99f
                ? glm::vec3(0.0f, 0.0f, 1.0f)
                : glm::vec3(0.0f, 1.0f, 0.0f);
            const glm::vec3 yAxis = glm::normalize(referenceUp - xAxis * glm::dot(referenceUp, xAxis));
            const glm::vec3 zAxis = glm::normalize(glm::cross(xAxis, yAxis));

            glm::mat4 result(1.0f);
            result[0] = glm::vec4(xAxis, 0.0f);
            result[1] = glm::vec4(yAxis, 0.0f);
            result[2] = glm::vec4(zAxis, 0.0f);
            return result;
        }
    }

    LineGeometry BuildPointGeometry()
    {
        LineGeometry geometry;
        geometry.Vertices.reserve(kCircleSegments * 6);
        geometry.Indices.reserve(kCircleSegments * 6);
        AddCircle(geometry, 0, 0.0f, kPointRadius);
        AddCircle(geometry, 1, 0.0f, kPointRadius);
        AddCircle(geometry, 2, 0.0f, kPointRadius);
        return geometry;
    }

    LineGeometry BuildDirectionalGeometry()
    {
        LineGeometry geometry;
        AddLine(geometry, glm::vec3(0.0f), glm::vec3(kDirectionalLength, 0.0f, 0.0f));

        constexpr float headX = 0.55f;
        constexpr float headRadius = 0.16f;
        for (uint32_t segment = 0; segment < 8; ++segment)
        {
            const float angle = glm::two_pi<float>() * static_cast<float>(segment) / 8.0f;
            AddLine(
                geometry,
                glm::vec3(kDirectionalLength, 0.0f, 0.0f),
                glm::vec3(headX, std::cos(angle) * headRadius, std::sin(angle) * headRadius));
        }
        AddCircle(geometry, 0, headX, headRadius);
        return geometry;
    }

    LineGeometry BuildSpotGeometry()
    {
        LineGeometry geometry;
        AddLine(geometry, glm::vec3(0.0f), glm::vec3(kSpotLength, 0.0f, 0.0f));
        AddCircle(geometry, 0, kSpotLength, 1.0f);

        for (uint32_t segment = 0; segment < 8; ++segment)
        {
            const float angle = glm::two_pi<float>() * static_cast<float>(segment) / 8.0f;
            AddLine(
                geometry,
                glm::vec3(0.0f),
                glm::vec3(kSpotLength, std::cos(angle), std::sin(angle)));
        }
        return geometry;
    }

    bool TryBuildModel(const XJTransformComponent& transform, const XJLightComponent& light, glm::mat4& model)
    {
        glm::vec3 position;
        if (transform.ReadWorldPosition(position) != XJTransformStatus::Success)
            return false;

        const XJLightType type = light.XJGetLightType();
        model = glm::translate(glm::mat4(1.0f), position);
        if (type != XJLightType::Point)
            model *= BuildDirectionRotation(XJLightSceneUtils::BuildDirection(transform));

        glm::vec3 scale(1.0f);
        if (type == XJLightType::Spot)
        {
            const float coneRadius = std::min(
                std::tan(glm::radians(light.XJGetOuterAngleDegrees())) * kSpotLength, 1.0f);
            scale.y = coneRadius;
            scale.z = coneRadius;
        }
        model = glm::scale(model, scale);
        return true;
    }
}
