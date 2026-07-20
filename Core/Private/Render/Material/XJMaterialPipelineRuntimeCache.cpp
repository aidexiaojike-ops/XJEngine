#include "Render/Material/XJMaterialPipelineRuntimeCache.h"

#include "Asset/Serialization/XJShaderAssetSerializer.h"
#include "Render/Material/XJMaterialPipelineRuntimeBuilder.h"
#include "Render/Shader/XJShaderAsset.h"

namespace XJ
{
    bool XJMaterialPipelineRuntimeCache::Initialize(
        const XJMaterialPipelineRuntimeCacheContext& context)
    {
        Clear();

        if (!context.Device || !context.RenderPass)
        {
            spdlog::error("Material pipeline runtime cache initialize failed: invalid context.");
            return false;
        }

        if (context.DefaultShaderPath.empty())
        {
            spdlog::error("Material pipeline runtime cache initialize failed: default shader path is empty.");
            return false;
        }

        mContext = context;

        mDefaultRuntime = GetOrCreate(mContext.DefaultShaderPath);
        if (!mDefaultRuntime || !mDefaultRuntime->IsValid())
        {
            spdlog::error(
                "Material pipeline runtime cache initialize failed: default runtime invalid, shader='{}'.",
                mContext.DefaultShaderPath.generic_string());
            return false;
        }

        return true;
    }

    XJMaterialPipelineRuntime* XJMaterialPipelineRuntimeCache::GetDefault()
    {
        return mDefaultRuntime;
    }

    const XJMaterialPipelineRuntime* XJMaterialPipelineRuntimeCache::GetDefault() const
    {
        return mDefaultRuntime;
    }

    XJMaterialPipelineRuntime* XJMaterialPipelineRuntimeCache::GetOrCreate(
        const std::filesystem::path& shaderPath)
    {
        if (shaderPath.empty())
            return mDefaultRuntime;

        const std::string runtimeKey = MakeRuntimeKey(shaderPath);

        auto runtimeIt = mRuntimes.find(runtimeKey);
        if (runtimeIt != mRuntimes.end())
            return &runtimeIt->second;

        auto shaderAsset = XJShaderAssetSerializer::LoadFromFile(shaderPath);
        if (!shaderAsset)
        {
            spdlog::error(
                "Material pipeline runtime failed to load shader asset: {}",
                shaderPath.generic_string());
            return nullptr;
        }

        XJMaterialPipelineRuntime runtime{};

        XJMaterialPipelineRuntimeBuildContext buildContext{};
        buildContext.Device = mContext.Device;
        buildContext.RenderPass = mContext.RenderPass;
        buildContext.SampleCount = mContext.SampleCount;

        if (!XJMaterialPipelineRuntimeBuilder::Build(*shaderAsset, buildContext, runtime))
        {
            spdlog::error(
                "Material pipeline runtime failed to build: {}",
                shaderPath.generic_string());
            return nullptr;
        }

        auto [insertIt, inserted] = mRuntimes.emplace(runtimeKey, std::move(runtime));
        return &insertIt->second;
    }

    XJMaterialPipelineRuntime* XJMaterialPipelineRuntimeCache::Resolve(
        const std::filesystem::path& shaderPath)
    {
        if (!mDefaultRuntime || !mDefaultRuntime->IsValid())
            return nullptr;

        if (shaderPath.empty())
            return mDefaultRuntime;

        XJMaterialPipelineRuntime* runtime = GetOrCreate(shaderPath);
        if (!runtime || !runtime->IsValid())
        {
            const std::string key = MakeRuntimeKey(shaderPath);

            if (mWarnedFallbackShaderPaths.insert(key).second)
            {
                spdlog::warn(
                    "Resolve material pipeline runtime fallback: shader '{}' failed to create runtime.",
                    key);
            }

            return mDefaultRuntime;
        }

        return runtime;
    }

    void XJMaterialPipelineRuntimeCache::Clear()
    {
        mDefaultRuntime = nullptr;

        for (auto& entry : mRuntimes)
            entry.second.Clear();

        mRuntimes.clear();
        mWarnedFallbackShaderPaths.clear();
        mContext = XJMaterialPipelineRuntimeCacheContext{};
    }

    std::string XJMaterialPipelineRuntimeCache::MakeRuntimeKey(
        const std::filesystem::path& shaderPath) const
    {
        return shaderPath.lexically_normal().generic_string();
    }
}