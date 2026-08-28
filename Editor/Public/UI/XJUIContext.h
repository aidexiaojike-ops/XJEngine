// ImGui 生命周期
#ifndef XJ_UI_CONTEXT_H
#define XJ_UI_CONTEXT_H

#include <imgui.h>          // ImDrawData
#include <filesystem>
#include <string>

struct GLFWwindow;          // 在 XJ 命名空间外面

namespace XJ
{
    
    class XJUIContext
    {
        private:
            // ImGuiIO::IniFilename 只保存指针，字符串必须活到 Context 销毁。
            std::string mIniFilename;
            bool mInitialized = false;
        public:
            ~XJUIContext();

            bool Init(GLFWwindow* window, const std::filesystem::path& iniPath);
            void BeginFrame();
            void EndFrame();
            ImDrawData* XJGetDrawData();              // 渲染器用它来录 vkCmd
            void Shutdown();

            
            bool WantsCaptureMouse() const;               // 引擎判断是否吞掉鼠标/键盘
            bool WantsCaptureKeyboard() const;              // 引擎判断是否吞掉鼠标/键盘
    };
    
 
}


#endif
