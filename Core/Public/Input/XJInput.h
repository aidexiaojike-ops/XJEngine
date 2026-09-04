#ifndef XJ_INPUT_H
#define XJ_INPUT_H

#include "Edit/Mathinclude.h"
#include "Event/XJKeyEvent.h"
#include "Event/XJMouseEvent.h"

namespace XJ
{
    class XJGlfwWindow;

    // 输入轴向（游戏逻辑常用）。
    enum class XJInputAxis
    {
        None = 0,
        Horizontal,//A/D 或者 Left/Right
        Vertical,//W/S 或者 Up/Down
    };

    // 每帧由主循环填写的输入快照；系统只读，不修改。
    struct XJInputState
    {
        public:
            // 键盘：按住 / 本帧按下（触发）/ 本帧释放。
            bool IsKeyDown(Key key) const;
            bool IsKeyPressed(Key key) const;
            bool IsKeyReleased(Key key) const;

            // 鼠标按键。
            bool IsMouseButtonDown(MouseButton button) const;
            bool IsMouseButtonPressed(MouseButton button) const;
            bool IsMouseButtonReleased(MouseButton button) const;

            glm::vec2 MousePosition = glm::vec2(0.0f, 0.0f);// 鼠标位置，左上角为(0,0)，右下角为窗口大小
            glm::vec2 MouseScrollOffset = glm::vec2(0.0f, 0.0f);// 鼠标滚轮偏移量，垂直滚轮为Y，水平滚轮为X
            glm::vec2 MouseDelta{0.0f};      // 本帧相对上一帧的位移
            glm::vec2 ScrollDelta{0.0f};     // 本帧滚轮增量

            // WASD + 方向键合成的 -1..1 轴值。
            float GetAxis(XJInputAxis axis) const;

        private:
            friend class XJInput;

            bool mKeysHeld[KEY_LAST + 1] = { false };
            bool mKeysPressed[KEY_LAST + 1] = { false };
            bool mKeysReleased[KEY_LAST + 1] = { false };

            bool mMouseButtonsHeld[MOUSE_BUTTON_LAST + 1] = { false };
            bool mMouseButtonsPressed[MOUSE_BUTTON_LAST + 1] = { false };
            bool mMouseButtonsReleased[MOUSE_BUTTON_LAST + 1] = { false };
    };

    // 输入单例：主循环每帧 Update()，游戏系统在 OnUpdate/OnFixedUpdate 里查询。
    class XJInput
    {
        public:
            static XJInput& XJGetInstance();

            void SetWindow(XJGlfwWindow* window);
            void Update(float deltaTime);// 每帧调用一次：轮询 GLFW，计算边沿与增量。

            const XJInputState& GetInputState() const { return mInputState; }

            XJInput(const XJInput&) = delete;
            XJInput& operator=(const XJInput&) = delete;

        private:
            XJInput() = default;

            XJGlfwWindow* mWindow = nullptr;
            XJInputState mInputState;

            bool mPrevKeysHeld[KEY_LAST + 1] = { false };
            bool mPrevMouseButtonsHeld[MOUSE_BUTTON_LAST + 1] = { false };
            glm::vec2 mPrevMousePosition{0.0f};
            bool mHasPrevMousePosition = false;
    };

}

#endif
