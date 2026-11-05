#include "Tools/XJTransformGizmoInteractionMath.h"

#include <array>
#include <cmath>

namespace XJ
{
    namespace
    {
        const std::vector<XJTransformGizmoMeshData>& CachedGeometry()
        {
            static const auto geometry = BuildTransformGizmoGeometry();
            return geometry;
        }

        bool Finite(const glm::dvec3& value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
        }

        bool Finite(const glm::dvec4& value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) && std::isfinite(value.w);
        }

        bool Invert(const glm::dmat4& matrix, glm::dmat4& inverse)
        {
            for (int column = 0; column < 4; ++column)
            {
                if (!Finite(matrix[column]))
                    return false;
            }

            const double determinant = glm::determinant(matrix);

            if (!std::isfinite(determinant) || std::abs(determinant) < 1.0e-18)
            {
                return false;
            }

            inverse = glm::inverse(matrix);

            for (int column = 0; column < 4; ++column)
            {
                if (!Finite(inverse[column]))
                    return false;
            }

            return true;
        }

        bool IntersectTriangle(
            const glm::dvec3& origin,
            const glm::dvec3& direction,
            const glm::dvec3& a,
            const glm::dvec3& b,
            const glm::dvec3& c,
            double maximumDistance)//射线与三角形相交
        {
            // Möller–Trumbore intersection algorithm
            const glm::dvec3 edge1 = b-a;
            const glm::dvec3 edge2 = c-a;
 
            const glm::dvec3 p = glm::cross(direction, edge2);// 辅助向量
            const double determinant = glm::dot(edge1, p);// 判别式/分母
             // 双面拾取，与 Gizmo 的双面绘制一致。
            if(std::abs(determinant) < 1.0e-12)//判断是否平行 向量 ，平行就不相交
                return false;

            const double inverseDeterminant = 1.0 / determinant;

            const glm::dvec3 offset = origin - a;
            const double u = glm::dot(offset, p) * inverseDeterminant;

            if(u<0.0 ||u>1.0)
                return false;

            const glm::dvec3 q = glm::cross(offset, edge1);
            const double v = glm::dot(direction, q) * inverseDeterminant;
            if (v < 0.0 || u + v > 1.0)
                return false;
        
            const double distance = glm::dot(edge2, q) * inverseDeterminant;

            return std::isfinite(distance) && distance >= 0.0 && distance <= maximumDistance;

        }

        bool PlaneParameter(
            const XJGizmoMoveConstraint& constraint,
            const XJGizmoRay& ray,
            double& parameter)//求鼠标射线与一个"约束平面"的交点 然后把该交点投影到约束轴 Axis 上，得到交点沿轴方向的有符号坐标 parameter
        { 
            const double denominator = glm::dot(ray.Direction, constraint.PlaneNormal);
            
            if(std::abs(denominator) < 1.0e-6)
                return false;

            const double distance = glm::dot(constraint.StartPosition - ray.Origin, constraint.PlaneNormal) / denominator;

            if(!std::isfinite(distance) || distance < 0.0f)
                return false;

            const glm::dvec3 hit = ray.Origin + ray.Direction * distance;

            parameter = glm::dot(hit - constraint.StartPosition, constraint.Axis);

            return std::isfinite(parameter);
        }
    }
    bool BuildGizmoRay(
        const glm::dmat4& inverseViewProjection,
        const glm::dvec2& uv,
        XJGizmoRay& ray)
    {
        if (!std::isfinite(uv.x) || !std::isfinite(uv.y))
            return false;
        
         // 对应当前正高度 Vulkan viewport 和未翻转的 ImGui 图像 UV。
        const glm::dvec2 ndc = uv * 2.0 - glm::dvec2(1.0);
        const glm::dvec4 nearH = inverseViewProjection * glm::dvec4(ndc, 0.0, 1.0);

        const glm::dvec4 farH = inverseViewProjection * glm::dvec4(ndc, 1.0, 1.0);

        if (!Finite(nearH) || !Finite(farH) || std::abs(nearH.w) < 1.0e-12 || std::abs(farH.w) < 1.0e-12)
            return false;

        const glm::dvec3 nearPosition =  glm::dvec3(nearH) / nearH.w;
        const glm::dvec3 farPosition = glm::dvec3(farH) / farH.w;
        const glm::dvec3 difference = farPosition - nearPosition;

        const double length = glm::length(difference);

        if (!Finite(nearPosition) ||  !Finite(farPosition) ||  !std::isfinite(length) ||  length < 1.0e-12)
            return false;
        
        ray.Origin = nearPosition;
        ray.Direction = difference / length;
        ray.MaximumDistance = length;

        return true;   
    }


    XJTransformGizmoHandle PickTransformGizmo(
        const XJGizmoRenderedFrame& frame,
        const glm::dvec2& uv,
        const glm::dvec2& imageSize)
    {
        using Handle = XJTransformGizmoHandle;

        if (!frame.Valid || !std::isfinite(imageSize.x) || !std::isfinite(imageSize.y) || imageSize.x <= 1.0 || imageSize.y <= 1.0)
            return Handle::None;
        

        glm::dmat4 inverseVP;
        glm::dmat4 inverseModel;

        if (!Invert(glm::dmat4(frame.Projection) * glm::dmat4(frame.View), inverseVP) ||
            !Invert(glm::dmat4(frame.Model), inverseModel))
        {
            return Handle::None;
        }

        // 中心精确命中优先。
        // 没命中时，再用周围 4 像素采样增加细杆可点击性。
        constexpr std::array<glm::dvec2, 9> offsets = {
            glm::dvec2(0.0, 0.0),      // 中心
            glm::dvec2(4.0, 0.0),      // 右 4 像素
            glm::dvec2(-4.0, 0.0),     // 左 4 像素
            glm::dvec2(0.0, 4.0),      // 下 4 像素
            glm::dvec2(0.0, -4.0),     // 上 4 像素
            glm::dvec2(3.0, 3.0),      // 右下 3 像素
            glm::dvec2(-3.0, 3.0),     // 左下 3 像素
            glm::dvec2(3.0, -3.0),     // 右上 3 像素
            glm::dvec2(-3.0, -3.0)     // 左上 3 像素
        };

        const auto& parts = CachedGeometry();

        for (const auto& pixelOffset : offsets)
        {
            const glm::dvec2 sampleUV = uv + pixelOffset / imageSize;

            // 不从图像外部采样被裁掉的手柄。
            if (sampleUV.x < 0.0 || sampleUV.x > 1.0 ||
                sampleUV.y < 0.0 || sampleUV.y > 1.0)
            {
                continue;
            }

            XJGizmoRay ray;

            if (!BuildGizmoRay(inverseVP, sampleUV, ray))
                continue;

            const glm::dvec3 localOrigin =  glm::dvec3(inverseModel * glm::dvec4(ray.Origin, 1.0));

            // 不重新 normalize。
            // 保持 distance 参数仍然对应世界射线距离。
            const glm::dvec3 localDirection =
                glm::dvec3(
                    inverseModel *
                    glm::dvec4(ray.Direction, 0.0));

            // Gizmo 不做深度测试，后画部件覆盖先画部件。
            // 因此拾取按绘制顺序倒序检查，而不是按场景深度排序。
            for (auto part = parts.rbegin();
                 part != parts.rend();
                 ++part)
            {
                const auto& positions = part->Positions;

                for (std::size_t i = 0;
                     i + 2 < positions.size();
                     i += 3)
                {
                    if (IntersectTriangle(
                            localOrigin,
                            localDirection,
                            glm::dvec3(positions[i]),
                            glm::dvec3(positions[i + 1]),
                            glm::dvec3(positions[i + 2]),
                            ray.MaximumDistance))
                    {
                        return part->Handle;
                    }
                }
            }
        }

        return Handle::None;
    }

    bool BeginGizmoMoveConstraint(
        const XJGizmoRenderedFrame& frame,
        XJTransformGizmoHandle handle,
        const glm::dvec2& uv,
        XJGizmoMoveConstraint& constraint)
    {
        if (!frame.Valid ||
            GetGizmoOperation(handle) !=
                XJTransformGizmoOperation::Move)
        {
            return false;
        }

        XJGizmoMoveConstraint candidate;

        if (!Invert(
                glm::dmat4(frame.Projection) *
                glm::dmat4(frame.View),
                candidate.InverseViewProjection))
        {
            return false;
        }

        glm::dvec3 axis(0.0);

        switch (GetGizmoAxis(handle))
        {
            case XJTransformGizmoAxis::X:
                axis.x = 1.0;
                break;

            case XJTransformGizmoAxis::Y:
                axis.y = 1.0;
                break;

            case XJTransformGizmoAxis::Z:
                axis.z = 1.0;
                break;
        }

        const double rotationLength =
            glm::length(frame.Orientation);

        if (!std::isfinite(rotationLength) ||
            rotationLength < 1.0e-12)
        {
            return false;
        }

        candidate.Axis =
            (frame.Orientation / rotationLength) * axis;

        candidate.StartPosition =
            frame.WorldPosition;

        XJGizmoRay ray;

        if (!BuildGizmoRay(
                candidate.InverseViewProjection,
                uv,
                ray))
        {
            return false;
        }

        // 构造包含移动轴、尽量朝向相机的拖动平面。
        const glm::dvec3 normal =
            ray.Direction -
            candidate.Axis *
            glm::dot(ray.Direction, candidate.Axis);

        const double normalLength =
            glm::length(normal);

        // 轴几乎正对相机时，单轴移动条件很差。
        // 本版明确拒绝，而不是让物体瞬移。
        if (!std::isfinite(normalLength) || normalLength < 0.15)
             return false;
        

        candidate.PlaneNormal = normal / normalLength;

        if (!PlaneParameter(candidate, ray, candidate.StartParameter))
            return false;

        constraint = candidate;
        return true;
    }

    bool CalculateGizmoMoveTarget(const XJGizmoMoveConstraint& constraint, const glm::dvec2& uv, glm::dvec3& target)
    {
        XJGizmoRay ray;

        if (!BuildGizmoRay(constraint.InverseViewProjection, uv, ray))
            return false;
        

        double parameter = 0.0;

        if (!PlaneParameter(constraint, ray, parameter))
            return false;

        const glm::dvec3 candidate =
            constraint.StartPosition +
            constraint.Axis *
            (parameter - constraint.StartParameter);

        if (!Finite(candidate))
            return false;

        target = candidate;
        return true;
    }
}
