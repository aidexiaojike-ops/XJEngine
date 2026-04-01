#include "Edit/XJGlfwWindow.h"
#include "Edit/SpdlogDebug.h"

namespace XJ
{
    XJGlfwWindow::XJGlfwWindow(int windowWidth, int windowHeight, const char *title)
    {
        glfwInit();//初始化Glfw
        if(!glfwInit())
        {
            spdlog::error("GLFW 初始化失败！");
            return;
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);//不创建OpenGL上下文
        //glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);//窗口不可调整大小
        glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);//窗口可见
        
        // 传入 width, height，并把 std::string 转为 C 字符串
        window = glfwCreateWindow(windowWidth, windowHeight, title, nullptr, nullptr);
        if(!window)
        {
            spdlog::error("GLFW 创建窗口失败！");
            return;
        }

        windowMonitor = glfwGetPrimaryMonitor();//获取主显示器

        if(windowMonitor)
        {
            glfwGetMonitorWorkarea(windowMonitor, &windowMonitorXPos, &windowMonitorYPos, &windowMonitorWidth, &windowMonitorHeight);//获取显示器工作区信息
            spdlog::info("主显示器工作区位置：({},{})，大小：{}x{}", windowMonitorXPos, windowMonitorYPos, windowMonitorWidth, windowMonitorHeight);
            glfwSetWindowPos(window, windowMonitorXPos + (windowMonitorWidth - windowWidth) / 2,
                             windowMonitorYPos + (windowMonitorHeight - windowHeight) / 2);//窗口居中显示
        }

        glfwMakeContextCurrent(window);//设置当前窗口的上下文
        //show window 显示window
        glfwShowWindow(window);
     
    }
    void XJGlfwWindow::PollEvents()//轮询事件
    {   
        glfwPollEvents();//
    }
    XJGlfwWindow::~XJGlfwWindow()
    {
        glfwDestroyWindow(window);
        glfwTerminate();
    }
    bool XJGlfwWindow::ShouldClose()
    {
        return glfwWindowShouldClose(window);
        spdlog::error("Should close: {0},{1}", glfwWindowShouldClose(window),"GLFW 内部状态返回窗口是否关闭");
    }
    void XJGlfwWindow::SwapBuffer()
    {
        glfwSwapBuffers(window);
    }
}