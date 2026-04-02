#ifndef XJ_EVENT_H
#define XJ_EVENT_H

#include <string>

namespace XJ
{
    enum XJEventType//事件类型枚举
    {
        //Window Event
        EVENT_TYPE_FRAMEBUFFER_RESIZE,//帧缓冲区大小改变事件
        EVENT_TYPE_WINDOW_FOCUS, //窗口获得焦点事件
        EVENT_TYPE_WINDOW_LOST_FOCUS, //窗口失去焦点事件
        EVENT_TYPE_WINDOW_MOVE, //窗口移动事件
        EVENT_TYPE_WINDOW_RESIZE,//窗口大小改变事件
        EVENT_TYPE_WINDOW_CLOSE,//窗口关闭事件
        //KEY
        EVENT_TYPE_KEY_PRESS, //按键按下事件
        EVENT_TYPE_KEY_RELEASE, //按键释放事件
        EVENT_TYPE_KEY_REPEAT, //按键持续按下事件
        //MOUSE
        EVENT_TYPE_MOUSE_BUTTON_PRESS, //鼠标按钮按下事件
        EVENT_TYPE_MOUSE_BUTTON_RELEASE, //鼠标按钮释放事件
        EVENT_TYPE_MOUSE_MOVE, //鼠标移动事件
        EVENT_TYPE_MOUSE_SCROLL, //鼠标滚轮事件
    };

//事件基类，所有事件都将从这个类继承
#define EVENT_CLASS_TYPE(type) static XJEventType XJGetStaticType() { return type; }\
                               virtual XJEventType XJGetEventType() const override { return XJGetStaticType(); }\
                               virtual const char* XJGetEventTypeName() const override { return #type; }

    class XJEvent
    {
        private:
            /* data */
        public:
            [[nodiscard]] virtual XJEventType XJGetEventType() const = 0;//获取事件类型
            [[nodiscard]] virtual const char *XJGetEventTypeName() const = 0;//获取事件类型名称
            [[nodiscard]] virtual const char *XJGetEventInfo() const = 0;//获取事件信息
            [[nodiscard]] virtual std::string ToString() const = 0;//将事件信息转换为字符串
             virtual ~XJEvent() = default;
    };
    
}



#endif