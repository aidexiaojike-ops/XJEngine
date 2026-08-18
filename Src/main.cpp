#include "XJEntryPoint.h"
#include "XJEditorApplication.h"

XJ::XJApplication* CreateApplicationEntryPoint()
{
    return new XJ::XJEditorApplication();
}
