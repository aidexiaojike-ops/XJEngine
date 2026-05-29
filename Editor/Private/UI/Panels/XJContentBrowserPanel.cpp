#include "UI/Panels/XJContentBrowserPanel.h"

#include "UI/XJEditorUILayer.h"
#include "Asset/XJAssetRegistry.h"
#include "Asset/XJAsset.h"

#include <imgui.h>
#include <algorithm>
#include <cstring>
#include <vector>
#include <string>


namespace XJ
{
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

        DrawToolbar();
        DrawAssetTable();

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
        // read filter state (static in toolbar) 
        // In production these should be members; here we re-read from a known pattern
        // For simplicity we duplicate the static logic inline
        static char* filterBuf = nullptr; // same buffer as DrawToolbar uses
        // This approach has scoping issues — better to move filterBuf/showType/showHandle to class members
        // For now, just iterate all without filter support, showing basic table

        ImGuiTableFlags tableFlags = ImGuiTableFlags_BordersV//显示垂直边框
                                   | ImGuiTableFlags_BordersOuterH//显示水平外边框
                                   | ImGuiTableFlags_Resizable//允许调整列宽
                                   | ImGuiTableFlags_RowBg//交替行背景色
                                   | ImGuiTableFlags_ScrollY;//启用垂直滚动

        if(ImGui::BeginTable("AssetTable", 4, tableFlags))
        {
           
            ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch);//名称列占满剩余空间
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80.0f);//类型列固定宽度
            ImGui::TableSetupColumn("Handle", ImGuiTableColumnFlags_WidthFixed, 180.0f);//句柄列固定宽度
            ImGui::TableSetupColumn("Source Path", ImGuiTableColumnFlags_WidthStretch);//源路径列占满剩余空间
            ImGui::TableHeadersRow();//显示表头

            for (const auto& [handle, meta] : metas)
            {

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
                bool isSelected = (mState.SelectedAsset == handle);
                if (ImGui::Selectable(meta.Name.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns))
                {
                    HandleSelection(handle);
                }
                 // right-click context menu 右键点击打开上下文菜单，提供选项如 "Select in Inspector" 或 "Delete Asset"
                if (ImGui::BeginPopupContextItem())
                {
                    if(ImGui::MenuItem("Select in Inspector"))
                    {
                        HandleSelection(handle);
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
            default:                    return "Unknown";
        }
    }
    void XJContentBrowserPanel::HandleSelection(XJAssetHandle handle)
    {
        mState.SelectedAsset = handle;
    }
}