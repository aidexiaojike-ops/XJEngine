#ifndef XJ_PHYSICS_TYPES_H
#define XJ_PHYSICS_TYPES_H

#include <cstdint>

#include <glm/vec3.hpp>
#include <vector>
#include <glm/gtc/quaternion.hpp>

//引擎类型、结果码、句柄，
namespace XJ
{
    // 运动类型。与 Jolt 的 EMotionType 对应，但这是引擎自己的枚举，
    // 公开头文件不引用任何 Jolt 声明。
    enum class XJMotionType
    {
        Static = 0,
        Kinematic,
        Dynamic
    };

    // 统一结果码。不用 bool，因为后续查询和创建需要区分失败原因。
    enum class XJPhysicsResult
    {
        Success = 0,
        NotInitialized,
        AlreadyInitialized,
        InvalidArgument,
        ShapeCreationFailed,
        BodyCreationFailed,
        RuntimeUnavailable
    };

    enum class XJColliderShapeType
    {
        Box = 0,
        Sphere,
        Capsule
    };
    // 单个子形状的物理描述。
    // 这是"物理层"的描述，不含欧拉角——旋转在这里已经是四元数，
    // 欧拉角到四元数的转换由 ECS 层（XJPhysicsSystem）负责。
    struct XJPhysicsShapeDesc
    {
        XJColliderShapeType Type = XJColliderShapeType::Box;

        glm::dvec3 LocalPosition{ 0.0 };
        glm::dquat LocalRotation{ 1.0, 0.0, 0.0, 0.0 };

        glm::dvec3 HalfExtent{ 0.5 };   // Box：半长（沿三个轴）
        double Radius = 0.5;            // Sphere / Capsule
        double HalfHeight = 0.5;        // Capsule：圆柱段半高，不含两端半球
    };

    // 一个刚体的完整创建描述。
    // 一个实体 = 一个 Body；多个 Shape 会合成一个复合形状。
    struct XJPhysicsBodyDesc
    {
        std::vector<XJPhysicsShapeDesc> Shapes;

        glm::dvec3 Position{ 0.0 };
        glm::dquat Rotation{ 1.0, 0.0, 0.0, 0.0 };

        // 创建时烘焙进形状尺寸，运行期不再变化。
        glm::dvec3 Scale{ 1.0 };

        XJMotionType MotionType = XJMotionType::Static;

        double Mass = 1.0;              // 仅 Dynamic 使用

        // Body 级材质（阶段 2 决定：不放 Collider）
        float Friction = 0.2f;
        float Restitution = 0.0f;

        float LinearDamping = 0.05f;
        float AngularDamping = 0.05f;
        float GravityFactor = 1.0f;

        bool AllowSleep = true;
        bool ContinuousCollision = false;  // true -> EMotionQuality::LinearCast
        bool IsSensor = false;

        // 轴向锁。true 表示该轴被锁定。
        bool LockLinearX = false;
        bool LockLinearY = false;
        bool LockLinearZ = false;
        bool LockAngularX = false;
        bool LockAngularY = false;
        bool LockAngularZ = false;
    };

    inline const char* XJPhysicsResultToString(XJPhysicsResult result)
    {
        switch(result)
        {
            case XJPhysicsResult::Success:           return "Success";
            case XJPhysicsResult::NotInitialized:    return "NotInitialized";
            case XJPhysicsResult::AlreadyInitialized:return "AlreadyInitialized";
            case XJPhysicsResult::InvalidArgument:   return "InvalidArgument";
            case XJPhysicsResult::ShapeCreationFailed:return "ShapeCreationFailed";
            case XJPhysicsResult::BodyCreationFailed:return "BodyCreationFailed";
            case XJPhysicsResult::RuntimeUnavailable:return "RuntimeUnavailable";
            default:                                 return "Unknown";
        }
    }

    // 刚体句柄：不透明值，只有物理世界能解释。
    // 内部就是 Jolt BodyID 的原始 32 位（索引 + 序列号）。
    // 序列号让实体销毁后的旧句柄被 Jolt 自动判为失效，
    // 避免"删了又建、旧句柄误伤新刚体"。
    struct XJPhysicsBodyId
    {
        // 0xFFFFFFFF 与 Jolt 的 BodyID::cInvalidBodyID 一致（已核对 5.7 源码）。
        static constexpr uint32_t kInvalid = 0xFFFFFFFFu;
        uint32_t Value = kInvalid;

        bool IsValid() const{return Value != kInvalid;}
    };

    struct XJPhysicsWorldConfig//物理世界设置
    {
        glm::dvec3 Gravity{ 0.0, -9.81, 0.0 };

        uint32_t MaxBodies = 1024;
        uint32_t MaxBodyPairs = 4096;
        uint32_t MaxContactConstraints = 1024;

         // Jolt 每帧从这里分配临时内存，不够会断言。
        uint32_t TempAllocatorSize = 10u * 1024u * 1024u;

        // 0 = 单线程 JobSystem（确定性好、好调试）；
        // > 0 = 线程池，指定工作线程数。
        uint32_t WorkerThreadCount = 0;
    };
    

}
#endif