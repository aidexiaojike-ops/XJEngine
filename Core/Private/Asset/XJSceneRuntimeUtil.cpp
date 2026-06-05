#include "Asset/XJSceneRuntimeUtil.h"
#include "ECS/XJScene.h"
#include "ECS/XJEntity.h"
#include "ECS/Component/XJCameraComponent.h"


namespace XJ
{
    
    XJEntity* XJSceneRuntimeUtil::FindPrimaryCameraEntity(XJScene& scene) 
    {
        static constexpr uint64_t PreviewCameraUUID = 0x30000001ull;

        auto& reg = scene.XJGetEcsRegistry();
        auto view = reg.view<XJCameraComponent>();

        //任意启用的 camera
        for (auto e : view)
        {
            auto* entity = scene.XJGetEntities(e);
            if (!entity)
                continue;
            
            if (entity->XJGetUUID() == XJUUID(PreviewCameraUUID))
                continue;
            
            return entity;
        }

        return nullptr;
    }

}