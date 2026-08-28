#include "UI/XJEditorUIHost.h"

#include "Edit/XJGlfwWindow.h"
#include "Graphic/XJVulkanDevice.h"
#include "Graphic/XJVulkanPhysicalDevices.h"
#include "Graphic/XJVulkanRenderPass.h"
#include "Graphic/XJVulkanSwapchain.h"
#include "Graphic/XJVulkanCommandBuffer.h"
#include "Render/XJEditorFrameRenderer.h"
#include "Render/XJRenderContext.h"
#include "UI/XJEditorRenderer.h"
#include "UI/XJUIContext.h"
#include "UI/XJEditorUILayer.h"
#include "UI/XJEditorUIState.h"

#include <imgui.h>
#include <spdlog/spdlog.h>

namespace XJ
{
    XJEditorUIHost::XJEditorUIHost() = default;
    XJEditorUIHost::~XJEditorUIHost()
    {
        Shutdown();
    }

    bool XJEditorUIHost::Init(const XJEditorUIHostInitInfo& info)
    {
        Shutdown();

        if (!info.Window ||
            !info.RenderContext ||
            !info.FrameRenderer ||
            !info.UIState ||
            info.ConfigPath.empty() ||
            info.ProjectResourceRoot.empty() ||
            info.ImGuiIniPath.empty())
        {
            spdlog::error(
                "Editor UI initialization failed: "
                "required service is null.");
            return false;
        }

        XJVulkanDevice* device = info.RenderContext->XJGetDevice();

        XJVulkanPhysicalDevices* physicalDevices = info.RenderContext->XJGetPhysicalDevices();

        XJVulkanSwapchain* swapchain = info.RenderContext->XJGetSwapchain();

        XJVulkanInstance* instance = info.RenderContext->XJGetInstance();

        XJVulkanRenderPass* renderPass = info.FrameRenderer->GetRenderPass();

        if (!device || !physicalDevices || !swapchain ||
            !instance || !renderPass ||
            !device->XJGetDefaultCmdPool() ||
            !device->XJGetFirstGraphicQueue())
        {
            spdlog::error("Editor UI initialization failed: " "Vulkan services are incomplete.");
            return false;
        }

        mRenderContext = info.RenderContext;
        mContext = std::make_unique<XJUIContext>();

        if (!mContext->Init(
                info.Window->XJGetImplWindowPointer(),
                info.ImGuiIniPath))
        {
            spdlog::error("Editor ImGui context initialization failed.");
            Shutdown();
            return false;
        }

        XJEditorRendererInitInfo rendererInfo{};
        rendererInfo.instance = instance->XJGetInstance();
        rendererInfo.apiVersion = instance->XJGetApiVersion();
        rendererInfo.physicalDevice = physicalDevices->XJGetPhysicalDevice();
        rendererInfo.device = device->XJGetDevice();
        rendererInfo.renderPass = renderPass->XJGetRenderPass();
        rendererInfo.commandPool = device->XJGetDefaultCmdPool()->XJGetCommandPool();
        rendererInfo.queueFamily = physicalDevices->XJGetGraphicQueueFamilyInfo().queueFamilyIndex;
        rendererInfo.queue = device->XJGetFirstGraphicQueue()->XJGetQueue();
        rendererInfo.imageCount = static_cast<uint32_t>(swapchain->XJGetSwapchainImages().size());
        rendererInfo.msaaSamples = info.FrameRenderer->GetSampleCount();
        rendererInfo.colorFormat = swapchain->XJGetSurfaceInfo().surfaceFormat.format;

        mRenderer = std::make_unique<XJEditorRenderer>();

        if (!mRenderer->Init(rendererInfo))
        {
            spdlog::error("Editor Vulkan UI renderer initialization failed.");
            Shutdown();
            return false;
        }

        mLayer = std::make_unique<XJEditorUILayer>(*info.UIState);
        mLayer->Init(
            info.ConfigPath,
            info.ProjectResourceRoot);

        mInitialized = true;
        return true;
    }


    void XJEditorUIHost::BeginFrame()
    {
        if (mInitialized && mContext)
            mContext->BeginFrame();
    }

    void XJEditorUIHost::EndFrame()
    {
        if (mInitialized && mContext)
            mContext->EndFrame();
    }

    ImDrawData* XJEditorUIHost::GetDrawData() const
    {
        return mInitialized && mContext
            ? mContext->XJGetDrawData()
            : nullptr;
    }

    XJEditorRenderer* XJEditorUIHost::GetRenderer() const
    {
        return mRenderer.get();
    }
    
    bool XJEditorUIHost::IsInitialized() const
    {
        return mInitialized;
    }

    void XJEditorUIHost::DrawUI()
    {
        if (!mInitialized || !mLayer)
            return;

        // UILayer 内部绘制 DockSpace、菜单和所有编辑器 Panel。
        mLayer->DrawUI();
    }

    void XJEditorUIHost::Shutdown()
    {
        if (!mRenderContext && !mContext && !mRenderer && !mLayer)
            return;
        
        // Panel 持有 Workspace UIState 引用，必须最先销毁。
        if (mLayer)
        {
            mLayer->SaveConfig();
            mLayer->Shutdown();
            mLayer.reset();
        }

        if (ImGui::GetCurrentContext())
        {
            const char* iniPath = ImGui::GetIO().IniFilename;
            if (iniPath)
                ImGui::SaveIniSettingsToDisk(iniPath);
        }
        XJVulkanDevice* device =
            mRenderContext
                ? mRenderContext->XJGetDevice()
                : nullptr;
                
        if (device)
            device->WaitIdle();
        // Vulkan backend 必须在 ImGui context 前关闭。
        if (mRenderer)
        {
            mRenderer->Shutdown();
            mRenderer.reset();
        }
        if (mContext)
        {
            mContext->Shutdown();
            mContext.reset();
        }
        mRenderContext = nullptr;
        mInitialized = false;
    }

}
