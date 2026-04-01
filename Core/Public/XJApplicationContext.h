#ifndef XJ_APPLICATION_CONTEXT_H
#define XJ_APPLICATION_CONTEXT_H


namespace XJ
{
    class XJApplication;
    class XJScene;
    class XJRenderContext;

    struct XJAppContext
    {
        XJApplication *app = nullptr;
        XJScene *scene = nullptr;
        XJRenderContext *renderContext = nullptr;
    };
}


#endif