#include "Edit/XJEventTesting.h"
#include "Event/XJMouseEvent.h"
#include "Event/XJKeyEvent.h"
#include "Event/XJWindowEvent.h"
#include "Edit/SpdlogDebug.h"

namespace XJ
{
    XJEventTesting::XJEventTesting()
    {
        mObserver =std::make_shared<XJ::XJEventObserver>();
      

        mObserver->OnEvent<XJMouseButtonReleaseEvent>([this](const XJMouseButtonReleaseEvent& event)//鼠标按钮释放事件处理函数
        {    //spdlog::info("Mouse Button Released at position: ({})",event.ToString());
        });
        mObserver->OnEvent<XJMouseButtonPressEvent>([](const XJMouseButtonPressEvent& event)//鼠标按钮按下事件处理函数
        {    //spdlog::info("Mouse Button Pressed at position: ({})",event.ToString());
        });

        //mObserver->OnEvent<XJMouseMoveEvent>([](const XJMouseMoveEvent& event)//鼠标移动事件处理函数
        //{    spdlog::info("Mouse Moved to position: ({})",event.ToString());});
        mObserver->OnEvent<XJMouseScrollEvent>([](const XJMouseScrollEvent& event)//鼠标滚轮事件处理函数
        {    //spdlog::info("Mouse Scroll to position: ({})",event.ToString());
        });

        
        mObserver->OnEvent<XJFrameBufferResizeEvent>([](const XJFrameBufferResizeEvent& event)//帧缓冲区大小改变事件处理函数
        {    //spdlog::info("Key Pressed: {}", event.ToString());
        });
        mObserver->OnEvent<XJWindowFocusEvent>([](const XJWindowFocusEvent& event)//窗口获得焦点事件处理函数
        {    //spdlog::info("Window Focus: {}", event.ToString());
        });
        mObserver->OnEvent<XJWindowLostFocusEvent>([](const XJWindowLostFocusEvent& event)//窗口失去焦点事件处理函数
        {    //spdlog::info("Window Lost Focus: {}", event.ToString());
        });
        mObserver->OnEvent<XJWindowMoveEvent>([](const XJWindowMoveEvent& event)//窗口移动事件处理函数
        {    //spdlog::info("Window Move: {}", event.ToString());
        });
        mObserver->OnEvent<XJWindowResizeEvent>([](const XJWindowResizeEvent& event)//窗口大小改变事件处理函数
        {    //spdlog::info("Window Resize: {}", event.ToString());
        });
        mObserver->OnEvent<XJWindowCloseEvent>([](const XJWindowCloseEvent& event)//窗口关闭事件处理函数
        {    spdlog::info("Window Close: {}", event.ToString());
        });

      

        mObserver->OnEvent<XJKeyInputPressEvent>([](const XJKeyInputPressEvent& event)//按键按下事件处理函数
        {    //spdlog::info("Key Pressed: {}", event.ToString());
        });
         mObserver->OnEvent<XJKeyReleaseEvent>([](const XJKeyReleaseEvent& event)//按键释放事件处理函数
        {    //spdlog::info("Key Released: {}", event.ToString());
        });
   

    }
    XJEventTesting::~XJEventTesting()
    {
        mObserver.reset();
    }
    void XJEventTesting::TestMemberFunc(const XJMouseButtonReleaseEvent &event)
    {
        spdlog::info("Mouse Button Released at position: ({})",event.ToString());
    }
}