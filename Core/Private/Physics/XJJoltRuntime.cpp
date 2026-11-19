#include "Physics/XJJoltRuntime.h"

#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/IssueReporting.h>
#include <Jolt/RegisterTypes.h>

#include <spdlog/spdlog.h>

#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <stdexcept>
#include <string>

namespace XJ
{
    namespace 
    {
        std::mutex gJoltRuntimeMutex; // 保护下面这个计数
        uint32_t gJoltRuntimeRefCount = 0;// 当前有多少"持有者"

        // Jolt 的 Trace 是 C 风格可变参数函数，只能在这里落地。
        void JoltTraceImpl(const char* inFMT, ...)
        {
            if(inFMT == nullptr)
                return;

            va_list args;
            va_start(args, inFMT);

            char buffer[1024] = {};
            std::vsnprintf(buffer, sizeof(buffer), inFMT, args);

            va_end(args);

            spdlog::error("[Jolt] {}", buffer);
        }
   
    
#ifdef JPH_ENABLE_ASSERTS
        // 只在启用断言时存在（JPH_DEBUG 会自动定义 JPH_ENABLE_ASSERTS）。
        // 返回 false 表示不触发断点；这里直接抛标准异常，
        // 符合项目约定：先记录日志，再抛标准异常。
        bool JoltAssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, JPH::uint inLine)
        {
            spdlog::critical("[Jolt] Assertion failed: {} ({}:{}){}",
                    inExpression != nullptr ? inExpression : "",
                    inFile != nullptr ? inFile : "",
                    static_cast<unsigned int>(inLine),
                    inMessage != nullptr ? std::string(": ") + inMessage : std::string());
            
            throw std::runtime_error(std::string("Jolt assertion failed: ") + (inExpression != nullptr ? inExpression : ""));
        }
#endif
        // 顺序不能变，Jolt 官方文档要求严格按此顺序。
        void InitializeGlobalJolt()
        {
            JPH::RegisterDefaultAllocator();//分配器就位:Jolt 内部大量内存分配都走它自己的分配接口
            JPH::Factory::sInstance = new JPH::Factory();//工厂单例存在：Factory::sInstance 是 Jolt 用来反序列化对象（如保存的场景数据）的全局单例
            //类型注册 + 异常安全
            try
            {
                JPH::RegisterTypes();//类型注册表填充：RegisterTypes() 把所有内置的 shape、constraint 等类型登记进工厂，否则 BodyCreationSettings 构造时可能直接空指针崩溃。
            }
            catch(...)
            {
                delete JPH::Factory::sInstance;
                JPH::Factory::sInstance = nullptr;
                throw;
            }
            JPH::Trace = JoltTraceImpl;//Jolt 的日志输出回调，必须由应用覆盖

#ifdef JPH_ENABLE_ASSERTS
        JPH::AssertFailed = JoltAssertFailedImpl;//断言失败处理器，只在 JPH_ENABLE_ASSERTS 开启时安装。它的签名返回 bool：true 表示希望触发断点，false 表示继续执行。
#endif
        }

        void ShutdownGlobalJolt()//释放内存 严格按照实例的顺序
        {
            JPH::UnregisterTypes();

            delete JPH::Factory::sInstance;
            JPH::Factory::sInstance = nullptr;
        }

    }

    bool XJJoltRuntime::Acquire()
    {
        std::lock_guard<std::mutex> lock(gJoltRuntimeMutex);//构造时加锁、作用域结束（含异常退出）时自动解锁。

        if(gJoltRuntimeRefCount > 0)
        {
            ++gJoltRuntimeRefCount;
            return true;
        }

        try
        {
            InitializeGlobalJolt();
        }
        catch(const std::exception& e)
        {
            spdlog::error("Jolt global initialization failed: {}", e.what());
            return false;
        }
        catch(...)
        {
            spdlog::error("Jolt global initialization failed with an unknown exception.");
            return false;
        }

        gJoltRuntimeRefCount = 1;
        return true;
    }

    void XJJoltRuntime::Release()
    {
        std::lock_guard<std::mutex> lock(gJoltRuntimeMutex);

        if(gJoltRuntimeRefCount == 0)
        {
            spdlog::warn("XJJoltRuntime::Release called without a matching Acquire.");
            return;
        }

        --gJoltRuntimeRefCount;
        if(gJoltRuntimeRefCount > 0) 
            return;

        ShutdownGlobalJolt();
    }

    bool XJJoltRuntime::IsAcquired()
    {
        std::lock_guard<std::mutex> lock(gJoltRuntimeMutex);
        return gJoltRuntimeRefCount > 0;
    }

    uint32_t XJJoltRuntime::GetAcquireCount()
    {
        std::lock_guard<std::mutex> lock(gJoltRuntimeMutex);
        return gJoltRuntimeRefCount;
    }
}