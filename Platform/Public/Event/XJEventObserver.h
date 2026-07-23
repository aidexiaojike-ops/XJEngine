#ifndef XJ_EVENT_OBSERVER_H
#define XJ_EVENT_OBSERVER_H

#include "Event/XJEventDispatcher.h"

namespace XJ
{
    class XJEventObserver
    {
        public:
            XJEventObserver() = default;
            XJEventObserver(const XJEventObserver&) = delete;
            XJEventObserver& operator=(const XJEventObserver&) = delete;
            
            ~XJEventObserver()
            {
                if (auto* dispatcher = XJEventDispatcher::XJGetInstance())
                {
                    dispatcher->DestroyObserverHandler(this);
                }
            };

            template<typename T>
            void OnEvent(const std::function<void(const T&)>& handler)
            {
                if (auto* dispatcher = XJEventDispatcher::XJGetInstance())
                {
                    dispatcher->AddObserverHandler<T>(this, handler);
                }
            }

            
    };
}


#endif