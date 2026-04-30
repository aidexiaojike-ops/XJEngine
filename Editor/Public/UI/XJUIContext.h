// ImGui 生命周期
#ifndef XJ_UI_CONTEXT_H
#define XJ_UI_CONTEXT_H

#include <imgui.h>          // ImDrawData

struct GLFWwindow;          // 在 XJ 命名空间外面

namespace XJ
{
    
    class XJUIContext
    {
        private:
            /* data */
        public:
            bool Init(GLFWwindow* window);
            void BeginFrame();
            void EndFrame();
            ImDrawData* XJGetDrawData();              // 渲染器用它来录 vkCmd
            void Shutdown();

            
            bool WantsCaptureMouse() const;               // 引擎判断是否吞掉鼠标/键盘
            bool WantsCaptureKeyboard() const;              // 引擎判断是否吞掉鼠标/键盘
    };
    
 
}


#endif