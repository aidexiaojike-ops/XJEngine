#ifndef XJ_SPDLOG_SCRIPT_LOG_SERVICE_H
#define XJ_SPDLOG_SCRIPT_LOG_SERVICE_H

#include "Script/Host/XJScriptHostServices.h"

#include <spdlog/spdlog.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace XJ
{
    class XJSpdlogScriptLogService final
        : public IXJScriptLogService
    {
        public:
            XJScriptHostStatus Write(//写入
                const XJScriptCallContext& context,//输入的字符
                XJScriptLogLevel level,//log等级
                std::string_view message) override
            {
                if (level != XJScriptLogLevel::Info &&
                    level != XJScriptLogLevel::Warning &&
                    level != XJScriptLogLevel::Error)
                {
                    return {XJScriptHostError::InvalidArgument, "Unknown script log level"};
                }

                const auto now = Clock::now();

                if (now - mWindow >= std::chrono::seconds(1))
                {
                    mWindow = now;
                    mGlobalCount = 0;
                    mCounts.clear();
                }

                if (mGlobalCount >= 256)
                {
                    ++mDropped;
                    return {};
                }

                const Key key{context.Owner, context.SlotId};

                auto& count = mCounts[key];

                if (count >= 16)
                {
                    ++mDropped;
                    return {};
                }

                ++count;
                ++mGlobalCount;

                message = message.substr(0, 4096);

                switch (level)
                {
                    case XJScriptLogLevel::Info:
                        spdlog::info(
                            "[Script scope={} entity={}:{} slot={} frame={}] {}",
                            context.Owner.ScopeId,
                            context.Owner.Index,
                            context.Owner.Generation,
                            context.SlotId,
                            context.FrameId,
                            message);
                        break;

                    case XJScriptLogLevel::Warning:
                        spdlog::warn(
                            "[Script scope={} entity={}:{} slot={} frame={}] {}",
                            context.Owner.ScopeId,
                            context.Owner.Index,
                            context.Owner.Generation,
                            context.SlotId,
                            context.FrameId,
                            message);
                        break;

                    case XJScriptLogLevel::Error:
                        spdlog::error(
                            "[Script scope={} entity={}:{} slot={} frame={}] {}",
                            context.Owner.ScopeId,
                            context.Owner.Index,
                            context.Owner.Generation,
                            context.SlotId,
                            context.FrameId,
                            message);
                        break;
                }

                return {};
            }

            uint64_t GetDroppedCount() const
            {
                return mDropped;
            }

        private:
            using Clock = std::chrono::steady_clock;

            struct Key
            {
                XJScriptEntityRef Owner;
                uint64_t SlotId = 0;

                bool operator==(const Key&) const = default;
            };

            struct KeyHash
            {
                size_t operator()(const Key& key) const
                {
                    size_t result =
                        std::hash<uint64_t>{}(key.Owner.ScopeId);

                    auto combine = [&result](uint64_t value)
                    {
                        result ^= std::hash<uint64_t>{}(value) +
                                  static_cast<size_t>(0x9e3779b9) +
                                  (result << 6) +
                                  (result >> 2);
                    };

                    combine(key.Owner.Index);
                    combine(key.Owner.Generation);
                    combine(key.SlotId);

                    return result;
                }
            };

            Clock::time_point mWindow = Clock::now();

            unsigned mGlobalCount = 0;
            uint64_t mDropped = 0;

            std::unordered_map<Key, unsigned, KeyHash> mCounts;
    };
}

#endif
