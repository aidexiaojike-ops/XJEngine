#ifndef XJ_JOLT_CONVERSIONS_H
#define XJ_JOLT_CONVERSIONS_H

#include <Jolt/Jolt.h>

#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>
//类型转换适配层（防腐层） 定义了一组在 GLM 数学库和 Jolt 物理库之间双向转换的函数，并立下一条硬性约定：
namespace XJ
{
    // 全项目唯一的 GLM <-> Jolt 转换点。
    // 其他文件禁止直接构造 JPH::Vec3 / JPH::Quat，
    // 否则将来切双精度或改坐标系时要满仓库找。
    JPH::Vec3  ToJoltVec3(const glm::vec3& value);
    JPH::Vec3  ToJoltVec3(const glm::dvec3& value);
    JPH::RVec3 ToJoltRVec3(const glm::dvec3& value);
    JPH::Quat  ToJoltQuat(const glm::dquat& value);

    glm::vec3  ToGlmVec3(JPH::Vec3Arg value);
    glm::dvec3 ToGlmDVec3(JPH::RVec3Arg value);
    glm::dquat ToGlmDQuat(JPH::QuatArg value);
}

#endif