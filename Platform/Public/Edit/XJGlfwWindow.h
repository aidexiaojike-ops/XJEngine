#ifndef GLFW_WINDOW_H
#define GLFW_WINDOW_H


#include "Edit/EditIncludes.h"
// 第三方库
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include "Event/XJMouseEvent.h"
#include "Edit/Mathinclude.h"

#include <functional>

namespace XJ
{
    class XJGlfwWindow
    {
        public:
            using DropCallback = std::function<void(int count, const char** paths)>;
            // 累积滚轮增量并清零（供每帧输入轮询消费）。
            glm::vec2 XJConsumeScrollDelta();

        private:
            double mScrollAccumX = 0.0;
            double mScrollAccumY = 0.0;
            /* data */
            GLFWwindow* mGLFWwindow = nullptr;//窗口句柄
            DropCallback mDropCallback;

            GLFWmonitor *windowMonitor = nullptr;
            int windowMonitorXPos = 0;
            int windowMonitorYPos = 0;
            int windowMonitorWidth = 0;
            int windowMonitorHeight = 0;

            void XJSetWindowCallbacks();//设置窗口回调函数
     

        public:
            XJGlfwWindow(int windowWidth = 800, int windowHeight = 600, const char *title = "XJEngine Application");
            ~XJGlfwWindow();
            XJGlfwWindow(const XJGlfwWindow&) = delete;
            XJGlfwWindow& operator=(const XJGlfwWindow&) = delete;

            void PollEvents();

            // GLFWwindow* XJGetWindow() { return window; }
            bool ShouldClose();

            GLFWwindow* XJGetImplWindowPointer() const  {return mGLFWwindow;};//获取底层窗口指针，供Vulkan使用
            void SetDropCallback(DropCallback callback);

            void XJGetMousePos(glm::vec2 &mousPose);//获取鼠标位置，供Vulkan使用
            bool IsMouseDown(MouseButton mouseButton = MOUSE_BUTTON_LEFT) const;//获取鼠标按键状态，供Vulkan使用
            bool IsMouseUp(MouseButton mouseButton = MOUSE_BUTTON_LEFT) const;//获取鼠标按键状态，供Vulkan使用

            bool IsKeyDown(int key) const;//获取键盘按键状态，供Vulkan使用
            bool IsKeyUp(int key) const;//获取键盘按键

            bool IsWindowMinimized() const {return mGLFWwindow && glfwGetWindowAttrib(mGLFWwindow, GLFW_ICONIFIED) != 0; }//窗口最小化
            VkExtent2D XJGetFramebufferExtent() const;
    };
    
}
#endif

