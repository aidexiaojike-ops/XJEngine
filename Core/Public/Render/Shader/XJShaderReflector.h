#ifndef XJ_SHADER_REFLECTOR_H
#define XJ_SHADER_REFLECTOR_H

#include "Render/Shader/XJShaderReflection.h"

#include <filesystem>

namespace  XJ
{
    class XJShaderReflector
    {
        private:
            /* data */
        public:
            static XJShaderReflectionResult ReflectShaderProgram(const std::filesystem::path& vertexPath,const std::filesystem::path& fragmentPath);//反射着色器程序
            static XJShaderReflectionResult ReflectFromSpirvFile(const std::filesystem::path& path, XJShaderStage stage);//反射 SPIR-V 文件
    };
 
    
}


#endif