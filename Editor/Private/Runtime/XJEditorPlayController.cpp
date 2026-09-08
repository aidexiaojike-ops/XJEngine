#include "Runtime/XJEditorPlayController.h"

#include "Asset/Instantiation/XJSceneInstantiator.h"
#include "Asset/Serialization/XJSceneAssetSerializer.h"
#include "Asset/XJAssetRegistry.h"
#include "Asset/XJSceneRuntimeUtil.h"
#include "ECS/Component/XJCameraComponent.h"
#include "ECS/Component/XJTransformComponent.h"
#include "ECS/System/XJSystemScheduler.h"
#include "ECS/XJEntity.h"
#include "ECS/XJScene.h"

#include <spdlog/spdlog.h>

#include <utility>
#include <vector>

namespace XJ
{
    class XJEditorPlayController::Impl
    {
        public:
            std::unique_ptr<XJScene> RuntimeScene;
            XJSystemScheduler Scheduler;
            std::vector<SystemFactory> SystemFactories;
            XJUUID RuntimeCameraId{0};
            XJEditorPlayState State = XJEditorPlayState::Edit;
    };

    XJEditorPlayController::XJEditorPlayController()
        : mImpl(std::make_unique<Impl>())
    {
    }

    XJEditorPlayController::~XJEditorPlayController()
    {
        Stop();
    }

    bool XJEditorPlayController::Start(
        const XJScene& editorScene,
        XJAssetRegistry& registry,
        const std::shared_ptr<XJTexture>& defaultTexture,
        const std::shared_ptr<XJSampler>& defaultSampler)
    {
        if (mImpl->State != XJEditorPlayState::Edit || !defaultTexture || !defaultSampler)
            return false;

        std::shared_ptr<XJSceneAsset> snapshot = XJSceneAssetSerializer::BuildFromScene(editorScene);
        if (!snapshot)
        {
            spdlog::error("Play failed: editor scene snapshot could not be created.");
            return false;
        }

        auto candidateScene = std::make_unique<XJScene>();
        XJSceneInstantiateContext context;
        context.Registry = &registry;
        context.DefaultTexture = defaultTexture;
        context.DefaultSampler = defaultSampler;
        context.SourceScene = {};

        if (!XJSceneInstantiator::Instantiate(*snapshot, *candidateScene, &context))
        {
            spdlog::error("Play failed: runtime scene clone could not be instantiated.");
            return false;
        }

        XJEntity* runtimeCamera = XJSceneRuntimeUtil::FindPrimaryCameraEntity(*candidateScene);
        if (!runtimeCamera)
        {
            runtimeCamera = candidateScene->CreateEntityWithTransform("GameCamera");
            if (!runtimeCamera)
                return false;

            auto& transform = runtimeCamera->GetComponent<XJTransformComponent>();
            transform.position = glm::vec3(0.0f, 1.5f, 3.0f);
            transform.rotation = glm::vec3(-90.0f, 0.0f, 0.0f);
            transform.UpdateModelMatrix();

            auto& camera = runtimeCamera->AddComponent<XJCameraComponent>();
            camera.XJSetFov(60.0f);
            camera.XJSetNear(0.1f);
            camera.XJSetFar(100.0f);
        }

        std::vector<std::shared_ptr<XJSystem>> systems;
        systems.reserve(mImpl->SystemFactories.size());
        for (const SystemFactory& factory : mImpl->SystemFactories)
        {
            if (!factory)
                continue;

            std::shared_ptr<XJSystem> system = factory(*candidateScene);
            if (!system)
            {
                spdlog::error("Play failed: a runtime system factory returned null.");
                return false;
            }
            systems.push_back(std::move(system));
        }

        // 到这里所有可失败准备都已成功，再提交 RuntimeScene 和 Scheduler。
        mImpl->RuntimeCameraId = runtimeCamera->XJGetUUID();
        mImpl->RuntimeScene = std::move(candidateScene);
        mImpl->Scheduler.Clear();
        for (auto& system : systems)
            mImpl->Scheduler.AddSystem(std::move(system));
        mImpl->Scheduler.Start();
        mImpl->State = XJEditorPlayState::Playing;
        return true;
    }

    bool XJEditorPlayController::Pause()
    {
        if (mImpl->State != XJEditorPlayState::Playing)
            return false;
        mImpl->State = XJEditorPlayState::Paused;
        return true;
    }

    bool XJEditorPlayController::Resume()
    {
        if (mImpl->State != XJEditorPlayState::Paused)
            return false;
        mImpl->State = XJEditorPlayState::Playing;
        return true;
    }

    void XJEditorPlayController::Update(float deltaTime)
    {
        if (mImpl->State == XJEditorPlayState::Playing)
            mImpl->Scheduler.Update(deltaTime);
    }

    void XJEditorPlayController::Stop()
    {
        mImpl->Scheduler.Clear();
        mImpl->RuntimeScene.reset();
        mImpl->RuntimeCameraId = XJUUID{0};
        mImpl->State = XJEditorPlayState::Edit;
    }

    bool XJEditorPlayController::RegisterSystemFactory(SystemFactory factory)
    {
        if (!factory || mImpl->State != XJEditorPlayState::Edit)
            return false;
        mImpl->SystemFactories.push_back(std::move(factory));
        return true;
    }

    void XJEditorPlayController::ClearSystemFactories()
    {
        if (mImpl->State == XJEditorPlayState::Edit)
            mImpl->SystemFactories.clear();
    }

    XJEditorPlayState XJEditorPlayController::GetState() const { return mImpl->State; }
    XJScene* XJEditorPlayController::GetRuntimeScene() const { return mImpl->RuntimeScene.get(); }
    XJEntity* XJEditorPlayController::GetRuntimeCamera() const
    {
        return mImpl->RuntimeScene ? mImpl->RuntimeScene->FindEntityByUUID(mImpl->RuntimeCameraId) : nullptr;
    }
}
