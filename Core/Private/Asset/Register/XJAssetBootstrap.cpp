#include "Asset/Register/XJAssetBootstrap.h"
#include "Asset/Serialization/XJSceneAssetSerializer.h"
#include <filesystem>
#include "spdlog/spdlog.h"

namespace XJ
{
    void XJAssetBootstrap::RegisterBootstrapAssets()
    {
        mAssetRegistry.RegisterAsset
        ({
            mDefaultSceneHandle,
            XJ::XJAssetType::Scene,
            "DefaultScene",
            "Resource/Scenes/Default.xjscene",
            {}
        });

        mAssetRegistry.RegisterAsset
        ({
            mMonkeyMeshHandle,
            XJ::XJAssetType::Mesh,
            "Monkey",
            "Resource/Mesh/Monkey.glb",
            {}
        });
    }

    void XJAssetBootstrap::LoadOrCreateAssetRegistry()
    {
        const std::filesystem::path registryPath = "Resource/Config/AssetRegistry.json";
        if (std::filesystem::exists(registryPath) && mAssetRegistry.Load(registryPath))
            return;

        RegisterBootstrapAssets();
        mAssetRegistry.Save(registryPath);
    }
    std::shared_ptr<XJ::XJSceneAsset> XJAssetBootstrap::LoadOrCreateDefaultSceneAsset()
    {
        const std::filesystem::path scenePath = "Resource/Scenes/Default.xjscene";//读取场景json文件
        if (std::filesystem::exists(scenePath))
        {
            auto defaultScene = XJSceneAssetSerializer::LoadFromFile(scenePath);
            if (defaultScene)
                return defaultScene;
        }
         // 不存在则创建默认空场景并保存
        auto loaded = std::make_shared<XJSceneAsset>();
        XJSceneAssetSerializer::SaveToFile(*loaded, scenePath);

        return loaded;
    }
    //std::shared_ptr<XJ::XJSceneAsset> XJSceneSerialization::CreateDefaultSceneAsset()
    //{
    //    auto sceneAsset = std::make_shared<XJ::XJSceneAsset>();
    //    sceneAsset->mHandle = kDefaultSceneHandle;
    //    sceneAsset->mName = "DefaultScene";
    //    sceneAsset->mPath = "Resource/Scenes/Default.xjscene";

    //    XJ::XJSceneEntityData previewCamera;
    //    previewCamera.Id = kPreviewCameraEntityId;
    //    previewCamera.Name = "PreviewCamera";
    //    previewCamera.Transform.Position = glm::vec3(0.0f, 1.5f, 3.0f);
    //    previewCamera.Camera.Enabled = true;
    //    previewCamera.Camera.Fov = 65.0f;
    //    previewCamera.Camera.NearClip = 0.1f;
    //    previewCamera.Camera.FarClip = 100.0f;

    //    XJ::XJSceneEntityData gameCamera;
    //    gameCamera.Id = kGameCameraEntityId;
    //    gameCamera.Name = "GameCamera";
    //    gameCamera.Transform.Position = glm::vec3(0.0f, 1.0f, 3.0f);
    //    gameCamera.Camera.Enabled = true;
    //    gameCamera.Camera.Primary = true;
    //    gameCamera.Camera.Fov = 65.0f;
    //    gameCamera.Camera.NearClip = 0.1f;
    //    gameCamera.Camera.FarClip = 100.0f;

    //    XJ::XJSceneEntityData monkey;
    //    monkey.Id = kMonkeyEntityId;
    //    monkey.Name = "Monkey";
    //    monkey.Transform.Position = glm::vec3(0.0f, 0.0f, 0.0f);
    //    monkey.Transform.Scale = glm::vec3(1.0f);
    //    monkey.MeshRenderer.Mesh = { kMonkeyMeshHandle, XJ::XJAssetType::Mesh };

    //    sceneAsset->Entities = { previewCamera, gameCamera, monkey };
    //    return sceneAsset;
    //}
}