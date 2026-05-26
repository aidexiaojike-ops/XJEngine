#include "Asset/XJSceneRuntimeUtil.h"
#include "ECS/XJScene.h"
#include "ECS/XJEntity.h"
#include "ECS/Component/XJCameraComponent.h"


namespace XJ
{
    
    XJEntity* XJSceneRuntimeUtil::FindPrimaryCameraEntity(XJScene& scene) 
    {
        auto& reg = scene.XJGetEcsRegistry();
        auto view = reg.view<XJCameraComponent>();

        //任意启用的 camera
        for (auto e : view)
        {
            auto* entity = scene.XJGetEntities(e);
            if (entity)
                return entity;
        }

        return nullptr;
    }

}