#include "UI/XJEditorLog.h"
#include <chrono>
#include <sstream>
#include <iomanip>

#include <spdlog/details/log_msg.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <mutex>

namespace XJ
{

    namespace
    {
        XJEditorLogLevel ToEditorLogLevel(spdlog::level::level_enum level)
        {
            switch(level){
                case spdlog::level::trace:
                    return XJEditorLogLevel::Trace;

                case spdlog::level::warn:
                    return XJEditorLogLevel::Warning;

                case spdlog::level::err:
                    return XJEditorLogLevel::Error;

                case spdlog::level::critical:
                    return XJEditorLogLevel::Critical;

                // 当前编辑器没有单独的 Debug 级别，
                // 因此 Debug 和 Info 都显示为 Info。
                case spdlog::level::debug:
                case spdlog::level::info:
                default:
                    return XJEditorLogLevel::Info;
            }
        }

        class XJEditorLogSink final
            : public spdlog::sinks::base_sink<std::mutex>
        {
            protected:
                void sink_it_(
                    const spdlog::details::log_msg& message) override
                {
                    // log_msg::payload 只在本次 sink 调用期间有效，
                    // 必须立即复制，不能把 string_view 保存到 UI 队列。
                    const std::string text(
                        message.payload.data(),
                        message.payload.size());

                    XJEditorLog::XJGet().Push(
                        ToEditorLogLevel(message.level),
                        text);
                }

                void flush_() override
                {
                    // UI 日志位于内存中，不需要额外 flush。
                }
        };
    }

     XJEditorLog& XJEditorLog::XJGet()
    {
        static XJEditorLog instance;
        return instance;
    }

    void XJEditorLog::Push(XJEditorLogLevel level, const std::string& message)
    {
        // 获取当前时间戳，格式化为字符串，包含日期、时间和毫秒
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        std::tm tm = {};
        localtime_s(&tm, &time);

        std::ostringstream timeStr;
        timeStr << std::setfill('0')
                << std::setw(2) << tm.tm_hour << ":"
                << std::setw(2) << tm.tm_min << ":"
                << std::setw(2) << tm.tm_sec << "."
                << std::setw(3) << ms.count();
        // 创建日志条目，包含日志级别、消息内容和时间戳
        XJEditorLogEntry entry;
        entry.Level   = level;
        entry.Message = message;
        entry.Timestamp    = timeStr.str();

        {
            std::lock_guard<std::mutex> lock(mMutex);
            mEntries.push_back(std::move(entry));
            while (static_cast<int>(mEntries.size()) > mMaxLines)
                mEntries.pop_front();
        }

    }

    void XJEditorLog::Clear()
    {
        std::lock_guard<std::mutex> lock(mMutex);
        mEntries.clear();
    }

    void XJEditorLog::SetMaxLines(int maxLines)
    {
        if(maxLines < 10) // 最小保留 10 行日志，防止过度限制日志输出
            maxLines = 10;

        std::lock_guard<std::mutex> lock(mMutex);
        mMaxLines = maxLines;//更新最大行数设置后，立即丢弃超过新限制的旧日志条目
        while (static_cast<int>(mEntries.size()) > mMaxLines)//如果当前日志条目数量超过新的最大行数限制，丢弃最旧的条目，直到满足限制
            mEntries.pop_front();//丢弃最旧的日志条目
    }
    
    std::deque<XJEditorLogEntry> XJEditorLog::XJGetEntriesCopy() const
    {
        std::lock_guard<std::mutex> lock(mMutex);
        return mEntries;
    }

    void XJEditorLog::AttachToSpdlog()
    {
        auto logger = spdlog::default_logger();
        if(!logger)
            return;

        std::shared_ptr<spdlog::sinks::sink> sink;
        
        {
            std::lock_guard<std::mutex> lock(mMutex);

            // Init 避免同一条日志在面板中出现多次。
            if (mSpdlogSink)
                return;

            sink = std::make_shared<XJEditorLogSink>();
            sink->set_level(spdlog::level::trace);
            mSpdlogSink = sink;

        }
        // logger 的 sinks 列表应只在主线程启动/关闭阶段修改，
        // 不要在其他线程正在写日志时动态增删 sink。
        logger->sinks().push_back(std::move(sink));
    }
    void XJEditorLog::DetachFromSpdlog()
    {
        std::shared_ptr<spdlog::sinks::sink> sink;

        {
            std::lock_guard<std::mutex> lock(mMutex);
            sink = std::move(mSpdlogSink);
        }

        if (!sink)
            return;

        auto logger = spdlog::default_logger();
        if(!logger)
            return;

        auto& sinks = logger->sinks();
        sinks.erase(std::remove(sinks.begin(), sinks.end(), sink),
            sinks.end());
    }
}