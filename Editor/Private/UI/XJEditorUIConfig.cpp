#include "UI/XJEditorUIConfig.h"
#include <fstream>
#include <nlohmann/json.hpp>

namespace XJ
{
    bool XJEditorUIConfig::Load(const std::filesystem::path& path)
    {
        std::ifstream in(path);
        if (!in)
            return false;
        // 解析 JSON 文件
        nlohmann::json j;
        try
        {
            j = nlohmann::json::parse(in);
        }
        catch (const nlohmann::json::exception&)
        {
            return false;
        }

        version = j.value("version", 1);// 读取版本号，默认为 1

        if (j.contains("layout"))// 读取布局配置
        {
            auto& layoutJson = j["layout"];
            layout.dockspace = layoutJson.value("dockspace", true);
            layout.saveImguiIni = layoutJson.value("saveImguiIni", true);
            layout.imguiIniPath = layoutJson.value("imguiIniPath", "Resource/Config/imgui.ini");
        }

        if(j.contains("panels"))// 读取面板配置
        {
            auto& p = j["panels"];

            if (p.contains("contentBrowser"))
            {
                auto& cb = p["contentBrowser"];
                panels.contentBrowser.visible = cb.value("visible", true);
                panels.contentBrowser.title = cb.value("title", "Content Browser");
                panels.contentBrowser.rootPath = cb.value("rootPath", "Resource");
                panels.contentBrowser.currentPath = cb.value("currentPath", "Resource");
                panels.contentBrowser.showFolders = cb.value("showFolders", true);
                panels.contentBrowser.showAssetType = cb.value("showAssetType", true);
                panels.contentBrowser.showHandle = cb.value("showHandle", true);
                panels.contentBrowser.filter = cb.value("filter", "");
            }

            if (p.contains("hierarchy"))
            {
                auto& h = p["hierarchy"];
                panels.hierarchy.visible = h.value("visible", true);
                panels.hierarchy.title = h.value("title", "World Outliner");
                panels.hierarchy.showEntityId = h.value("showEntityId", true);
                panels.hierarchy.showAssetSource = h.value("showAssetSource", false);
            }

            if (p.contains("inspector"))
            {
                auto& i = p["inspector"];
                panels.inspector.visible = i.value("visible", true);
                panels.inspector.title = i.value("title", "Details");
                panels.inspector.showTransform = i.value("showTransform", true);
                panels.inspector.showMeshRenderer = i.value("showMeshRenderer", true);
                panels.inspector.showCamera = i.value("showCamera", true);
                panels.inspector.showAssetRefs = i.value("showAssetRefs", true);
            }

            if (p.contains("debugConsole"))
            {
                auto& dc = p["debugConsole"];
                panels.debugConsole.visible = dc.value("visible", true);
                panels.debugConsole.title = dc.value("title", "Output Log");
                panels.debugConsole.maxLines = dc.value("maxLines", 1000);
                panels.debugConsole.showInfo = dc.value("showInfo", true);
                panels.debugConsole.showWarning = dc.value("showWarning", true);
                panels.debugConsole.showError = dc.value("showError", true);
                panels.debugConsole.autoScroll = dc.value("autoScroll", true);
            }
        }

        return true;
    }
    bool XJEditorUIConfig::Save(const std::filesystem::path& path) const
    {
        if (path.has_parent_path())// 确保目录存在
            std::filesystem::create_directories(path.parent_path());

        nlohmann::json j;// 构建 JSON 对象
        j["version"] = version;

        j["layout"]["dockspace"] = layout.dockspace;
        j["layout"]["saveImguiIni"] = layout.saveImguiIni;
        j["layout"]["imguiIniPath"] = layout.imguiIniPath;

        j["panels"]["contentBrowser"]["visible"] = panels.contentBrowser.visible;
        j["panels"]["contentBrowser"]["title"] = panels.contentBrowser.title;
        j["panels"]["contentBrowser"]["rootPath"] = panels.contentBrowser.rootPath;
        j["panels"]["contentBrowser"]["currentPath"] = panels.contentBrowser.currentPath;
        j["panels"]["contentBrowser"]["showFolders"] = panels.contentBrowser.showFolders;
        j["panels"]["contentBrowser"]["showAssetType"] = panels.contentBrowser.showAssetType;
        j["panels"]["contentBrowser"]["showHandle"] = panels.contentBrowser.showHandle;
        j["panels"]["contentBrowser"]["filter"] = panels.contentBrowser.filter;

        j["panels"]["hierarchy"]["visible"] = panels.hierarchy.visible;
        j["panels"]["hierarchy"]["title"] = panels.hierarchy.title;
        j["panels"]["hierarchy"]["showEntityId"] = panels.hierarchy.showEntityId;
        j["panels"]["hierarchy"]["showAssetSource"] = panels.hierarchy.showAssetSource;

        j["panels"]["inspector"]["visible"] = panels.inspector.visible;
        j["panels"]["inspector"]["title"] = panels.inspector.title;
        j["panels"]["inspector"]["showTransform"] = panels.inspector.showTransform;
        j["panels"]["inspector"]["showMeshRenderer"] = panels.inspector.showMeshRenderer;
        j["panels"]["inspector"]["showCamera"] = panels.inspector.showCamera;
        j["panels"]["inspector"]["showAssetRefs"] = panels.inspector.showAssetRefs;

        j["panels"]["debugConsole"]["visible"] = panels.debugConsole.visible;
        j["panels"]["debugConsole"]["title"] = panels.debugConsole.title;
        j["panels"]["debugConsole"]["maxLines"] = panels.debugConsole.maxLines;
        j["panels"]["debugConsole"]["showInfo"] = panels.debugConsole.showInfo;
        j["panels"]["debugConsole"]["showWarning"] = panels.debugConsole.showWarning;
        j["panels"]["debugConsole"]["showError"] = panels.debugConsole.showError;
        j["panels"]["debugConsole"]["autoScroll"] = panels.debugConsole.autoScroll;

        std::ofstream out(path);// 写入 JSON 文件
        out << j.dump(2);
        return out.good();// 返回写入是否成功
    }
}