#ifndef GLFW_WINDOW_H
#define GLFW_WINDOW_H


#include "Edit/EditIncludes.h"
// 第三方库
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include "Event/XJMouseEvent.h"
#include "Edit/Mathinclude.h"

namespace XJ
{
    class XJGlfwWindow
    {
        private:
            /* data */
            GLFWwindow* mGLFWwindow = nullptr;//窗口句柄

            GLFWmonitor *windowMonitor = nullptr;
            int windowMonitorXPos = 0;
            int windowMonitorYPos = 0;
            int windowMonitorWidth = 0;
            int windowMonitorHeight = 0;

            void XJSetWindowCallbacks();//设置窗口回调函数
     

        public:
            XJGlfwWindow(int windowWidth = 800, int windowHeight = 600, const char *title = "XJEngine Application");
            ~XJGlfwWindow();
            void PollEvents();
            void SwapBuffer();

            // GLFWwindow* XJGetWindow() { return window; }
            bool ShouldClose();

            void* XJGetImplWindowPointer() const  {return mGLFWwindow;};//获取底层窗口指针，供Vulkan使用

            void XJGetMousePos(glm::vec2 &mousPose);//获取鼠标位置，供Vulkan使用
            bool IsMouseDown(MouseButton mouseButton = MOUSE_BUTTON_LEFT) const;//获取鼠标按键状态，供Vulkan使用
            bool IsMouseUp(MouseButton mouseButton = MOUSE_BUTTON_LEFT) const;//获取鼠标按键状态，供Vulkan使用

            bool IsKeyDown(int key) const;//获取键盘按键状态，供Vulkan使用
            bool IsKeyUp(int key) const;//获取键盘按键
    };
    
}
#endif

