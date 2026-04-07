#ifndef XJ_MOUSE_EVENT_H
#define XJ_MOUSE_EVENT_H

#include "Event/XJKeyEvent.h"

namespace XJ
{
    enum MouseButton
    {
        MOUSE_BUTTON_LEFT = 0,
        MOUSE_BUTTON_RIGHT = 1,
        MOUSE_BUTTON_MIDDLE = 2,
        MOUSE_BUTTON_4 = 3,
        MOUSE_BUTTON_5 = 4,
        MOUSE_BUTTON_6 = 5,
        MOUSE_BUTTON_7 = 6,
        MOUSE_BUTTON_8 = 7,
        MOUSE_BUTTON_LAST = MOUSE_BUTTON_8
    };
//鼠标按钮事件类，包含鼠标按钮按下和鼠标按钮释放事件
    static const char* XJMouseButtonToStr(MouseButton button){
        switch (button) {
            case MOUSE_BUTTON_LEFT: return "MOUSE_BUTTON_LEFT";
            case MOUSE_BUTTON_RIGHT: return "MOUSE_BUTTON_RIGHT";
            case MOUSE_BUTTON_MIDDLE: return "MOUSE_BUTTON_MIDDLE";
            case MOUSE_BUTTON_4: return "MOUSE_BUTTON_4";
            case MOUSE_BUTTON_5: return "MOUSE_BUTTON_5";
            case MOUSE_BUTTON_6: return "MOUSE_BUTTON_6";
            case MOUSE_BUTTON_7: return "MOUSE_BUTTON_7";
            case MOUSE_BUTTON_8: return "MOUSE_BUTTON_8";
        }
        return "unknown";
    }

    //鼠标按钮按下事件类，包含鼠标按钮按下事件
    class XJMouseButtonPressEvent : public XJEvent
    {
        public:
            XJMouseButtonPressEvent(MouseButton mouseButton, KeyMod keyMod, bool repeat) 
            : mMouseButton(mouseButton), mKeyMod(keyMod), mRepeat(repeat){};
            [[nodiscard]] std::string ToString() const override
            {
                std::stringstream ss;
                ss << XJEvent::ToString();
                ss << "mouseButton: " << XJMouseButtonToStr(mMouseButton);
                ss << "keyMod: " << XJKeyModToStr(mKeyMod);
                ss << "isRepeat: " << mRepeat;
                return ss.str();
            }
            //判断是否按下了Shift、Control、Alt、Super、CapsLock和NumLock键，以及是否是重复事件
            [[nodiscard]] bool IsShiftPressed() const { return mKeyMod & KEY_MOD_SHIFT; };
            [[nodiscard]] bool IsControlPressed() const { return mKeyMod & KEY_MOD_CONTROL; };
            [[nodiscard]] bool IsAltPressed() const { return mKeyMod & KEY_MOD_ALT; };
            [[nodiscard]] bool IsSuperPressed() const { return mKeyMod & KEY_MOD_SUPER; };
            [[nodiscard]] bool IsCapsLockPressed() const { return mKeyMod & KEY_MOD_CAPS_LOCK; };   
            [[nodiscard]] bool IsNumLockPressed() const { return mKeyMod & KEY_MOD_NUM_LOCK; };      
            [[nodiscard]] bool IsRepeat() const { return mRepeat; };

            MouseButton mMouseButton;
            KeyMod mKeyMod;
            bool mRepeat;
            EVENT_CLASS_TYPE(EVENT_TYPE_MOUSE_BUTTON_PRESS);
    };

//鼠标按钮释放事件类，包含鼠标按钮释放事件
    class XJMouseButtonReleaseEvent : public XJEvent
    {
        public:
            XJMouseButtonReleaseEvent(MouseButton mouseButton) : mMouseButton(mouseButton){};//构造函数，初始化鼠标按钮成员变量
            [[nodiscard]] std::string ToString() const override//将事件信息转换为字符串
            {
                std::stringstream ss;
                ss << XJEvent::ToString();
                ss << "mouseButton: " << XJMouseButtonToStr(mMouseButton);
                return ss.str();
            }

            MouseButton mMouseButton;//鼠标按钮
            EVENT_CLASS_TYPE(EVENT_TYPE_MOUSE_BUTTON_RELEASE);//事件类型为鼠标按钮释放事件
    };

////鼠标移动
    class XJMouseMoveEvent : public XJEvent
    {
        public:
            XJMouseMoveEvent(double xPos, double yPos) : mXPos(xPos), mYPos(yPos){};
            [[nodiscard]] std::string ToString() const override
            {
                std::stringstream ss;
                ss << XJEvent::ToString();
                ss << "xPos: " << mXPos << " yPos: " << mYPos;
                return ss.str();
            }

            double mXPos, mYPos;//鼠标位置
            EVENT_CLASS_TYPE(EVENT_TYPE_MOUSE_MOVE);
    };
    //滚轮事件类，包含鼠标滚轮事件
    class XJMouseScrollEvent : public XJEvent
    {
        public:
            XJMouseScrollEvent(double xOffset, double yOffset) : mXOffset(xOffset), mYOffset(yOffset){};
            [[nodiscard]] std::string ToString() const override
            {
                std::stringstream ss;
                ss << XJEvent::ToString();
                ss << "xOffset: " << mXOffset << " yOffset: " << mYOffset;
                return ss.str();
            }

            double mXOffset, mYOffset;//滚轮偏移量
            EVENT_CLASS_TYPE(EVENT_TYPE_MOUSE_SCROLL);
    };
}




#endif