#include "UI/XJUIContext.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

namespace XJ
{
    XJUIContext::~XJUIContext()
    {
        Shutdown();
    }

    bool XJUIContext::Init(GLFWwindow* window)
    {
        Shutdown();

        IMGUI_CHECKVERSION();//防止链接错 ImGui 库
        ImGui::CreateContext();//创建 ImGui 上下文

        ImGuiIO& kIo = ImGui::GetIO();
        //kIo.IniFilename = "imgui.ini"; //固定路径
        kIo.IniFilename = "Resource/Config/imgui.ini";
        kIo.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls  键盘输入
        kIo.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking            停靠
        kIo.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows  多视口/平台窗口
        ImGui::StyleColorsDark();//设置 ImGui 样式
        // ImGui::StyleColorsClassic();
        // ImGui::StyleColorsLight();
        if (!ImGui_ImplGlfw_InitForVulkan(window, true))//初始化 ImGui GLFW 后端，告诉它我们使用 Vulkan 渲染器
        {
            ImGui::DestroyContext();
            return false;
        }

        mInitialized = true;
        return true;
    }
    void XJUIContext::BeginFrame()
    {
        if (!mInitialized)
            return;

        // 即使当前 Vulkan 后端 NewFrame 为空操作，也保持官方调用顺序，避免后端升级后遗漏状态更新。
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();//告诉 ImGui GLFW 后端开始新的一帧
        ImGui::NewFrame();
    }
    void XJUIContext::EndFrame()
    {
        if (!mInitialized)
            return;

        ImGui::Render();  // 产出 DrawData，不碰 Vulkan
    }
    ImDrawData* XJUIContext::XJGetDrawData()  
    {
        return ImGui::GetDrawData();
    }
    void XJUIContext::Shutdown()
    {
        if (!mInitialized)
            return;

        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        mInitialized = false;
    }

    
    bool XJUIContext::WantsCaptureMouse() const              // 引擎判断是否吞掉鼠标/键盘
    {
        auto& kIo = ImGui::GetIO();
        return kIo.WantCaptureMouse;
    }
     bool XJUIContext::WantsCaptureKeyboard() const              // 引擎判断是否吞掉鼠标/键盘
    {
        auto& kIo = ImGui::GetIO();
        return kIo.WantCaptureKeyboard;
    }
}
