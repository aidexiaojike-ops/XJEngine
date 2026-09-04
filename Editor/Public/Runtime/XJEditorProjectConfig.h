#ifndef XJ_EDITOR_PROJECT_CONFIG_H
#define XJ_EDITOR_PROJECT_CONFIG_H

#include "Asset/XJAsset.h"
#include "Graphic/VulkanCommon.h"

#include <filesystem>

namespace XJ
{
    struct XJEditorProjectPaths
    {
        // 项目源码根目录，例如 E:/VSCode/XJEngine。
        std::filesystem::path ProjectRoot;

        // 可提交 Git 的源资产目录。
        std::filesystem::path ProjectResourceRoot;

        // exe 所在运行目录，例如 E:/VSCode/XJEngine/bin。
        std::filesystem::path RuntimeRoot;

        // 构建/打包后的运行时资源目录。
        std::filesystem::path RuntimeResourceRoot;

        // 以下文件由编辑器修改，应位于 ProjectResourceRoot。
        std::filesystem::path RegistryPath;
        std::filesystem::path DefaultScenePath;

        // 编辑器 UI 初始模板（提交进 Git），位于 ProjectResourceRoot。
        std::filesystem::path InitialUIConfigPath;

        // 编辑器 UI 运行状态，属于用户数据，放在 RuntimeRoot/Saved。
        std::filesystem::path UIConfigPath;

        // ImGui ini 属于用户运行状态，放在 RuntimeRoot/Saved。
        std::filesystem::path ImGuiIniPath;

        bool IsValid() const
        {
            return !ProjectRoot.empty() &&
                   !ProjectResourceRoot.empty() &&
                   !RuntimeRoot.empty() &&
                   !RuntimeResourceRoot.empty() &&
                   !RegistryPath.empty() &&
                   !InitialUIConfigPath.empty() &&
                   !UIConfigPath.empty() &&
                   !DefaultScenePath.empty() &&
                   !ImGuiIniPath.empty();
        }

        static XJEditorProjectPaths FromRoots(
            const std::filesystem::path& projectRoot,
            const std::filesystem::path& runtimeRoot)
        {
            XJEditorProjectPaths paths;

            paths.ProjectRoot =
                projectRoot.lexically_normal();

            paths.ProjectResourceRoot =
                (paths.ProjectRoot / "Resource")
                    .lexically_normal();

            paths.RuntimeRoot =
                runtimeRoot.lexically_normal();

            paths.RuntimeResourceRoot =
                (paths.RuntimeRoot / "Resource")
                    .lexically_normal();

            paths.RegistryPath =
                (paths.ProjectResourceRoot /
                 "Config/AssetRegistry.json")
                    .lexically_normal();

            paths.InitialUIConfigPath =
                (paths.ProjectResourceRoot /
                 "Config/EditorUI.json")
                    .lexically_normal();

            paths.UIConfigPath =
                (paths.RuntimeRoot /
                 "Saved/Config/EditorUI.json")
                    .lexically_normal();

            paths.DefaultScenePath =
                (paths.ProjectResourceRoot /
                 "Scenes/Default.xjscene")
                    .lexically_normal();

            paths.ImGuiIniPath =
                (paths.RuntimeRoot /
                 "Saved/Config/imgui.ini")
                    .lexically_normal();

            return paths;
        }
    };

    struct XJEditorProjectConfig
    {
        XJEditorProjectPaths Paths;

        XJAssetHandle DefaultSceneHandle =
            0x10000001ull;

        XJAssetHandle InitialSceneMeshHandle =
            0x20000001ull;

        XJAssetHandle DefaultComponentMeshHandle =
            0x20000002ull;

        VkSampleCountFlagBits SampleCount =
            VK_SAMPLE_COUNT_1_BIT;

        bool IsValid() const
        {
            return Paths.IsValid() &&
                   DefaultSceneHandle != 0 &&
                   InitialSceneMeshHandle != 0 &&
                   DefaultComponentMeshHandle != 0;
        }
    };
}

#endif
