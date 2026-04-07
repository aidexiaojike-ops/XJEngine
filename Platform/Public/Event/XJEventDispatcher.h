#ifndef XJ_EVENT_DISPATCHER_H
#define XJ_EVENT_DISPATCHER_H

#include "Event/XJEvent.h"
#include <functional>
#include <unordered_map>
#include <vector>
#include <algorithm>

//事件分发器类，用于将事件分发给相应的事件处理函数
namespace XJ
{
    class XJEventObserver;//事件观察者类，包含事件处理函数
    using FuncEventHandler = std::function<void(XJEvent& e)>;//事件处理函数类型，接受一个事件对象作为参数

    struct EventHandlerEntry//事件处理函数条目，包含事件观察者和事件处理函数
    {
        XJEventObserver *eventType;
        FuncEventHandler funchandler;
    };

    class XJEventDispatcher
    {
        public:
            XJEventDispatcher(const XJEventDispatcher&) = delete;
            XJEventDispatcher& operator=(const XJEventDispatcher&) = delete;
            ~XJEventDispatcher();

            static XJEventDispatcher* XJGetInstance(){return sInstance;};//获取事件分发器实例

            template<typename T>
            void AddObserverHandler(XJEventObserver* observer, std::function<void(const T&)> funchandler)
            {
               if(!observer || !funchandler) return;

               FuncEventHandler eventFunc = [funchandler](const XJEvent& e)
               {
                    const T& event = static_cast<const T&>(e);
                    return funchandler(event);
               };

               EventHandlerEntry handler
               {
                    .eventType = observer,
                    .funchandler = eventFunc
               };
               mObserverHandlerMap[T::XJGetStaticType()].push_back(handler);
            }

            void DestroyObserverHandler(XJEventObserver* observer)//销毁事件观察者的事件处理函数
            {
                for(auto &mapIt : mObserverHandlerMap)
                {
                    mapIt.second.erase(std::remove_if(mapIt.second.begin(), mapIt.second.end(),
                    [observer](const EventHandlerEntry& handler)                    
                    {
                        return (handler.eventType && handler.eventType == observer);
                    }), mapIt.second.end());
                }
            }


            void DispatchEvent(XJEvent& event);//分发事件，将事件传递给相应的事件处理函数
        private:
            XJEventDispatcher() = default;
            static XJEventDispatcher* sInstance;//事件分发器实例
            
            std::unordered_map<XJEventType, std::vector<EventHandlerEntry>> mObserverHandlerMap;//提供一个注册订阅者的函数
          
    };
    
}


#endif