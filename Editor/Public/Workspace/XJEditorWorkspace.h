#ifndef XJ_EDITOR_WORKSPACE_H
#define XJ_EDITOR_WORKSPACE_H

#include "UI/XJEditorSelection.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <vector>

namespace XJ
{
    class XJScene;
    struct XJAssetDragPayload;
    struct XJEditorUIState;
    class XJTexture;
    class XJSampler;

    struct XJEditorWorkspaceInitInfo
    {
        std::filesystem::path ResourceRoot = "Resource";
        std::filesystem::path RegistryPath =
            "Resource/Config/AssetRegistry.json";
        std::filesystem::path DefaultScenePath =
            "Resource/Scenes/Default.xjscene";

        XJAssetHandle DefaultSceneHandle = 0;
        XJAssetHandle InitialSceneMeshHandle = 0;
        XJAssetHandle DefaultComponentMeshHandle = 0;
    };

    struct XJEditorWorkspaceSceneHooks
    {
        std::function<void(
            XJScene&,
            const std::vector<XJEditorEntityId>&)>
            BeforeDelete;

        std::function<void()> BeforeOpen;
        std::function<void(XJScene&)> AfterOpen;
        std::function<void()> AfterMutation;

        std::function<bool(XJEditorEntityId)> CanDeleteEntity;
        std::function<bool(XJEditorEntityId)> ShouldExposeEntity;
    };

    class XJEditorWorkspace
    {
        public:
            XJEditorWorkspace();
            ~XJEditorWorkspace();

            XJEditorWorkspace(const XJEditorWorkspace&) = delete;
            XJEditorWorkspace& operator=(const XJEditorWorkspace&) = delete;

            bool Init(const XJEditorWorkspaceInitInfo& info);
            bool AttachScene(XJScene& scene);
            void DetachScene(XJScene* scene);

            void Update();
            void Shutdown();

            XJEditorUIState& GetUIState();
            const XJEditorUIState& GetUIState() const;
            
            void SetDefaultResources(std::shared_ptr<XJTexture> texture, std::shared_ptr<XJSampler> sampler);
            void SetSceneHooks(XJEditorWorkspaceSceneHooks hooks);
            void ClearSceneHooks();
            bool HandleSceneAssetDrop(const XJAssetDragPayload& payload);
            void SelectEntityFromViewportRay(
                const glm::vec3& rayOrigin,
                const glm::vec3& rayDirection,
                float maxDistance);

        private:
            class Impl;
            std::unique_ptr<Impl> mImpl;
    };
}

#endif
