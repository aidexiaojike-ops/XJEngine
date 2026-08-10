#include "Render/System/XJMaterialSystem.h"
#include "XJApplication.h"
#include "Render/XJRenderTarget.h"
#include "Render/XJRenderContext.h"
#include "ECS/Component/XJCameraComponent.h"
#include "ECS/XJEntity.h"

namespace XJ
{
    XJApplication *XJMaterialSystem::XJGetApp() const
    {
        XJAppContext *kAppContext = XJApplication::XJGetAppContext();
        if(kAppContext)
        {
            return kAppContext->app;
        }
        return nullptr;
    }
    XJScene *XJMaterialSystem::XJGetScene() const
    {
        XJAppContext *kAppContext = XJApplication::XJGetAppContext();
        if(kAppContext)
        {
            return kAppContext->scene;
        }
        return nullptr;
    }
    XJVulkanDevice *XJMaterialSystem::XJGetDevice() const
    {
        XJAppContext *kAppContext = XJApplication::XJGetAppContext();
        if(kAppContext && kAppContext->renderContext)
        {
            return kAppContext->renderContext->XJGetDevice();
        }
        return nullptr;
    }
    const glm::mat4 XJMaterialSystem::XJGetProjMat(XJRenderTarget *renderTarget) const
    {
        glm::mat4 projMat{1.f};

        if(!renderTarget)
            return projMat;

        XJEntity *kCamera = renderTarget->XJGetCamera();
        if(XJEntity::HasComponent<XJCameraComponent>(kCamera))
        {
            auto &kCameraComp = kCamera->GetComponent<XJCameraComponent>();
            if (!kCameraComp.XJGetEnabled())
                return projMat;

            projMat = kCameraComp.XJGetProjectionMatrix();
        }
        return projMat;

    }
    const glm::mat4 XJMaterialSystem::XJGetViewMat(XJRenderTarget *renderTarget) const
    {
        glm::mat4 viewMat{1.f};

        if (!renderTarget)
            return viewMat;
        XJEntity *kCamera = renderTarget->XJGetCamera();
        if(XJEntity::HasComponent<XJCameraComponent>(kCamera))
        {
            auto &kCameraComp = kCamera->GetComponent<XJCameraComponent>();
            if (!kCameraComp.XJGetEnabled())
                return viewMat;

            viewMat = kCameraComp.XJGetViewMatrix();
        }
        return viewMat;

    }
}

