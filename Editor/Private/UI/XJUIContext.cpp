#include "UI/XJUIContext.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"

namespace XJ
{
    bool XJUIContext::Init(GLFWwindow* window)
    {
        IMGUI_CHECKVERSION();//防止链接错 ImGui 库
        ImGui::CreateContext();//创建 ImGui 上下文

        ImGuiIO& kIo = ImGui::GetIO();
        kIo.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls  键盘输入
        ImGui::StyleColorsDark();//设置 ImGui 样式
        // ImGui::StyleColorsClassic();
        // ImGui::StyleColorsLight();
        ImGui_ImplGlfw_InitForVulkan(window, true);//初始化 ImGui GLFW 后端，告诉它我们使用 Vulkan 渲染器
        return true;
    }
    void XJUIContext::BeginFrame()
    {
        ImGui_ImplGlfw_NewFrame();//告诉 ImGui GLFW 后端开始新的一帧
        ImGui::NewFrame();
    }
    void XJUIContext::EndFrame()
    {
        ImGui::Render();  // 产出 DrawData，不碰 Vulkan
    }
    ImDrawData* XJUIContext::XJGetDrawData()  
    {
        return ImGui::GetDrawData();
    }
    void XJUIContext::Shutdown()
    {
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
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