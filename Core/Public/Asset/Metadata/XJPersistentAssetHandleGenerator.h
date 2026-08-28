#ifndef XJ_PERSISTENT_ASSET_HANDLE_GENERATOR_H
#define XJ_PERSISTENT_ASSET_HANDLE_GENERATOR_H

#include "Asset/XJAsset.h"

#include <cstdint>
#include <functional>
#include <optional>


namespace XJ
{

    class XJPersistentAssetHandleGenerator//持久生成器
    {
        public:
            using AvailabilityCallback = std::function<bool(XJAssetHandle)>;

            // 只生成候选值，不检查 Registry 或其他 .xjmeta。
            static XJAssetHandle GenerateCandidate();

            // 调用方提供“该 Handle 是否可用”的判断。
            // 返回 nullopt 表示尝试次数内一直发生冲突。
            static std::optional<XJAssetHandle> GenerateUnique(const AvailabilityCallback& isAvailable, uint32_t maxAttempts = 128);

            XJPersistentAssetHandleGenerator() = delete;
    };

}

#endif