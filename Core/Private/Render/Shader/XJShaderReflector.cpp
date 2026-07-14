#include "Render/Shader/XJShaderReflector.h"

#include <spirv_reflect.h>

#include <fstream>
#include <vector>

namespace XJ
{
    namespace
    {
   
        std::filesystem::path ResolveSpirvPath(const std::filesystem::path& path)//解析 SPIR-V 文件路径
        {
            if (path.empty())
                return {};

            if (std::filesystem::exists(path))//如果路径存在，直接返回
                return path;

            std::filesystem::path binPath = std::filesystem::path("bin") / path;//如果路径不存在，则尝试在 bin 目录下查找
            if (std::filesystem::exists(binPath))
                return binPath;

            return path;
        }

        bool ReadSpirvFile(const std::filesystem::path& path, std::vector<uint32_t>& outWords)//读取 SPIR-V 文件
        {
            std::ifstream in(path, std::ios::binary | std::ios::ate);
            if (!in.is_open())
                return false;

            const std::streamsize size = in.tellg();
            if(size <= 0 || (size % 4) != 0)
                return false;

            in.seekg(0, std::ios::beg);

            outWords.resize(static_cast<size_t>(size) / sizeof(uint32_t));
            if(!in.read(reinterpret_cast<char*>(outWords.data()), size))
            {
                outWords.clear();
                return false;
            }

            return true;
        }

        const char* SafeName(const char* value)//获取安全的名称
        {
            return value ? value : "";
        }

        bool IsUboDescriptor(SpvReflectDescriptorType type)//判断是否是 UBO 描述符
        {
            return type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }

        bool IsSamplerDescriptor(SpvReflectDescriptorType type)//判断是否是采样器描述符
        {
            return type == SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER ||
                   type == SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
                   type == SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        }

        std::string GetUboName(const SpvReflectDescriptorBinding& binding)//获取 UBO 名称
        {
            if(binding.block.type_description && binding.block.type_description -> type_name)
            {
                const std::string typeName = binding.block.type_description->type_name;
                if(!typeName.empty())
                    return typeName;
            }   

            return SafeName(binding.name);

        }

        uint32_t GetMemberSize(const SpvReflectBlockVariable& member)//获取成员大小
        {
            if(member.size != 0)
                return member.size;

            return member.padded_size;
        }


        XJShaderDescriptorType ConvertDescriptorType(SpvReflectDescriptorType type)//转换描述符类型
        {
            switch (type)
            {
                case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                    return XJShaderDescriptorType::UniformBuffer;

                case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                    return XJShaderDescriptorType::CombinedImageSampler;

                case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
                    return XJShaderDescriptorType::Sampler;

                case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
                    return XJShaderDescriptorType::SampledImage;

                default:
                    return XJShaderDescriptorType::Unknown;
            }
        }

        void AddUbo(const SpvReflectDescriptorBinding& binding, XJShaderStage stage, XJShaderReflectionResult& result)//添加 UBO
        {
            XJShaderReflectedUbo ubo;
            ubo.Name = GetUboName(binding);
            ubo.Set = binding.set;
            ubo.Binding = binding.binding;
            ubo.Size = binding.block.size;
            ubo.Stage = stage;
            ubo.DescriptorType = ConvertDescriptorType(binding.descriptor_type);

            for(uint32_t index = 0; index < binding.block.member_count; ++index)
            {
                const SpvReflectBlockVariable& reflectedMember = binding.block.members[index];

                XJShaderReflectedMember member;
                member.Name = SafeName(reflectedMember.name);
                member.Offset = reflectedMember.offset;
                member.Size = GetMemberSize(reflectedMember);

                if(!member.Name.empty())
                    ubo.Members.push_back(member);
            }

            if(!ubo.Name.empty())
                result.Ubos.push_back(ubo);

        }

        void AddSampler(const SpvReflectDescriptorBinding& binding, XJShaderStage stage, XJShaderReflectionResult& result)//添加采样器
        {
            XJShaderReflectedSampler sampler;
            sampler.Name = SafeName(binding.name);
            sampler.Set = binding.set;
            sampler.Binding = binding.binding;
            sampler.Stage = stage;
            sampler.DescriptorType = ConvertDescriptorType(binding.descriptor_type);

            if (!sampler.Name.empty())
                result.Samplers.push_back(sampler);
        }

        bool SameUboIdentity(const XJShaderReflectedUbo& a, const XJShaderReflectedUbo& b)//判断两个 UBO 是否相同
        {
            return a.Name == b.Name &&
                   a.Set == b.Set &&
                   a.Binding == b.Binding;
        }

        bool SameSamplerIdentity(const XJShaderReflectedSampler& a, const XJShaderReflectedSampler& b)//判断两个采样器是否相同
        {
            return a.Name == b.Name &&
                   a.Set == b.Set &&
                   a.Binding == b.Binding;
        }

        void MergeReflectionResult(XJShaderReflectionResult& dst, const XJShaderReflectionResult& src)//合并反射结果
        {
            dst.Errors.insert(dst.Errors.end(), src.Errors.begin(), src.Errors.end());//将源结果的错误信息插入到目标结果的错误信息中

            for (const auto& ubo : src.Ubos)
            {
                bool exists = false;
                for (const auto& existing : dst.Ubos)
                {
                    if (SameUboIdentity(existing, ubo))
                    {
                        exists = true;
                        break;
                    }
                }

                if (!exists)
                    dst.Ubos.push_back(ubo);//如果目标结果中不存在相同的 UBO，则将源结果的 UBO 添加到目标结果中
            }

            for (const auto& sampler : src.Samplers)
            {
                bool exists = false;
                for (const auto& existing : dst.Samplers)
                {
                    if (SameSamplerIdentity(existing, sampler))
                    {
                        exists = true;
                        break;
                    }
                }

                if (!exists)
                    dst.Samplers.push_back(sampler);
            }

            dst.Valid = dst.Valid || src.Valid;
            
        }


    }

    XJShaderReflectionResult XJShaderReflector::ReflectFromSpirvFile(const std::filesystem::path& path, XJShaderStage stage)
    {
        XJShaderReflectionResult result;

        const std::filesystem::path resolvedPath = ResolveSpirvPath(path);

        std::vector<uint32_t> spirWords;
        if(!ReadSpirvFile(resolvedPath, spirWords))//读取 SPIR-V 文件失败
        {
            result.Errors.push_back("Failed to read SPIR-V file: " + resolvedPath.generic_string());
            return result;
        }

        SpvReflectShaderModule module{};
        SpvReflectResult reflectResult = spvReflectCreateShaderModule(spirWords.size() * sizeof(uint32_t), spirWords.data(), &module);//创建 SPIR-V 反射模块

        if(reflectResult != SPV_REFLECT_RESULT_SUCCESS)//创建 SPIR-V 反射模块失败
        {
            result.Errors.push_back("Failed to create SPIR-V reflection module for file: " + resolvedPath.generic_string());
            return result;
        }

        uint32_t bindingCount = 0;
        reflectResult = spvReflectEnumerateDescriptorBindings(&module, &bindingCount, nullptr);//枚举描述符绑定数量
        if (reflectResult != SPV_REFLECT_RESULT_SUCCESS)
        {
            result.Errors.push_back("Failed to enumerate SPIR-V descriptor bindings: " + path.generic_string());
            spvReflectDestroyShaderModule(&module);
            return result;
        }

        std::vector<SpvReflectDescriptorBinding*> bindings(bindingCount);
        reflectResult = spvReflectEnumerateDescriptorBindings(&module, &bindingCount, bindings.data());//枚举描述符绑定
        if (reflectResult != SPV_REFLECT_RESULT_SUCCESS)
        {
            result.Errors.push_back("Failed to read SPIR-V descriptor bindings: " + path.generic_string());
            spvReflectDestroyShaderModule(&module);
            return result;
        }

        // Process each binding and categorize it as UBO or Sampler   处理每个绑定并将其分类为 UBO 或采样器
        for (const SpvReflectDescriptorBinding* binding : bindings)
        {
            if (!binding)
                continue;

            if (IsUboDescriptor(binding->descriptor_type))
            {
                AddUbo(*binding, stage, result);
            }
            else if (IsSamplerDescriptor(binding->descriptor_type))
            {
                AddSampler(*binding, stage, result);
            }
        }

        result.Valid = true;
        spvReflectDestroyShaderModule(&module);
        return result;
    }
    
    XJShaderReflectionResult XJShaderReflector::ReflectShaderProgram(const std::filesystem::path& vertexPath, const std::filesystem::path& fragmentPath)
    {
        XJShaderReflectionResult result;

        if (!vertexPath.empty())
            MergeReflectionResult(result, ReflectFromSpirvFile(vertexPath, XJShaderStage::Vertex));

        if (!fragmentPath.empty())
            MergeReflectionResult(result, ReflectFromSpirvFile(fragmentPath, XJShaderStage::Fragment));

        return result;
    }
}