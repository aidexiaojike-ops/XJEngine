#include "Render/Material/XJUnlitMaterialRenderItemBuilder.h"

#include "ECS/XJScene.h"
#include "ECS/Component/XJTransformComponent.h"
#include "ECS/Component/Material/XJUnlitMaterialComponent.h"
#include "Render/Resource/XJMesh.h"

namespace XJ
{
    std::vector<XJMaterialRenderItem> XJUnlitMaterialRenderItemBuilder::Build(XJScene& scene)
    {
        std::vector<XJMaterialRenderItem> items;
        Build(scene, items);
        return items;
    }
    
    void XJUnlitMaterialRenderItemBuilder::Build(XJScene& scene, std::vector<XJMaterialRenderItem>& outItems)
    {
        outItems.clear();

        const auto& registry = scene.XJGetEcsRegistry();
        auto view = registry.view<XJTransformComponent, XJUnlitMaterialComponent>();

        view.each([&outItems](const auto& entity, const XJTransformComponent& transComp, const XJUnlitMaterialComponent& matComp)
        {
            (void)entity;

            for (const auto& slot : matComp.XJGetSlots())
            {
                if (!slot.Mesh || !slot.Material)
                    continue;
            
                XJUnlitMaterial* material = slot.Material.get();
                if (material->XJGetIndex() < 0)
                    continue;
            
                XJMaterialRenderItem item{};
                item.Material = material;
                item.Mesh = slot.Mesh;
                item.ModelMatrix = transComp.GetModelMatrix();
            
                outItems.push_back(item);
            }
        });
    }
}
