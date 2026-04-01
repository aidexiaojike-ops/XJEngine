#ifndef XJ_BASEMATERIALCOMPONENT_H
#define XJ_BASEMATERIALCOMPONENT_H

#include "ECS/XJComponent.h"

namespace XJ
{
    enum BaseMaterialCol
    {
        COLOR_TYPE_NORMAL,
        COLOR_TYPE_TEXCOORD
    };

    class XJBaseMaterialComponent : public XJComponent
    {
        BaseMaterialCol colorType;
    };
    
    
}

#endif