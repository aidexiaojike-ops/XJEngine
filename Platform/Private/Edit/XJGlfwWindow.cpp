#include "Edit/XJGlfwWindow.h"
#include "Edit/SpdlogDebug.h"
#include "Event/XJWindowEvent.h"
#include "Event/XJEventDispatcher.h"

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
        mGLFWwindow = glfwCreateWindow(windowWidth, windowHeight, title, nullptr, nullptr);
        if(!mGLFWwindow)
        {
            spdlog::error("GLFW 创建窗口失败！");
            return;
        }

        windowMonitor = glfwGetPrimaryMonitor();//获取主显示器

        if(windowMonitor)
        {
            glfwGetMonitorWorkarea(windowMonitor, &windowMonitorXPos, &windowMonitorYPos, &windowMonitorWidth, &windowMonitorHeight);//获取显示器工作区信息
            spdlog::info("主显示器工作区位置：({},{})，大小：{}x{}", windowMonitorXPos, windowMonitorYPos, windowMonitorWidth, windowMonitorHeight);
            glfwSetWindowPos(mGLFWwindow, windowMonitorXPos + (windowMonitorWidth - windowWidth) / 2,
                             windowMonitorYPos + (windowMonitorHeight - windowHeight) / 2);//窗口居中显示
        }

        glfwMakeContextCurrent(mGLFWwindow);//设置当前窗口的上下文

        XJSetWindowCallbacks(); //设置窗口回调函数
        //show window 显示window
        glfwShowWindow(mGLFWwindow);
     
    }
    void XJGlfwWindow::PollEvents()//轮询事件
    {   
        glfwPollEvents();//
    }
    XJGlfwWindow::~XJGlfwWindow()
    {
        glfwDestroyWindow(mGLFWwindow);
        glfwTerminate();
    }
    bool XJGlfwWindow::ShouldClose()
    {
        return glfwWindowShouldClose(mGLFWwindow);
        spdlog::error("Should close: {0},{1}", glfwWindowShouldClose(mGLFWwindow),"GLFW 内部状态返回窗口是否关闭");
    }
    void XJGlfwWindow::SwapBuffer()
    {
        glfwSwapBuffers(mGLFWwindow);
    }

    void XJGlfwWindow::XJSetWindowCallbacks()//设置窗口回调函数
    {
        glfwSetWindowUserPointer(mGLFWwindow, this);//设置用户指针为当前窗口实例，供回调函数使用
            //设置窗口大小回调函数
        glfwSetFramebufferSizeCallback(mGLFWwindow, [](GLFWwindow* window, int width, int height)
        {
            auto *kWindow = static_cast<XJGlfwWindow*>(glfwGetWindowUserPointer(window));
            if(kWindow)
            {
                XJFrameBufferResizeEvent fbResizeEvent{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};//创建帧缓冲区大小改变事件
                XJEventDispatcher::XJGetInstance()->DispatchEvent(fbResizeEvent);//分发事件
            }
        });


         glfwSetWindowFocusCallback(mGLFWwindow, [](GLFWwindow* window, int focused)
        {//获取焦点
            auto *kWindow = static_cast<XJGlfwWindow *>(glfwGetWindowUserPointer(window));
            if(kWindow)
            {
                if(focused)
                {
                    XJWindowFocusEvent windowFocusEvent{};
                    XJEventDispatcher::XJGetInstance()->DispatchEvent(windowFocusEvent);
                } else 
                {
                    XJWindowLostFocusEvent windowLostFocusEvent{};
                    XJEventDispatcher::XJGetInstance()->DispatchEvent(windowLostFocusEvent);
                }
            }
        });

        glfwSetWindowPosCallback(mGLFWwindow, [](GLFWwindow* window, int xpos, int ypos)
        {//窗口位置变化
            auto *kWindow = static_cast<XJGlfwWindow *>(glfwGetWindowUserPointer(window));
            if(kWindow)
            {
                XJWindowMoveEvent windowMovedEvent{ static_cast<uint32_t>(xpos), static_cast<uint32_t>(ypos) };
                XJEventDispatcher::XJGetInstance()->DispatchEvent(windowMovedEvent);
            }
        });

        glfwSetWindowCloseCallback(mGLFWwindow, [](GLFWwindow* window)
        {//窗口关闭
            auto *kWindow = static_cast<XJGlfwWindow *>(glfwGetWindowUserPointer(window));
            if(kWindow)
            {
                XJWindowCloseEvent windowCloseEvent{};
                XJEventDispatcher::XJGetInstance()->DispatchEvent(windowCloseEvent);
            }
        });

        glfwSetKeyCallback(mGLFWwindow, [](GLFWwindow* window, int key, int scancode, int action, int mods)
        {//键盘事件
            auto *kWindow = static_cast<XJGlfwWindow *>(glfwGetWindowUserPointer(window));
            if(kWindow)
            {
                if(action == GLFW_RELEASE)
                {
                    XJKeyReleaseEvent keyReleaseEvent{static_cast<Key>(key) };
                    XJEventDispatcher::XJGetInstance()->DispatchEvent(keyReleaseEvent);
                } else 
                {
                    XJKeyInputPressEvent keyPressEvent{static_cast<Key>(key), static_cast<KeyMod>(mods), action == GLFW_REPEAT };
                    XJEventDispatcher::XJGetInstance()->DispatchEvent(keyPressEvent);
                }
            }
        });

        glfwSetMouseButtonCallback(mGLFWwindow, [](GLFWwindow* window, int button, int action, int mods)
        {//鼠标事件
            auto *kWindow = static_cast<XJGlfwWindow *>(glfwGetWindowUserPointer(window));
            if(kWindow){
                if(action == GLFW_PRESS)
                {
                    XJMouseButtonPressEvent mouseButtonPressEvent{static_cast<MouseButton>(button), static_cast<KeyMod>(mods), false };
                    XJEventDispatcher::XJGetInstance()->DispatchEvent(mouseButtonPressEvent);
                }
                if(action == GLFW_RELEASE)
                {
                    XJMouseButtonReleaseEvent mouseButtonReleaseEvent{static_cast<MouseButton>(button) };
                    XJEventDispatcher::XJGetInstance()->DispatchEvent(mouseButtonReleaseEvent);
                }
            }
        });

        glfwSetCursorPosCallback(mGLFWwindow, [](GLFWwindow* window, double xpos, double ypos)
        {//鼠标指针位置
            auto *kWindow = static_cast<XJGlfwWindow *>(glfwGetWindowUserPointer(window));
            if(kWindow)
            {
                XJMouseMoveEvent mouseMovedEvent{ static_cast<float>(xpos), static_cast<float>(ypos) };
                XJEventDispatcher::XJGetInstance()->DispatchEvent(mouseMovedEvent);
            }
        });

        glfwSetScrollCallback(mGLFWwindow, [](GLFWwindow* window, double xoffset, double yoffset)
        {
            //滚轮
            auto *kWindow = static_cast<XJGlfwWindow *>(glfwGetWindowUserPointer(window));
            if(kWindow)
            {
                XJMouseScrollEvent mouseScrollEvent{ static_cast<float>(xoffset), static_cast<float>(yoffset) };
                XJEventDispatcher::XJGetInstance()->DispatchEvent(mouseScrollEvent);
            }
        });
    }


    void XJGlfwWindow::XJGetMousePos(glm::vec2 &mousPose) //获取鼠标位置，供Vulkan使用
    {
        double xPos, yPos;
        glfwGetCursorPos(mGLFWwindow, &xPos, &yPos);
        mousPose = glm::vec2(static_cast<float>(xPos), static_cast<float>(yPos));
    }
    bool XJGlfwWindow::IsMouseDown(MouseButton mouseButton) const//获取鼠标按键状态，供Vulkan使用
    {   
        return glfwGetMouseButton(mGLFWwindow, mouseButton) == GLFW_PRESS;
    }
    bool XJGlfwWindow::IsMouseUp(MouseButton mouseButton) const//获取鼠标按键状态，供Vulkan使用
    {
        return glfwGetMouseButton(mGLFWwindow, mouseButton) == GLFW_RELEASE;
        
    }

    
    bool XJGlfwWindow::IsKeyDown(int key) const//获取键盘按键状态，供Vulkan使用
    {
        return glfwGetKey(mGLFWwindow, key) == GLFW_PRESS;
    }
    bool XJGlfwWindow::IsKeyUp(int key) const//获取键盘按键
    {
        return glfwGetKey(mGLFWwindow, key) == GLFW_RELEASE;
    }
}