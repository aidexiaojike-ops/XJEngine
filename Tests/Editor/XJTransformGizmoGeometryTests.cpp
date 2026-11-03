#include "Tools/XJTransformGizmoGeometry.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

#include <spdlog/spdlog.h>

#include <cmath>
#include <cstdlib>
#include <set>
#include <string_view>

namespace
{
    void Check(bool value, std::string_view message)
    {
        if (!value)
        {
            spdlog::error("FAILED: {}", message);
            std::exit(EXIT_FAILURE);
        }
    }

    void TestGeometry()
    {
        using namespace XJ;

        const auto parts =
            BuildTransformGizmoGeometry();

        Check(
            parts.size() == 9,
            "Unified gizmo has nine parts");

        std::set<XJTransformGizmoHandle> handles;

        for (const auto& part : parts)
        {
            Check(
                part.Handle != XJTransformGizmoHandle::None,
                "Part has valid handle");

            Check(
                handles.insert(part.Handle).second,
                "Handle is unique");

            Check(
                !part.Positions.empty(),
                "Part contains geometry");

            Check(
                part.Positions.size() % 3 == 0,
                "Geometry is a triangle list");

            for (const auto& position : part.Positions)
            {
                Check(
                    std::isfinite(position.x) &&
                    std::isfinite(position.y) &&
                    std::isfinite(position.z),
                    "Position is finite");

                Check(
                    glm::length(position) <=
                        XJTransformGizmoMetrics::OuterExtent,
                    "Geometry fits declared extent");
            }

            for (std::size_t i = 0;
                 i < part.Positions.size();
                 i += 3)
            {
                const glm::vec3 a =
                    part.Positions[i + 1] -
                    part.Positions[i];

                const glm::vec3 b =
                    part.Positions[i + 2] -
                    part.Positions[i];

                Check(
                    glm::length(glm::cross(a, b)) >
                        1.0e-9f,
                    "Triangle is not degenerate");
            }
        }

        Check(
            GetGizmoOperation(
                XJTransformGizmoHandle::RotateZ) ==
                XJTransformGizmoOperation::Rotate,
            "Rotation handle operation");

        Check(
            GetGizmoAxis(
                XJTransformGizmoHandle::ScaleY) ==
                XJTransformGizmoAxis::Y,
            "Scale handle axis");
    }

    glm::mat4 MakeProjection()
    {
        glm::mat4 projection =
            glm::perspectiveRH_ZO(
                glm::radians(60.0f),
                1280.0f / 720.0f,
                0.1f,
                1000.0f);

        projection[1][1] *= -1.0f;

        return projection;
    }

    void TestScreenSize()
    {
        using namespace XJ;

        const glm::mat4 projection =
            MakeProjection();

        const auto makeFrame =
            [&](float distance)
        {
            const glm::mat4 view =
                glm::lookAtRH(
                    glm::vec3(0.0f, 0.0f, distance),
                    glm::vec3(0.0f),
                    glm::vec3(0.0f, 1.0f, 0.0f));

            return BuildTransformGizmoFrame(
                glm::dvec3(0.0),
                glm::dquat(1.0, 0.0, 0.0, 0.0),
                view,
                projection,
                1280,
                720);
        };

        const auto nearFrame = makeFrame(5.0f);
        const auto farFrame = makeFrame(20.0f);

        Check(
            nearFrame.Valid && farFrame.Valid,
            "Valid frames at both distances");

        const double ratio =
            double(farFrame.WorldScale) /
            double(nearFrame.WorldScale);

        Check(
            std::abs(ratio - 4.0) < 1.0e-3,
            "World scale follows perspective distance");

        const auto invalid =
            BuildTransformGizmoFrame(
                glm::dvec3(0.0),
                glm::dquat(1.0, 0.0, 0.0, 0.0),
                glm::mat4(1.0f),
                projection,
                1280,
                0);

        Check(
            !invalid.Valid,
            "Zero viewport height is rejected");
    }
}

int main()
{
    TestGeometry();
    TestScreenSize();

    spdlog::info(
        "XJTransformGizmoGeometryTests passed.");

    return EXIT_SUCCESS;
}