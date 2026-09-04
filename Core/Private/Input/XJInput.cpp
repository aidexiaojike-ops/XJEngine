#include "Input/XJInput.h"

#include "Edit/XJGlfwWindow.h"

namespace XJ
{
    XJInput& XJInput::XJGetInstance()
    {
        static XJInput instance;
        return instance;
    }

    void XJInput::SetWindow(XJGlfwWindow* window)
    {
        mWindow = window;
    }

    void XJInput::Update(float deltaTime)
    {
        (void)deltaTime; // 输入快照为纯轮询，deltaTime 预留用于未来平滑处理。

        XJInputState next;

        if (!mWindow)
        {
            mInputState = std::move(next);
            return;
        }

        // ---- 键盘：轮询 held，与上一帧比较得到 pressed / released ----
        // 从 KEY_SPACE(32) 开始：GLFW 对 < GLFW_KEY_SPACE 的 key 会触发
        // GLFW_INVALID_ENUM 错误回调（此处绑定 spdlog error），逐帧刷日志导致卡顿。
        for(int key = KEY_SPACE; key <= KEY_LAST; ++key)
        {
            bool isHeld = mWindow->IsKeyDown(key);
            next.mKeysHeld[key] = isHeld;
            next.mKeysPressed[key] = isHeld && !mPrevKeysHeld[key];
            next.mKeysReleased[key] = !isHeld && mPrevKeysHeld[key];
            mPrevKeysHeld[key] = isHeld;
        }

        // ---- 鼠标按键 ----
        for(int button = 0; button <= MOUSE_BUTTON_LAST; ++button)
        {
            bool isHeld = mWindow->IsMouseDown(static_cast<MouseButton>(button));
            next.mMouseButtonsHeld[button] = isHeld;
            next.mMouseButtonsPressed[button] = isHeld && !mPrevMouseButtonsHeld[button];
            next.mMouseButtonsReleased[button] = !isHeld && mPrevMouseButtonsHeld[button];
            mPrevMouseButtonsHeld[button] = isHeld;
        }

        // ---- 鼠标位置与增量 ----
        glm::vec2 mousePos{0.0f};
        mWindow->XJGetMousePos(mousePos);
        next.MousePosition = mousePos;

        if(mHasPrevMousePosition)
            next.MouseDelta = mousePos - mPrevMousePosition;
        else
            next.MouseDelta = glm::vec2(0.0f);

        mPrevMousePosition = mousePos;
        mHasPrevMousePosition = true;

        // ---- 滚轮（由窗口累积后消费） ----
        next.ScrollDelta = mWindow->XJConsumeScrollDelta();

        mInputState = std::move(next);
    }

    bool XJInputState::IsKeyDown(Key key) const
    {
        return key >= 0 && key <= KEY_LAST && mKeysHeld[key];
    }
    bool XJInputState::IsKeyPressed(Key key) const
    {
        return key >= 0 && key <= KEY_LAST && mKeysPressed[key];
    }
    bool XJInputState::IsKeyReleased(Key key) const
    {
        return key >= 0 && key <= KEY_LAST && mKeysReleased[key];
    }

    bool XJInputState::IsMouseButtonDown(MouseButton button) const
    {
        return button >= 0 && button <= MOUSE_BUTTON_LAST && mMouseButtonsHeld[button];
    }
    bool XJInputState::IsMouseButtonPressed(MouseButton button) const
    {
        return button >= 0 && button <= MOUSE_BUTTON_LAST && mMouseButtonsPressed[button];
    }
    bool XJInputState::IsMouseButtonReleased(MouseButton button) const
    {
        return button >= 0 && button <= MOUSE_BUTTON_LAST && mMouseButtonsReleased[button];
    }

    float XJInputState::GetAxis(XJInputAxis axis) const
    {
        switch(axis)
        {
            case XJInputAxis::Horizontal:
                return (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT) ? 1.0f : 0.0f) +
                       (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT) ? -1.0f : 0.0f);
            case XJInputAxis::Vertical:
                return (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP) ? 1.0f : 0.0f) +
                       (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN) ? -1.0f : 0.0f);
            default:
                return 0.0f;// 同时按相反方向时抵消为 0
        }
    }
}
