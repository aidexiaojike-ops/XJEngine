#ifndef XJ_JOLT_RUNTIME_H
#define XJ_JOLT_RUNTIME_H

#include <cstdint>

namespace XJ
{
    // Jolt 全局运行时。进程级唯一，引用计数。
    // 负责：默认分配器、Factory、类型注册、Trace/Assert 转发到 spdlog。
    // 多个 XJPhysicsWorld 同时初始化时不会重复注册（重复注册会崩）。
    class XJJoltRuntime
    {
        public:
            static bool Acquire();   // 首次调用完成全局初始化
            static void Release();   // 引用归零时反初始化

            static bool IsAcquired();
            static uint32_t GetAcquireCount();
    };
}

#endif