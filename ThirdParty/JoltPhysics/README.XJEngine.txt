Jolt Physics 在 XJEngine 中的集成说明
================================================================

本目录为 Jolt Physics 5.7.0 的精简源码，由 XJEngine 自行管理构建。
上游的 Build/ 目录已移除，构建定义在:

  cmake/JoltPhysics.cmake

对应的实施规划见 PHYSICS_JOLT_PLAN.txt 第四节。

一、接入方式
----------------------------------------------------------------
1. cmake/ThirdParty.cmake 末尾:

     include(${CMAKE_CURRENT_LIST_DIR}/JoltPhysics.cmake)

   注意用 include() 而不是 add_subdirectory()，因为本目录不再提供
   CMakeLists.txt。

2. Core/CMakeLists.txt 以 PRIVATE 链接:

     target_link_libraries(Core PRIVATE Jolt)

   PRIVATE 是刻意的选择：Jolt 的头文件路径与编译宏只对 Core 的
   源码可见，不会传播到 Editor 与可执行程序。公开头文件
   （Core/Public）不应出现任何 #include <Jolt/...>。

3. 生成的静态库输出到 ${CMAKE_BINARY_DIR}/lib，与 Core 一致。

二、头文件引用约定
----------------------------------------------------------------
Jolt target 的 include 根为 ThirdParty/JoltPhysics，因此:

  #include <Jolt/Jolt.h>
  #include <Jolt/Physics/PhysicsSystem.h>
  #include <Jolt/RegisterTypes.h>

不要写相对路径（如 "Jolt/Jolt.h"），也不要把 ThirdParty/JoltPhysics
加进全局 include_directories。

三、编译配置（全部锁定在 Jolt target 内）
----------------------------------------------------------------
已启用:

  JPH_OBJECT_LAYER_BITS=16    必须定义, ObjectLayer.h 缺少会 #error
  JPH_USE_SSE4_1              仅 x86/x64 生效
  JPH_USE_SSE4_2              仅 x86/x64 生效
  JPH_DEBUG                   仅 Debug 配置
  /Zc:__cplusplus             MSVC，PUBLIC 传播（头文件依赖正确的
                              __cplusplus 值判断语言版本）

已关闭（宏未定义即为关闭）:

  JPH_DOUBLE_PRECISION        单精度。大世界需求另做评估，
                              不能只切这一个宏
  JPH_USE_DX12                 GPU compute，后期独立评估
  JPH_USE_VK                   GPU compute，未使用
  JPH_USE_MTL                  GPU compute，未使用
  JPH_USE_CPU_COMPUTE          CPU compute，未使用
  JPH_DEBUG_RENDERER           调试渲染器未编译进库
  JPH_PROFILE_ENABLED          性能分析器未启用
  JPH_OBJECT_STREAM            对象序列化未编译，
                               场景数据一律用 .xjscene 自己的格式
  JPH_USE_AVX / AVX2 / AVX512  不默认要求 AVX2 及以上指令集

刻意不做的事:

  - 不设置全局 CMAKE_CXX_STANDARD。上游 Jolt.cmake 会把全局标准
    降为 C++17，本项目改为 target_compile_features(Jolt PRIVATE
    cxx_std_17)，引擎其余部分保持 C++20。
  - 不加 /Wall /WX。仅为 Jolt target 关闭若干噪音告警。
  - 不加 /GR- /EHsc-。不给整个引擎禁用 RTTI 与异常。
  - 不开启 LTO。上游 Release 默认开 IPO，本项目不跟随，
    避免与引擎自身的构建配置冲突。

四、坐标与单位约定（写代码前先读）
----------------------------------------------------------------
  - 一场景单位 = 1 米
  - 重力 (0, -9.81, 0)
  - Jolt 与 GLM 同为右手系 Y 轴向上，位置与四元数可直接对应
  - 位置使用 JPH::RVec3，为将来双精度留出余地
  - 所有 glm <-> Jolt 的转换集中在 Core/Private/Physics/
    XJJoltConversions.cpp，其他文件禁止直接构造 JPH::Vec3

五、运行时初始化顺序（顺序错会崩）
----------------------------------------------------------------
  JPH::RegisterDefaultAllocator()
  JPH::Factory::sInstance = new JPH::Factory()
  JPH::RegisterTypes()
  JPH::Trace        = <转发到 spdlog::error>
  JPH::AssertFailed = <先 spdlog::critical 再抛标准异常>
  创建 JobSystem
  创建 PhysicsSystem 并 Init

关闭时严格逆序，最后:
  JPH::UnregisterTypes()
  JPH::Factory::sInstance->deleteInstance()
  JPH::Factory::sInstance = nullptr

这是进程级单例，只初始化一次，由运行时封装负责，不要每次 Play
都重复注册。

六、升级步骤
----------------------------------------------------------------
1. 在独立分支操作，不要跟随 master。
2. 下载目标版本的发布归档，对照新版 Jolt/Jolt.cmake 的
   JOLT_PHYSICS_SRC_FILES 基础清单，更新 cmake/JoltPhysics.cmake
   中的源文件列表。
   只同步 Jolt/ 源码，不要引入 Build/、Samples/、UnitTests/ 等目录。
3. 检查 Jolt/Jolt.cmake 中新增或改名的 option，确认本文件的
   第三节能覆盖到。
4. 阅读上游 Docs/APIChanges.md，检查封装层是否需要适配。
5. 更新 VERSION.txt 的版本号、来源与提交 SHA。
6. 单独提交这次升级，并跑完整测试。
7. 若源文件清单遗漏导致功能缺失，恢复对应源码文件，
   不要写空实现。

七、当前状态
----------------------------------------------------------------
已完成: 依赖接入与静态库构建（阶段 0）
未开始: 运行时封装、ECS 组件、场景序列化、查询与接触事件
        （阶段 1 及之后）

阶段 0 的验收标准是 lib/Jolt.lib 能产出且 Core 可链接；
真正的功能验证从阶段 1 的裸物理测试开始（静态地面 + 动态箱子，
120 次 1/60 秒步进）。
