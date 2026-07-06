#ifndef XJ_SHADER_ASSET_H
#define XJ_SHADER_ASSET_H

#include "Asset/XJAsset.h"
#include "Render/Shader/XJShaderSchema.h"
#include "Render/Shader/XJShaderValidation.h"
#include "Render/Shader/XJShaderReflection.h"


#include <filesystem>
#include <string>

namespace XJ
{
    class XJShaderAsset : public XJAsset
    {

        public:
            uint32_t Version = 1;//版本
            //路径
            std::filesystem::path VertexPath;
            std::filesystem::path FragmentPath;
            std::filesystem::path SchemaPath;
        
            XJShaderSchema Schema;
            XJShaderValidationResult Validation;
            XJShaderReflectionResult Reflection;
    };



}

#endif