//负责读取/保存 UI JSON 配置，提供给 EditorUILayer 使用
#ifndef XJ_EDITOR_UI_CONFIG_H
#define XJ_EDITOR_UI_CONFIG_H

#include <string>
#include <filesystem>

namespace XJ
{
    struct XJEditorPanelConfig_ContentBrowser//内容浏览器面板的配置项
    {
        bool visible = true;//是否显示内容浏览器面板
        std::string title = "Content Browser";//面板标题
        std::string rootPath = "Resource";//内容浏览器的根目录，默认为 Resource 文件夹
        bool showAssetType = true;//是否在内容浏览器中显示资产类型标签
        bool showHandle = true;//是否在内容浏览器中显示资产句柄
        std::string filter;//内容浏览器的过滤器字符串，用于搜索和筛选资产，例如 "Mesh" 或 "Texture"
    };
    struct XJEditorPanelConfig_Hierarchy//层级面板的配置项
    {
        bool visible = true;
        std::string title = "World Outliner";
        bool showEntityId = true;
        bool showAssetSource = false;
    };
    struct XJEditorPanelConfig_Inspector//细节面板的配置项
    {
        bool visible = true;
        std::string title = "Details";
        bool showTransform = true;
        bool showMeshRenderer = true;
        bool showCamera = true;
        bool showAssetRefs = true;
    };
    struct XJEditorPanelConfig_DebugConsole//调试控制台面板的配置项
    {
        bool visible = true;
        std::string title = "Output Log";
        int maxLines = 1000;
        bool showInfo = true;
        bool showWarning = true;
        bool showError = true;
        bool autoScroll = true;
    };
    struct XJEditorPanelConfigs//所有面板的配置项集合
    {
        XJEditorPanelConfig_ContentBrowser contentBrowser;
        XJEditorPanelConfig_Hierarchy hierarchy;
        XJEditorPanelConfig_Inspector inspector;
        XJEditorPanelConfig_DebugConsole debugConsole;
    };
    struct XJEditorLayoutConfig//布局配置项，例如窗口停靠、分割比例等
    {
        bool dockspace = true;
        bool saveImguiIni = true;
        std::string imguiIniPath = "Resource/Config/imgui.ini";
    };

    struct XJEditorUIConfig//整个 UI 的配置项，包括布局和各个面板的配置
    {
        int version = 1;//配置版本号，方便未来升级时进行兼容性处理
        XJEditorLayoutConfig layout;//布局配置
        XJEditorPanelConfigs panels;//  各个面板的配置

        bool Load(const std::filesystem::path& path);//从指定路径加载 UI 配置，返回是否成功
        bool Save(const std::filesystem::path& path) const;//将当前 UI 配置保存到指定路径，返回是否成功
    };
}

#endif