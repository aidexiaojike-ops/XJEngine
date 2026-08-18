#include "Runtime/XJEditorRuntime.h"

#include "Render/XJEditorFrameRenderer.h"
#include "Workspace/XJEditorWorkspace.h"
#include "Render/XJEditorRenderResources.h"
#include "Render/Resource/XJMaterialFactory.h"

#include "Edit/XJGlfwWindow.h"
#include "Render/XJRenderContext.h"
#include "ECS/XJScene.h"
#include "Graphic/VulkanCommon.h"
#include "UI/XJEditorUIHost.h"
#include "UI/Viewports/XJEditorViewportSystem.h"
#include "Input/XJEditorInputBindings.h"

#include <spdlog/spdlog.h>
#include <utility>
#include <vector>

namespace XJ
{
    class XJEditorRuntime::Impl
    {
        public:
            XJGlfwWindow* Window = nullptr;
            XJRenderContext* RenderContext = nullptr;
            XJScene* Scene = nullptr;
            XJEditorProjectConfig Config;
            // Runtime 按初始化依赖顺序拥有顶层子系统。
            std::unique_ptr<XJEditorRenderResources> Resources;
            std::unique_ptr<XJEditorFrameRenderer> FrameRenderer;
            std::unique_ptr<XJEditorWorkspace> Workspace;
            std::unique_ptr<XJEditorUIHost> UI;
            std::unique_ptr<XJEditorViewportSystem> Viewports;
            std::unique_ptr<XJEditorInputBindings> Input;
        
            bool Initialized = false;
            bool UIInitialized = false;
    };

    XJEditorRuntime::XJEditorRuntime()
        : mImpl(std::make_unique<Impl>())
    {
    }

    XJEditorRuntime::~XJEditorRuntime()
    {
        Shutdown();
    }

    bool XJEditorRuntime::Init(const XJEditorRuntimeInitInfo& info)
    {
        Shutdown();

        if (!info.Window || !info.RenderContext)
        {
            spdlog::error(
                "Editor runtime initialization failed: "
                "window or render context is null.");
            return false;
        }

        mImpl->Window = info.Window;
        mImpl->RenderContext = info.RenderContext;
        mImpl->Config = info.Config;

        // 默认纹理和 sampler 是 Workspace/Viewport 创建材质的基础资源。
        mImpl->Resources = std::make_unique<XJEditorRenderResources>();
        if (!mImpl->Resources->Init(*mImpl->RenderContext))
        {
            spdlog::error(
                "Editor runtime initialization failed: "
                "default render resources failed.");
            Shutdown();
            return false;
        }

        mImpl->FrameRenderer = std::make_unique<XJEditorFrameRenderer>();

        // 当前主 RenderPass 使用 1x MSAA。
        // 后续由 XJEditorProjectConfig 提供这个配置。
        if (!mImpl->FrameRenderer->Init(*mImpl->RenderContext, mImpl->Config.SampleCount))
        {
            spdlog::error(
                "Editor runtime initialization failed: "
                "frame renderer initialization failed.");
            Shutdown();
            return false;
        }
        //
        mImpl->Workspace = std::make_unique<XJEditorWorkspace>();
        XJEditorWorkspaceInitInfo workspaceInfo{
            .ResourceRoot = mImpl->Config.ResourceRoot,
            .RegistryPath = mImpl->Config.RegistryPath,
            .DefaultScenePath = mImpl->Config.DefaultScenePath,
            .DefaultSceneHandle = mImpl->Config.DefaultSceneHandle,
            .InitialSceneMeshHandle = mImpl->Config.InitialSceneMeshHandle,
            .DefaultComponentMeshHandle = mImpl->Config.DefaultComponentMeshHandle
        };

        if (!mImpl->Workspace->Init(workspaceInfo))
        {
            spdlog::error("Editor workspace initialization failed.");
            Shutdown();
            return false;
        }

        // Workspace 保存 shared_ptr，Resources 仍是默认资源的根所有者。
        mImpl->Workspace->SetDefaultResources(
        mImpl->Resources->GetDefaultTexture(),
        mImpl->Resources->GetDefaultSampler());

        //UIHost 创建初始化
        mImpl->UI = std::make_unique<XJEditorUIHost>();
        XJEditorUIHostInitInfo uiInfo{
            .Window = mImpl->Window,
            .RenderContext = mImpl->RenderContext,
            .FrameRenderer = mImpl->FrameRenderer.get(),
            .UIState = &mImpl->Workspace->GetUIState(),
            .ConfigPath = mImpl->Config.UIConfigPath
        };      

        if (!mImpl->UI->Init(uiInfo))
        {
            spdlog::error(
                "Editor runtime initialization failed: "
                "UI host initialization failed.");      

            Shutdown();
            return false;
        }   
        //编辑器UI
        mImpl->Viewports = std::make_unique<XJEditorViewportSystem>();
        XJEditorViewportSystemInitInfo viewportInfo{
            .Window = mImpl->Window,
            .RenderContext = mImpl->RenderContext,
            .MainRenderTarget = mImpl->FrameRenderer->GetRenderTarget(),
            .Resources = mImpl->Resources.get()
        };

        if (!mImpl->Viewports->Init(viewportInfo))
        {
            spdlog::error(
                "Editor viewport system initialization failed.");
            Shutdown();
            return false;
        }

        // Workspace 不直接依赖 ViewportSystem，通过 hooks 协调相机生命周期。
        XJEditorViewportSystem* viewports = mImpl->Viewports.get();
        XJEditorWorkspaceSceneHooks hooks;

        hooks.BeforeDelete = [viewports](XJScene& scene, const std::vector<XJEditorEntityId>& ids)
        {
            viewports->BeforeDeleteEntities(scene, ids);
        };

        hooks.BeforeOpen = [viewports]()
        {
            viewports->BeforeOpenScene();
        };

        hooks.AfterOpen = [viewports](XJScene& scene)
        {
            viewports->AfterOpenScene(scene);
        };

        hooks.AfterMutation = [viewports]()
        {
            viewports->RefreshSceneCameras();
        };

        hooks.CanDeleteEntity = [viewports](XJEditorEntityId id)
        {
            return !viewports->IsProtectedEditorEntity(id);
        };

        hooks.ShouldExposeEntity = [viewports](XJEditorEntityId id)
        {
            return !viewports->IsProtectedEditorEntity(id);
        };

        mImpl->Workspace->SetSceneHooks(std::move(hooks));

        // InputBindings 依赖已创建的 Workspace 和 ViewportSystem，必须最后初始化。
        mImpl->Input = std::make_unique<XJEditorInputBindings>();
        XJEditorInputBindingsInitInfo inputInfo{
            .Window = mImpl->Window,
            .Workspace = mImpl->Workspace.get(),
            .Viewports = mImpl->Viewports.get()
        };

        if (!mImpl->Input->Init(inputInfo))
        {
            spdlog::error("Editor input bindings initialization failed.");
            Shutdown();
            return false;
        }
        
        // 必须等所有当前阶段子系统初始化成功后再置 true。
        mImpl->UIInitialized = true;
        mImpl->Initialized = true;
        

        spdlog::info("Editor runtime initialized: " "resources, renderer, workspace, UI, " "viewports and input are ready.");

        return true;
    }

    //场景的生命周期
    bool XJEditorRuntime::AttachScene(XJScene& scene)
    {
        if (!mImpl->Initialized || mImpl->Scene)
            return false;

        // Viewports 先绑定 scene；Workspace 加载默认场景时会触发 AfterOpen hook。
        if (!mImpl->Viewports->AttachScene(scene))
            return false;

        if (!mImpl->Workspace->AttachScene(scene))
        {
            mImpl->Viewports->DetachScene(&scene);
            return false;
        }
        
        mImpl->Scene = &scene;
        return true;
    }

    void XJEditorRuntime::DetachScene(XJScene* scene)
    {
        if (!mImpl->Scene)
            return;

        if (scene && scene != mImpl->Scene)
            return;

        // 先确认销毁的是当前 scene，再解除 viewport 和 workspace 绑定。
        if (mImpl->Viewports)
            mImpl->Viewports->DetachScene(mImpl->Scene);
        
        if (mImpl->Workspace)
            mImpl->Workspace->DetachScene(mImpl->Scene);

        mImpl->Scene = nullptr;
    }

    void XJEditorRuntime::BeginUI()
    {
        if (mImpl->Initialized && mImpl->UIInitialized && mImpl->UI)
        {
            mImpl->UI->BeginFrame();
        }
    }

    void XJEditorRuntime::Update(float deltaTime)
    {
        if (!mImpl->Initialized)
            return; 

        // UI 与 Viewport 先生成请求，Workspace 随后统一处理。
        // 保留生命周期入口，后续在这里处理 UI、请求和相机更新。
        (void)deltaTime;
        // UI 先生成请求。
        if (mImpl->UIInitialized && mImpl->UI)
            mImpl->UI->DrawUI();

        // Preview 窗口不属于普通 Panel，由 ViewportSystem 单独绘制。
        if (mImpl->Viewports)
            mImpl->Viewports->DrawUI();

        if (mImpl->Input)
            mImpl->Input->EndUIFrame();

        if (mImpl->Workspace)
            mImpl->Workspace->Update();

        if (mImpl->Viewports)
            mImpl->Viewports->Update(deltaTime);
    }

    void XJEditorRuntime::EndUI()
    {
        if (mImpl->Initialized && mImpl->UIInitialized && mImpl->UI)
        {
            mImpl->UI->EndFrame();
        }
    }

    void XJEditorRuntime::Render()
    {
         if (!mImpl->Initialized || !mImpl->FrameRenderer || !mImpl->Window)
        {
            return;
        }

        XJEditorFrameRenderInput input;
        input.Window = mImpl->Window;
        // FrameRenderer 统一提交离屏 Viewport 与主 ImGui RenderPass。
        input.UIRenderer = mImpl->UI
            ? mImpl->UI->GetRenderer()
            : nullptr;

        input.DrawData = mImpl->UI
            ? mImpl->UI->GetDrawData()
            : nullptr;

        input.ScenePreview =
            mImpl->Viewports
                ? mImpl->Viewports->GetScenePreview()
                : nullptr;      

        input.GamePreview =
            mImpl->Viewports
                ? mImpl->Viewports->GetGamePreview()
                : nullptr;      

        if (!mImpl->FrameRenderer->RenderFrame(input))
        {
            // 最小化、swapchain resize 或暂时无法 acquire 时也可能返回 false，
            // 因此这里只使用 trace，避免正常 resize 刷 warning。
            spdlog::trace("Editor frame was not presented.");
        }

    }

    void XJEditorRuntime::ShutdownUI()
    {
        if (!mImpl)
            return;

        if (mImpl->Input)
        {
            mImpl->Input->Shutdown();
            mImpl->Input.reset();
        }

        // Hooks 捕获 ViewportSystem 裸指针，必须在 Viewports 销毁前解除。
        if (mImpl->Workspace)
            mImpl->Workspace->ClearSceneHooks();

        // 不只修改标志。UIHost 保存了 RenderContext 非拥有指针，
        // 必须在 XJApplication 销毁 RenderContext 之前真正释放。
        if (mImpl->Viewports)
        {
            mImpl->Viewports->Shutdown();
            mImpl->Viewports.reset();
        }


        if (mImpl->UI)
        {
            mImpl->UI->Shutdown();
            mImpl->UI.reset();
        }

        // 后续按 Panels -> Viewports -> Vulkan UI Backend 顺序关闭。
        mImpl->UIInitialized = false;
    }

    void XJEditorRuntime::Shutdown()
    {
        if (!mImpl)
            return;

            // FrameRenderer 持有 Vulkan 资源，必须在 RenderContext
            // 和 Window 被 XJApplication 销毁之前释放。
        ShutdownUI();
        DetachScene(nullptr);

        if (mImpl->Workspace)
        {
            mImpl->Workspace->Shutdown();
            // MaterialFactory 保存 registry 裸指针，Workspace 销毁前必须解除。
            XJMaterialFactory::GetInstance()->ClearCaches();
            mImpl->Workspace.reset();
        }

        // 按初始化依赖的逆序释放：FrameRenderer 先于默认 GPU Resources。
        if (mImpl->FrameRenderer)
        {
            mImpl->FrameRenderer->Shutdown();
            mImpl->FrameRenderer.reset();
        }

        if (mImpl->Resources)
        {
            mImpl->Resources->Shutdown();
            mImpl->Resources.reset();
        }

        mImpl->Window = nullptr;
        mImpl->RenderContext = nullptr;
        mImpl->Initialized = false;
    }

    
}
