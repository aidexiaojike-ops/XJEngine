//默认场景配置与加载
#ifndef XJ_EDITOR_PROJECT_CONFIG_H
#define XJ_EDITOR_PROJECT_CONFIG_H

#include "Asset/XJAsset.h"
#include "Graphic/VulkanCommon.h"

#include <filesystem>

namespace XJ
{
    struct XJEditorProjectConfig
    {
        //资产
        std::filesystem::path ResourceRoot = "Resource";
        //资产注册表
        std::filesystem::path RegistryPath = "Resource/Config/AssetRegistry.json";
        //ui编辑器
        std::filesystem::path UIConfigPath = "Resource/Config/EditorUI.json";
        //默认场景
        std::filesystem::path DefaultScenePath = "Resource/Scenes/Default.xjscene";
        //默认场景
        XJAssetHandle DefaultSceneHandle = 0x10000001ull;
        // 首次创建默认场景时使用的 Mesh。
        XJAssetHandle InitialSceneMeshHandle = 0x20000001ull;
        // Inspector 添加 MeshRenderer 时使用的默认 Mesh。
        XJAssetHandle DefaultComponentMeshHandle = 0x20000002ull;
        //多重采样
        VkSampleCountFlagBits SampleCount = VK_SAMPLE_COUNT_1_BIT;
    };
}

#endif