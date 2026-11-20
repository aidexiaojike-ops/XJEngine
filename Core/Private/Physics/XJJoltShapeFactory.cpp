#include "Physics/XJJoltShapeFactory.h"

#include "Physics/XJJoltConversions.h"

#include <Jolt/Geometry/Plane.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/PlaneShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>

#include <algorithm>
#include <vector>


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

        JPH::ShapeRefC CreateCapsule(double halfHeight, double radius)
        {
            // 注意参数顺序：Jolt 是 (halfHeightOfCylinder, radius)。
            // 第一个参数只算圆柱段，不含半球。
            if (!IsFinitePositive(radius) || !std::isfinite(halfHeight) || halfHeight < 0.0)
            {
                spdlog::error("CreateCapsule rejected invalid half height or radius.");
                return nullptr;
            }

            JPH::CapsuleShapeSettings settings{ float(halfHeight), float(radius) };
            return FinishCreation(settings.Create(), "Capsule");
        }

                // 局部变换是否为单位（用于决定能否省掉复合形状/装饰形状）
        bool IsNearZero(const glm::dvec3& v)
        {
            constexpr double kEpsilon = 1.0e-6;
            return std::abs(v.x) < kEpsilon && std::abs(v.y) < kEpsilon &&
                std::abs(v.z) < kEpsilon;
        }

        bool IsNearIdentity(const glm::dquat& q)
        {
            return std::abs(q.w) > 1.0 - 1.0e-6;
        }

        // 把世界缩放烘焙进单个形状的尺寸。
        // Box 支持非均匀；Sphere/Capsule 非均匀时取最大分量并警告，
        // 因为它们的半径只有一个值，无法表达椭球。
        JPH::ShapeRefC CreateScaledShape(const XJPhysicsShapeDesc& shape, const glm::dvec3& scale)
        {
            const double ax = std::abs(scale.x);
            const double ay = std::abs(scale.y);
            const double az = std::abs(scale.z);

            switch (shape.Type)
            {
                case XJColliderShapeType::Box:
                {
                    // Box 可以精确表达非均匀缩放
                    const glm::dvec3 halfExtent{ shape.HalfExtent.x * ax,
                        shape.HalfExtent.y * ay, shape.HalfExtent.z * az };
                    return CreateBox(halfExtent);
                }

                case XJColliderShapeType::Sphere:
                {
                    const double maxScale = std::max(ax, std::max(ay, az));
                    const double minScale = std::min(ax, std::min(ay, az));
                    if (maxScale - minScale > 1.0e-4)
                    {
                        spdlog::warn(
                            "Sphere collider under non-uniform scale; using max scale {}. "
                            "Result is a sphere, not an ellipsoid.", maxScale);
                    }
                    return CreateSphere(shape.Radius * maxScale);
                }

                case XJColliderShapeType::Capsule:
                {
                    const double radialScale = std::max(ax, az);
                    if (std::abs(ax - az) > 1.0e-4)
                    {
                        spdlog::warn(
                            "Capsule collider under non-uniform X/Z scale; using max {}. "
                            "Result is a capsule, not an elliptical one.", radialScale);
                    }
                    return CreateCapsule(shape.HalfHeight * ay, shape.Radius * radialScale);
                }

                default:
                    spdlog::error("CreateScaledShape received unknown shape type.");
                    return nullptr;
            }
        }

        JPH::ShapeRefC BuildShape(const XJPhysicsBodyDesc& desc)
        {
            if (desc.Shapes.empty())
            {
                spdlog::error("BuildShape rejected empty shape list.");
                return nullptr;
            }

            const double ax = std::abs(desc.Scale.x);
            const double ay = std::abs(desc.Scale.y);
            const double az = std::abs(desc.Scale.z);
            if (!std::isfinite(ax) || !std::isfinite(ay) || !std::isfinite(az) ||
                ax <= 0.0 || ay <= 0.0 || az <= 0.0)
            {
                spdlog::error("BuildShape rejected zero or non-finite scale.");
                return nullptr;
            }

            // 先为每个子形状单独建 Shape（缩放已经烘焙进尺寸）
            std::vector<JPH::ShapeRefC> localShapes;
            localShapes.reserve(desc.Shapes.size());
            for (const XJPhysicsShapeDesc& shape : desc.Shapes)
            {
                JPH::ShapeRefC local = CreateScaledShape(shape, desc.Scale);
                if (!local)
                    return nullptr;
                localShapes.push_back(std::move(local));
            }

            // 只有一个形状且局部变换是单位 -> 直接返回，省掉一层包装
            if (localShapes.size() == 1 &&
                IsNearZero(desc.Shapes.front().LocalPosition) &&
                IsNearIdentity(desc.Shapes.front().LocalRotation))
            {
                return localShapes.front();
            }

            // 多形状或带局部偏移 -> 合成 StaticCompoundShape。
            // 阶段 2 没有运行期改子形状的需求，所以用 StaticCompound
            // （比 MutableCompound 更省内存）。
            JPH::StaticCompoundShapeSettings compound;
            for (size_t index = 0; index < desc.Shapes.size(); ++index)
            {
                const XJPhysicsShapeDesc& shape = desc.Shapes[index];
                compound.AddShape(ToJoltVec3(shape.LocalPosition),
                    ToJoltQuat(shape.LocalRotation),
                    localShapes[index]);
            }

            return FinishCreation(compound.Create(), "StaticCompound");
        }

    }
}