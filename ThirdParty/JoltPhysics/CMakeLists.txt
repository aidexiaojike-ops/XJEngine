# ============================================================
# Jolt Physics 5.7.0 - 引擎精简集成
# 只编译核心物理库（刚体/碰撞/射线），关闭所有 GPU compute、
# ObjectStream、debug renderer、profiler 等引擎暂不需要的功能。
# 需要 CMake >= 3.20（Jolt 5.7 官方要求）。
# ============================================================

# Jolt.cmake 依赖此变量定位 Jolt/ 源码目录
set(PHYSICS_REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

# ---- 库类型与功能开关 ----
set(JPH_BUILD_SHARED_LIBS OFF)      # 编译为静态库
set(ENABLE_OBJECT_STREAM OFF)       # 关闭序列化（不需要）
set(JPH_USE_DX12 OFF)               # 关闭 DirectX 12 compute
set(JPH_USE_VK OFF)                 # 关闭 Vulkan compute（基础物理在 CPU，无需 GPU）
set(JPH_USE_MTL OFF)                # 关闭 Metal
set(JPH_USE_CPU_COMPUTE OFF)        # 关闭 CPU compute

# ---- 编译选项 ----
set(GENERATE_DEBUG_SYMBOLS ON)
set(USE_ASSERTS OFF)
set(DOUBLE_PRECISION OFF)           # 单精度位置（世界范围够用）
set(CROSS_PLATFORM_DETERMINISTIC OFF)
set(FLOATING_POINT_EXCEPTIONS_ENABLED OFF)
set(DISABLE_CUSTOM_ALLOCATOR OFF)
set(OBJECT_LAYER_BITS 16)
set(TRACK_BROADPHASE_STATS OFF)
set(TRACK_NARROWPHASE_STATS OFF)
set(JPH_TRACK_SIMULATION_STATS OFF)

# ---- Debug renderer / profiler ----
set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF)
set(DEBUG_RENDERER_IN_DISTRIBUTION OFF)
set(PROFILER_IN_DEBUG_AND_RELEASE OFF)
set(PROFILER_IN_DISTRIBUTION OFF)
set(JPH_USE_EXTERNAL_PROFILE OFF)

# ---- x86 SIMD 指令集 ----
# 只开 SSE4.1/SSE4.2（现代 CPU 都支持），关闭 AVX 系列。
# 原因：MSVC 下 AVX 会通过 target_compile_options(PUBLIC /arch:AVX2)
# 强制传播给链接方（整个引擎），关闭后只传播几个无副作用的宏。
set(USE_SSE4_1 ON)
set(USE_SSE4_2 ON)
set(USE_AVX OFF)
set(USE_AVX2 OFF)
set(USE_AVX512 OFF)
set(USE_LZCNT OFF)
set(USE_TZCNT OFF)
set(USE_F16C OFF)
set(USE_FMADD OFF)

# ---- 保存引擎 C++ 标准（Jolt.cmake 内部会强制改成 C++17）----
set(_XJ_SAVED_CXX_STANDARD ${CMAKE_CXX_STANDARD})
set(_XJ_SAVED_CXX_STANDARD_REQUIRED ${CMAKE_CXX_STANDARD_REQUIRED})
set(_XJ_SAVED_CXX_EXTENSIONS ${CMAKE_CXX_EXTENSIONS})

# ---- 编译 Jolt 库（内部执行 add_library(Jolt ...)）----
include(${PHYSICS_REPO_ROOT}/Jolt/Jolt.cmake)

# ---- 恢复引擎 C++ 标准（引擎其余模块继续用 C++20）----
set(CMAKE_CXX_STANDARD ${_XJ_SAVED_CXX_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED ${_XJ_SAVED_CXX_STANDARD_REQUIRED})
set(CMAKE_CXX_EXTENSIONS ${_XJ_SAVED_CXX_EXTENSIONS})

# 清理临时变量，避免污染全局
unset(_XJ_SAVED_CXX_STANDARD)
unset(_XJ_SAVED_CXX_STANDARD_REQUIRED)
unset(_XJ_SAVED_CXX_EXTENSIONS)