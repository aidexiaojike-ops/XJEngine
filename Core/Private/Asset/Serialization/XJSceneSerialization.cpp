#include "Asset/Serialization/XJSceneSerialization.h"
#include "Asset/Importer/XJGltfImporter.h"
#include "ECS/XJScene.h"
#include "ECS/XJEntity.h"
#include "ECS/Component/XJTransformComponent.h"
#include "ECS/Component/Material/XJUnlitMaterialComponent.h"
#include "Render/Resource/XJMeshFactory.h"
#include "Render/Resource/XJMaterialFactory.h"
#include "Render/Resource/XJTexture.h"
#include <tiny_gltf.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/quaternion.hpp>
#include <spdlog/spdlog.h>

namespace XJ
{
   std::unordered_map<std::string, std::shared_ptr<XJMesh>> XJSceneSerialization::mMeshCache;
   void XJSceneSerialization::ProcessNode(XJScene* scene, const tinygltf::Model& model, 
                                        int knodeIdx,const glm::mat4& parentTransform,const std::shared_ptr<XJTexture>& defaultTex,const std::shared_ptr<XJSampler>& defaultSampler)
   {
        const auto& kNode = model.nodes[knodeIdx];
        glm::mat4 kLocal = glm::mat4(1.0f);
        if (kNode.matrix.size() == 16)
        {
            const auto& m = kNode.matrix;
            kLocal = glm::mat4(
                m[0], m[1], m[2],  m[3],
                m[4], m[5], m[6],  m[7],
                m[8], m[9], m[10], m[11],
                m[12],m[13],m[14], m[15]
            );
        }
        else
        {
            if (kNode.translation.size() == 3)
                kLocal = glm::translate(kLocal, glm::vec3(kNode.translation[0], kNode.translation[1], kNode.translation[2]));
            if (kNode.rotation.size() == 4)
                kLocal = kLocal * glm::toMat4(glm::quat((float)kNode.rotation[3], (float)kNode.rotation[0], (float)kNode.rotation[1], (float)kNode.rotation[2]));
            if (kNode.scale.size() == 3)
                kLocal = glm::scale(kLocal, glm::vec3(kNode.scale[0], kNode.scale[1], kNode.scale[2]));
        }

        glm::mat4 kWorld = parentTransform * kLocal;

        // 有 mesh → 创建 entity
        if (kNode.mesh >= 0)
        {
            auto asset = XJGltfImporter::ExtractMesh(model, kNode.mesh);
            if (asset && !asset->mVertices.empty())
            {
                // 用 gltfPath 作为 cache key（从调用方传入，这里简化）
                std::string kCacheKey = std::to_string(reinterpret_cast<uintptr_t>(&model))
                                            + "_" + std::to_string(kNode.mesh);

                // Mesh Cache：不重复 GPU 上传同一 mesh
                if (!mMeshCache.count(kCacheKey))
                    mMeshCache[kCacheKey] = XJMeshFactory::CreateFromAsset(*asset);
                auto& gpuMesh = mMeshCache[kCacheKey];

                if (gpuMesh)
                {
                    auto kMat = XJ::XJMaterialFactory::GetInstance()->CreateMaterial<XJUnlitMaterial>();
                    kMat->XJSetBaseColorA(glm::vec3(0.8f, 0.6f, 0.2f));
                    kMat->XJSetBaseColorB(glm::vec3(0.8f, 0.6f, 0.2f));
                    kMat->XJSetTextureView(UNLIT_MAT_BASE_COLOR_A, defaultTex, defaultSampler);
                    kMat->UpdateTextureViewEnable(UNLIT_MAT_BASE_COLOR_A, false);
                    kMat->XJSetTextureView(UNLIT_MAT_BASE_COLOR_B, defaultTex, defaultSampler);
                    kMat->UpdateTextureViewEnable(UNLIT_MAT_BASE_COLOR_B, false);

                    auto entity = scene->CreateEntity(!kNode.name.empty() ? kNode.name : "glTF_KNode");
                    entity->AddComponent<XJUnlitMaterialComponent>();
                    auto& comp = entity->GetComponent<XJUnlitMaterialComponent>();
                    comp.AddMesh(gpuMesh.get(), kMat.get());

                    auto& trans = entity->GetComponent<XJTransformComponent>();
                    trans.position = glm::vec3(kWorld[3]);
                    trans.scale   = glm::vec3(1.0f); // 简化：先不提取 scale
                    trans.UpdateModelMatrix();

                    spdlog::info("glTF entity '{}': {} vertices", kNode.name, asset->mVertices.size());
                }
            }
        }

        // 递归子节点
        for (int child : kNode.children)
            ProcessNode(scene, model, child, kWorld, defaultTex, defaultSampler);
   }

    bool XJSceneSerialization::InstantiateGltfScene(XJScene* outScene,
        XJGltfImporter& importer,
        const std::shared_ptr<XJTexture>& defaultTex,
        const std::shared_ptr<XJSampler>& defaultSampler)
    {
        const auto& model = importer.XJGetModel();
        for (const auto& gltfScene : model.scenes)
            for (int root : gltfScene.nodes)
                ProcessNode(outScene, model, root, glm::mat4(1.0f), defaultTex, defaultSampler);
        return true;
    }
}