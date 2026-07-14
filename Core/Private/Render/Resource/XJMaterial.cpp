#include "Render/Resource/XJMaterial.h"
#include "Render/Material/XJMaterialParameterBlockWriter.h"

#include <spdlog/spdlog.h>


namespace XJ
{


    bool XJMaterial::SetParameterValue(const std::string& parameterName, const XJMaterialParameterValue& value)
    {
        const XJMaterialParameterBinding* binding = mParameterLayout.FindParameterBinding(parameterName);
        if (!binding)
        {
            spdlog::warn("SetParameterValue failed: parameter binding not found: {}", parameterName);
            return false;
        }

        if(!WriteMaterialParameterValueToBlock(mParameterBlock, *binding, value))
        {
            spdlog::warn(
                "SetParameterValue failed: write block failed: {}, offset={}, size={}, blockSize={}",
                parameterName,
                binding->Offset,
                binding->Size,
                mParameterBlock.GetSize());
            return false;
        }
        
        MarkParameterDirty();
        return true;
    }

    bool XJMaterial::SetUboMemberValue(const std::string& uboName,const std::string& memberName,XJShaderParameterType type,const XJMaterialParameterValue& value)
    {
        const XJMaterialUboMemberBinding* memberBinding  = mParameterLayout.FindUboMemberBinding(uboName, memberName);
        if (!memberBinding)
        {
            spdlog::warn("SetUboMemberValue failed: UBO member not found: {}.{}", uboName, memberName);
            return false;
        }

        XJMaterialParameterBinding binding;
        binding.Type = type;
        binding.UboName = memberBinding->UboName;
        binding.MemberName = memberBinding->MemberName;
        binding.Set = memberBinding->Set;
        binding.Binding = memberBinding->Binding;
        binding.Offset = memberBinding->Offset;
        binding.Size = memberBinding->Size;

        if (!WriteMaterialParameterValueToBlock(mParameterBlock, binding, value))
        {
            spdlog::warn(
                "SetUboMemberValue failed: write block failed: {}.{}, offset={}, size={}, blockSize={}",
                uboName,
                memberName,
                binding.Offset,
                binding.Size,
                mParameterBlock.GetSize());
            return false;
        }

        spdlog::info(
            "SetUboMemberValue ok: {}.{}, offset={}, size={}, blockSize={}",
            uboName,
            memberName,
            binding.Offset,
            binding.Size,
            mParameterBlock.GetSize());

        if (std::holds_alternative<glm::vec4>(value))
        {
            const glm::vec4& v = std::get<glm::vec4>(value);
            spdlog::info(
                "Write UBO vec4: {}.{}, rgba=({}, {}, {}, {}), offset={}",
                uboName,
                memberName,
                v.r,
                v.g,
                v.b,
                v.a,
                binding.Offset);
        }
        
        MarkParameterDirty();
        return true;
    }

    bool XJMaterial::SetUboMemberBytes(const std::string& uboName, const std::string& memberName, const void* data, uint32_t size)
    {
        const XJMaterialUboMemberBinding* memberBinding  = mParameterLayout.FindUboMemberBinding(uboName, memberName);
        if (!memberBinding)
        {
            spdlog::warn("SetUboMemberBytes failed: UBO member not found: {}.{}", uboName, memberName);
            return false;
        }


        const uint32_t writeSize = (memberBinding->Size != 0 && memberBinding->Size < size) ? memberBinding->Size : size;
        if (!mParameterBlock.SetBytes(memberBinding->Offset, data, writeSize))
        {
            spdlog::warn(
                "SetUboMemberBytes failed: write block failed: {}.{}, offset={}, size={}, blockSize={}",
                uboName,
                memberName,
                memberBinding->Offset,
                writeSize,
                mParameterBlock.GetSize());
            return false;
        }
        
        MarkParameterDirty();
        return true;
    }


    bool XJMaterial::HasTexture(uint32_t id) const 
    {
        return mTextures.find(id) != mTextures.end();
    }

    const TextureView *XJMaterial::GetTextureView(uint32_t id) const
    {
        auto it = mTextures.find(id);
        if (it == mTextures.end())
            return nullptr;

        return &it->second;
    }

    void XJMaterial::SetTextureView(uint32_t id, const std::shared_ptr<XJTexture>& texture, const std::shared_ptr<XJSampler>& sampler)//更新纹理和采样方式
    {
        auto it = mTextures.find(id);
        if (it != mTextures.end())
        {
            it->second.texture = texture;
            it->second.sampler = sampler;
        }
        else
        {
            mTextures[id] = { texture, sampler };
        }

        MarkTextureDirty();//纹理变更 更新材质
    }
    void XJMaterial::UpdateTextureViewEnable(uint32_t id, bool enable) 
    {
         auto it = mTextures.find(id);
        if (it == mTextures.end())
            return;

        it->second.bEnable = enable;
        MarkParameterDirty();//更新参数
    }

    void XJMaterial::UpdateTextureViewUVTranslation(uint32_t id, const glm::vec2 &uvTranslation) 
    {
        auto it = mTextures.find(id);
        if (it == mTextures.end())
            return;

        it->second.uvTranslation = uvTranslation;
        MarkParameterDirty();
    }

    void XJMaterial::UpdateTextureViewUVRotation(uint32_t id, float uvRotation) 
    {
        auto it = mTextures.find(id);
        if (it == mTextures.end())
            return;

        it->second.uvRotation = uvRotation;
        MarkParameterDirty();
    }

    void XJMaterial::UpdateTextureViewUVScale(uint32_t id, const glm::vec2 &uvScale) 
    {
        auto it = mTextures.find(id);
        if (it == mTextures.end())
            return;

        it->second.uvScale = uvScale;
        MarkParameterDirty();
    }

    const std::string& XJMaterial::GetPrimaryUboName() const
    {
        return mParameterLayout.GetUboName();
    }   

    bool XJMaterial::SetPrimaryUboMemberValue(
        const std::string& memberName,
        XJShaderParameterType type,
        const XJMaterialParameterValue& value)
    {
        const std::string& uboName = GetPrimaryUboName();
        if (uboName.empty())
        {
            spdlog::warn("SetPrimaryUboMemberValue failed: primary UBO name is empty: {}", memberName);
            return false;
        }   

        return SetUboMemberValue(uboName, memberName, type, value);
    }   

    bool XJMaterial::SetPrimaryUboMemberBytes(
        const std::string& memberName,
        const void* data,
        uint32_t size)
    {
        const std::string& uboName = GetPrimaryUboName();
        if (uboName.empty())
        {
            spdlog::warn("SetPrimaryUboMemberBytes failed: primary UBO name is empty: {}", memberName);
            return false;
        }   

        return SetUboMemberBytes(uboName, memberName, data, size);
    }


    bool XJMaterial::HasSamplerTexture(const std::string& samplerName) const
    {
        return mSamplerTextures.find(samplerName) != mSamplerTextures.end();
    }
    
    const TextureView* XJMaterial::GetSamplerTextureView(const std::string& samplerName) const
    {
        auto it = mSamplerTextures.find(samplerName);
        if (it == mSamplerTextures.end())
            return nullptr;
    
        return &it->second;
    }
    
    void XJMaterial::SetSamplerTextureView(
        const std::string& samplerName,
        const std::shared_ptr<XJTexture>& texture,
        const std::shared_ptr<XJSampler>& sampler)
    {
        auto it = mSamplerTextures.find(samplerName);
        if (it != mSamplerTextures.end())
        {
            it->second.texture = texture;
            it->second.sampler = sampler;
        }
        else
        {
            mSamplerTextures[samplerName] = { texture, sampler };
        }
    
        MarkTextureDirty();
    }
    
    void XJMaterial::UpdateSamplerTextureViewEnable(const std::string& samplerName, bool enable)
    {
        auto it = mSamplerTextures.find(samplerName);
        if (it == mSamplerTextures.end())
            return;
    
        it->second.bEnable = enable;
        MarkParameterDirty();
    }

}