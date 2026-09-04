#include "XJEditorApplication.h"

#include <spdlog/spdlog.h>
#include <stdexcept>
#include <utility>
#include <cstdlib>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

namespace XJ
{
    namespace
    {
        std::filesystem::path ResolveExecutableDirectory()
        {
            // exe 自身所在目录，不依赖进程工作目录：
            // 双击 bin/XJEngine.exe 或从任意 CWD 启动都能正确定位运行时根目录。
#ifdef _WIN32
            wchar_t buffer[MAX_PATH] = {};
            const DWORD length = GetModuleFileNameW(nullptr, buffer, MAX_PATH);

            if (length > 0 && length < MAX_PATH)
            {
                const std::filesystem::path executable(buffer);
                return executable.parent_path();
            }
#endif
            // 非 Windows 或获取失败时退回工作目录。
            std::error_code ec;
            return std::filesystem::current_path(ec);
        }

        std::filesystem::path ResolveProjectRoot(const XJApplication* app)
        {
            // 优先级：命令行 --project > 环境变量 XJ_PROJECT_ROOT > 编译期默认源根。
            // 源项目是唯一真相，与启动方式（VS Code / 双击 bin exe）无关。
            if (app)
            {
                const std::filesystem::path projectPath = app->XJGetProjectPath();
                if (!projectPath.empty())
                    return projectPath;
            }

            if (const char* environment =
                    std::getenv("XJ_PROJECT_ROOT"))
            {
                if (environment[0] != '\0')
                {
                    std::filesystem::path environmentPath(environment);
                    if (environmentPath.is_relative() && app && !app->XJGetLaunchWorkingDirectory().empty())
                        environmentPath = app->XJGetLaunchWorkingDirectory() / environmentPath;
                    return environmentPath.lexically_normal();
                }
            }

#ifdef XJ_DEFAULT_PROJECT_ROOT
            return std::filesystem::path(
                XJ_DEFAULT_PROJECT_ROOT);
#else
            // 没有 CMake 默认值时，不猜测路径。
            return {};
#endif
        }

        std::filesystem::path ResolveRuntimeRoot()
        {
            // 运行时资源与用户状态（imgui.ini 等）跟随 exe 目录，
            // 不受启动时工作目录影响。
            return ResolveExecutableDirectory();
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

        const std::filesystem::path projectRoot = ResolveProjectRoot(this);
        const std::filesystem::path runtimeRoot = ResolveRuntimeRoot();

        // 开发阶段 Resource/... 统一相对项目根解析；RuntimeRoot 已从 exe 路径独立取得。
        std::error_code workingDirectoryError;
        std::filesystem::current_path(projectRoot, workingDirectoryError);
        if (workingDirectoryError)
        {
            spdlog::critical(
                "Failed to use editor project root '{}': {}",
                projectRoot.string(),
                workingDirectoryError.message());
            throw std::runtime_error("Failed to activate editor project root");
        }

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
