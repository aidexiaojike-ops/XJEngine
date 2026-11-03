#include "Tools/XJTransformGizmoGeometry.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <utility>

namespace XJ
{
    namespace
    {
        using Positions = std::vector<glm::vec3>;

        constexpr uint32_t kCylinderSegments = 20;//圆柱体分段数
        constexpr uint32_t kRingSegments  = 96;//圆环分段数
        constexpr uint32_t kTubeSegments = 8;//圆环管分段数

        void AddTriangle(Positions& positions, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)//添加三角形
        {
            positions.push_back(a);
            positions.push_back(b);
            positions.push_back(c);
        }

        void AddQuad(Positions& positions, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, const glm::vec3& d)//添加四边形
        {
            AddTriangle(positions, a, b, c);
            AddTriangle(positions, a, c, d);
        }

        // 几何统一沿 +X 创建。
        // 通过纯旋转转换为 +Y、+Z。
        glm::vec3 OrientToAxis(const glm::vec3& value, XJTransformGizmoAxis axis)//将几何体旋转到指定轴
        {
            switch(axis)
            {
                case XJTransformGizmoAxis::X:
                    return value;

                case XJTransformGizmoAxis::Y:
                    // 绕 Z 轴 +90°。
                    return {-value.y, value.x, value.z};

                case XJTransformGizmoAxis::Z:
                    // 绕 Y 轴 -90°。
                    return {-value.z, value.y, value.x};
            }
            return value;
        }

        void OrientPositions(Positions& positions, XJTransformGizmoAxis axis)
        {
            for (auto& position : positions)
                position = OrientToAxis(position, axis);
        }

        void OrientToAxis(Positions& positions, XJTransformGizmoAxis axis)//将几何体旋转到指定轴
        {
            for(auto& position : positions)
                position = OrientToAxis(position, axis);
        }

        glm::vec3 CirclePoint(float x, float radius, float angle)//计算圆环点
        {
            return {x, radius * std::cos(angle), radius * std::sin(angle)};
        }

        void AppendCylinder(Positions& positions, float start, float end, float radius)//添加圆柱体
        {
            const glm::vec3 startCenter{start, 0.0f, 0.0f};
            const glm::vec3 endCenter{end, 0.0f, 0.0f};

            for(uint32_t i = 0; i < kCylinderSegments; ++i)
            {
                const float a = glm::two_pi<float>() * float(i) / float(kCylinderSegments);
                const float b = glm::two_pi<float>() * float(i + 1) / float(kCylinderSegments);
                //
                const glm::vec3 p0 = CirclePoint(start, radius, a);
                const glm::vec3 p1 = CirclePoint(start, radius, b);
                const glm::vec3 p2 = CirclePoint(end, radius, b);
                const glm::vec3 p3 = CirclePoint(end, radius, a);

                AddQuad(positions, p0, p1, p2, p3);
                AddTriangle(positions, startCenter, p1, p0);
                AddTriangle(positions, endCenter, p2, p3);
            }
        }

        void AppendCone(Positions& positions, float base, float tip, float radius)//添加圆锥体
        {
            const glm::vec3 center(base, 0.0f, 0.0f);
            const glm::vec3 tipPosition(tip, 0.0f, 0.0f);

            for(uint32_t i = 0; i < kCylinderSegments; ++i)
            {
                const float a = glm::two_pi<float>() * float(i) / float(kCylinderSegments);
                const float b = glm::two_pi<float>() * float(i + 1) / float(kCylinderSegments);

                const glm::vec3 p0 = CirclePoint(base, radius, a);
                const glm::vec3 p1 = CirclePoint(base, radius, b);

                AddTriangle(positions, center, p1, p0);
                AddTriangle(positions, tipPosition, p0, p1);
            }
        }

        void AppendBox(Positions& positions, float centerX, float halfSize)//添加方块
        {
            const float x0 = centerX - halfSize;
            const float x1 = centerX + halfSize;

            const float y0 = -halfSize;
            const float y1 = halfSize;

            const float z0 = -halfSize;
            const float z1 = halfSize;

            const glm::vec3 p000(x0, y0, z0);
            const glm::vec3 p001(x0, y0, z1);
            const glm::vec3 p010(x0, y1, z0);
            const glm::vec3 p011(x0, y1, z1);

            const glm::vec3 p100(x1, y0, z0);
            const glm::vec3 p101(x1, y0, z1);
            const glm::vec3 p110(x1, y1, z0);
            const glm::vec3 p111(x1, y1, z1);

            AddQuad(positions, p000, p001, p011, p010);
            AddQuad(positions, p100, p110, p111, p101);

            AddQuad(positions, p000, p100, p101, p001);
            AddQuad(positions, p010, p011, p111, p110);

            AddQuad(positions, p000, p010, p110, p100);
            AddQuad(positions, p001, p101, p111, p011);
        }

        glm::vec3 TorusPoint(float u, float v)//计算圆环点
        {
            const float ringRadius = XJTransformGizmoMetrics::RingRadius;
            const float tubeRadius = XJTransformGizmoMetrics::RingTubeRadius;
            const float radius = ringRadius + tubeRadius * std::cos(v);

            // 圆环位于 YZ 平面，法线为 +X。
            return {tubeRadius * std::sin(v), radius * std::cos(u), radius * std::sin(u)};
        }

        void AppendRing(Positions& positions)//添加圆环
        {
            for(uint32_t i = 0; i < kRingSegments; ++i)
            {
                const float u0 = glm::two_pi<float>() * float(i) / float(kRingSegments);
                const float u1 = glm::two_pi<float>() * float(i + 1) / float(kRingSegments);

                for (uint32_t j = 0; j < kTubeSegments; ++j)
                {
                    const float v0 = glm::two_pi<float>() * float(j) / float(kTubeSegments);
                    const float v1 = glm::two_pi<float>() * float(j + 1) / float(kTubeSegments);

                    AddQuad(positions, TorusPoint(u0, v0), TorusPoint(u1, v0), TorusPoint(u1, v1), TorusPoint(u0, v1));
                }
            }
        }

        bool IsFinite(const glm::dvec3& value)//检查向量是否有限
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        }
       
        bool IsFinite(const glm::dvec4& value)//
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) && std::isfinite(value.w);
        }

        //检查矩阵是否有限
        bool IsFinite(const glm::mat4& value)
        {
            for (int column = 0; column < 4; ++column)
            {
                for (int row = 0; row < 4; ++row)
                {
                    if (!std::isfinite(value[column][row]))
                        return false;
                }
            }

            return true;
        }
    }

    XJTransformGizmoOperation GetGizmoOperation(XJTransformGizmoHandle handle)
    {
        switch (handle)
        {
            case XJTransformGizmoHandle::MoveX:
            case XJTransformGizmoHandle::MoveY:
            case XJTransformGizmoHandle::MoveZ:
                return XJTransformGizmoOperation::Move;

            case XJTransformGizmoHandle::RotateX:
            case XJTransformGizmoHandle::RotateY:
            case XJTransformGizmoHandle::RotateZ:
                return XJTransformGizmoOperation::Rotate;

            case XJTransformGizmoHandle::ScaleX:
            case XJTransformGizmoHandle::ScaleY:
            case XJTransformGizmoHandle::ScaleZ:
                return XJTransformGizmoOperation::Scale;

            default:
                return XJTransformGizmoOperation::None;
        }
    }

    XJTransformGizmoAxis GetGizmoAxis(XJTransformGizmoHandle handle)
    {
        switch (handle)
        {
            case XJTransformGizmoHandle::MoveY:
            case XJTransformGizmoHandle::RotateY:
            case XJTransformGizmoHandle::ScaleY:
                return XJTransformGizmoAxis::Y;

            case XJTransformGizmoHandle::MoveZ:
            case XJTransformGizmoHandle::RotateZ:
            case XJTransformGizmoHandle::ScaleZ:
                return XJTransformGizmoAxis::Z;

            default:
                // None 也回退到 X。
                // 使用前应先判断操作是否为 None。
                return XJTransformGizmoAxis::X;
        }
    }

    glm::vec4 GetGizmoAxisColor(XJTransformGizmoAxis axis)
    {
        switch (axis)
        {
            case XJTransformGizmoAxis::X:
                return {0.95f, 0.22f, 0.18f, 1.0f};

            case XJTransformGizmoAxis::Y:
                return {0.25f, 0.90f, 0.30f, 1.0f};

            case XJTransformGizmoAxis::Z:
                return {0.22f, 0.48f, 1.00f, 1.0f};
        }

        return glm::vec4(1.0f);
    }

    std::vector<XJTransformGizmoMeshData> BuildTransformGizmoGeometry()
    {
        // 生成变换控件的几何体数据。
        using Handle = XJTransformGizmoHandle;
        // using Axis = XJTransformGizmoAxis;
        using Metrics = XJTransformGizmoMetrics;

        std::vector<XJTransformGizmoMeshData> result;
        result.reserve(9);

        const auto append = [&](Handle handle, Positions positions)
        {
            OrientPositions(positions, GetGizmoAxis(handle));

            XJTransformGizmoMeshData part;
            part.Handle = handle;
            part.Positions = std::move(positions);

            result.push_back(std::move(part));
        };

        // 先画圆环，再画箭头，最后画缩放方块。
        // 当前使用 Overlay 绘制，后画的部件会覆盖前面的部件。
        for (const Handle handle : {Handle::RotateX, Handle::RotateY, Handle::RotateZ})
        {
            Positions positions;
            AppendRing(positions);

            append(handle, std::move(positions));
        }

        for (const Handle handle : {Handle::MoveX, Handle::MoveY, Handle::MoveZ})
        {
            Positions positions;

            AppendCylinder(
                positions,
                Metrics::ArrowStart,
                Metrics::ArrowShaftEnd,
                Metrics::ArrowShaftRadius);

            AppendCone(
                positions,
                Metrics::ArrowShaftEnd,
                Metrics::ArrowEnd,
                Metrics::ArrowHeadRadius);

            append(handle, std::move(positions));
        }

        for (const Handle handle : {Handle::ScaleX, Handle::ScaleY, Handle::ScaleZ})
        {
            Positions positions;

            AppendCylinder(
                positions,
                Metrics::ScaleStemStart,
                Metrics::ScaleStemEnd,
                Metrics::ScaleStemRadius);

            AppendBox(
                positions,
                Metrics::ScaleBoxCenter,
                Metrics::ScaleBoxHalfSize);

            append(handle, std::move(positions));
        }

        return result;
    }

    XJTransformGizmoFrame BuildTransformGizmoFrame(//构建变换控件帧
        const glm::dvec3& worldPosition,
        const glm::dquat& orientation,
        const glm::mat4& view,
        const glm::mat4& projection,
        uint32_t viewportWidth,
        uint32_t viewportHeight)
    {
        XJTransformGizmoFrame result;

        if (viewportWidth == 0 || viewportHeight == 0 || !IsFinite(worldPosition) || !IsFinite(view) || !IsFinite(projection))
            return result;
        

        // 当前渲染仍使用 float 世界坐标。
        constexpr double maximumCoordinate = 1.0e6;

        if (std::abs(worldPosition.x) > maximumCoordinate || std::abs(worldPosition.y) > maximumCoordinate || std::abs(worldPosition.z) > maximumCoordinate)
            return result;
        

        const double quaternionLength =  glm::length(orientation);

        if (!std::isfinite(quaternionLength) || quaternionLength < 1.0e-12)
            return result;

        // 旋转四元数必须归一化。
        const glm::dquat rotation = orientation / quaternionLength;
        const glm::dmat4 vp = glm::dmat4(projection) * glm::dmat4(view);
        const double determinant = glm::determinant(vp);

        if (!std::isfinite(determinant) || std::abs(determinant) < 1.0e-18)
            return result;

        const glm::dvec4 clip = vp * glm::dvec4(worldPosition, 1.0);

        if (!IsFinite(clip) || clip.w <= 1.0e-8)
            return result;

        const glm::dvec3 ndc = glm::dvec3(clip) / clip.w;

        // Vulkan 深度范围。
        if (ndc.z < 0.0 || ndc.z > 1.0)
            return result;

        const glm::dmat4 inverseVP = glm::inverse(vp);

        // 在相同深度上，反投影相差一个屏幕像素的两点。
        // 由此得到当前位置的“世界单位/像素”。
        const glm::dvec4 firstH = inverseVP * glm::dvec4(ndc, 1.0);
        const glm::dvec4 secondH =
            inverseVP *
            glm::dvec4(
                ndc.x,
                ndc.y + 2.0 / double(viewportHeight),
                ndc.z,
                1.0);

        if (!IsFinite(firstH) || !IsFinite(secondH) || std::abs(firstH.w) < 1.0e-12 || std::abs(secondH.w) < 1.0e-12)
            return result;

        // 反投影后，得到的两个点在世界空间中相差一个屏幕像素。
        const glm::dvec3 first = glm::dvec3(firstH) / firstH.w;
        const glm::dvec3 second = glm::dvec3(secondH) / secondH.w;
        const double worldUnitsPerPixel = glm::length(second - first);
        const double scale =
            worldUnitsPerPixel *
            double(XJTransformGizmoMetrics::ScreenRadiusPixels) /
            double(XJTransformGizmoMetrics::OuterExtent);

        if (!std::isfinite(scale) || scale < 1.0e-8 || scale > 1.0e5)
            return result;

        const glm::dmat4 model =
            glm::translate(glm::dmat4(1.0), worldPosition) *
            glm::mat4_cast(rotation) *
            glm::scale(
                glm::dmat4(1.0),
                glm::dvec3(scale));

        const glm::mat4 floatModel(model);

        if (!IsFinite(floatModel))
            return result;

        result.Valid = true;
        result.Model = floatModel;
        result.WorldScale = float(scale);

        return result;
    }
}