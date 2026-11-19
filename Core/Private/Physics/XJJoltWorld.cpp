#include "Physics/XJPhysicsWorld.h"

#include "Physics/XJJoltConversions.h"
#include "Physics/XJJoltRuntime.h"
#include "Physics/XJJoltShapeFactory.h"

#include <Jolt/Jolt.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <spdlog/spdlog.h>

#include <cmath>
#include <memory>

namespace XJ
{
    namespace
    {
        // 阶段 1 只用两个对象层：静止和可动。
        // 阶段 5 做碰撞矩阵时，这一整段会被 XJPhysicsLayerConfig 替换。
        namespace ObjectLayers//对象层 (ObjectLayer)//它定义了“谁”和“谁”在逻辑上应该发生碰撞。
        {
            constexpr JPH::ObjectLayer NonMoving = 0;
            constexpr JPH::ObjectLayer Moving = 1;
            constexpr JPH::uint Count = 2;
        }

        namespace BroadPhaseLayerIds//宽阶段层 (BroadPhaseLayer)  物理引擎内部用于空间加速的粗粒度分类 比如BVH快速检索
        {
            constexpr JPH::BroadPhaseLayer NonMoving(0);
            constexpr JPH::BroadPhaseLayer Moving(1);
            constexpr JPH::uint Count = 2;
        }

        // 对象层 -> BroadPhase 层
        class XJBroadPhaseLayerInterface final : public JPH::BroadPhaseLayerInterface
        {
            public:
                XJBroadPhaseLayerInterface()//建立映射表
                //项目中所有 NonMoving（静态）类型的对象层，都归入 NonMoving 宽阶段层；所有 Moving（动态）类型的对象层，都归入 Moving 宽阶段层
                {
                    mObjectToBroadPhase[ObjectLayers::NonMoving] = BroadPhaseLayerIds::NonMoving;
                    mObjectToBroadPhase[ObjectLayers::Moving] = BroadPhaseLayerIds::Moving;
                }

                JPH::uint GetNumBroadPhaseLayers() const override//告知总数  多少宽阶段层
                {
                    return BroadPhaseLayerIds::Count;
                }

                JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override//执行查询
                {
                    JPH_ASSERT(inLayer < ObjectLayers::Count);
                    return mObjectToBroadPhase[inLayer];
                }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
                const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
                {
                    switch (inLayer.GetValue())
                    {
                        case 0:  return "NonMoving";
                        case 1:  return "Moving";
                        default: return "Invalid";
                    }
                }
#endif

            private:
                JPH::BroadPhaseLayer mObjectToBroadPhase[ObjectLayers::Count];
        };

        // BroadPhase 层的粗过滤
        class XJObjectVsBroadPhaseLayerFilter final : public JPH::ObjectVsBroadPhaseLayerFilter
        {
            public:
                bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
                {
                    switch (inLayer1)
                    {
                        case ObjectLayers::NonMoving:
                            // 静止物体之间不检测（地面之间不需要碰撞）
                            return inLayer2 == BroadPhaseLayerIds::Moving;

                        case ObjectLayers::Moving:
                            return true;

                        default:
                            return false;
                    }
                }
        };

        // 对象层两两过滤
        class XJObjectLayerPairFilter final : public JPH::ObjectLayerPairFilter
        {
            public:
                bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::ObjectLayer inLayer2) const override
                {
                    switch (inLayer1)
                    {
                        case ObjectLayers::NonMoving:
                            return inLayer2 == ObjectLayers::Moving;

                        case ObjectLayers::Moving:
                            return true;

                        default:
                            return false;
                    }
                }
        };

        JPH::ObjectLayer ToObjectLayer(XJMotionType motionType)//返回层
        {
            return motionType == XJMotionType::Static
                ? ObjectLayers::NonMoving
                : ObjectLayers::Moving;
        }

        JPH::EMotionType ToJoltMotionType(XJMotionType motionType)//返回状态
        {
            switch (motionType)
            {
                case XJMotionType::Static:    return JPH::EMotionType::Static;
                case XJMotionType::Kinematic: return JPH::EMotionType::Kinematic;
                case XJMotionType::Dynamic:   return JPH::EMotionType::Dynamic;
                default:                      return JPH::EMotionType::Static;
            }
        }

    }

    class XJPhysicsWorld::Impl
    {
        public:
            XJPhysicsResult Initialize(const XJPhysicsWorldConfig& config);
            void Shutdown();

            XJPhysicsResult CreateBody(const JPH::Shape* shape,
                const glm::dvec3& position, const glm::dquat& rotation,
                XJMotionType motionType, double mass, XJPhysicsBodyId& outBodyId);

            bool IsReady() const { return Initialized && System != nullptr; }
            //状态与配置字段
            XJPhysicsWorldConfig Config;//物理世界的初始化参数（重力、最大刚体数、子步数、线程数等）
            bool Initialized = false;//物理系统是否初始化成功
            bool RuntimeAcquired = false;//是否已通过 XJJoltRuntime::Acquire() 持有全局 Jolt 设施
            uint32_t BodyCount = 0;//当前创建的刚体数量，供统计/上限检查用

            // 声明顺序很重要：析构是反向的。
            // 过滤器必须比 PhysicsSystem 活得久，因为 Init 保存的是引用。
            //三个碰撞过滤器
            XJBroadPhaseLayerInterface BroadPhaseLayerInterface;//对象层 → 宽阶段层
            XJObjectVsBroadPhaseLayerFilter ObjectVsBroadPhaseLayerFilter;//对象层能否与某个宽阶段层碰撞
            XJObjectLayerPairFilter ObjectLayerPairFilter;//两个对象层之间是否碰撞
            //Jolt 核心组件   作用和生命周期
            std::unique_ptr<JPH::PhysicsSystem> System;
            std::unique_ptr<JPH::TempAllocatorImpl> TempAllocator;
            std::unique_ptr<JPH::JobSystem> JobSystem;

    };

    XJPhysicsResult XJPhysicsWorld::Impl::Initialize(const XJPhysicsWorldConfig& config)
    {
        if(Initialized)//重入检查
            return XJPhysicsResult::AlreadyInitialized;

        if(config.MaxBodies == 0|| config.MaxBodyPairs == 0|| config.MaxContactConstraints == 0 || config.TempAllocatorSize == 0)//容量配置校验
        {
            spdlog::error("XJPhysicsWorld::Initialize rejected zero-sized capacity config.");
            return XJPhysicsResult::InvalidArgument;
        }

        if(!std::isfinite(config.Gravity.x) || !std::isfinite(config.Gravity.y)||!std::isfinite(config.Gravity.z))// 重力有限性校验
        {
            spdlog::error("XJPhysicsWorld::Initialize rejected non-finite gravity.");
            return XJPhysicsResult::InvalidArgument;
        }

        Config = config;//保存配置

        if (!XJJoltRuntime::Acquire())//获取全局 Jolt 运行时
        {
            spdlog::error("XJPhysicsWorld::Initialize failed: Jolt runtime unavailable.");
            return XJPhysicsResult::RuntimeUnavailable;
        }

        RuntimeAcquired = true;

        TempAllocator = std::make_unique<JPH::TempAllocatorImpl>(Config.TempAllocatorSize);//构造临时分配器

        if (Config.WorkerThreadCount == 0)//按线程数选择作业系统
        {
            JobSystem = std::make_unique<JPH::JobSystemSingleThreaded>(
                static_cast<JPH::uint>(JPH::cMaxPhysicsJobs));
        }
        else
        {
            JobSystem = std::make_unique<JPH::JobSystemThreadPool>(
                static_cast<JPH::uint>(JPH::cMaxPhysicsJobs),
                static_cast<JPH::uint>(JPH::cMaxPhysicsBarriers),
                static_cast<int>(Config.WorkerThreadCount));
        }

        System = std::make_unique<JPH::PhysicsSystem>();//构造物理系统

        // inNumBodyMutexes = 0 表示自动选择，见 PhysicsSystem.h:53。
        System->Init(Config.MaxBodies, 0, Config.MaxBodyPairs,
            Config.MaxContactConstraints, BroadPhaseLayerInterface,
            ObjectVsBroadPhaseLayerFilter, ObjectLayerPairFilter);

        System->SetGravity(ToJoltVec3(Config.Gravity));

        Initialized = true;
        spdlog::info("XJPhysicsWorld initialized: maxBodies={}, workerThreads={}",
            Config.MaxBodies, Config.WorkerThreadCount);

        return XJPhysicsResult::Success;
    }

    void XJPhysicsWorld::Impl::Shutdown()
    {
        // 顺序：先销毁引擎侧拥有的 Jolt 对象，再释放全局运行时。
        System.reset();
        TempAllocator.reset();
        JobSystem.reset();

        BodyCount = 0;
        Initialized = false;

        if (RuntimeAcquired)
        {
            XJJoltRuntime::Release();
            RuntimeAcquired = false;
        }
    }

    XJPhysicsResult XJPhysicsWorld::Impl::CreateBody(const JPH::Shape* shape,
        const glm::dvec3& position, const glm::dquat& rotation,
        XJMotionType motionType, double mass, XJPhysicsBodyId& outBodyId)//创建"刚体"  形状 位置 选择 状态 时有效  输出刚体
    {
        if (!IsReady())
            return XJPhysicsResult::NotInitialized;

        if (shape == nullptr)
            return XJPhysicsResult::ShapeCreationFailed;

        if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
        {
            spdlog::error("CreateBody rejected non-finite position.");
            return XJPhysicsResult::InvalidArgument;
        }

        if (motionType == XJMotionType::Dynamic && !(std::isfinite(mass) && mass > 0.0))
        {
            spdlog::error("CreateBody rejected non-positive dynamic mass: {}", mass);
            return XJPhysicsResult::InvalidArgument;
        }

        JPH::BodyCreationSettings settings(shape, ToJoltRVec3(position),
            ToJoltQuat(rotation), ToJoltMotionType(motionType),
            ToObjectLayer(motionType));// 构造 BodyCreationSettings

        if (motionType == XJMotionType::Dynamic)
        {
            // CalculateInertia = 用给定质量 + 按形状密度算出的惯性张量
            settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
            settings.mMassPropertiesOverride.mMass = float(mass);
        }

        JPH::BodyID bodyId = System->GetBodyInterface().CreateAndAddBody(
            settings, JPH::EActivation::Activate);

        if (bodyId.IsInvalid())
        {
            spdlog::error("CreateBody failed: Jolt returned an invalid body id.");
            return XJPhysicsResult::BodyCreationFailed;
        }

        outBodyId.Value = bodyId.GetIndexAndSequenceNumber();
        ++BodyCount;
        return XJPhysicsResult::Success;
    }

    XJPhysicsWorld::XJPhysicsWorld()
        : mImpl(std::make_unique<Impl>())
    {
    }

    XJPhysicsWorld::~XJPhysicsWorld()
    {
        if (mImpl)
            mImpl->Shutdown();
    }

    XJPhysicsWorld::XJPhysicsWorld(XJPhysicsWorld&&) noexcept = default;
    XJPhysicsWorld& XJPhysicsWorld::operator=(XJPhysicsWorld&&) noexcept = default;

    XJPhysicsResult XJPhysicsWorld::Initialize(const XJPhysicsWorldConfig& config)
    {
        return mImpl ? mImpl->Initialize(config) : XJPhysicsResult::RuntimeUnavailable;
    }

    void XJPhysicsWorld::Shutdown()
    {
        if (mImpl)
            mImpl->Shutdown();
    }

    bool XJPhysicsWorld::IsInitialized() const
    {
        return mImpl && mImpl->IsReady();
    }

    ///
    ///创造的碰撞物体
    ///
    XJPhysicsResult XJPhysicsWorld::CreateStaticPlane(const glm::dvec3& normal,
        const glm::dvec3& pointOnPlane, XJPhysicsBodyId& outBodyId)
    {
        if (!mImpl || !mImpl->IsReady())
            return XJPhysicsResult::NotInitialized;
        //// 1.调用方先自己创建 box 形状
        const JPH::ShapeRefC shape = XJJoltShapeFactory::CreatePlane(normal, pointOnPlane);
        if (!shape)
            return XJPhysicsResult::ShapeCreationFailed;

        // 平面形状的平面方程在局部空间，所以刚体放在原点。  // 2.再交给 CreateBody 建刚体
        return mImpl->CreateBody(shape.GetPtr(), glm::dvec3(0.0), glm::dquat(1.0, 0.0, 0.0, 0.0),
            XJMotionType::Static, 0.0, outBodyId);
    }

    XJPhysicsResult XJPhysicsWorld::CreateStaticBox(const glm::dvec3& halfExtent,
        const glm::dvec3& position, const glm::dquat& rotation, XJPhysicsBodyId& outBodyId)
    {
        if (!mImpl || !mImpl->IsReady())
            return XJPhysicsResult::NotInitialized;

        const JPH::ShapeRefC shape = XJJoltShapeFactory::CreateBox(halfExtent);
        if (!shape)
            return XJPhysicsResult::ShapeCreationFailed;

        return mImpl->CreateBody(shape.GetPtr(), position, rotation,
            XJMotionType::Static, 0.0, outBodyId);
    }

    XJPhysicsResult XJPhysicsWorld::CreateDynamicBox(const glm::dvec3& halfExtent,
        const glm::dvec3& position, const glm::dquat& rotation, double mass,
        XJPhysicsBodyId& outBodyId)
    {
        if (!mImpl || !mImpl->IsReady())
            return XJPhysicsResult::NotInitialized;

        const JPH::ShapeRefC shape = XJJoltShapeFactory::CreateBox(halfExtent);
        if (!shape)
            return XJPhysicsResult::ShapeCreationFailed;

        return mImpl->CreateBody(shape.GetPtr(), position, rotation,
            XJMotionType::Dynamic, mass, outBodyId);
    }

    XJPhysicsResult XJPhysicsWorld::CreateDynamicSphere(double radius,
        const glm::dvec3& position, const glm::dquat& rotation, double mass,
        XJPhysicsBodyId& outBodyId)
    {
        if (!mImpl || !mImpl->IsReady())
            return XJPhysicsResult::NotInitialized;

        const JPH::ShapeRefC shape = XJJoltShapeFactory::CreateSphere(radius);
        if (!shape)
            return XJPhysicsResult::ShapeCreationFailed;

        return mImpl->CreateBody(shape.GetPtr(), position, rotation,
            XJMotionType::Dynamic, mass, outBodyId);
    }


    bool XJPhysicsWorld::DestroyBody(XJPhysicsBodyId bodyId)//
    {
        if (!mImpl || !mImpl->IsReady() || !bodyId.IsValid())
            return false;

        const JPH::BodyID joltId(bodyId.Value);//从打包值还原 Jolt ID
        if (joltId.IsInvalid())
            return false;

        JPH::BodyInterface& bodies = mImpl->System->GetBodyInterface();//检查刚体是否真的在物理世界里
        if (!bodies.IsAdded(joltId))
            return false;

        // 必须先 RemoveBody 再 DestroyBody，顺序反了会断言。
        bodies.RemoveBody(joltId);//刚体从物理世界（PhysicsSystem）的活动列表中移除——它会从宽阶段空间树、激活列表、接触约束里摘除，不再参与模拟和碰撞检测。但刚体对象本身仍存在。
        bodies.DestroyBody(joltId);//释放刚体对象本身（归还到内部对象池），ID 变为无效。

        if (mImpl->BodyCount > 0)
            --mImpl->BodyCount;

        return true;
    }


    XJPhysicsResult XJPhysicsWorld::Step(double deltaTime)//推进物理模拟
    {
        if (!mImpl || !mImpl->IsReady())
            return XJPhysicsResult::NotInitialized;

        if (!(std::isfinite(deltaTime) && deltaTime > 0.0))//合法性校验  
        {
            spdlog::error("XJPhysicsWorld::Step rejected invalid delta time: {}", deltaTime);
            return XJPhysicsResult::InvalidArgument;
        }

        const JPH::EPhysicsUpdateError error = mImpl->System->Update(
            float(deltaTime), 1, mImpl->TempAllocator.get(), mImpl->JobSystem.get());//步进时长  碰撞检测子步数  每帧临时内存池  多线程作业后端

        // 缓存不够不是致命错误，加大 MaxBodyPairs / MaxContactConstraints 即可。
        // 这里只提示，不中断模拟。
        if (error != JPH::EPhysicsUpdateError::None)//容量警告，不中断
        {
            spdlog::warn("Jolt update reported capacity issue, flags={}",
                static_cast<uint32_t>(error));
        }

        return XJPhysicsResult::Success;
    }

    bool XJPhysicsWorld::GetBodyPose(XJPhysicsBodyId bodyId,
        glm::dvec3& outPosition, glm::dquat& outRotation) const//读取刚体位姿
    {
        if (!mImpl || !mImpl->IsReady() || !bodyId.IsValid())
            return false;
        //// ② 还原 Jolt ID
        const JPH::BodyID joltId(bodyId.Value);
        const JPH::BodyInterface& bodies = mImpl->System->GetBodyInterface();
        if (!bodies.IsAdded(joltId))// ③ 确认刚体还在世界里
            return false;

        JPH::RVec3 position;
        JPH::Quat rotation;
        bodies.GetPositionAndRotation(joltId, position, rotation);

        outPosition = ToGlmDVec3(position);
        outRotation = ToGlmDQuat(rotation);
        return true;
    }

    bool XJPhysicsWorld::GetLinearVelocity(XJPhysicsBodyId bodyId,
        glm::dvec3& outVelocity) const//读取线速度
    {
        if (!mImpl || !mImpl->IsReady() || !bodyId.IsValid())
            return false;

        const JPH::BodyID joltId(bodyId.Value);
        const JPH::BodyInterface& bodies = mImpl->System->GetBodyInterface();
        if (!bodies.IsAdded(joltId))
            return false;

        outVelocity = ToGlmDVec3(bodies.GetLinearVelocity(joltId));
        return true;
    }

    bool XJPhysicsWorld::IsBodyActive(XJPhysicsBodyId bodyId) const//查询是否激活
    {
        if (!mImpl || !mImpl->IsReady() || !bodyId.IsValid())
            return false;

        const JPH::BodyID joltId(bodyId.Value);
        const JPH::BodyInterface& bodies = mImpl->System->GetBodyInterface();
        if (!bodies.IsAdded(joltId))
            return false;

        return bodies.IsActive(joltId);
    }

    uint32_t XJPhysicsWorld::GetBodyCount() const//刚体计数
    {
        return mImpl ? mImpl->BodyCount : 0u;
    }

}