#ifndef XJ_BASEMATERIALCOMPONENT_H
#define XJ_BASEMATERIALCOMPONENT_H

#include "XJMaterialComponent.h"

namespace XJ
{
    enum BaseMaterialColor
    {
        COLOR_TYPE_NORMAL = 0, 
        COLOR_TYPE_TEXCOOR = 1
    };

    struct XJBaseMaterial : public XJMaterial
    {
        BaseMaterialColor colorType;
    };

    class XJBaseMaterialComponent : public XJMaterialComponent<XJBaseMaterial>
    {
       
    };
    
    
}

#endif