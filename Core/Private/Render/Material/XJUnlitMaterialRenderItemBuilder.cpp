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

        entt::registry& registry = scene.XJGetEcsRegistry();
        auto view = registry.view<XJTransformComponent, XJUnlitMaterialComponent>();

        view.each([&items](const auto& entity, const XJTransformComponent& transComp, const XJUnlitMaterialComponent& matComp)
        {
            (void)entity;

            auto meshMaterials = matComp.XJGetMeshMaterials();

            for (const auto& entry : meshMaterials)
            {
                XJUnlitMaterial* material = entry.first;
                if (!material || material->XJGetIndex() < 0)
                    continue;

                for (const auto& meshIndex : entry.second)
                {
                    XJMesh* mesh = matComp.XJGetMesh(meshIndex);
                    if (!mesh)
                        continue;

                    XJMaterialRenderItem item{};
                    item.Material = material;
                    item.Mesh = mesh;
                    item.ModelMatrix = transComp.GetModelMatrix();

                    items.push_back(item);
                }
            }
        });

        return items;
    }
}