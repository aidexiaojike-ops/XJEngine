#ifndef XJ_JOLT_SHAPE_FACTORY_H
#define XJ_JOLT_SHAPE_FACTORY_H

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

#include <glm/vec3.hpp>
//物理用到的基础模型
namespace XJ
{
    namespace XJJoltShapeFactory
    {
        // 全部失败路径返回空 Ref 并写 spdlog::error，不抛异常。
        // 空 Ref 由调用方转成 XJPhysicsResult::ShapeCreationFailed。
        JPH::ShapeRefC CreateBox(const glm::dvec3& halfExtent);
        JPH::ShapeRefC CreateSphere(double radius);
        JPH::ShapeRefC CreatePlane(const glm::dvec3& normal, const glm::dvec3& pointOnPlane);
    }
}

#endif