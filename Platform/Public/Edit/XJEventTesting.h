#ifndef XJ_EVENT_TESTING_H
#define XJ_EVENT_TESTING_H

#include "Event/XJEventObserver.h"
#include "Event/XJMouseEvent.h"
#include <memory>

namespace XJ
{
    class XJEventTesting
    {
        public:
            XJEventTesting();
            ~XJEventTesting() = default;
        private:
            std::shared_ptr<XJ::XJEventObserver> mObserver;
    };

}

#endif
