#include "UI/XJEditorLog.h"
#include <chrono>
#include <sstream>
#include <iomanip>

namespace XJ
{
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

}