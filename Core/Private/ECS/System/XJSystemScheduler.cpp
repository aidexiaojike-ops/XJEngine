#include "ECS/System/XJSystemScheduler.h"

#include <algorithm>   // std::clamp

namespace XJ
{
    XJSystemScheduler::XJSystemScheduler(float fixedDeltaTime, float maxDeltaTime)
        : mFixedDeltaTime(fixedDeltaTime), mMaxDeltaTime(maxDeltaTime)
    {
    }

    void XJSystemScheduler::AddSystem(std::shared_ptr<XJSystem> system)
    {
        if (system)
            mSystems.push_back(system);
    }

    void XJSystemScheduler::AddSystem(std::unique_ptr<XJSystem> system)
    {
        if (system)
            mSystems.push_back(std::move(system));
    }

    void XJSystemScheduler::Start()
    {
        if (mRunning)
            return;

        for (auto& system : mSystems)
        {
            if (system)
                system->OnCreate();
        }
        mFixedUpdateAccumulator = 0.f;
        mRunning = true;
    }

    void XJSystemScheduler::Stop()
    {
        if (!mRunning)
            return;

        for (auto it = mSystems.rbegin(); it != mSystems.rend(); ++it)
        {
            if (*it)
                (*it)->OnDestroy();
        }
        mRunning = false;
    }

    void XJSystemScheduler::Update(float deltaTime)
    {
        if (!mRunning)
            return;

        // Clamp delta time to avoid spiral of death
        deltaTime = std::clamp(deltaTime, 0.0f, mMaxDeltaTime);
   

        // Update all systems
        for (auto& system : mSystems)
        {
            if (system)
                system->OnUpdate(deltaTime);
        }

        // Fixed update
        mFixedUpdateAccumulator += deltaTime;
        while (mFixedUpdateAccumulator >= mFixedDeltaTime)
        {
            for (auto& system : mSystems)
            {
                if (system)
                    system->OnFixedUpdate(mFixedDeltaTime);
            }
            mFixedUpdateAccumulator -= mFixedDeltaTime;
        }
    }

    void XJSystemScheduler::Clear()
    {
        Stop();
        mSystems.clear();
    }
}