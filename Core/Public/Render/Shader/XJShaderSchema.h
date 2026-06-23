#ifndef XJ_SHADER_SCHEMA_H
#define XJ_SHADER_SCHEMA_H

#include "Render/Shader/XJShaderParameter.h"

#include <optional>
#include <string>
#include <vector>


namespace XJ
{
    struct XJShaderSchema
    {
        uint32_t Version = 1;//版本
        std::vector<XJParameterDef> Parameters;

        const XJParameterDef* FindParameter(const std::string& name) const
        {
            for (const auto& parameter : Parameters)
            {
                if(parameter.Name == name)
                    return &parameter;
            }
            return nullptr;
        }

        XJParameterDef* FindParameter(const std::string& name)
        {
            for (auto& parameter : Parameters)
            {
                if (parameter.Name == name)
                    return &parameter;
            }

            return nullptr;
        }
    };
}

#endif