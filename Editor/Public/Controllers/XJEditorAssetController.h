#ifndef XJ_EDITOR_ASSET_CONTROLLER_H
#define XJ_EDITOR_ASSET_CONTROLLER_H

#include <filesystem>

namespace XJ
{
    class XJAssetRegistry;
    struct XJEditorUIState;
    class XJScene;

    class XJEditorAssetController
    {
        public:
            void SetAssetRegistry(XJAssetRegistry* registry);
            void SetRegistryPath(const std::filesystem::path& path);
            void SetRootPath(const std::filesystem::path& path);

            void ProcessRequests(XJEditorUIState& uiState);
            void SetScene(XJScene* scene);

        private:
            XJAssetRegistry* mAssetRegistry = nullptr;
            std::filesystem::path mRegistryPath = "Resource/Config/AssetRegistry.json";
            std::filesystem::path mRootPath = "Resource";

            XJScene* mScene = nullptr;
    };
}

#endif
