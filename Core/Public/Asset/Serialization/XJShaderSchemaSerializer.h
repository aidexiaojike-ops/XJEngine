#ifndef XJ_SHADER_SCHEMA_SERIALIZER_H
#define XJ_SHADER_SCHEMA_SERIALIZER_H

#include "Render/Shader/XJShaderSchema.h"

#include <filesystem>
#include <memory>

namespace XJ
{
    class XJShaderSchemaSerializer//读取和存储shader文件
    {
        public:
            static std::shared_ptr<XJShaderSchema> LoadFromFile(const std::filesystem::path& path);//加载shader文件
            static bool SaveToFile(const XJShaderSchema& schema, const std::filesystem::path& path);//保持shader文件
    };
}

#endif