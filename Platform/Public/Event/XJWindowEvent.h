#ifndef XJ_WINDOW_EVENT_H
#define XJ_WINDOW_EVENT_H

#include "Event/XJEvent.h"

namespace XJ
{
    
    class XJFrameBufferResizeEvent : public XJEvent//帧缓冲区大小改变事件类，继承自XJEvent
    {
        public:
            XJFrameBufferResizeEvent(uint32_t width, uint32_t height)//构造函数，接受新的宽度和高度参数
                : mWidth(width), mHeight(height) {}
            [[nodiscard]] std::string ToString() const override
            {
                return XJEvent::ToString() + "(with: " + std::to_string(mWidth) + ", height: " + std::to_string(mHeight) + ")";
            }
    

        uint32_t mWidth;
        uint32_t mHeight;

        EVENT_CLASS_TYPE(EVENT_TYPE_FRAMEBUFFER_RESIZE);
    };

    class XJWindowFocusEvent : public XJEvent//窗口获得焦点事件类，继承自XJEvent
    {
        public:
            XJWindowFocusEvent() = default;
            [[nodiscard]] std::string ToString() const override
            {
                return XJEvent::ToString();
            }
        EVENT_CLASS_TYPE(EVENT_TYPE_WINDOW_FOCUS);
    };

    class XJWindowLostFocusEvent : public XJEvent//窗口失去焦点事件类，继承自XJEvent
    {
        public:
            XJWindowLostFocusEvent() = default;
            [[nodiscard]] std::string ToString() const override
            {
                return XJEvent::ToString();
            }
        EVENT_CLASS_TYPE(EVENT_TYPE_WINDOW_LOST_FOCUS);
    };

    class XJWindowMoveEvent : public XJEvent//窗口移动事件类，继承自XJEvent
    {
        public:
            XJWindowMoveEvent(int32_t xPos, int32_t yPos)//构造函数，接受新的X和Y位置参数
                : mXPos(xPos), mYPos(yPos) {}
            [[nodiscard]] std::string ToString() const override
            {
                return XJEvent::ToString() + "(xPos: " + std::to_string(mXPos) + ", yPos: " + std::to_string(mYPos) + ")";
            }
        private:
            int32_t mXPos;
            int32_t mYPos;

        EVENT_CLASS_TYPE(EVENT_TYPE_WINDOW_MOVE);
    };

    class XJWindowResizeEvent : public XJEvent//窗口大小改变事件类，继承自XJEvent
    {
        public:
            XJWindowResizeEvent(uint32_t width, uint32_t height)//构造函数，接受新的宽度和高度参数
                : mWidth(width), mHeight(height) {}
            [[nodiscard]] std::string ToString() const override
            {
                return XJEvent::ToString() + "(width: " + std::to_string(mWidth) + ", height: " + std::to_string(mHeight) + ")";
            }
        private:
            uint32_t mWidth;
            uint32_t mHeight;

        EVENT_CLASS_TYPE(EVENT_TYPE_WINDOW_RESIZE);
    };

    class XJWindowCloseEvent : public XJEvent//窗口关闭事件类，继承自XJEvent
    {
        public:
            XJWindowCloseEvent() = default;
            [[nodiscard]] std::string ToString() const override
            {
                return XJEvent::ToString();
            }
        EVENT_CLASS_TYPE(EVENT_TYPE_WINDOW_CLOSE);
    };
}


#endif