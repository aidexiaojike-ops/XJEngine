#ifndef XJ_MOUSE_EVENT_H
#define XJ_MOUSE_EVENT_H

#include "Event/XJEventDispatcher.h"

namespace XJ
{
    class XJEventObserver
    {
        public:
            XJEventObserver() = default;
            ~XJEventObserver()
            {
                 XJEventDispatcher::XJGetInstance()->DestroyObserverHandler(this);
            };

            template<typename T>
            void OnEvent(const std::function<void(const T&)>& handler)
            {
                XJEventDispatcher::XJGetInstance()->AddObserverHandler<T>(this, handler);
            }

            
    };
}


#endif