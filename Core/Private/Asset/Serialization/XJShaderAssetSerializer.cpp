#include "Asset/Serialization/XJShaderAssetSerializer.h"
#include "Asset/Serialization/XJShaderSchemaSerializer.h"
#include "Render/Shader/XJShaderSchemaValidator.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace XJ
{
    namespace
    {
       
        std::filesystem::path ResolveRelativePath(const std::filesystem::path& ownerFile, const std::filesystem::path& referencedPath)
        {
            if (referencedPath.empty())
                return referencedPath;
        
            if (referencedPath.is_absolute())
                return referencedPath.lexically_normal();
        
            if (std::filesystem::exists(referencedPath))
                return referencedPath.lexically_normal();
        
            std::filesystem::path resolved = ownerFile.parent_path() / referencedPath;
            return resolved.lexically_normal();
        }
    }

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
        shaderAsset->Version = root.value("version", 1u);

        shaderAsset->VertexPath = ResolveRelativePath(path, root.value("vertex", std::string{}));
        shaderAsset->FragmentPath = ResolveRelativePath(path, root.value("fragment", std::string{}));
        shaderAsset->SchemaPath = ResolveRelativePath(path, root.value("schema", std::string{}));

        if (!shaderAsset->SchemaPath.empty())
        {
            auto schema = XJShaderSchemaSerializer::LoadFromFile(shaderAsset->SchemaPath);
            if (schema)
                shaderAsset->Schema = *schema;
        }
        shaderAsset->Validation = XJShaderSchemaValidator::ValidateFromSourceFiles(shaderAsset->Schema, shaderAsset->VertexPath, shaderAsset->FragmentPath);

        return shaderAsset;
    }

    bool XJShaderAssetSerializer::SaveToFile(const XJShaderAsset& shaderAsset, const std::filesystem::path& path)
    {
        if (path.has_parent_path())
            std::filesystem::create_directories(path.parent_path());

        nlohmann::json root;
        root["version"] = shaderAsset.Version;
        root["vertex"] = shaderAsset.VertexPath.generic_string();
        root["fragment"] = shaderAsset.FragmentPath.generic_string();
        root["schema"] = shaderAsset.SchemaPath.generic_string();

        std::ofstream out(path);
        if (!out.is_open())
            return false;

        out << root.dump(2);
        return out.good();
    }
}