#include "Tools/XJTransformGizmoGeometry.h"
#include "Tools/XJTransformGizmoInteractionMath.h"

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

    void TestMoveConstraint()
    {
        using namespace XJ;

        XJGizmoRenderedFrame rendered;
        rendered.Valid = true;

        rendered.Width = 1280;
        rendered.Height = 720;

        rendered.View =
            glm::lookAtRH(
                glm::vec3(0.0f, 0.0f, 5.0f),
                glm::vec3(0.0f),
                glm::vec3(0.0f, 1.0f, 0.0f));

        rendered.Projection = MakeProjection();

        rendered.WorldPosition = glm::dvec3(0.0);
        rendered.Orientation =
            glm::dquat(1.0, 0.0, 0.0, 0.0);

        const auto frame =
            BuildTransformGizmoFrame(
                rendered.WorldPosition,
                rendered.Orientation,
                rendered.View,
                rendered.Projection,
                rendered.Width,
                rendered.Height);

        Check(frame.Valid, "Move test frame is valid");

        rendered.Model = frame.Model;

        XJGizmoMoveConstraint constraint;

        Check(
            BeginGizmoMoveConstraint(
                rendered,
                XJTransformGizmoHandle::MoveX,
                glm::dvec2(0.5, 0.5),
                constraint),
            "Begin X move constraint");

        glm::dvec3 target;

        Check(
            CalculateGizmoMoveTarget(
                constraint,
                glm::dvec2(0.6, 0.5),
                target),
            "Calculate X move target");

        Check(target.x > 0.0, "Dragging right moves positive X");

        Check(
            std::abs(target.y) < 1.0e-8 &&
            std::abs(target.z) < 1.0e-8,
            "Move stays on X axis");

        const glm::dvec3 firstTarget = target;

        Check(
            CalculateGizmoMoveTarget(
                constraint,
                glm::dvec2(0.6, 0.5),
                target),
            "Repeat target calculation");

        Check(
            glm::length(target - firstTarget) < 1.0e-10,
            "Repeated calculation does not accumulate movement");

        Check(
            !BeginGizmoMoveConstraint(
                rendered,
                XJTransformGizmoHandle::MoveZ,
                glm::dvec2(0.5, 0.5),
                constraint),
            "Reject axis facing directly at camera");

        // 将 Gizmo 局部 X 旋转到世界 Y。
        rendered.Orientation =
            glm::angleAxis(
                glm::radians(90.0),
                glm::dvec3(0.0, 0.0, 1.0));

        Check(
            BeginGizmoMoveConstraint(
                rendered,
                XJTransformGizmoHandle::MoveX,
                glm::dvec2(0.5, 0.5),
                constraint),
            "Begin rotated local X constraint");

        Check(
            CalculateGizmoMoveTarget(
                constraint,
                glm::dvec2(0.5, 0.4),
                target),
            "Calculate rotated local X move");

        Check(
            std::abs(target.x) < 1.0e-8 &&
            target.y > 0.0 &&
            std::abs(target.z) < 1.0e-8,
            "Local X follows world orientation");
    }

    void TestGizmoPicking()
    {
        using namespace XJ;

        XJGizmoRenderedFrame rendered;
        rendered.Valid = true;

        rendered.Width = 1280;
        rendered.Height = 720;

        rendered.View =
            glm::lookAtRH(
                glm::vec3(0.0f, 0.0f, 5.0f),
                glm::vec3(0.0f),
                glm::vec3(0.0f, 1.0f, 0.0f));

        rendered.Projection = MakeProjection();

        const auto frame =
            BuildTransformGizmoFrame(
                glm::dvec3(0.0),
                glm::dquat(1.0, 0.0, 0.0, 0.0),
                rendered.View,
                rendered.Projection,
                1280,
                720);

        Check(frame.Valid, "Pick test frame is valid");

        rendered.Model = frame.Model;

        // 位于 X 移动轴杆内部，避开圆环和缩放方块。
        const glm::vec4 clip =
            rendered.Projection *
            rendered.View *
            rendered.Model *
            glm::vec4(0.5f, 0.0f, 0.0f, 1.0f);

        const glm::dvec2 uv =
            glm::dvec2(clip) / double(clip.w) * 0.5 +
            glm::dvec2(0.5);

        Check(
            PickTransformGizmo(
                rendered,
                uv,
                glm::dvec2(1280.0, 720.0)) ==
                XJTransformGizmoHandle::MoveX,
            "Pick X movement shaft");

        rendered.Valid = false;

        Check(
            PickTransformGizmo(
                rendered,
                uv,
                glm::dvec2(1280.0, 720.0)) ==
                XJTransformGizmoHandle::None,
            "Invalid frame cannot be picked");
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
    TestMoveConstraint();
    TestGizmoPicking();


    spdlog::info(
        "XJTransformGizmoGeometryTests passed.");

    return EXIT_SUCCESS;
}
