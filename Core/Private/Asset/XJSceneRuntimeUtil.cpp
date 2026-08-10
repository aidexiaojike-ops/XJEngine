#include "Asset/XJSceneRuntimeUtil.h"
#include "ECS/XJScene.h"
#include "ECS/XJEntity.h"
#include "ECS/Component/XJCameraComponent.h"
#include "ECS/XJReservedUUID.h"


namespace XJ
{
    
    XJEntity* XJSceneRuntimeUtil::FindPrimaryCameraEntity(XJScene& scene) 
    {
        const auto& reg = scene.XJGetEcsRegistry();
        auto view = reg.view<XJCameraComponent>();

        //任意启用的 camera
        for (auto e : view)
        {
            auto* entity = scene.GetEntity(e);
            if (!entity)
                continue;
            
            if (entity->XJGetUUID() == XJUUID(XJ_PREVIEW_CAMERA_UUID))
                continue;

            if (!entity->GetComponent<XJCameraComponent>().XJGetEnabled())
                continue;
            
            return entity;
        }

        return nullptr;
    }

}
