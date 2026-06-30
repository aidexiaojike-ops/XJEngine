//资产  读取 AssetRegistry，显示资产列表，类似 UE 内容浏览器
#ifndef XJ_CONTENT_BROWSER_PANEL_H
#define XJ_CONTENT_BROWSER_PANEL_H

#include "Asset/XJAsset.h"
#include "UI/XJEditorAssetRequests.h"
#include "UI/XJEditorUIConfig.h"

#include <imgui.h>
#include <filesystem>
#include <string>
#include "UI/XJEditorSelection.h"

namespace XJ
{
    class XJEditorUIState;

    class XJContentBrowserPanel
    {
        public:
            XJContentBrowserPanel(XJEditorUIState& state, XJEditorPanelConfig_ContentBrowser* config);
            ~XJContentBrowserPanel();


            void DrawUI(); 
        private:
            void DrawToolbar();//显示工具栏，包含搜索框和过滤选项
            void DrawFolderTree();//显示文件夹树，允许用户浏览和选择不同的文件夹
            void DrawAssetTable();//显示资产列表，支持按类型、名称等属性进行排序和过滤
            void DrawFolderNode(const std::filesystem::path& folderPath);//显示文件夹节点，允许用户展开/收起以浏览子文件夹和资产
            void DrawCurrentFolderContent();//显示当前文件夹下的资产和子文件夹，支持双击打开资产或进入子文件夹

            void HandleSelection(XJAssetHandle handle);//处理用户选择资产的逻辑，例如更新编辑器状态中的 SelectedAsset，或在 Inspector 面板中显示选中资产的详细信息
            void SetCurrentPath(const std::filesystem::path& path);//设置当前浏览路径，更新内容浏览器显示的资产列表
            void BeginRenameAsset(XJAssetHandle handle, const std::string& currentName);//开始行内重命名资产
            void SubmitRenameAsset(XJAssetHandle handle);//提交资产重命名请求

            void OpenCreateFolderPopup(const std::filesystem::path& parent);//打开创建文件夹的弹窗，允许用户输入新文件夹的名称，并在指定父目录下创建新文件夹
            void DrawCreateFolderPopup();//绘制创建文件夹的弹窗 UI，包含输入框和确认/取消按钮
            void TryCreateFolder(const std::filesystem::path& parent, const std::string& folderName);//尝试创建新文件夹，检查名称合法性和是否已存在同名文件夹，如果成功则在父目录下创建新文件夹
            void TryDeleteFolder(const std::filesystem::path& folderPath);//尝试删除文件夹，检查文件夹是否为空或是否存在，如果成功则删除指定路径的文件夹    
            void RequestCreateAsset(XJEditorCreateAssetType type, const std::filesystem::path& directory);//请求创建资产
           
           
            static const char* AssetTypeToString(XJAssetType type);//将资产类型枚举转换为字符串，供 UI 显示使用

            
        private:
            XJEditorUIState& mState;
            XJEditorPanelConfig_ContentBrowser* mConfig = nullptr;

            std::filesystem::path mPendingCreateFolderParent;//当前浏览路径，初始值为配置中的 rootPath
            char mNewFolderNameBuffer[256] = {};//用于创建新文件夹的输入缓冲区
            bool mRequestOpenCreateFolderPopup = false;//标记是否请求打开创建文件夹的弹窗

            bool mPendingDeleteFolder = false;//标记是否有待删除的文件夹
            std::filesystem::path mPendingDeleteFolderPath;//待删除的文件夹路径

            XJAssetHandle mRenamingAsset = 0;
            char mRenameAssetBuffer[256] = {};
            bool mFocusRenameAssetInput = false;

            void HandleExternalFileDrop(const ImVec2& windowMin, const ImVec2& windowMax);
            std::filesystem::path GetCurrentAssetDirectory() const;

        
            
    };
}


#endif
