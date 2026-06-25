#include "UI/Panels/XJContentBrowserPanel.h"
#include "UI/XJEditorAssetDragPayload.h"

#include "UI/XJEditorUILayer.h"
#include "Asset/XJAssetRegistry.h"
#include "Asset/XJAsset.h"
#include "Asset/XJMaterialAsset.h"
#include "Asset/XJSceneAsset.h"
#include "Asset/Serialization/XJMaterialAssetSerializer.h"
#include "Asset/Serialization/XJSceneAssetSerializer.h"
#include "Asset/Register/XJAssetRegistryScanner.h"

#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <vector>
#include <string>


#ifdef _WIN32
#include <windows.h>
#endif

namespace XJ
{

    static void OpenFolderInExplorer(const std::filesystem::path& folderPath)
    {
        #ifdef _WIN32
            std::filesystem::path absPath = std::filesystem::absolute(folderPath);
            ShellExecuteW(
                nullptr,
                L"open",
                absPath.wstring().c_str(),
                nullptr,
                nullptr,
                SW_SHOWNORMAL
            );
        #endif
    }

    XJContentBrowserPanel::XJContentBrowserPanel(XJEditorUIState& state, XJEditorPanelConfig_ContentBrowser* config)
        : mState(state),mConfig(config)
    {
    }

    XJContentBrowserPanel::~XJContentBrowserPanel()
    {
    }

    void XJContentBrowserPanel::DrawUI()
    {
        if (!mState.ShowContentBrowser)
        {
            return;
            /* code */
        }
        const char* title = mConfig ? mConfig->title.c_str() : "Content Browser";

        ImGui::Begin(title);

        ImVec2 windowMin = ImGui::GetWindowPos();
        ImVec2 windowSize = ImGui::GetWindowSize();
        ImVec2 windowMax = ImVec2(windowMin.x + windowSize.x, windowMin.y + windowSize.y);

        if(ImGui::BeginTable("ContentBrowserLayout", 2,  ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("FolderTree", ImGuiTableColumnFlags_WidthFixed, 250.0f);
            ImGui::TableSetupColumn("ContentArea", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextRow();//下一行

            // Left Column: Folder Tree
            ImGui::TableSetColumnIndex(0);
            DrawFolderTree();

            // Right Column: Asset Table
            ImGui::TableSetColumnIndex(1);
            DrawCurrentFolderContent();

            ImGui::EndTable();
        }
        //DrawToolbar();
        //DrawAssetTable();
        DrawCreateFolderPopup();
        HandleExternalFileDrop(windowMin, windowMax);

        ImGui::End();
        
    }

    void XJContentBrowserPanel::DrawToolbar()
    {
        if (!mConfig)
            return;

        char filterBuffer[256] = {};
        std::strncpy(filterBuffer, mConfig->filter.c_str(), sizeof(filterBuffer) - 1);

        ImGui::PushItemWidth(200);
        if (ImGui::InputTextWithHint("##AssetFilter", "Search assets...", filterBuffer, sizeof(filterBuffer)))
            mConfig->filter = filterBuffer;
        ImGui::PopItemWidth();

        ImGui::SameLine();
        if (ImGui::Button("Clear"))
            mConfig->filter.clear();
        
        ImGui::SameLine();
        if (ImGui::Button("Refresh")&& mState.AssetRegistry)
        {
            XJAssetRegistryScanner::ScanResourceAssets(*mState.AssetRegistry, "Resource");
            mState.AssetRegistry->Save("Resource/Config/AssetRegistry.json");
        }
        
        ImGui::SameLine();
        ImGui::Checkbox("Show Type", &mConfig->showAssetType);

        ImGui::SameLine();
        ImGui::Checkbox("Show Handle", &mConfig->showHandle);

        ImGui::Separator();
    }

    void XJContentBrowserPanel::DrawAssetTable()
    {
        XJAssetRegistry* registry = mState.AssetRegistry;
        if (!registry)        
        {
            ImGui::Text("No Asset Registry");
            return;
        }

        const auto& metas = registry->XJGetAllMetas();
        if (metas.empty())
        {
            ImGui::TextDisabled("No assets registered");
            return;
        }
        const std::string filter = mConfig ? mConfig->filter : "";
        std::filesystem::path currentPath = mConfig ? std::filesystem::path(mConfig->currentPath) : std::filesystem::path();

        ImGuiTableFlags tableFlags = ImGuiTableFlags_BordersV//显示垂直边框
                                   | ImGuiTableFlags_BordersOuterH//显示水平外边框
                                   | ImGuiTableFlags_Resizable//允许调整列宽
                                   | ImGuiTableFlags_RowBg//交替行背景色
                                   | ImGuiTableFlags_ScrollY;//启用垂直滚动
                                   
        bool pendingDeleteAsset = false;
        XJAssetHandle pendingDeleteHandle = 0;
        std::filesystem::path pendingDeletePath;
        if(ImGui::BeginTable("AssetTable", 4, tableFlags))
        {
           
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);//名称列占满剩余空间
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80.0f);//类型列固定宽度
            ImGui::TableSetupColumn("Handle", ImGuiTableColumnFlags_WidthFixed, 180.0f);//句柄列固定宽度
            ImGui::TableSetupColumn("Source Path", ImGuiTableColumnFlags_WidthStretch);//源路径列占满剩余空间
            ImGui::TableHeadersRow();//显示表头

            for (const auto& [handle, meta] : metas)
            {
                std::filesystem::path sourcePath = meta.SourcePath;
                if (!currentPath.empty()  && sourcePath.parent_path() != currentPath)
                    continue;//如果当前路径不为空且资产的源路径的父路径不等于当前路径，则跳过该资产的显示，这样可以实现只显示当前文件夹下的资产

                if (!filter.empty())
                {
                    bool nameMatch = meta.Name.find(filter) != std::string::npos;
                    bool pathMatch = meta.SourcePath.string().find(filter) != std::string::npos;
                
                    if (!nameMatch && !pathMatch)
                        continue;
                }

                ImGui::TableNextRow();
                ImGui::TableNextColumn();   
                // apply filter 这里我们简单地根据资产名称进行过滤，如果用户在搜索框中输入了文本，但资产名称不包含该文本，则跳过该资产的显示
                
                //if (mState.Selection.SelectedAsset == pendingDeleteHandle)
                //{
                //    mState.Selection.SelectedAsset = 0;
                //}
                bool isSelected = (mState.Selection.SelectedAsset == handle);

               if (ImGui::Selectable(meta.Name.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick))
                {
                    HandleSelection(handle);

                    if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && meta.Type == XJAssetType::Scene)//双击场景Json文件
                    {
                        mState.SceneRequests.RequestOpenScene = true;
                        mState.SceneRequests.RequestedScenePath = meta.SourcePath;
                        mState.SceneRequests.RequestedSceneHandle = handle;
                    }
                }
                if(meta.Type == XJAssetType::Mesh && ImGui::BeginDragDropSource())//判断是是否是模型 然后再拖动
                {
                    XJEditorAssetDragPayload payload{};

                    payload.Handle = handle;
                    payload.Type = meta.Type;

                    ImGui::SetDragDropPayload(XJ_ASSET_PAYLOAD_NAME, &payload, sizeof(payload));

                    ImGui::Text("Mesh %s", meta.Name.c_str());
                    ImGui::EndDragDropSource();
                }
                
                 // right-click context menu 右键点击打开上下文菜单，提供选项如 "Select in Inspector" 或 "Delete Asset"
                if (ImGui::BeginPopupContextItem())
                {
                    if (ImGui::BeginMenu("Create"))
                    {
                        std::filesystem::path targetDirectory = currentPath.empty() ? meta.SourcePath.parent_path() : currentPath;
                    
                        if (ImGui::MenuItem("Material"))
                            CreateMaterialAsset(targetDirectory);
                    
                        if (ImGui::MenuItem("Scene"))
                            CreateSceneAsset(targetDirectory);
                    
                        ImGui::EndMenu();
                    }

                    ImGui::Separator();

                    if (ImGui::MenuItem("Find References In Scene"))
                    {
                        mState.SceneRequests.RequestFindEntitiesUsingAsset = handle;
                        mState.Selection.SelectedAsset = handle;
                        mState.Selection.SelectedEntity = XJ_INVALID_EDITOR_ENTITY_ID;
                        mState.Selection.HighlightedEntities.clear();
                    }
                
                    ImGui::Separator();
                
                    if (ImGui::MenuItem("Copy Asset Path"))
                    {
                        ImGui::SetClipboardText(meta.SourcePath.string().c_str());
                    }
                    ImGui::Separator();
                    
                    if (ImGui::MenuItem("Delete Asset"))
                    {
                        pendingDeleteAsset = true;
                        pendingDeleteHandle = handle;
                        pendingDeletePath = meta.SourcePath;
                    }
                
                    ImGui::EndPopup();
                }


                ImGui::TableNextColumn();
                ImGui::TextUnformatted(AssetTypeToString(meta.Type));//显示资产类型，使用 AssetTypeToString 函数将枚举转换为字符串

                ImGui::TableNextColumn();
                ImGui::Text("0x%016llX", static_cast<uint64_t>(handle));//显示资产句柄，格式化为 16 位十六进制数

                ImGui::TableNextColumn();
                ImGui::TextUnformatted(meta.SourcePath.string().c_str());//显示资产的源文件路径
            }

            ImGui::EndTable();
        }
        
        if(pendingDeleteAsset)
        {
            std::error_code ec;
            registry->RemoveAsset(pendingDeleteHandle);

            //if(!pendingDeletePath.empty() && std::filesystem::exists(pendingDeletePath))
            //{
            //    std::filesystem::remove(pendingDeletePath, ec);
            //}
            registry->Save("Resource/Config/AssetRegistry.json");//保存注册表以持久化删除操作

            if(mState.Selection.SelectedAsset == pendingDeleteHandle)
            {
                mState.Selection.SelectedAsset = 0;//如果被删除的资产正被选中，重置选择状态
            }
        }

    }

    const char* XJContentBrowserPanel::AssetTypeToString(XJAssetType type)
    {
        switch (type)
        {
            case XJAssetType::None:     return "None";
            case XJAssetType::Mesh:     return "Mesh";
            case XJAssetType::Texture:  return "Texture";
            case XJAssetType::Material: return "Material";
            case XJAssetType::Scene:    return "Scene";
            case XJAssetType::Shader: return "Shader";
            default:                    return "Unknown";
        }
    }
    void XJContentBrowserPanel::HandleSelection(XJAssetHandle handle)
    {
        mState.Selection.SelectedAsset = handle;
        mState.Selection.SelectedEntity = XJ_INVALID_EDITOR_ENTITY_ID;//切换到资源选择时，清除实体选择
        mState.Selection.HighlightedEntities.clear();
    }
    void XJContentBrowserPanel::DrawFolderTree()//左侧 Resource 文件夹树
    {
        if(!mConfig)
            return;

        std::filesystem::path rootPath = mConfig->rootPath;
        if(!std::filesystem::exists(rootPath))
        {
            ImGui::TextDisabled("Root path not found: %s", rootPath.string().c_str());
            return;
        }

        DrawFolderNode(rootPath);

        if (mPendingDeleteFolder)//删除文件夹的操作不能立即执行，因为可能正在遍历该文件夹的目录结构，这会导致迭代器失效和崩溃。因此我们设置一个标记，在当前帧结束后再执行删除操作，确保安全地修改文件系统
        {
            TryDeleteFolder(mPendingDeleteFolderPath);
            mPendingDeleteFolder = false;
            mPendingDeleteFolderPath.clear();
        }
    }

    void XJContentBrowserPanel::DrawFolderNode(const std::filesystem::path& folderPath)
    {
        bool hasChildren = false;

        std::error_code ec;
        std::filesystem::directory_iterator it(folderPath, ec);
        if (ec)
            return;

        for(const auto& entry : it)
        {
            if(entry.is_directory())
            {
                hasChildren = true;
                break;
            }
        }

        std::string folderName = folderPath.filename().string();
        if(folderName.empty())
            folderName = folderPath.string();//根目录显示完整路径

        // bool selected = mConfig && std::filesystem::equivalent(folderPath, std::filesystem::path(mConfig->currentPath));
        bool selected = mConfig && folderPath.generic_string() == std::filesystem::path(mConfig->currentPath).generic_string();

       // ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;//默认展开/收起行为，节点占满整行
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanFullWidth;//默认展开/收起行为，节点占满整行
        if(!hasChildren)
            flags |= ImGuiTreeNodeFlags_Leaf;//如果没有子文件夹，标记为叶节点
        if(selected)
            flags |= ImGuiTreeNodeFlags_Selected;//如果当前文件夹被选中，标记为选中状态
        
        bool opened = ImGui::TreeNodeEx(folderName.c_str(), flags);
        if(ImGui::IsItemClicked())
        {
            SetCurrentPath(folderPath);
        }

        if(ImGui::BeginPopupContextItem())
        {
            if(ImGui::MenuItem("Create Folder"))
            {
                OpenCreateFolderPopup(folderPath);//打开创建文件夹的弹窗，传入当前文件夹路径作为父路径
            }

            ImGui::Separator();
            if (ImGui::BeginMenu("Create Asset"))
            {
                if (ImGui::MenuItem("Material"))
                    CreateMaterialAsset(folderPath);
            
                if (ImGui::MenuItem("Scene"))
                    CreateSceneAsset(folderPath);
            
                ImGui::EndMenu();
            }

            ImGui::Separator();
            //if(folderPath != std::filesystem::path(mConfig->rootPath) && std::filesystem::is_empty(folderPath))//根目录不能删除，非空文件夹不能删除
            if(folderPath != std::filesystem::path(mConfig->rootPath))//根目录不能删除，非空文件夹不能删除
            {
                if(ImGui::MenuItem("Delete Folder"))
                {
                    //TryDeleteFolder(folderPath);   
                    mPendingDeleteFolder = true;
                    mPendingDeleteFolderPath = folderPath;
                }
            }
            ImGui::Separator();

            if(ImGui::MenuItem("Open Folder"))
            {
                OpenFolderInExplorer(folderPath);
            }
            

            ImGui::EndPopup();
        }

        if (opened)
        {
            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator(folderPath, ec))
            {
                if (entry.is_directory())
                {
                    DrawFolderNode(entry.path());
                }
            }
            ImGui::TreePop();
        }
    }

    void XJContentBrowserPanel::DrawCurrentFolderContent()
    {
        if(!mConfig)
            return;
        //打开对应的文件夹
        if (mState.RequestSelectAssetInContentBrowser && mState.AssetRegistry && mConfig)
        {
            mState.RequestSelectAssetInContentBrowser = false;
        
            XJAssetHandle handle = mState.RequestedContentBrowserAsset;
            mState.RequestedContentBrowserAsset = 0;
        
            auto metaOpt = mState.AssetRegistry->GetMeta(handle);
            if (metaOpt)
            {
                const auto& meta = metaOpt.value();
            
                mConfig->currentPath = meta.SourcePath.parent_path().generic_string();
                mConfig->filter.clear();
            
                mState.Selection.SelectedAsset = handle;
                mState.Selection.SelectedEntity = XJ_INVALID_EDITOR_ENTITY_ID;
                mState.Selection.HighlightedEntities.clear();
            }
        }
    
        ImGui::Text("Path Path: %s", mConfig->currentPath.c_str());
        ImGui::Separator();//分割线
        DrawToolbar();
        DrawAssetTable();

    }

    void XJContentBrowserPanel::SetCurrentPath(const std::filesystem::path& path)
    {
        if (mConfig)
            mConfig->currentPath = path.generic_string();
    }

    void XJContentBrowserPanel::OpenCreateFolderPopup(const std::filesystem::path& parent)
    {
        mPendingCreateFolderParent = parent;
        std::memset(mNewFolderNameBuffer, 0 , sizeof(mNewFolderNameBuffer));//默认新文件夹名称为 "NewFolder"，并确保缓冲区以 null 结尾
        std::strncpy(mNewFolderNameBuffer, "NewFolder", sizeof(mNewFolderNameBuffer) - 1);//将默认文件夹名称复制到输入缓冲区，确保不会溢出
        //ImGui::OpenPopup("Create New Folder");//打开 ImGui 弹窗，标题为 "Create New Folder"
        mRequestOpenCreateFolderPopup = true;
    }

    void XJContentBrowserPanel::DrawCreateFolderPopup()
    {
        if(mRequestOpenCreateFolderPopup)
        {
            ImGui::OpenPopup("Create New Folder");//打开 ImGui 弹窗，标题为 "Create New Folder"
            mRequestOpenCreateFolderPopup = false;
        }
        if(ImGui::BeginPopupModal("Create New Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::InputText("Folder Name", mNewFolderNameBuffer, sizeof(mNewFolderNameBuffer));

            if(ImGui::Button("Create"))
            {
                TryCreateFolder(mPendingCreateFolderParent, mNewFolderNameBuffer);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if(ImGui::Button("Cancel"))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void XJContentBrowserPanel::TryCreateFolder(const std::filesystem::path& parent, const std::string& name)
    {
        if(name.empty())
            return;

        std::filesystem::path newFolderPath = parent / name;
        if(std::filesystem::exists(newFolderPath))
            return;
        
        std::error_code ec;
        std::filesystem::create_directory(newFolderPath, ec);

        if (!ec)
        {
            SetCurrentPath(newFolderPath);
        }
       
    }

    void XJContentBrowserPanel::TryDeleteFolder(const std::filesystem::path& path)
    {
        if(!mConfig)
            return;

        std::filesystem::path rootPath = mConfig->rootPath;

        if(path == rootPath)
            return;

        if(!std::filesystem::exists(path))
            return;
        
        if(!std::filesystem::is_directory(path))
            return;

        if(!std::filesystem::is_empty(path))
        {
            ImGui::OpenPopup("Error");
            if(ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("Folder is not empty!");
                if(ImGui::Button("OK"))
                    ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
            return;
        }

        std::filesystem::remove(path);
         auto current = std::filesystem::path(mConfig->currentPath).generic_string();
        auto deleted = path.generic_string();

        if (current == deleted)
        {
            mConfig->currentPath = rootPath.generic_string();//如果当前浏览路径被删除了，重置为根路径
        }
        
    }
    void XJContentBrowserPanel::HandleExternalFileDrop(const ImVec2& windowMin, const ImVec2& windowMax)
    {
        if (!mState.HasPendingExternalDrop)
            return;

        const glm::vec2 dropPos = mState.PendingExternalDropMousePos;

        const bool insideContentBrowser =
            dropPos.x >= windowMin.x &&
            dropPos.x <= windowMax.x &&
            dropPos.y >= windowMin.y &&
            dropPos.y <= windowMax.y;

        //if (!insideContentBrowser)
        //{
        //    mState.PendingExternalDroppedFiles.clear();
        //    mState.HasPendingExternalDrop = false;
        //    return;
        //}
        if (!insideContentBrowser)
        {
            return;
        }

        auto droppedFiles = std::move(mState.PendingExternalDroppedFiles);
        mState.PendingExternalDroppedFiles.clear();
        mState.HasPendingExternalDrop = false;

        for (const auto& sourcePath : droppedFiles)
        {
            ImportExternalFile(sourcePath);
        }

        if (mState.AssetRegistry)
            mState.AssetRegistry->Save("Resource/Config/AssetRegistry.json");
    }

    std::filesystem::path XJContentBrowserPanel::BuildImportDestinationPath(const std::filesystem::path& sourcePath) const
    {
        std::filesystem::path targetFolder = "Resource";

        if (mConfig && !mConfig->currentPath.empty())
            targetFolder = std::filesystem::path(mConfig->currentPath);

        return targetFolder / sourcePath.filename();
    }

    std::filesystem::path XJContentBrowserPanel::BuildUniqueAssetPath(const std::filesystem::path& directory, const std::string& baseName, const std::string& extension) const
    {
        std::filesystem::path targetDirectory = directory.empty() ? std::filesystem::path("Resource") : directory;

        std::string ext = extension;
        if (!ext.empty() && ext.front() != '.')
            ext = "." + ext;

        std::filesystem::path candidate = targetDirectory / (baseName + ext);

        if (!std::filesystem::exists(candidate) &&
            (!mState.AssetRegistry || !mState.AssetRegistry->ContainsSourcePath(candidate)))
        {
            return candidate;
        }

        for (int index = 1; index < 1000; ++index)
        {
            std::filesystem::path numbered =
                targetDirectory / (baseName + "_" + std::to_string(index) + ext);

            if (!std::filesystem::exists(numbered) &&
                (!mState.AssetRegistry || !mState.AssetRegistry->ContainsSourcePath(numbered)))
            {
                return numbered;
            }
        }

        return {};
    }
    XJAssetHandle XJContentBrowserPanel::BuildUniqueAssetHandle(const std::filesystem::path& path, XJAssetType type) const
    {
        if (!mState.AssetRegistry)
            return 0;

        XJAssetHandle handle = XJAssetRegistryScanner::GenerateStableHandle(path, type);

        while (mState.AssetRegistry->Contains(handle))
            ++handle;

        return handle;
    }

    std::filesystem::path XJContentBrowserPanel::BuildUniqueImportPath(const std::filesystem::path& desiredPath) const
    {
       if (!std::filesystem::exists(desiredPath))
           return desiredPath;

       const std::filesystem::path parent = desiredPath.parent_path();
       const std::string stem = desiredPath.stem().string();
       const std::string extension = desiredPath.extension().string();

       for (int index = 1; index < 1000; ++index)
       {
           std::filesystem::path candidate =
               parent / (stem + "_" + std::to_string(index) + extension);

           if (!std::filesystem::exists(candidate))
               return candidate;
       }

       return {};
    }

    void XJContentBrowserPanel::ImportExternalFile(const std::filesystem::path& sourcePath)
    {
        if (!mState.AssetRegistry)
            return;

        if (!std::filesystem::exists(sourcePath) ||
            !std::filesystem::is_regular_file(sourcePath))
        {
            return;
        }

        XJAssetType type = XJAssetRegistryScanner::GetAssetTypeFromExtension(sourcePath);
        if (type == XJAssetType::None)
            return;

        std::filesystem::path desiredPath = BuildImportDestinationPath(sourcePath);
        std::filesystem::path destinationPath = BuildUniqueImportPath(desiredPath);

        if (destinationPath.empty())
            return;

        std::error_code ec;
        std::filesystem::create_directories(destinationPath.parent_path(), ec);
        if (ec)
            return;

        if (mState.AssetRegistry->ContainsSourcePath(destinationPath))
            return;
        std::filesystem::copy_file(sourcePath, destinationPath, std::filesystem::copy_options::none, ec);

        if (ec)
            return;


        XJAssetMeta meta;
        meta.Type = type;
        meta.Name = XJAssetRegistryScanner::GetAssetNameFromPath(destinationPath);
        meta.SourcePath = destinationPath.lexically_normal().generic_string();
        meta.ImportedPath = "";
        meta.Handle = XJAssetRegistryScanner::GenerateStableHandle(destinationPath, type);

        while (mState.AssetRegistry->Contains(meta.Handle))
            ++meta.Handle;

        mState.AssetRegistry->RegisterAsset(meta);
    }

    bool XJContentBrowserPanel::CreateMaterialAsset(const std::filesystem::path& directory)
    {
        if(!mState.AssetRegistry)
            return false;
        
        std::filesystem::path  path = BuildUniqueAssetPath(directory, "NewMaterial", ".xjmat");
        if (path.empty())
            return false;

        XJAssetHandle handle = BuildUniqueAssetHandle(path, XJAssetType::Material);
        if (handle == 0)
            return false;

        XJMaterialAsset material;
        material.Version = 2;
        material.mType = XJAssetType::Material;
        material.mName = path.stem().string();
        material.mPath = path;
        material.ShaderPath = "Resource/Shader/Unlit.xjshader";
        material.Parameters.clear();
        material.ParameterOverrides.clear();

        if (!XJMaterialAssetSerializer::SaveToFile(material, path))
        return false;

        RegisterCreatedAsset(path, XJAssetType::Material, handle);
        return true;
    }

    bool XJContentBrowserPanel::CreateSceneAsset(const std::filesystem::path& directory)
    {
        if (!mState.AssetRegistry)
            return false;

        std::filesystem::path path = BuildUniqueAssetPath(directory, "NewScene", ".xjscene");
        if (path.empty())
            return false;

        XJAssetHandle handle = BuildUniqueAssetHandle(path, XJAssetType::Scene);
        if (handle == 0)
            return false;

        XJSceneAsset scene;
        scene.mType = XJAssetType::Scene;
        scene.mName = path.stem().string();
        scene.mPath = path;
        scene.Entities.clear();

        if (!XJSceneAssetSerializer::SaveToFile(scene, path))
            return false;

        RegisterCreatedAsset(path, XJAssetType::Scene, handle);
        return true;
    }

    void XJContentBrowserPanel::RegisterCreatedAsset(const std::filesystem::path& path, XJAssetType type, XJAssetHandle handle)
    {
        if (!mState.AssetRegistry || handle == 0)
            return;

        XJAssetMeta meta;
        meta.Handle = handle;
        meta.Type = type;
        meta.Name = path.stem().string();
        meta.SourcePath = path.lexically_normal().generic_string();
        meta.ImportedPath = "";

        mState.AssetRegistry->RegisterAsset(meta);
        mState.AssetRegistry->Save("Resource/Config/AssetRegistry.json");

        if (mConfig)
        {
            mConfig->currentPath = path.parent_path().generic_string();
            mConfig->filter.clear();
        }

        mState.Selection.SelectedAsset = meta.Handle;
        mState.Selection.SelectedEntity = XJ_INVALID_EDITOR_ENTITY_ID;
        mState.Selection.HighlightedEntities.clear();
    }
}