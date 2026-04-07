#include "Edit/XJEventTesting.h"
#include "Edit/SpdlogDebug.h"

namespace XJ
{
    XJEventTesting::XJEventTesting()
    {
        mObserver =std::make_shared<XJ::XJEventObserver>();
        mObserver->OnEvent<XJMouseButtonReleaseEvent>([this](const XJMouseButtonReleaseEvent& event)
        {
            // TestMemberFunc(event);
            spdlog::info("Mouse Button Released at position: ({})",event.ToString());
        });
    }
    XJEventTesting::~XJEventTesting()
    {
        mObserver.reset();
    }
    void XJEventTesting::TestMemberFunc(const XJMouseButtonReleaseEvent &event)
    {
     
    }
}