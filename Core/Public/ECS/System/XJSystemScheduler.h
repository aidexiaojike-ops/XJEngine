#ifndef XJ_SYSTEM_SCHEDULER_H
#define XJ_SYSTEM_SCHEDULER_H

#include "ECS/XJSystem.h"
#include <memory>
#include <vector>

namespace XJ
{
    // 系统调度器：Start/Stop 管理生命周期，Update 驱动 OnUpdate + 固定步进 OnFixedUpdate。
    class XJSystemScheduler
    {
        public:
            XJSystemScheduler(float fixedDeltaTime = 1.f / 60.f, float maxDeltaTime = 0.25f);

            void AddSystem(std::shared_ptr<XJSystem> system);
            void AddSystem(std::unique_ptr<XJSystem> system);

            void Start();          // 依次 OnCreate
            void Stop();           // 逆序 OnDestroy
            void Update(float deltaTime); // OnUpdate + 固定步进
            void Clear();          // 停止并清空所有系统

            bool IsRunning() const {return mRunning;}
            float GetFixedDeltaTime() const{ return mFixedDeltaTime; }

        private:
            std::vector<std::shared_ptr<XJSystem>> mSystems;
            bool mRunning = false;
            float mFixedDeltaTime = 1.f / 60.f;
            float mMaxDeltaTime = 0.25f;
            float mFixedUpdateAccumulator = 0.f;


    };
}
#endif