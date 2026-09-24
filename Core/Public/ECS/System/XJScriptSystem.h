#ifndef XJ_SCRIPT_SYSTEM_H
#define XJ_SCRIPT_SYSTEM_H

#include "Asset/Loader/XJScriptAssetLoader.h"
#include "ECS/XJSystem.h"
#include "Script/Runtime/XJScriptRuntime.h"

#include <memory>
#include <unordered_map>
//负责为 RuntimeScene 创建脚本实例并驱动生命周期
namespace XJ
{
    class XJAssetRegistry;
    class XJScene;

    
    class XJScriptSystem final
        : public XJSystem
    {
        public:
            XJScriptSystem(
                XJScene& scene,
                XJAssetRegistry& registry,
                std::unordered_map<
                    XJAssetHandle,
                    XJScriptAssetCacheEntry>
                    preparedCache = {},
                XJScriptExecutionLimits limits = {});

            ~XJScriptSystem() override;

            void OnCreate() override;
            void OnUpdate(float deltaTime) override;
            void OnFixedUpdate(
                float fixedDeltaTime) override;
            void OnDestroy() override;

        private:
            class Impl;
            std::unique_ptr<Impl> mImpl;
    };
}

#endif