#include "Event/XJEventDispatcher.h"


namespace XJ
{
    XJEventDispatcher::XJEventDispatcher(const XJEventDispatcher&)
    {

    }
   
    XJEventDispatcher::~XJEventDispatcher()
    {
        mObserverHandlerMap.clear();
    }

    void XJEventDispatcher::DispatchEvent(XJEvent& event)
    {

    }
}