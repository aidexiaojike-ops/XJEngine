#include "ECS/System/XJSystemScheduler.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include <spdlog/spdlog.h>

namespace XJ
{
    namespace
    {
        bool IsValidConfig(const XJSystemSchedulerConfig& config)
        {
            return std::isfinite(config.FixedDeltaTime) && config.FixedDeltaTime > 0.0f && std::isfinite(config.MaxDeltaTime) && config.MaxDeltaTime > 0.0f && config.MaxFixedStepsPerFrame > 0;
        }
    }
  
    XJSystemScheduler::XJSystemScheduler(XJSystemSchedulerConfig config) : mConfig(config)
    {
        if (!IsValidConfig(mConfig))
        {
            throw std::invalid_argument("XJSystemSchedulerConfig is invalid");
        }
    }

    XJSystemScheduler::~XJSystemScheduler()
    {
        Clear();
    }

    bool XJSystemScheduler::AddSystem(std::shared_ptr<XJSystem> system)
    {
        if (!system)
            return false;

        if(mRunning || mTransitioning || mUpdating || mCreatedSystemCount != 0)
        {
            spdlog::error("AddSystem rejected: " "scheduler lifecycle is active.");
            return false;
        }

        mSystems.push_back(std::move(system));

        return true;
    }

    bool XJSystemScheduler::AddSystem(std::unique_ptr<XJSystem> system)
    {
        if (!system)
            return false;

        return AddSystem(std::shared_ptr<XJSystem>(std::move(system)));
    }

    bool XJSystemScheduler::SetSafePointCallback(SafePointCallback callback)
    {
        if (mRunning || mTransitioning || mUpdating)
        {
            spdlog::error("SetSafePointCallback rejected: ""scheduler is active.");
            return false;
        }

        mSafePointCallback = std::move(callback);

        return true;
    }

    bool XJSystemScheduler::ExecuteSafePoint() 
        noexcept
    {
        if(!mSafePointCallback)
            return true;

        try
        {
            mSafePointCallback();
            return true;
        }
        catch (const std::exception& exception)
        {
            spdlog::error("System scheduler safe point failed: {}", exception.what());
        }
        catch(...)
        {
            spdlog::error("System scheduler safe point failed ""with an unknown exception.");
        }

        return false;
    }

    void XJSystemScheduler::DestroyCreatedSystems() noexcept
    {
        while(mCreatedSystemCount > 0)
        {
            const size_t systemIndex = --mCreatedSystemCount;
            const auto& system = mSystems[systemIndex];

            if(!system)
                continue;

            try
            {
                system->OnDestroy();
            }
            catch(const std::exception& exception)
            {
                spdlog::error("System {} OnDestroy failed: {}", systemIndex, exception.what());
            }
            catch (...)
            {
                spdlog::error("System {} OnDestroy failed " "with an unknown exception.", systemIndex);
            }
        }
        ExecuteSafePoint();
    }

    bool XJSystemScheduler::Start()
    {
        if (mRunning)
            return true;

        if (mTransitioning || mUpdating || mCreatedSystemCount != 0)
        {
            spdlog::error( "System scheduler Start rejected: " "invalid lifecycle state.");
            return false;
        }

        mTransitioning = true;
        mFixedUpdateAccumulator = 0.0f;

        for(size_t index = 0; index < mSystems.size(); ++index)
        {
            const auto& system = mSystems[index];

            if(!system)
                continue;
            
            // 在 OnCreate 前计数，使部分创建后抛异常的
            // System 也能够收到 OnDestroy。
            mCreatedSystemCount = index + 1;

            try
            {
                system->OnCreate();
            }
            catch (const std::exception& exception)
            {
                spdlog::error("System {} OnCreate failed: {}", index, exception.what());

                DestroyCreatedSystems();
                mTransitioning = false;
                return false;
            }
            catch (...)
            {
                spdlog::error("System {} OnCreate failed " "with an unknown exception.", index);

                DestroyCreatedSystems();
                mTransitioning = false;
                return false;
            }
        }

        if(!ExecuteSafePoint())
        {
            DestroyCreatedSystems();
            mTransitioning = false;
            return false;
        }

        mRunning = true;
        mTransitioning = false;
        return true;
    }

    void XJSystemScheduler::Stop() noexcept
    {
        if (mUpdating)
        {
            spdlog::error("System scheduler Stop rejected " "during Update.");
            return;
        }

        if (mTransitioning)
            return;

        if (!mRunning &&
            mCreatedSystemCount == 0)
        {
            return;
        }

        mTransitioning = true;
        mRunning = false;

        DestroyCreatedSystems();

        mFixedUpdateAccumulator = 0.0f;
        mTransitioning = false;
    }

    void XJSystemScheduler::Update(float deltaTime)
    {
        if (!mRunning || mTransitioning || mUpdating)
        {
            return;
        }

        if (!std::isfinite(deltaTime))
        {
            spdlog::error("System update skipped: ""deltaTime is not finite.");
            return;
        }

        deltaTime = std::clamp(deltaTime, 0.0f, mConfig.MaxDeltaTime);

        mUpdating = true;

        for (size_t index = 0; index < mSystems.size(); ++index)
        {
            const auto& system = mSystems[index];

            if (!system)
                continue;

            try
            {
                system->OnUpdate(deltaTime);
            }
            catch (const std::exception& exception)
            {
                spdlog::error(
                    "System {} OnUpdate failed: {}",
                    index,
                    exception.what());
            }
            catch (...)
            {
                spdlog::error("System {} OnUpdate failed ""with an unknown exception.", index);
            }
        }

        // OnUpdate 中请求的实体删除在阶段结束后执行。
        ExecuteSafePoint();

        mFixedUpdateAccumulator += deltaTime;

        uint32_t fixedStepCount = 0;

        while (mFixedUpdateAccumulator >= mConfig.FixedDeltaTime && fixedStepCount < mConfig.MaxFixedStepsPerFrame)
        {
            for (size_t index = 0; index < mSystems.size(); ++index)
            {
                const auto& system = mSystems[index];

                if (!system)
                    continue;

                try
                {
                    system->OnFixedUpdate(
                        mConfig.FixedDeltaTime);
                }
                catch (
                    const std::exception& exception)
                {
                    spdlog::error(
                        "System {} OnFixedUpdate "
                        "failed: {}",
                        index,
                        exception.what());
                }
                catch (...)
                {
                    spdlog::error(
                        "System {} OnFixedUpdate "
                        "failed with an unknown "
                        "exception.",
                        index);
                }
            }

            // 每个 FixedUpdate 阶段结束后处理延迟删除。
            ExecuteSafePoint();

            mFixedUpdateAccumulator -= mConfig.FixedDeltaTime;

            ++fixedStepCount;
        }

        if (mFixedUpdateAccumulator >= mConfig.FixedDeltaTime)
        {
            const float droppedTime = mFixedUpdateAccumulator -
                std::fmod(mFixedUpdateAccumulator, mConfig.FixedDeltaTime);

            mFixedUpdateAccumulator = std::fmod(mFixedUpdateAccumulator, mConfig.FixedDeltaTime);

            spdlog::warn("Fixed update budget exceeded: " "steps={}, droppedTime={}.", fixedStepCount, droppedTime);
        }

        mUpdating = false;
    }

    void XJSystemScheduler::Clear() noexcept
    {
        if (mUpdating || mTransitioning)
        {
            spdlog::error("System scheduler Clear rejected " "during callback execution.");
            return;
        }

        Stop();

        mSystems.clear();
        mSafePointCallback = {};
        mFixedUpdateAccumulator = 0.0f;
        mCreatedSystemCount = 0;
    }
}
