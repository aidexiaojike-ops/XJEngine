#include "Render/Material/XJSurfaceMaterialRenderItemBuilder.h"

#include "ECS/XJScene.h"
#include "ECS/Component/XJTransformComponent.h"
#include "ECS/Component/Material/XJSurfaceMaterialComponent.h"
#include "Render/Resource/XJMesh.h"

namespace XJ
{
    std::vector<XJMaterialRenderItem> XJSurfaceMaterialRenderItemBuilder::Build(XJScene& scene)
    {
        std::vector<XJMaterialRenderItem> items;
        Build(scene, items);
        return items;
    }
    
    void XJSurfaceMaterialRenderItemBuilder::Build(XJScene& scene, std::vector<XJMaterialRenderItem>& outItems)
    {
        outItems.clear();

        const auto& registry = scene.XJGetEcsRegistry();
        auto view = registry.view<XJTransformComponent, XJSurfaceMaterialComponent>();

        view.each([&outItems](const auto& entity, const XJTransformComponent& transComp, const XJSurfaceMaterialComponent& matComp)
        {
            (void)entity;

            for (const auto& slot : matComp.XJGetSlots())
            {
                if (!slot.Mesh || !slot.Material)
                    continue;
            
                XJSurfaceMaterial* material = slot.Material.get();
                if (material->XJGetIndex() < 0)
                    continue;
            
                XJMaterialRenderItem item{};
                item.Material = material;
                item.Mesh = slot.Mesh;
                item.SubmeshIndex = slot.SubmeshIndex;
                item.ModelMatrix = transComp.GetModelMatrix();
            
                outItems.push_back(item);
            }
        });
    }
}
