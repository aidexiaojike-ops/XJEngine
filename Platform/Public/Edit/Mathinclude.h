#ifndef MATH_INCLUDE_H
#define MATH_INCLUDE_H



#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE   // Vulkan 必开
#define GLM_ENABLE_EXPERIMENTAL       // 如果你真要用 gtx

#include <glm/glm.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/dual_quaternion.hpp>
#include <glm/gtc/random.hpp>


// ====================================================================
// GLM 对齐策略（GPU 常量布局相关，改动前务必阅读）:
//
//   本引擎所有 UBO / PushConstant 结构体都是手工对齐并配套 static_assert
//   （FrameUbo / TextureParam / ModelPC / GlobalUbo / InstanceUbo 等），
//   其内存布局以 GLSL std140 / std430 规则为唯一标准。
//
//   这里刻意【不】启用 GLM_FORCE_DEFAULT_ALIGNED_GENTYPES / GLM_FORCE_ALIGNED_GENTYPES：
//     - 保证 sizeof(glm::vec3) == 12，便于按 std140 的 vec3 有效载荷写入 12 字节；
//     - 保留 GLM 的 constexpr 支持；
//     - 避免 GLM 内部 16 字节对齐类型悄悄改变各 GPU 结构体布局。
//
//   注意：不要用默认 glm::mat3 直接镜像 shader 里的 mat3。GPU 侧 mat3 有列 stride，
//   默认 glm::mat3 是 36 字节紧凑布局；需要上传矩阵常量时优先使用 mat4 或显式 padding。
//
//   若未来决定开启对齐 gentypes，必须：
//     1) 同步更新所有 GPU 常量结构的 static_assert 与手工 alignas；
//     2) 复核 XJMaterialParameterBlock::SetVec3（已固定写 12 字节，可不改，但需回归验证）。
//   下方静态断言把“未启用对齐 gentypes”这一决策固定为编译期事实。
// ====================================================================
static_assert(sizeof(glm::vec3) == 12,
    "XJEngine 依赖 sizeof(glm::vec3)==12：请保持 GLM 对齐 gentypes 关闭（见 Mathinclude.h 策略说明）");

#endif
