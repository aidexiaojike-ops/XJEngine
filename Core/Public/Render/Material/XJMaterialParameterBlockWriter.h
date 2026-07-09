//只负责 XJMaterialParameterValue -> XJMaterialParameterBlock
#ifndef XJ_MATERIAL_PARAMETER_BLOCK_WRITER_H
#define XJ_MATERIAL_PARAMETER_BLOCK_WRITER_H

#include "Render/Material/XJMaterialParameterBlock.h"
#include "Render/Material/XJMaterialParameterLayout.h"

namespace XJ
{
    bool WriteMaterialParameterValueToBlock(XJMaterialParameterBlock& block, const XJMaterialParameterBinding& binding, const XJMaterialParameterValue& value);
   
}

#endif