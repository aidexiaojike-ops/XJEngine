#include "XJEditorApplication.h"

#include <spdlog/spdlog.h>
#include <stdexcept>
#include <utility>

namespace XJ
{
    
    void XJEditorApplication::OnConfiguration(AppSettings* settings)//默认属性
    {
        settings->windowWidth = 1600;
        settings->windowHeight = 1200;
        settings->title = "XJEngine Editor";
    }

    void XJEditorApplication::OnInit()
    {
        XJAppContext* app = XJGetAppContext();
        XJRenderContext* renderContext = app ? app->renderContext : nullptr;

        if (!XJGetWindow() || !renderContext)
            throw std::runtime_error(
                "Editor requires window and render context");

        XJEditorProjectConfig config;
        config.ResourceRoot = "Resource";
        config.RegistryPath = "Resource/Config/AssetRegistry.json";
        config.UIConfigPath = "Resource/Config/EditorUI.json";
        config.DefaultScenePath = "Resource/Scenes/Default.xjscene";
        config.DefaultSceneHandle = 0x10000001ull;
        config.InitialSceneMeshHandle = 0x20000001ull;
        config.DefaultComponentMeshHandle = 0x20000002ull;
        config.SampleCount = VK_SAMPLE_COUNT_1_BIT;

        XJEditorRuntimeInitInfo info{
            .Window = XJGetWindow(),
            .RenderContext = renderContext,
            .Config = std::move(config)
        };

        if (!mEditor.Init(info))
            throw std::runtime_error(
                "Editor runtime initialization failed");
    }

    void XJEditorApplication::OnSceneInit(XJScene* scene)
    {
        if (!scene || !mEditor.AttachScene(*scene))
            throw std::runtime_error("Editor scene initialization failed");
    }

    void XJEditorApplication::OnSceneDestroy(XJScene* scene)
    {
        mEditor.DetachScene(scene);
    }

    void XJEditorApplication::OnUIBegin()
    {
        mEditor.BeginUI();
    }

    void XJEditorApplication::OnUpdate(float deltaTime)
    {
        mEditor.Update(deltaTime);
    }

    void XJEditorApplication::OnUIEnd()
    {
        mEditor.EndUI();
    }

    void XJEditorApplication::OnRender()
    {
        mEditor.Render();
    }

    void XJEditorApplication::OnUIDestroy()
    {
        mEditor.ShutdownUI();
    }

    void XJEditorApplication::OnDestroy()
    {
        mEditor.Shutdown();
    }

}