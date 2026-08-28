#include "Asset/Metadata/XJPersistentAssetHandleGenerator.h"

#include <chrono>
#include <functional>
#include <limits>
#include <random>
#include <thread>

namespace XJ
{
    namespace
    {
        uint64_t BuildThreadSeed()//随机种子
        {
            std::random_device randomDevice;

            const uint64_t randomPart = (static_cast<uint64_t>(randomDevice()) << 32) ^ static_cast<uint64_t>(randomDevice());

            const uint64_t timePart =
                static_cast<uint64_t>(
                    std::chrono::
                        high_resolution_clock::
                        now().
                        time_since_epoch().
                        count());

            const uint64_t threadPart = static_cast<uint64_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()));

            return randomPart ^ timePart ^ threadPart;
        }

        std::mt19937_64& GetThreadEngine()
        {
            // 每个线程独立随机引擎，不需要 mutex，
            // 也不会发生多个线程同时访问同一个 engine 的数据竞争。
            thread_local std::mt19937_64 engine(BuildThreadSeed());

            return engine;
        }
    }

    XJAssetHandle XJPersistentAssetHandleGenerator:: GenerateCandidate()
    {
        // 永久资产使用低 63 位。
        // [1, RuntimeHandleBit - 1]
        static thread_local
            std::uniform_int_distribution<XJAssetHandle>
            distribution(
                1,
                XJAsset::RuntimeHandleBit - 1);

        XJAssetHandle handle = distribution(GetThreadEngine());

        // distribution 已排除 0 和 high bit，
        // 这里保留防御检查，避免常量范围以后修改后静默出错。
        if (handle == XJAsset::InvalidHandle || XJAsset::IsRuntimeHandle(handle))
        {
            return XJAsset::InvalidHandle;
        }

        return handle;
    }

    std::optional<XJAssetHandle> XJPersistentAssetHandleGenerator::GenerateUnique(const AvailabilityCallback& isAvailable, uint32_t maxAttempts)
    {
        if (!isAvailable || maxAttempts == 0)
            return std::nullopt;

        for (uint32_t attempt = 0; attempt < maxAttempts; ++attempt)
        {
            const XJAssetHandle candidate = GenerateCandidate();

            if (candidate == XJAsset::InvalidHandle)
            {
                continue;
            }

            if (isAvailable(candidate))
                return candidate;
        }

        return std::nullopt;
    }
}