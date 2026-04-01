#ifndef XJ_MESHCOMPONENT_H
#define XJ_MESHCOMPONENT_H


#include "ECS/XJComponent.h"
#include "Render/XJMesh.h"

namespace XJ
{
    struct  XJMeshComponent : public XJComponent
    {
        /* data */
        XJMesh *mMesh = nullptr;
    };
    
}

#endif