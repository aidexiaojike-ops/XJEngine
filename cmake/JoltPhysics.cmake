# ============================================================
# Jolt Physics - XJEngine 精简集成
# 依据 PHYSICS_JOLT_PLAN.txt 第四节编写。
#
# 设计原则：
#   1. 只编译 CPU 物理核心。关闭 GPU compute(DX12/VK/MTL)、
#      CPU compute、ObjectStream 序列化、DebugRenderer 扩展、
#      Profiler 等可选能力。
#   2. 所有编译选项使用 target_* 命令，不修改全局
#      CMAKE_CXX_STANDARD / RTTI / 异常 / 警告等级，
#      避免上游 Jolt.cmake 把 C++20 降级为 C++17 的问题。
#   3. 宏配置集中在本文件，保证库本身与头文件消费端一致。
# ============================================================

set(JOLT_PHYSICS_ROOT ${CMAKE_CURRENT_LIST_DIR}/../ThirdParty/JoltPhysics)
get_filename_component(JOLT_PHYSICS_ROOT "${JOLT_PHYSICS_ROOT}" ABSOLUTE)

# ------------------------------------------------------------
# 核心源文件清单
# 取自上游 Jolt/Jolt.cmake 的 JOLT_PHYSICS_SRC_FILES 基础部分。
# 已剔除：ENABLE_OBJECT_STREAM 条件块、GPU/CPU compute 条件块、
#         Shaders 打包、Metal/DX12/Vulkan 平台实现。
# 注意：只裁剪构建脚本，不裁剪 Jolt/ 中功能依赖的源码。
# ------------------------------------------------------------
set(JOLT_PHYSICS_SRC_FILES
    ${JOLT_PHYSICS_ROOT}/Jolt/AABBTree/AABBTreeBuilder.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Compute/ComputeSystem.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Core/Color.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Core/Factory.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Core/IssueReporting.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Core/JobSystemSingleThreaded.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Core/JobSystemThreadPool.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Core/JobSystemWithBarrier.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Core/LinearCurve.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Core/Memory.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Core/Profiler.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Core/RTTI.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Core/Semaphore.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Core/StringTools.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Core/TickCounter.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Geometry/ConvexHullBuilder.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Geometry/ConvexHullBuilder2D.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Geometry/Indexify.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Geometry/OrientedBox.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Math/Vec3.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/ObjectStream/SerializableObject.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Body/Body.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Body/BodyCreationSettings.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Body/BodyInterface.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Body/BodyManager.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Body/MassProperties.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Body/MotionProperties.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Character/Character.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Character/CharacterBase.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Character/CharacterVirtual.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/BroadPhase/BroadPhase.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/BroadPhase/BroadPhaseBruteForce.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/BroadPhase/BroadPhaseQuadTree.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/BroadPhase/QuadTree.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/CastConvexVsTriangles.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/CastSphereVsTriangles.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/CollideConvexVsTriangles.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/CollideSphereVsTriangles.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/CollisionDispatch.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/CollisionGroup.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/EstimateCollisionResponse.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/GroupFilter.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/GroupFilterTable.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/ManifoldBetweenTwoFaces.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/NarrowPhaseQuery.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/NarrowPhaseStats.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/PhysicsMaterial.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/PhysicsMaterialSimple.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/Shape/BoxShape.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/Shape/CapsuleShape.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/Shape/CompoundShape.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/Shape/ConvexHullShape.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/Shape/ConvexShape.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/Shape/CylinderShape.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/Shape/DecoratedShape.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/Shape/EmptyShape.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/Shape/HeightFieldShape.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/Shape/MeshShape.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/Shape/MutableCompoundShape.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/Shape/OffsetCenterOfMassShape.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/Shape/PlaneShape.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/Shape/RotatedTranslatedShape.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/Shape/ScaledShape.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/Shape/Shape.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/Shape/SphereShape.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/Shape/StaticCompoundShape.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/Shape/TaperedCapsuleShape.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/Shape/TaperedCylinderShape.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/Shape/TriangleShape.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Collision/TransformedShape.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Constraints/ConeConstraint.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Constraints/Constraint.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Constraints/ConstraintManager.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Constraints/ContactConstraintManager.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Constraints/DistanceConstraint.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Constraints/FixedConstraint.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Constraints/GearConstraint.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Constraints/HingeConstraint.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Constraints/MotorSettings.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Constraints/PathConstraint.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Constraints/PathConstraintPath.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Constraints/PathConstraintPathHermite.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Constraints/PointConstraint.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Constraints/PulleyConstraint.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Constraints/RackAndPinionConstraint.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Constraints/SixDOFConstraint.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Constraints/SliderConstraint.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Constraints/SpringSettings.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Constraints/SwingTwistConstraint.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Constraints/TwoBodyConstraint.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/DeterminismLog.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Hair/Hair.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Hair/HairSettings.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Hair/HairShaders.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Hair/RegisterHair.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/IslandBuilder.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/LargeIslandSplitter.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/PhysicsScene.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/PhysicsSystem.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/PhysicsUpdateContext.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Ragdoll/Ragdoll.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/SoftBody/SoftBodyCreationSettings.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/SoftBody/SoftBodyMotionProperties.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/SoftBody/SoftBodyShape.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/SoftBody/SoftBodySharedSettings.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/StateRecorderImpl.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Vehicle/MotorcycleController.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Vehicle/TrackedVehicleController.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Vehicle/VehicleAntiRollBar.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Vehicle/VehicleCollisionTester.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Vehicle/VehicleConstraint.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Vehicle/VehicleController.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Vehicle/VehicleDifferential.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Vehicle/VehicleEngine.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Vehicle/VehicleTrack.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Vehicle/VehicleTransmission.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Vehicle/Wheel.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Physics/Vehicle/WheeledVehicleController.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/RegisterTypes.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Renderer/DebugRenderer.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Renderer/DebugRendererPlayback.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Renderer/DebugRendererRecorder.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Renderer/DebugRendererSimple.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Skeleton/SkeletalAnimation.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Skeleton/Skeleton.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Skeleton/SkeletonMapper.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/Skeleton/SkeletonPose.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/TriangleSplitter/TriangleSplitter.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/TriangleSplitter/TriangleSplitterBinning.cpp
    ${JOLT_PHYSICS_ROOT}/Jolt/TriangleSplitter/TriangleSplitterMean.cpp
)

# Windows 下附加 natvis（仅调试可视化，不参与编译）
if(WIN32)
    list(APPEND JOLT_PHYSICS_SRC_FILES ${JOLT_PHYSICS_ROOT}/Jolt/Jolt.natvis)
endif()

# ------------------------------------------------------------
# 创建静态库 target
# ------------------------------------------------------------
add_library(Jolt STATIC ${JOLT_PHYSICS_SRC_FILES})
add_library(Jolt::Jolt ALIAS Jolt)

# include 根设为 ThirdParty/JoltPhysics，使 <Jolt/xxx.h> 可解析。
target_include_directories(Jolt PUBLIC ${JOLT_PHYSICS_ROOT})

# ------------------------------------------------------------
# 编译定义
# PUBLIC：保证库本体与包含 Jolt 头的编译单元使用同一套宏，
#         否则会出现类布局/ODR 不一致。
# 未定义即为关闭：JPH_DOUBLE_PRECISION、JPH_DEBUG_RENDERER、
#                 JPH_PROFILE_ENABLED、JPH_OBJECT_STREAM、
#                 JPH_USE_DX12/VK/MTL/CPU_COMPUTE、JPH_USE_AVX*
# ------------------------------------------------------------
target_compile_definitions(Jolt PUBLIC
    JPH_OBJECT_LAYER_BITS=16
    $<$<CONFIG:Debug>:JPH_DEBUG>
)

# ------------------------------------------------------------
# 保守 SIMD 基线：SSE4.2（x86/x64），不启用 AVX/AVX2/AVX512。
#
# 关键点：宏必须和编译器指令集开关成对出现。
# Jolt/Core/Core.h:177-182 会用 __SSE4_1__ / __SSE4_2__ 自动推导
# JPH_USE_SSE4_1 / JPH_USE_SSE4_2；如果只手动定义宏却不加 -msse4.x，
# GCC/MinGW 会启用 _mm_blendv_ps 等 SSE4.1 内建函数，
# 然后报 "inlining failed ... target specific option mismatch"。
#
# 因此：
#   MSVC      -> 只定义宏（MSVC 不按 /arch 限制 SSE4.x 内建函数）
#   GCC/Clang -> 只加 -msse4.2 -mpopcnt，宏交给 Jolt 自己推导
#
# 关闭本选项可退化为 SSE2 基线（任何 x64 CPU 都能跑，性能略低）。
# ------------------------------------------------------------
option(XJ_JOLT_USE_SSE42 "Build Jolt with SSE4.2 baseline (requires CPU from 2008+)" ON)

if(XJ_JOLT_USE_SSE42 AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|AMD64|amd64|x86|i.86)$")
    if(MSVC)
        target_compile_definitions(Jolt PUBLIC
            JPH_USE_SSE4_1
            JPH_USE_SSE4_2
        )
    else()
        # PUBLIC：Jolt 的 .inl 头文件内部就调用这些内建函数，
        # 包含 Jolt 头的编译单元必须使用相同目标指令集，否则同样报错。
        target_compile_options(Jolt PUBLIC -msse4.2 -mpopcnt)
    endif()
endif()

# ------------------------------------------------------------
# 编译器选项（全部限定在 Jolt target 内，不影响全局）
# ------------------------------------------------------------
if(MSVC)
    # Jolt 头文件通过 __cplusplus 判断 C++ 版本，需修正 MSVC 的默认值。
    # 该选项必须传播给消费端，否则头文件按错误的语言版本走分支。
    target_compile_options(Jolt PUBLIC /Zc:__cplusplus)

    # 仅本 target：并行编译、浮点模型对齐上游默认测试配置。
    if(CMAKE_GENERATOR MATCHES "Visual Studio")
        target_compile_options(Jolt PRIVATE /MP)
    endif()
    target_compile_options(Jolt PRIVATE /fp:fast)

    # 关闭 Jolt 源码中未使用参数等噪音告警（不上 /Wall /WX）。
    target_compile_options(Jolt PRIVATE /wd4100 /wd4127 /wd4324 /wd4244 /wd4267)
else()
    target_compile_options(Jolt PRIVATE -Wno-unused-parameter)
endif()

# Jolt 至少需要 C++17。用 PRIVATE，避免修改引擎全局的 C++20 设置。
target_compile_features(Jolt PRIVATE cxx_std_17)

# 静态库输出到与 Core 一致的目录，便于统一查找。
set_target_properties(Jolt PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)
