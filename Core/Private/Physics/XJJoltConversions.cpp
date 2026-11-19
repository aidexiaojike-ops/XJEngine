#include "Physics/XJJoltConversions.h"

namespace XJ
{
    // JPH::Vec3 永远是 float（无 JPH_DOUBLE_PRECISION 时）。
    JPH::Vec3 ToJoltVec3(const glm::vec3& value)
    {
        return JPH::Vec3(value.x, value.y, value.z);
    }

    JPH::Vec3 ToJoltVec3(const glm::dvec3& value)
    {
        return JPH::Vec3(float(value.x), float(value.y), float(value.z));
    }

    // JPH::RVec3 的类型随 JPH_DOUBLE_PRECISION 变化：
    //   关闭时 = JPH::Vec3(float)，开启时 = JPH::DVec3(double)。
    // 所以必须显式转到 JPH::Real，不能写死 float 或 double。
    JPH::RVec3 ToJoltRVec3(const glm::dvec3& value)
    {
        return JPH::RVec3(JPH::Real(value.x), JPH::Real(value.y), JPH::Real(value.z));
    }

    // Jolt Quat 构造顺序是 (x, y, z, w)，GLM 是 (w, x, y, z)。
    // 顺序写反不会报错，但物体会乱转，这是最容易踩的坑。
    JPH::Quat ToJoltQuat(const glm::dquat& value)
    {
        return JPH::Quat(float(value.x), float(value.y), float(value.z), float(value.w));
    }

    glm::vec3 ToGlmVec3(JPH::Vec3Arg value)
    {
        return glm::vec3(value.GetX(), value.GetY(), value.GetZ());
    }

    glm::dvec3 ToGlmDVec3(JPH::RVec3Arg value)
    {
        return glm::dvec3(double(value.GetX()), double(value.GetY()), double(value.GetZ()));
    }

    glm::dquat ToGlmDQuat(JPH::QuatArg value)
    {
        return glm::dquat(double(value.GetW()), double(value.GetX()),
            double(value.GetY()), double(value.GetZ()));
    }
}