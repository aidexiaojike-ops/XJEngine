// /不是真 SPIR-V reflection，而是先用 GLSL source 解析字段。

#ifndef XJ_SHADER_SCHEMA_VALIDATOR_H
#define XJ_SHADER_SCHEMA_VALIDATOR_H

#include "Render/Shader/XJShaderSchema.h"
#include "Render/Shader/XJShaderValidation.h"

#include <filesystem>

namespace XJ
{
    class XJShaderSchemaValidator
    {
        public:
            static XJShaderValidationResult ValidateFromSourceFiles(const XJShaderSchema& schema, const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath);//从源文件验证
    };
}

#endif
