#include "UI/XJEditorUILayer.h"

#include "UI/Panels/XJContentBrowserPanel.h"
#include "UI/Panels/XJHierarchyPanel.h"
#include "UI/Panels/XJInspectorPanel.h"
#include "UI/Panels/XJDebugConsolePanel.h"

#include <imgui.h>


namespace XJ
{
    XJEditorUILayer::XJEditorUILayer(XJEditorUIState& state)
        : mState(state)
    {
    }

    XJEditorUILayer::~XJEditorUILayer()
    {
        Shutdown();
    }

    void XJEditorUILayer::Init(const std::filesystem::path& configPath)
    {
        mConfigPath = configPath;
        mConfig.Load(mConfigPath);
        // 根据配置设置初始状态
        mState.ShowContentBrowser = mConfig.panels.contentBrowser.visible;
        mState.ShowHierarchy = mConfig.panels.hierarchy.visible;
        mState.ShowInspector = mConfig.panels.inspector.visible;
        mState.ShowDebugConsole = mConfig.panels.debugConsole.visible;
        // 创建面板实例
        mContentBrowser = std::make_unique<XJContentBrowserPanel>(
            mState,
            &mConfig.panels.contentBrowser
        );
        
        mHierarchy = std::make_unique<XJHierarchyPanel>(
            mState,
            &mConfig.panels.hierarchy
        );
        
        mInspector = std::make_unique<XJInspectorPanel>(
            mState,
            &mConfig.panels.inspector
        );
        
        mDebugConsole = std::make_unique<XJDebugConsolePanel>(
            mState,
            &mConfig.panels.debugConsole
        );
    }

    void XJEditorUILayer::DrawUI()
    {
        ImGui::DockSpaceOverViewport(ImGui::GetMainViewport()->ID);// 全局停靠空间
        DrawMainMenuBar();

        if (mContentBrowser) mContentBrowser->DrawUI();
        if (mHierarchy)      mHierarchy->DrawUI();
        if (mInspector)      mInspector->DrawUI();
        if (mDebugConsole)   mDebugConsole->DrawUI();
    }

    void XJEditorUILayer::SaveConfig()
    {
        // 将当前 UI 状态保存到配置对象
        mConfig.panels.inspector.visible = mState.ShowInspector;
        mConfig.panels.contentBrowser.visible = mState.ShowContentBrowser;
        mConfig.panels.hierarchy.visible = mState.ShowHierarchy;    
        mConfig.panels.debugConsole.visible = mState.ShowDebugConsole;

        mConfig.Save(mConfigPath);
    }

    void XJEditorUILayer::Shutdown()
    {
        mContentBrowser.reset();
        mHierarchy.reset();
        mInspector.reset();
        mDebugConsole.reset();
    }


    void XJEditorUILayer::SetAssetRegistry(XJAssetRegistry* registry)
    {
        mState.AssetRegistry = registry;//更新资产注册表
    }
    // 切换面板显示状态的方法，通常绑定到菜单项或快捷键
    void XJEditorUILayer::ToggleContentBrowser()
    {
        mState.ShowContentBrowser = !mState.ShowContentBrowser;
    }

    void XJEditorUILayer::ToggleHierarchy()
    {
        mState.ShowHierarchy = !mState.ShowHierarchy;
    }

    void XJEditorUILayer::ToggleInspector()
    {
        mState.ShowInspector = !mState.ShowInspector;
    }

    void XJEditorUILayer::ToggleDebugConsole()
    {
        mState.ShowDebugConsole = !mState.ShowDebugConsole;
    }

    void XJEditorUILayer::DrawMainMenuBar()
    {
        if (ImGui::BeginMainMenuBar())// 绘制主菜单栏
        {
            
            if(ImGui::BeginMenu("Window"))
            {
                ImGui::MenuItem("Content Browser", nullptr, &mState.ShowContentBrowser);
                ImGui::MenuItem("World Outliner",  nullptr, &mState.ShowHierarchy);
                ImGui::MenuItem("Details",          nullptr, &mState.ShowInspector);
                ImGui::MenuItem("Output Log",       nullptr, &mState.ShowDebugConsole);
                ImGui::EndMenu();// 结束 Window 菜单
            }
            // 可以在这里添加更多菜单，例如 Edit、Help 等
            if(ImGui::BeginMenu("Layout"))
            {
                if (ImGui::MenuItem("Save Layout"))
                {
                    SaveConfig();
                }
                if (ImGui::MenuItem("Reset Layout"))
                {
                    mState.ShowContentBrowser = true;
                    mState.ShowHierarchy      = true;
                    mState.ShowInspector      = true;
                    mState.ShowDebugConsole   = true;
                    SaveConfig();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();// 结束主菜单栏
        }
    }
}