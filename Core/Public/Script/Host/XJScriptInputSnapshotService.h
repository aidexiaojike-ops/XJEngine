#ifndef XJ_SCRIPT_INPUT_SNAPSHOT_SERVICE_H
#define XJ_SCRIPT_INPUT_SNAPSHOT_SERVICE_H

#include "Script/Host/XJScriptHostServices.h"
#include "Input/XJInput.h"

#include <map>
#include <array>

namespace XJ
{
    class XJScriptInputSnapshotService final
        : public IXJScriptInputService
    {
        public:
            // 可由平台输入或测试/回放数据构造；不暴露 XJInputState 的内部存储。
            struct KeyboardSnapshot
            {
                std::array<bool, KEY_LAST + 1> Held{};
                std::array<bool, KEY_LAST + 1> Pressed{};
                std::array<bool, KEY_LAST + 1> Released{};
            };

            XJScriptInputSnapshotService()
            {
                for (int index = 0; index < 26; ++index)
                {
                    mKeys.emplace(
                        std::string(1, static_cast<char>('A' + index)),
                        static_cast<Key>(KEY_A + index));
                }

                for (int index = 0; index < 10; ++index)
                {
                    mKeys.emplace(
                        std::string(1, static_cast<char>('0' + index)),
                        static_cast<Key>(KEY_0 + index));
                }

                mKeys.emplace("Space", KEY_SPACE);
                mKeys.emplace("Escape", KEY_ESCAPE);
                mKeys.emplace("Enter", KEY_ENTER);
                mKeys.emplace("Tab", KEY_TAB);

                mKeys.emplace("Left", KEY_LEFT);
                mKeys.emplace("Right", KEY_RIGHT);
                mKeys.emplace("Up", KEY_UP);
                mKeys.emplace("Down", KEY_DOWN);

                mKeys.emplace("LeftShift", KEY_LEFT_SHIFT);
                mKeys.emplace("RightShift", KEY_RIGHT_SHIFT);
                mKeys.emplace("LeftControl", KEY_LEFT_CONTROL);
                mKeys.emplace("RightControl", KEY_RIGHT_CONTROL);
            }

            void Publish(
                const XJInputState& input,
                bool enabled)
            {
                KeyboardSnapshot snapshot;
                for (const auto& [name, key] : mKeys)
                {
                    snapshot.Held[key] = input.IsKeyDown(key);
                    snapshot.Pressed[key] = input.IsKeyPressed(key);
                    snapshot.Released[key] = input.IsKeyReleased(key);
                }
                PublishKeyboard(snapshot, enabled);
            }

            void PublishKeyboard(const KeyboardSnapshot& input, bool enabled)
            {
                // 重新获得焦点的第一帧不提供边沿，避免 UI 点击穿透。
                mAllowEdges = enabled && mEnabled;
                mEnabled = enabled;
                mState = input;
            }

            void Reset()
            {
                mState = {};
                mEnabled = false;
                mAllowEdges = false;
            }

            XJScriptHostResult<double> GetAxis(std::string_view name) const override//获取轴
            {
                if (name == "Horizontal")
                    return {Axis(KEY_D, KEY_RIGHT, KEY_A, KEY_LEFT), {}};
                if (name == "Vertical")
                    return {Axis(KEY_W, KEY_UP, KEY_S, KEY_DOWN), {}};
                return Invalid<double>("Unknown input axis");
            }

            XJScriptHostResult<bool> IsKeyHeld(std::string_view name) const override//键盘是否按下
            {
                const auto found = mKeys.find(name);

                if (found == mKeys.end())
                    return Invalid<bool>("Unknown input key");

                return {
                    mEnabled && mState.Held[found->second],
                    {}
                };
            }

            XJScriptHostResult<bool> WasKeyPressed(std::string_view name,XJScriptPhase phase) const override//按键按下
            {
                const auto found = mKeys.find(name);

                if (found == mKeys.end())
                    return Invalid<bool>("Unknown input key");

                return {
                    mAllowEdges &&
                    phase == XJScriptPhase::Update &&
                    mState.Pressed[found->second],
                    {}
                };
            }

            XJScriptHostResult<bool> WasKeyReleased(std::string_view name, XJScriptPhase phase) const override
            {
                const auto found = mKeys.find(name);

                if (found == mKeys.end())
                    return Invalid<bool>("Unknown input key");

                return {
                    mAllowEdges &&
                    phase == XJScriptPhase::Update &&
                    mState.Released[found->second],
                    {}
                };
            }

        private:
            double Axis(Key positive, Key positiveAlt, Key negative, Key negativeAlt) const
            {
                if (!mEnabled)
                    return 0.0;
                return (mState.Held[positive] || mState.Held[positiveAlt] ? 1.0 : 0.0) -
                       (mState.Held[negative] || mState.Held[negativeAlt] ? 1.0 : 0.0);
            }

            template<typename T>
            static XJScriptHostResult<T> Invalid(
                std::string message)
            {
                return {
                    {},
                    {
                        XJScriptHostError::InvalidArgument,
                        std::move(message)
                    }
                };
            }

            std::map<std::string, Key, std::less<>> mKeys;

            KeyboardSnapshot mState;

            bool mEnabled = false;
            bool mAllowEdges = false;
    };
}

#endif
