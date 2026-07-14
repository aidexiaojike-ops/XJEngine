#include "Render/Shader/XJShaderSchemaValidator.h"
#include "Render/Shader/XJShaderReflectionUtils.h"
#include "Render/Shader/XJShaderSchemaBindingResolver.h"

#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace XJ
{

    namespace
    {
        struct XJSourceShaderInfo
        {
            std::unordered_map<std::string, std::unordered_set<std::string>> UboMembers;
            std::unordered_set<std::string> Samplers;
        };

        std::string ReadTextFile(const std::filesystem::path& path)//读取文本文件
        {
            std::ifstream in(path);
            if(!in.is_open())
                return {};
            
            std::stringstream ss;
            ss << in.rdbuf();
            return ss.str();
        }

        std::filesystem::path StripSpvExtension(const std::filesystem::path& path)
        {
            if (path.extension() != ".spv")//读取文件后缀
                return path;

            std::filesystem::path sourcePath = path;
            sourcePath.replace_extension("");
            return sourcePath;
        }

        std::filesystem::path ResolveShaderSourcePath(const std::filesystem::path& path)//解析着色器源路径
        {
            if (path.empty())
                return {};

            // .xjshader points at .spv, but C2 validation reads GLSL source.
            // Prefer Resource/Shader/Unlit.frag over Resource/Shader/Unlit.frag.spv.
            if (path.extension() == ".spv")
            {
                std::filesystem::path sourcePath = path;
                sourcePath.replace_extension("");
            
                if (std::filesystem::exists(sourcePath))
                    return sourcePath;
            }

            if (std::filesystem::exists(path))
                return path;

            return path;
        }

        std::string RemoveComments(const std::string& source)
        {
            std::string result = source;

            // Remove block comments first.
            result = std::regex_replace(
                result,
                std::regex(R"(/\*[\s\S]*?\*/)"),
                "");

            // Remove line comments.
            result = std::regex_replace(
                result,
                std::regex(R"(//[^\n\r]*)"),
                "");

            return result;
        }

        void ParseSamplers(const std::string& source, XJSourceShaderInfo& info)//解析采样器
        {
            // Handles:
            // layout(set = 2, binding = 0) uniform sampler2D textureA;
            // uniform sampler2D textureA;
            std::regex samplerRegex(R"(uniform\s+sampler\w+\s+([A-Za-z_][A-Za-z0-9_]*)\s*;)");//正则搜索迭代器，用于遍历一个字符串中所有匹配给定正则的子串。
            auto begin = std::sregex_iterator(source.begin(), source.end(), samplerRegex);//起止迭代器和正则对象构造，它会在构造时立刻执行第一次搜索，指向第一个匹配。
            auto end = std::sregex_iterator();

            for(auto it = begin; it != end; ++it)
                info.Samplers.insert((*it)[1].str());
        }

        void ParseUniformBuffers(const std::string& source, XJSourceShaderInfo& info)//解析统一缓冲区
        {
             // Handles:
            // layout(set = 1, binding = 0, std140) uniform MaterialUbo { ... } materialUbo;
            // uniform MaterialUbo { ... } materialUbo;
            std::regex uboRegex(R"(uniform\s+([A-Za-z_][A-Za-z0-9_]*)\s*\{([\s\S]*?)\}\s*([A-Za-z_][A-Za-z0-9_]*)?\s*;)",
                std::regex::ECMAScript);
            
            auto begin = std::sregex_iterator(source.begin(), source.end(), uboRegex);//起止迭代器和正则对象构造，它会在构造时立刻执行第一次搜索，指向第一个匹配。
            auto end = std::sregex_iterator();

            std::regex meberRegex(R"(\b(?:float|int|uint|bool|vec2|vec3|vec4|ivec2|ivec3|ivec4|uvec2|uvec3|uvec4|mat3|mat4|[A-Za-z_][A-Za-z0-9_]*)\s+([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[[^\]]*\])?\s*;)");
        
            for(auto it = begin; it != end; ++it)
            {
                const std::string uboNmae = (*it)[1].str();
                const std::string body = (*it)[2].str();

                auto memberBegin = std::sregex_iterator(body.begin(), body.end(), meberRegex);
                auto memberEnd = std::sregex_iterator();

                for(auto memberIt = memberBegin; memberIt != memberEnd; ++memberIt)
                    info.UboMembers[uboNmae].insert((*memberIt)[1].str());
            }
        }

        void MergeSourceInfo(const std::string& source, XJSourceShaderInfo& info)//合并源信息
        {
            const std::string cleanedSource = RemoveComments(source);
            ParseUniformBuffers(cleanedSource, info);
            ParseSamplers(cleanedSource, info);
        }

        void AddMessage(XJShaderValidationResult& result, XJShaderValidationSeverity severity, const std::string& parameterName, const std::string& message)//添加消息
        {
            XJShaderValidationMessage validationMessage;
            validationMessage.Severity = severity;
            validationMessage.ParameterName = parameterName;
            validationMessage.Message = message;
            result.Messages.push_back(validationMessage);
        }

    }

    XJShaderValidationResult XJShaderSchemaValidator::ValidateFromSourceFiles(const XJShaderSchema& schema, const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath)
    {
        XJShaderValidationResult result;
        XJSourceShaderInfo shaderInfo;
        //读取顶点数据和片元数据
        const std::filesystem::path resolvedVertexPath = ResolveShaderSourcePath(vertexPath);
        const std::filesystem::path resolvedFragmentPath = ResolveShaderSourcePath(fragmentPath);

        const std::string vertexSource = ReadTextFile(resolvedVertexPath);
        const std::string fragmentSource = ReadTextFile(resolvedFragmentPath);

        if(vertexSource.empty() && !vertexPath.empty())
        {
            AddMessage(result, XJShaderValidationSeverity::Warning,{},"Vertex shader source could not be read for schema validation: " + vertexPath.generic_string());

        }
        if (fragmentSource.empty() && !fragmentPath.empty())
        {
            AddMessage(result, XJShaderValidationSeverity::Warning, {}, "Fragment shader source could not be read for schema validation: " + fragmentPath.generic_string());
        }

        MergeSourceInfo(vertexSource, shaderInfo);
        MergeSourceInfo(fragmentSource, shaderInfo);

        for(const auto& parameter : schema.Parameters)//遍历 每个参数
        {
            if(IsTextureParameter(parameter.Type))
            {
                if(parameter.SamplerName.empty())
                {  
                    AddMessage(result, XJShaderValidationSeverity::Warning, parameter.Name, "Texture parameter has no sampler binding."); //警告：纹理参数缺少采样器绑定。
                    continue;
                    
                }

                if(shaderInfo.Samplers.find(parameter.SamplerName) == shaderInfo.Samplers.end())
                {
                    AddMessage(result, XJShaderValidationSeverity::Error, parameter.Name, "Sampler not found in shader source: " + parameter.SamplerName);
                }

                continue;
            }

            if(parameter.UboName.empty() || parameter.MemberName.empty())
            {
                AddMessage(result, XJShaderValidationSeverity::Warning, parameter.Name, "Non-texture parameter has no ubo/member binding.");
                continue;
            }

            auto uboIt = shaderInfo.UboMembers.find(parameter.UboName);
            if (uboIt == shaderInfo.UboMembers.end())
            {
                AddMessage(result, XJShaderValidationSeverity::Error, parameter.Name, "UBO not found in shader source: " + parameter.UboName);
                continue;
            
            }

            if (uboIt->second.find(parameter.MemberName) == uboIt->second.end())
            {
                AddMessage(result, XJShaderValidationSeverity::Error, parameter.Name, "UBO member not found in shader source: " + parameter.UboName + "." + parameter.MemberName);
            }
        }

        return result;
    }

    XJShaderValidationResult XJShaderSchemaValidator::ValidateFromReflection(const XJShaderSchema& schema, const XJShaderReflectionResult& reflection)
    {
        XJShaderValidationResult result;

        const XJShaderSchemaBindingResolveResult resolveResult =
            ResolveShaderSchemaBindings(schema, reflection);

        for (const auto& error : resolveResult.Errors)
            AddMessage(result, XJShaderValidationSeverity::Error, {}, error);

        for (const auto& warning : resolveResult.Warnings)
            AddMessage(result, XJShaderValidationSeverity::Warning, {}, warning);

        return result;
        
    }
}