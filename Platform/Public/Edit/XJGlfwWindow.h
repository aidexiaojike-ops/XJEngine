#ifndef GLFW_WINDOW_H
#define GLFW_WINDOW_H


#include "Edit/EditIncludes.h"
// 第三方库
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>


namespace XJ
{
    class XJGlfwWindow
    {
        private:
            /* data */
            GLFWwindow* window = nullptr;//窗口句柄

            GLFWmonitor *windowMonitor = nullptr;
            int windowMonitorXPos = 0;
            int windowMonitorYPos = 0;
            int windowMonitorWidth = 0;
            int windowMonitorHeight = 0;

        public:
            XJGlfwWindow(int windowWidth = 800, int windowHeight = 600, const char *title = "XJEngine Application");
            ~XJGlfwWindow();
            void PollEvents();
            void SwapBuffer();

            GLFWwindow* XJGetWindow() { return window; }
            bool ShouldClose();
   
    };
    
}
#endif