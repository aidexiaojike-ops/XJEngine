#include "Event/XJEventDispatcher.h"
#include "spdlog/stopwatch.h"
#include "Edit/SpdlogDebug.h"


namespace XJ
{
    // XJEventDispatcher* XJEventDispatcher::sInstance{}  = new XJEventDispatcher();
    XJEventDispatcher* XJEventDispatcher::sInstance = new XJEventDispatcher();

    XJEventDispatcher::~XJEventDispatcher()
    {
        mObserverHandlerMap.clear();
    }

    void XJEventDispatcher::DispatchEvent(XJEvent& event)
    {
        auto it = mObserverHandlerMap.find(event.XJGetEventType());
        if(it == mObserverHandlerMap.end())//是否有订阅者
        {
            return;
        }
        auto& observers = it->second;//获取订阅者列表
        if(observers.empty())//如果没有订阅者
        {
            return;
        }
        spdlog::stopwatch stopwatch;//创建一个计时器
        stopwatch.reset();//开始计时
        for(const auto& observer : observers)
        {
            if(observer.funchandler)
            {
                observer.funchandler(event);//调用事件处理函数
            }
        }
        spdlog::trace("Event {} dispatched in {} ms", event.XJGetEventTypeName(), stopwatch.elapsed().count() * 1000);//输出事件分发时间
    }
}