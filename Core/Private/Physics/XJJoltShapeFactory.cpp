#include "Physics/XJJoltShapeFactory.h"

#include "Physics/XJJoltConversions.h"

#include <Jolt/Geometry/Plane.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/PlaneShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>

#include <spdlog/spdlog.h>

#include <cmath>

namespace XJ
{
    namespace XJJoltShapeFactory
    {
        namespace
        {
            bool IsFinitePositive(double value)
            {
                return std::isfinite(value) && value > 0.0;
            }

            JPH::ShapeRefC FinishCreation(const JPH::ShapeSettings::ShapeResult& result, const char* shapeName)
            {
                if(!result.IsValid())
                {
                    spdlog::error("{} shape creation failed: {}", shapeName, std::string(result.GetError().c_str()));
                    return nullptr;
                }

                return result.Get();
            }
        }

        JPH::ShapeRefC CreateBox(const glm::dvec3& halfExtent)
        {
            if(!IsFinitePositive(halfExtent.x) || !IsFinitePositive(halfExtent.y) || !IsFinitePositive(halfExtent.z))
            {
                spdlog::error("CreateBox rejected non-positive or non-finite half extent.");
                return nullptr;
            }

            JPH::BoxShapeSettings settings(ToJoltVec3(halfExtent));
            return FinishCreation(settings.Create(),"Box");
        }

        JPH::ShapeRefC CreateSphere(double radius)
        {
            if (!IsFinitePositive(radius))
            {
                spdlog::error("CreateSphere rejected non-positive or non-finite radius.");
                return nullptr;
            }

            // 注意：不能写 settings(float(radius))。
            // C++ 会把 float(radius) 解析成"参数类型为 float、名为 radius"的
            // 函数声明（most vexing parse），settings 就变成了函数而不是对象。
            // 用花括号初始化可以强制走构造函数。
            JPH::SphereShapeSettings settings{float(radius)};
            return FinishCreation(settings.Create(), "Sphere");
        }

        JPH::ShapeRefC CreatePlane(const glm::dvec3& normal, const glm::dvec3& pointOnPlane)
        {
            const double lengthSquared = glm::dot(normal, normal);
            if (!std::isfinite(lengthSquared) || lengthSquared <= 0.0)
            {
                spdlog::error("CreatePlane rejected zero or non-finite normal.");
                return nullptr;
            }

            const glm::dvec3 unitNormal = normal / std::sqrt(lengthSquared);

            // Jolt 约定：平面负半空间是实体。
            // 平面方程 dot(n, x) + constant = 0，所以 constant = -dot(n, point)。
            const double constant = -glm::dot(unitNormal, pointOnPlane);

            const JPH::Plane plane(ToJoltVec3(unitNormal), float(constant));

            JPH::PlaneShapeSettings settings(plane);
            return FinishCreation(settings.Create(), "Plane");
        }
    }
}