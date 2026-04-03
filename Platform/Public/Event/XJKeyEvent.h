#ifndef XJ_KEY_EVENT_H
#define XJ_KEY_EVENT_H

#include "Event/XJEvent.h"
#include <sstream>

namespace XJ
{
    enum Key//键枚举，定义了各种键的值
    {
        KEY_UNKNOWN            = -1,

        /* Printable keys */
        KEY_SPACE              = 32,
        KEY_APOSTROPHE         = 39,  /* ' */
        KEY_COMMA              = 44,  /* , */
        KEY_MINUS              = 45,  /* - */
        KEY_PERIOD             = 46,  /* . */
        KEY_SLASH              = 47,  /* / */
        KEY_0                  = 48,
        KEY_1                  = 49,
        KEY_2                  = 50,
        KEY_3                  = 51,
        KEY_4                  = 52,
        KEY_5                  = 53,
        KEY_6                  = 54,
        KEY_7                  = 55,
        KEY_8                  = 56,
        KEY_9                  = 57,
        KEY_SEMICOLON          = 59,  /* , */
        KEY_EQUAL              = 61,  /* = */
        KEY_A                  = 65,
        KEY_B                  = 66,
        KEY_C                  = 67,
        KEY_D                  = 68,
        KEY_E                  = 69,
        KEY_F                  = 70,
        KEY_G                  = 71,
        KEY_H                  = 72,
        KEY_I                  = 73,
        KEY_J                  = 74,
        KEY_K                  = 75,
        KEY_L                  = 76,
        KEY_M                  = 77,
        KEY_N                  = 78,
        KEY_O                  = 79,
        KEY_P                  = 80,
        KEY_Q                  = 81,
        KEY_R                  = 82,
        KEY_S                  = 83,
        KEY_T                  = 84,
        KEY_U                  = 85,
        KEY_V                  = 86,
        KEY_W                  = 87,
        KEY_X                  = 88,
        KEY_Y                  = 89,
        KEY_Z                  = 90,
        KEY_LEFT_BRACKET       = 91,  /* [ */
        KEY_BACKSLASH          = 92,  /* \ */
        KEY_RIGHT_BRACKET      = 93,  /* ] */
        KEY_GRAVE_ACCENT       = 96,  /* ` */
        KEY_WORLD_1            = 161, /* non-US #1 */
        KEY_WORLD_2            = 162, /* non-US #2 */

        /* Function keys */
        KEY_ESCAPE             = 256,
        KEY_ENTER              = 257,
        KEY_TAB                = 258,
        KEY_BACKSPACE          = 259,
        KEY_INSERT             = 260,
        KEY_DELETE             = 261,
        KEY_RIGHT              = 262,
        KEY_LEFT               = 263,
        KEY_DOWN               = 264,
        KEY_UP                 = 265,
        KEY_PAGE_UP            = 266,
        KEY_PAGE_DOWN          = 267,
        KEY_HOME               = 268,
        KEY_END                = 269,
        KEY_CAPS_LOCK          = 280,
        KEY_SCROLL_LOCK        = 281,
        KEY_NUM_LOCK           = 282,
        KEY_PRINT_SCREEN       = 283,
        KEY_PAUSE              = 284,
        KEY_F1                 = 290,
        KEY_F2                 = 291,
        KEY_F3                 = 292,
        KEY_F4                 = 293,
        KEY_F5                 = 294,
        KEY_F6                 = 295,
        KEY_F7                 = 296,
        KEY_F8                 = 297,
        KEY_F9                 = 298,
        KEY_F10                = 299,
        KEY_F11                = 300,
        KEY_F12                = 301,
        KEY_F13                = 302,
        KEY_F14                = 303,
        KEY_F15                = 304,
        KEY_F16                = 305,
        KEY_F17                = 306,
        KEY_F18                = 307,
        KEY_F19                = 308,
        KEY_F20                = 309,
        KEY_F21                = 310,
        KEY_F22                = 311,
        KEY_F23                = 312,
        KEY_F24                = 313,
        KEY_F25                = 314,
        KEY_KP_0               = 320,
        KEY_KP_1               = 321,
        KEY_KP_2               = 322,
        KEY_KP_3               = 323,
        KEY_KP_4               = 324,
        KEY_KP_5               = 325,
        KEY_KP_6               = 326,
        KEY_KP_7               = 327,
        KEY_KP_8               = 328,
        KEY_KP_9               = 329,
        KEY_KP_DECIMAL         = 330,
        KEY_KP_DIVIDE          = 331,
        KEY_KP_MULTIPLY        = 332,
        KEY_KP_SUBTRACT        = 333,
        KEY_KP_ADD             = 334,
        KEY_KP_ENTER           = 335,
        KEY_KP_EQUAL           = 336,
        KEY_LEFT_SHIFT         = 340,
        KEY_LEFT_CONTROL       = 341,
        KEY_LEFT_ALT           = 342,
        KEY_LEFT_SUPER         = 343,
        KEY_RIGHT_SHIFT        = 344,
        KEY_RIGHT_CONTROL      = 345,
        KEY_RIGHT_ALT          = 346,
        KEY_RIGHT_SUPER        = 347,
        KEY_MENU               = 348,
        KEY_LAST               = KEY_MENU,
    };
    enum KeyMod//键修饰符枚举，定义了各种键修饰符的值
    {
        MOD_SHIFT   = 0x0001,
        MOD_CONTROL = 0x0002,
        MOD_ALT     = 0x0004,
        MOD_SUPER   = 0x0008,
        MOD_CAPS_LOCK = 0x0010,
        MOD_NUM_LOCK = 0x0020,
    };
    static const char* KeyToString(Key key)//将键枚举转换为字符串
    {
        switch (key)
        {
            case KEY_UNKNOWN: return "KEY_UNKNOWN";
            case KEY_SPACE: return "KEY_SPACE";
            case KEY_APOSTROPHE: return "KEY_APOSTROPHE";
            case KEY_COMMA: return "KEY_COMMA";
            case KEY_MINUS: return "KEY_MINUS";
            case KEY_PERIOD: return "KEY_PERIOD";
            case KEY_SLASH: return "KEY_SLASH";
            case KEY_0: return "KEY_0";
            case KEY_1: return "KEY_1";
            case KEY_2: return "KEY_2";
            case KEY_3: return "KEY_3";
            case KEY_4: return "KEY_4";
            case KEY_5: return "KEY_5";
            case KEY_6: return "KEY_6";
            case KEY_7: return "KEY_7";
            case KEY_8: return "KEY_8";
            case KEY_9: return "KEY_9";
            case KEY_SEMICOLON: return "KEY_SEMICOLON";
            case KEY_EQUAL: return "KEY_EQUAL";
            case KEY_A: return "KEY_A";
            case KEY_B: return "KEY_B";
            case KEY_C: return "KEY_C";
            case KEY_D: return "KEY_D";
            case KEY_E: return "KEY_E";
            case KEY_F: return "KEY_F";
            case KEY_G: return "KEY_G";
            case KEY_H: return "KEY_H";
            case KEY_I: return "KEY_I";
            case KEY_J: return "KEY_J";
            case KEY_K: return "KEY_K";
            case KEY_L: return "KEY_L";
            case KEY_M: return "KEY_M";
            case KEY_N: return "KEY_N";
            case KEY_O: return "KEY_O";
            case KEY_P: return "KEY_P";
            case KEY_Q: return "KEY_Q";
            case KEY_R: return "KEY_R";
            case KEY_S: return "KEY_S";
            case KEY_T: return "KEY_T";
            case KEY_U: return "KEY_U";
            case KEY_V: return "KEY_V";
            case KEY_W: return "KEY_W";
            case KEY_X: return "KEY_X";
            case KEY_Y: return "KEY_Y";
            case KEY_Z: return "KEY_Z";
            case KEY_LEFT_BRACKET: return "KEY_LEFT_BRACKET";
            case KEY_BACKSLASH: return "KEY_BACKSLASH";
            case KEY_RIGHT_BRACKET: return "KEY_RIGHT_BRACKET";
            case KEY_GRAVE_ACCENT: return "KEY_GRAVE_ACCENT";
            case KEY_WORLD_1: return "KEY_WORLD_1";
            case KEY_WORLD_2: return "KEY_WORLD_2";
            case KEY_ESCAPE: return "KEY_ESCAPE";
            case KEY_ENTER: return "KEY_ENTER";
            case KEY_TAB: return "KEY_TAB";
            case KEY_BACKSPACE: return "KEY_BACKSPACE";
            case KEY_INSERT: return "KEY_INSERT";
            case KEY_DELETE: return "KEY_DELETE";
            case KEY_RIGHT: return "KEY_RIGHT";
            case KEY_LEFT: return "KEY_LEFT";
            case KEY_DOWN: return "KEY_DOWN";
            case KEY_UP: return "KEY_UP";
            case KEY_PAGE_UP: return "KEY_PAGE_UP";
            case KEY_PAGE_DOWN: return "KEY_PAGE_DOWN";
            case KEY_HOME: return "KEY_HOME";
            case KEY_END: return "KEY_END";
            case KEY_CAPS_LOCK: return "KEY_CAPS_LOCK";
            case KEY_SCROLL_LOCK: return "KEY_SCROLL_LOCK";
            case KEY_NUM_LOCK: return "KEY_NUM_LOCK";
            case KEY_PRINT_SCREEN: return "KEY_PRINT_SCREEN";
            case KEY_PAUSE: return "KEY_PAUSE";
            case KEY_F1: return "KEY_F1";
            case KEY_F2: return "KEY_F2";
            case KEY_F3: return "KEY_F3";
            case KEY_F4: return "KEY_F4";
            case KEY_F5: return "KEY_F5";
            case KEY_F6: return "KEY_F6";
            case KEY_F7: return "KEY_F7";
            case KEY_F8: return "KEY_F8";
            case KEY_F9: return "KEY_F9";
            case KEY_F10: return "KEY_F10";
            case KEY_F11: return "KEY_F11";
            case KEY_F12: return "KEY_F12";
            case KEY_F13: return "KEY_F13";
            case KEY_F14: return "KEY_F14";
            case KEY_F15: return "KEY_F15";
            case KEY_F16: return "KEY_F16";
            case KEY_F17: return "KEY_F17";
            case KEY_F18: return "KEY_F18";
            case KEY_F19: return "KEY_F19";
            case KEY_F20: return "KEY_F20";
            case KEY_F21: return "KEY_F21";
            case KEY_F22: return "KEY_F22";
            case KEY_F23: return "KEY_F23";
            case KEY_F24: return "KEY_F24";
            case KEY_F25: return "KEY_F25";
            case KEY_KP_0: return "KEY_KP_0";
            case KEY_KP_1: return "KEY_KP_1";
            case KEY_KP_2: return "KEY_KP_2";
            case KEY_KP_3: return "KEY_KP_3";
            case KEY_KP_4: return "KEY_KP_4";
            case KEY_KP_5: return "KEY_KP_5";
            case KEY_KP_6: return "KEY_KP_6";
            case KEY_KP_7: return "KEY_KP_7";
            case KEY_KP_8: return "KEY_KP_8";
            case KEY_KP_9: return "KEY_KP_9";
            case KEY_KP_DECIMAL: return "KEY_KP_DECIMAL";
            case KEY_KP_DIVIDE: return "KEY_KP_DIVIDE";
            case KEY_KP_MULTIPLY: return "KEY_KP_MULTIPLY";
            case KEY_KP_SUBTRACT: return "KEY_KP_SUBTRACT";
            case KEY_KP_ADD: return "KEY_KP_ADD";
            case KEY_KP_ENTER: return "KEY_KP_ENTER";
            case KEY_KP_EQUAL: return "KEY_KP_EQUAL";
            case KEY_LEFT_SHIFT: return "KEY_LEFT_SHIFT";
            case KEY_LEFT_CONTROL: return "KEY_LEFT_CONTROL";
            case KEY_LEFT_ALT: return "KEY_LEFT_ALT";
            case KEY_LEFT_SUPER: return "KEY_LEFT_SUPER";
            case KEY_RIGHT_SHIFT: return "KEY_RIGHT_SHIFT";
            case KEY_RIGHT_CONTROL: return "KEY_RIGHT_CONTROL";
            case KEY_RIGHT_ALT: return "KEY_RIGHT_ALT";
            case KEY_RIGHT_SUPER: return "KEY_RIGHT_SUPER";
            case KEY_MENU: return "KEY_MENU";
        }
        return "KEY_UNKNOWN";
    }
    
    static const char* XJKeyModToStr(KeyMod keyMod){
        switch (keyMod) {
            case MOD_SHIFT: return "MOD_SHIFT";
            case MOD_CONTROL: return "MOD_CONTROL";
            case MOD_ALT: return "MOD_ALT";
            case MOD_SUPER: return "MOD_SUPER";
            case MOD_CAPS_LOCK: return "MOD_CAPS_LOCK";
            case MOD_NUM_LOCK: return "MOD_NUM_LOCK";
        }
        return "unknown";
    }

//按键事件类，包含按键按下和按键释放事件
    class XJKeyInputPressEvent : public XJEvent
    {
        public:
            XJKeyInputPressEvent(Key keyCode, KeyMod keyMod, bool repeat) : mKeyCode(keyCode), mKeyMod(keyMod), mRepeat(repeat)
            {};

            [[nodiscard]] std::string ToString() const override 
            {
                std::stringstream ss;
                ss << XJEvent::ToString() << " KeyCode: " << KeyToString(mKeyCode) 
                << " KeyMod: " << mKeyMod << " Repeat: " << mRepeat;
                return ss.str();
            }

            [[nodiscard]] bool IsShiftPressed() const { return (mKeyMod & MOD_SHIFT) != 0; }//判断Shift键是否被按下
            [[nodiscard]] bool IsControlPressed() const { return (mKeyMod & MOD_CONTROL) != 0; }//判断Control键是否被按下
            [[nodiscard]] bool IsAltPressed() const { return (mKeyMod & MOD_ALT) != 0; }//  判断Alt键是否被按下
            [[nodiscard]] bool IsSuperPressed() const { return (mKeyMod & MOD_SUPER) != 0; }//判断Super键是否被按下
            [[nodiscard]] bool IsCapsLockOn() const { return (mKeyMod & MOD_CAPS_LOCK) != 0; }//判断CapsLock键是否被按下
            [[nodiscard]] bool IsNumLockOn() const { return (mKeyMod & MOD_NUM_LOCK) != 0; }//判断NumLock键是否被按下
            [[nodiscard]] bool IsRepeat() const { return mRepeat; }//判断是否为重复按键事件

            Key mKeyCode;
            KeyMod mKeyMod;
            bool mRepeat;

            EVENT_CLASS_TYPE(EVENT_TYPE_KEY_PRESS);

    };
//按键释放事件类，包含按键释放事件
    class XJKeyReleaseEvent : public XJEvent 
    {
        public:
            XJKeyReleaseEvent(Key KeyCode) : mKeyCode(KeyCode){};
            [[nodiscard]] std::string ToString() const override
            {
                std::stringstream ss;
                ss << XJEvent::ToString();
                ss << " KeyCode: " << KeyToString(mKeyCode);
                return ss.str();
            }
        
            Key mKeyCode;
            EVENT_CLASS_TYPE(EVENT_TYPE_KEY_RELEASE);
    };

}


#endif