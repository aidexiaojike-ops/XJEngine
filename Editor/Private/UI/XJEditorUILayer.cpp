#include "UI/XJEditorUILayer.h"

#include "UI/Panels/XJContentBrowserPanel.h"
#include "UI/Panels/XJHierarchyPanel.h"
#include "UI/Panels/XJInspectorPanel.h"
#include "UI/Panels/XJDebugConsolePanel.h"
#include "UI/XJEditorLog.h"

#include <imgui.h>
#include <spdlog/spdlog.h>
#include <algorithm>
#include <cctype>


namespace XJ
{
    namespace
    {
        bool PathComponentEquals(
            const std::filesystem::path& left,
            const std::filesystem::path& right)
        {
            std::string leftText = left.generic_string();
            std::string rightText = right.generic_string();

#ifdef _WIN32
            auto toLower = [](std::string& value)
            {
                std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch)
                {
                    return static_cast<char>(std::tolower(ch));
                });
            };
            toLower(leftText);
            toLower(rightText);
#endif

            return leftText == rightText;
        }

        bool IsPathInsideRoot(
            const std::filesystem::path& candidate,
            const std::filesystem::path& root)
        {
            const auto normalizedCandidate = candidate.lexically_normal();
            const auto normalizedRoot = root.lexically_normal();
            auto candidateIt = normalizedCandidate.begin();

            for (auto rootIt = normalizedRoot.begin(); rootIt != normalizedRoot.end(); ++rootIt, ++candidateIt)
            {
                if (candidateIt == normalizedCandidate.end() ||
                    !PathComponentEquals(*candidateIt, *rootIt))
                {
                    return false;
                }
            }

            return true;
        }

        std::filesystem::path ResolveContentBrowserPath(
            const std::filesystem::path& storedPath,
            const std::filesystem::path& projectResourceRoot)
        {
            const auto root = projectResourceRoot.lexically_normal();
            if (storedPath.empty())
                return root;

            if (storedPath.is_absolute())
            {
                const auto normalized = storedPath.lexically_normal();
                return IsPathInsideRoot(normalized, root) ? normalized : root;
            }

            std::filesystem::path relative = storedPath.lexically_normal();
            if (!relative.empty() && PathComponentEquals(*relative.begin(), "Resource"))
            {
                std::filesystem::path withoutResource;
                auto it = relative.begin();
                ++it;
                for (; it != relative.end(); ++it)
                    withoutResource /= *it;
                relative = withoutResource;
            }

            const auto resolved = (root / relative).lexically_normal();
            return IsPathInsideRoot(resolved, root) ? resolved : root;
        }

        std::filesystem::path MakePortableContentPath(
            const std::filesystem::path& absolutePath,
            const std::filesystem::path& projectResourceRoot)
        {
            if (!IsPathInsideRoot(absolutePath, projectResourceRoot))
                return "Resource";

            const auto relative = absolutePath.lexically_normal().lexically_relative(
                projectResourceRoot.lexically_normal());

            return relative.empty() || relative == "."
                ? std::filesystem::path("Resource")
                : std::filesystem::path("Resource") / relative;
        }
    }

    XJEditorUILayer::XJEditorUILayer(XJEditorUIState& state)
        : mState(state)
    {
    }

    XJEditorUILayer::~XJEditorUILayer()
    {
        Shutdown();
    }

    void XJEditorUILayer::Init(
        const std::filesystem::path& configPath,
        const std::filesystem::path& projectResourceRoot)
    {
        Shutdown();

        if (configPath.empty() || projectResourceRoot.empty())
        {
            spdlog::error("Editor UI Layer initialization failed: path is empty.");
            return;
        }

        // spdlog 已由 XJApplication 初始化。Editor 只追加一个内存 sink，
        // 不修改现有控制台和轮转文件 sink。
        XJEditorLog::XJGet().AttachToSpdlog();

        mConfig = {};
        mConfigPath = configPath.lexically_normal();
        mProjectResourceRoot = projectResourceRoot.lexically_normal();
        mConfig.Load(mConfigPath);

        const auto currentPath = ResolveContentBrowserPath(
            mConfig.panels.contentBrowser.currentPath,
            mProjectResourceRoot);

        // Panel 运行时必须使用绝对项目路径；持久化时再转换回 Resource/...。
        mConfig.panels.contentBrowser.rootPath = mProjectResourceRoot.generic_string();
        mConfig.panels.contentBrowser.currentPath = currentPath.generic_string();
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

        // 文本框有自己的撤销栈，输入文字时不抢占 Ctrl+Z/Ctrl+Y。
        if (!ImGui::GetIO().WantTextInput)
        {
            if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Z))
                mState.SceneRequests.RequestUndo = true;

            if (ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiKey_Y) ||
                ImGui::Shortcut(ImGuiMod_Ctrl | ImGuiMod_Shift | ImGuiKey_Z))
            {
                mState.SceneRequests.RequestRedo = true;
            }
        }

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

        if (mConfigPath.empty() || mProjectResourceRoot.empty())
            return;

        XJEditorUIConfig persistedConfig = mConfig;
        persistedConfig.panels.contentBrowser.rootPath = "Resource";
        persistedConfig.panels.contentBrowser.currentPath = MakePortableContentPath(
            mConfig.panels.contentBrowser.currentPath,
            mProjectResourceRoot).generic_string();
        persistedConfig.layout.imguiIniPath = "Saved/Config/imgui.ini";

        if (!persistedConfig.Save(mConfigPath))
            spdlog::error("Failed to save Editor UI config '{}'.", mConfigPath.string());
    }

    void XJEditorUILayer::Shutdown()
    {
        mContentBrowser.reset();
        mHierarchy.reset();
        mInspector.reset();
        mDebugConsole.reset();

         // UI 面板销毁后不再收集日志，并避免重复创建 UI 时重复安装 sink。
        XJEditorLog::XJGet().DetachFromSpdlog();
        mProjectResourceRoot.clear();
        mConfigPath.clear();
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
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Save Scene"))
                {
                    mState.SceneRequests.RequestSaveScene = true;
                }
            
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Edit"))
            {
                if (ImGui::MenuItem("Undo", "Ctrl+Z"))
                    mState.SceneRequests.RequestUndo = true;

                if (ImGui::MenuItem("Redo", "Ctrl+Y"))
                    mState.SceneRequests.RequestRedo = true;

                ImGui::EndMenu();
            }
            
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
