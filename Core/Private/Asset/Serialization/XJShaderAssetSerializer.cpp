#include "Asset/Serialization/XJShaderAssetSerializer.h"
#include "Asset/Serialization/XJShaderSchemaSerializer.h"
#include "Render/Shader/XJShaderSchemaValidator.h"
#include "Render/Shader/XJShaderReflector.h"
#include "Asset/XJAssetPathUtils.h" 
#include "Asset/Serialization/XJJsonIO.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace XJ
{

    std::shared_ptr<XJShaderAsset> XJShaderAssetSerializer::LoadFromFile(const std::filesystem::path& path)
    {
        std::ifstream in(path);
        if(!in.is_open())
            return nullptr;
        
        nlohmann::json root;
        try
        {
            root = nlohmann::json::parse(in);
        }
        catch(const nlohmann::json::exception&)
        {
            return nullptr;
        }
        
        auto shaderAsset = std::make_shared<XJShaderAsset>();
        shaderAsset->mType = XJAssetType::Shader;
        shaderAsset->mName = path.stem().string();
        shaderAsset->mPath = path;
        shaderAsset->Version = JsonReadUInt32Or(root, "version", 1u);

        shaderAsset->VertexPath = ResolveRelativePath(path, JsonReadStringOr(root, "vertex"));
        shaderAsset->FragmentPath = ResolveRelativePath(path, JsonReadStringOr(root, "fragment"));
        shaderAsset->SchemaPath = ResolveRelativePath(path, JsonReadStringOr(root, "schema"));

        if (!shaderAsset->SchemaPath.empty())
        {
            auto schema = XJShaderSchemaSerializer::LoadFromFile(shaderAsset->SchemaPath);
            if (schema)
                shaderAsset->Schema = *schema;
        }
        shaderAsset->Reflection = XJShaderReflector::ReflectShaderProgram(shaderAsset->VertexPath, shaderAsset->FragmentPath);     

       shaderAsset->Validation = XJShaderSchemaValidator::Validate(
                                shaderAsset->Schema,
                                shaderAsset->Reflection,
                                shaderAsset->VertexPath,
                                shaderAsset->FragmentPath);

        return shaderAsset;
    }

    bool XJShaderAssetSerializer::SaveToFile(const XJShaderAsset& shaderAsset, const std::filesystem::path& path)
    {
        nlohmann::json root;
        root["version"] = shaderAsset.Version;
        root["vertex"] = shaderAsset.VertexPath.generic_string();
        root["fragment"] = shaderAsset.FragmentPath.generic_string();
        root["schema"] = shaderAsset.SchemaPath.generic_string();

        return WriteJsonFileAtomic(path, root);
    }
}
