#include "XJEditorApplication.h"

#include <spdlog/spdlog.h>
#include <stdexcept>
#include <utility>

namespace XJ
{
    namespace
    {
        std::filesystem::path ResolveProjectRoot()
        {
            // 环境变量优先，方便以后打开其他项目：
            // XJ_PROJECT_ROOT=D:/Projects/MyGame
            if (const char* environment =
                    std::getenv("XJ_PROJECT_ROOT"))
            {
                if (environment[0] != '\0')
                {
                    return std::filesystem::path(
                        environment);
                }
            }

    #ifdef XJ_DEFAULT_PROJECT_ROOT
            return std::filesystem::path(
                XJ_DEFAULT_PROJECT_ROOT);
    #else
            // 没有 CMake 默认值时，不猜测 parent_path。
            // 猜测 exe 上级目录在打包版本中是不可靠的。
            return {};
    #endif
        }

        std::filesystem::path ResolveRuntimeRoot()
        {
            std::error_code ec;

            const std::filesystem::path runtimeRoot =
                std::filesystem::current_path(ec);

            if (ec)
                return {};

            return runtimeRoot;
        }
    }
    
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

        const std::filesystem::path projectRoot = ResolveProjectRoot();
        const std::filesystem::path runtimeRoot = ResolveRuntimeRoot();

        XJEditorProjectConfig config;

        config.Paths = XJEditorProjectPaths::FromRoots(projectRoot, runtimeRoot);
            
        config.DefaultSceneHandle = 0x10000001ull;
        config.InitialSceneMeshHandle = 0x20000001ull;
        config.DefaultComponentMeshHandle = 0x20000002ull;
        config.SampleCount = VK_SAMPLE_COUNT_1_BIT;
            
        if (!config.IsValid())
        {
            spdlog::critical(
                "Editor project configuration is invalid. "
                "projectRoot='{}', runtimeRoot='{}'.",
                projectRoot.string(),
                runtimeRoot.string());
            
            throw std::runtime_error(
                "Invalid editor project paths");
        }

        spdlog::info("Editor project root: '{}'", config.Paths.ProjectRoot.string());
        spdlog::info("Editor project resources: '{}'", config.Paths.ProjectResourceRoot.string());
        spdlog::info("Editor runtime resources: '{}'", config.Paths.RuntimeResourceRoot.string());
        
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
