#include "Event/XJEventDispatcher.h"
#include "spdlog/stopwatch.h"
#include "Edit/SpdlogDebug.h"


namespace XJ
{

    XJEventDispatcher::~XJEventDispatcher()
    {
        std::lock_guard<std::mutex> lock(mObserverHandlerMutex);
        mObserverHandlerMap.clear();
    }

    void XJEventDispatcher::DispatchEvent(XJEvent& event)
    {
        std::vector<EventHandlerEntry> observersSnapshot;

        {
            std::lock_guard<std::mutex> lock(mObserverHandlerMutex);

            auto it = mObserverHandlerMap.find(event.XJGetEventType());
            if(it == mObserverHandlerMap.end())//是否有订阅者
            {
                return;
            }

            observersSnapshot = it->second;//获取订阅者列表
        }

        if(observersSnapshot.empty())//如果没有订阅者
        {
            return;
        }
        spdlog::stopwatch stopwatch;//创建一个计时器
        stopwatch.reset();//开始计时
        for(const auto& observer : observersSnapshot)
        {
            if(observer.funchandler)
            {
                observer.funchandler(event);//调用事件处理函数
            }
        }
        spdlog::trace("Event {} dispatched in {} ms", event.XJGetEventTypeName(), stopwatch.elapsed().count() * 1000);//输出事件分发时间
    }

    XJEventDispatcher* XJEventDispatcher::XJGetInstance()
    {
        static XJEventDispatcher instance;
        return &instance;
    }

    void XJEventDispatcher::DestroyObserverHandler(XJEventObserver* observer)
    {
        if (!observer)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(mObserverHandlerMutex);

        for(auto &mapIt : mObserverHandlerMap)
        {
            auto& handlers = mapIt.second;
            handlers.erase(
                std::remove_if(
                    handlers.begin(),
                    handlers.end(),
                    [observer](const EventHandlerEntry& handler)
                    {
                        return handler.eventType == observer;
                    }),
                handlers.end());
        }
    }

}