#ifndef XJ_PERFORMANCE_DATA_H
#define XJ_PERFORMANCE_DATA_H

#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <spdlog/spdlog.h>
// /统计数据和脚本计时器
namespace XJ
{
    struct XJCompletedFrameStatistics//已完成帧统计数据
    {
        uint64_t FrameId = 0;
        double WallMilliseconds = 0.0;
    };

    struct XJScriptStatistics//脚本统计数据
    {
        uint64_t FrameId = 0;
        double CpuMilliseconds = 0.0;
        uint32_t ActiveInstances = 0;
    };

    struct XJRenderStatistics//渲染统计数据
    {
        uint64_t FrameId = 0;
        bool CommandsRecorded = false;

        uint64_t DrawCalls = 0;
        uint64_t MaterialParameterUploads = 0;
        uint64_t MaterialResourceUploads = 0;
    };

     // 每个 ScriptSystem 自己持有，不是全局对象。
    class XJScriptStatisticsRecorder
    {
        public:
            void BeginFrame(uint64_t frameId)
            {
                if(mDepth != 0)
                {
                    spdlog::error("Cannot reset script statistics during execution.");
                    throw std::logic_error("Cannot reset script statistics during execution");
                }

                mCurrent = {};
                mCurrent.FrameId = frameId;
            }

            XJScriptStatistics GetSnapshot(uint32_t activeInstances) const
            {
                auto result = mCurrent;
                result.ActiveInstances = activeInstances;
                return result;
            }
            class Scope//范围
            {
                public:
                    explicit Scope(XJScriptStatisticsRecorder& recorder)
                    : mRecorder(recorder),
                    mEnabled(recorder.mCurrent.FrameId != 0)
                    {
                        if(!mEnabled)
                            return;
                        
                        mOutermost = mRecorder.mDepth++ == 0;

                        if(mOutermost)
                            mStart = Clock::now();
                    }

                    ~Scope()
                    {
                        if (!mEnabled)
                            return;

                        --mRecorder.mDepth;

                        // Synchronize / Tick / 生命周期的嵌套不重复计算。
                        if (mOutermost)
                        {
                            mRecorder.mCurrent.CpuMilliseconds +=
                                std::chrono::duration<double, std::milli>(
                                    Clock::now() - mStart).count();
                        }
                    }

                    Scope(const Scope&) = delete;
                    Scope& operator=(const Scope&) = delete;
                
                private:
                    using Clock = std::chrono::steady_clock;

                    XJScriptStatisticsRecorder& mRecorder;
                    Clock::time_point mStart{};
                    bool mEnabled = false;
                    bool mOutermost = false;
            };
        private:
            XJScriptStatistics mCurrent;
            uint32_t mDepth = 0;
    };
    
}

#endif
