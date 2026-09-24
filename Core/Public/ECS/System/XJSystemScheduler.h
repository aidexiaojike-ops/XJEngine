#ifndef XJ_SYSTEM_SCHEDULER_H
#define XJ_SYSTEM_SCHEDULER_H

#include "ECS/XJSystem.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace XJ
{
    struct XJSystemSchedulerConfig
    {
        float FixedDeltaTime = 1.0f / 60.0f;
        float MaxDeltaTime = 0.25f;

        // 防止卡顿帧产生无限 FixedUpdate。
        uint32_t MaxFixedStepsPerFrame = 8;
    };
    // 系统调度器：Start/Stop 管理生命周期，Update 驱动 OnUpdate + 固定步进 OnFixedUpdate。
    class XJSystemScheduler
    {
        public:
            using SafePointCallback = std::function<void()>;
            explicit XJSystemScheduler(XJSystemSchedulerConfig coffig = {});
            ~XJSystemScheduler();
            
            XJSystemScheduler(const XJSystemScheduler&) = delete;
            XJSystemScheduler& operator=(const XJSystemScheduler&) = delete;

            bool AddSystem(std::shared_ptr<XJSystem> system);
            bool AddSystem(std::unique_ptr<XJSystem> system);

             // 全部 OnCreate 成功后才进入 Running。
            bool Start();
            // 保证所有已经创建的 System 都收到 OnDestroy。
            void Stop() noexcept;
            void Update(float deltaTime);
            // Stop、删除 Systems、解除 SafePoint。
            void Clear() noexcept;
            bool SetSafePointCallback(SafePointCallback callback);


            bool IsRunning() const {return mRunning;}
            float GetFixedDeltaTime() const{ return mConfig.FixedDeltaTime; }
            uint32_t GetMaxFixedStepsPerFrame() const {return mConfig.MaxFixedStepsPerFrame;}

        private:
         bool ExecuteSafePoint() noexcept;
            void DestroyCreatedSystems() noexcept;

            XJSystemSchedulerConfig mConfig;
            std::vector<std::shared_ptr<XJSystem>> mSystems;
            SafePointCallback mSafePointCallback;
             // 已经开始执行 OnCreate 的系统数量。
            size_t mCreatedSystemCount = 0;

            float mFixedUpdateAccumulator = 0.0f;

            bool mRunning = false;
            bool mTransitioning = false;
            bool mUpdating = false;

    };
}
#endif