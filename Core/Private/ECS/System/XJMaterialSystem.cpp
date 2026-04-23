#include "ECS/System/XJMaterialSystem.h"
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
        if(kAppContext)
        {
            return kAppContext->renderContext->XJGetDevice();
        }
        return nullptr;
    }
    const glm::mat4 XJMaterialSystem::XJGetProjMat(XJRenderTarget *renderTarget) const
    {
        glm::mat4 kProjMat{1.f};
        XJEntity *kCamera = renderTarget->XJGetCamera();
        if(XJEntity::HasComponent<XJCameraComponent>(kCamera))
        {
            auto &kCameraComp = kCamera->GetComponent<XJCameraComponent>();
            kProjMat = kCameraComp.XJGetProjectionMatrix();
        }
        return kProjMat;

    }
    const glm::mat4 XJMaterialSystem::XJGetViewMat(XJRenderTarget *renderTarget) const
    {
        glm::mat4 vKiewMat{1.f};
        XJEntity *kCamera = renderTarget->XJGetCamera();
        if(XJEntity::HasComponent<XJCameraComponent>(kCamera))
        {
            auto &kCameraComp = kCamera->GetComponent<XJCameraComponent>();
            vKiewMat = kCameraComp.XJGetViewMatrix();
        }
        return vKiewMat;

    }
}

