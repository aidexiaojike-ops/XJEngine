#ifndef XJ_JOLT_SHAPE_FACTORY_H
#define XJ_JOLT_SHAPE_FACTORY_H

#include "Physics/XJPhysicsTypes.h"

#include <Jolt/Jolt.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>

#include <glm/vec3.hpp>

//物理用到的基础模型
namespace XJ
{
    namespace XJJoltShapeFactory
    {
        // 单形状创建。失败返回空 Ref 并写 spdlog::error，不抛异常。
        JPH::ShapeRefC CreateBox(const glm::dvec3& halfExtent);
        JPH::ShapeRefC CreateSphere(double radius);

        // halfHeight 是圆柱段半高（不含半球）。
        JPH::ShapeRefC CreateCapsule(double halfHeight, double radius);

        JPH::ShapeRefC CreatePlane(const glm::dvec3& normal, const glm::dvec3& pointOnPlane);

        // 把引擎的 Body 描述构建成一个 Jolt Shape。
        //   0 个形状        -> 返回 nullptr
        //   1 个且无局部变换 -> 直接返回该形状
        //   其他            -> 返回 StaticCompoundShape
        // 缩放会在这里烘焙进形状尺寸；非均匀缩放球/胶囊会输出警告。
        JPH::ShapeRefC BuildShape(const XJPhysicsBodyDesc& desc);
    }
}

#endif