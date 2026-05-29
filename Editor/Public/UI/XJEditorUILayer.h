//管理 负责统一 DrawUI，管理所有 Panel，共享选中状态
#ifndef XJ_EDITOR_UI_LAYER_H
#define XJ_EDITOR_UI_LAYER_H

#include "UI/XJEditorUIConfig.h"
#include "Asset/XJAsset.h"
#include <memory>
#include <filesystem>
#include "UI/XJEditorUIState.h"

namespace XJ
{
    class XJScene;
    class XJAssetRegistry;
    // 面板
    class XJContentBrowserPanel;
    class XJHierarchyPanel;
    class XJInspectorPanel;
    class XJDebugConsolePanel;

    class XJEditorUILayer
    {
        public:
            XJEditorUILayer(XJEditorUIState& state);
            ~XJEditorUILayer();

            void Init(const std::filesystem::path& configPath);
            void DrawUI();
            void SaveConfig();// 将当前 UI 配置保存到磁盘
            void Shutdown();

            void SetScene(XJScene* scene);// 设置当前编辑的场景
            void SetAssetRegistry(XJAssetRegistry* registry);// 设置资产注册表

            void ToggleContentBrowser();
            void ToggleHierarchy();
            void ToggleInspector();
            void ToggleDebugConsole();
             // 其他 UI 相关方法，例如打开/关闭面板、更新选中状态等

        private:
            void DrawMainMenuBar();// 绘制主菜单栏

            XJEditorUIState& mState;// 对编辑器 UI 状态的引用，允许在各个面板之间共享和修改状态
            XJEditorUIConfig mConfig;// 从 JSON 文件加载的 UI 配置，包含布局和面板设置
            std::filesystem::path mConfigPath;// UI 配置文件的路径
            // 面板实例
            std::unique_ptr<XJContentBrowserPanel> mContentBrowser;
            std::unique_ptr<XJHierarchyPanel> mHierarchy;
            std::unique_ptr<XJInspectorPanel> mInspector;
            std::unique_ptr<XJDebugConsolePanel> mDebugConsole;
    };
}

#endif