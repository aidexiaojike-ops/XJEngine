#include "Asset/Metadata/XJAssetMetadataSerializer.h"

#include "Asset/Metadata/XJAssetMetadataPath.h"
#include "Asset/Serialization/XJJsonIO.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

namespace XJ
{
    namespace
    {
        const char* AssetTypeToString(XJAssetType type)
        {
            switch (type)
            {
                case XJAssetType::Mesh:
                    return "Mesh";
                case XJAssetType::Texture:
                    return "Texture";
                case XJAssetType::Material:
                    return "Material";
                case XJAssetType::Scene:
                    return "Scene";
                case XJAssetType::Shader:
                    return "Shader";
                default:
                    return "None";
            }
        }

        std::optional<XJAssetType> AssetTypeFromString(const std::string& value)
        {
            if (value == "Mesh")
                return XJAssetType::Mesh;
            if (value == "Texture")
                return XJAssetType::Texture;
            if (value == "Material")
                return XJAssetType::Material;
            if (value == "Scene")
                return XJAssetType::Scene;
            if (value == "Shader")
                return XJAssetType::Shader;

            return std::nullopt;
        }

        XJAssetMetadataLoadResult MakeError(XJAssetMetadataLoadStatus status, std::string message)
        {
            XJAssetMetadataLoadResult result;
            result.Status = status;
            result.Error = std::move(message);
            return result;
        }
    }

    XJAssetMetadataLoadResult XJAssetMetadataSerializer::Load(const std::filesystem::path& assetPath)
    {
        if(assetPath.empty())
            return MakeError(XJAssetMetadataLoadStatus::InvalidData, "Asset path is empty.");
        
        const std::filesystem::path metadataPath = BuildAssetMetadataPath(assetPath);

        std::error_code ec;
        const bool exists = std::filesystem::exists(metadataPath, ec);

        if(ec)
            return MakeError(XJAssetMetadataLoadStatus::IoError, "Failed to inspect metadata file: " + ec.message());
        
        if (!exists)
            return MakeError(XJAssetMetadataLoadStatus::NotFound, "Metadata file does not exist.");

        std::ifstream input(metadataPath);
        if (!input.is_open())
            return MakeError(XJAssetMetadataLoadStatus::IoError, "Failed to open metadata file.");
        

        nlohmann::json root;
        try
        {
            root = nlohmann::json::parse(input);
        }
        catch (const nlohmann::json::exception& e)
        {
            return MakeError(XJAssetMetadataLoadStatus::InvalidJson, e.what());
        }

        if (!root.is_object())
            return MakeError(XJAssetMetadataLoadStatus::InvalidData, "Metadata root must be an object.");

        XJAssetMetadata metadata;
        metadata.Version = JsonReadUInt32Or(root, "version", 0);
        
        if (metadata.Version !=
            XJAssetMetadata::CurrentVersion)
        {
            return MakeError(
                XJAssetMetadataLoadStatus::
                    UnsupportedVersion,
                "Unsupported metadata version.");
        }

        if (!root.contains("handle") || !JsonReadUInt64(root["handle"], metadata.Handle))
            return MakeError(XJAssetMetadataLoadStatus::InvalidData, "Metadata handle is invalid.");
        

        const std::string typeName = JsonReadStringOr(root, "type");
        const auto type = AssetTypeFromString(typeName);

        if (!type)
            return MakeError(XJAssetMetadataLoadStatus::InvalidData, "Metadata asset type is invalid.");

        metadata.Type = *type;
        metadata.Importer = JsonReadStringOr(root, "importer");

        metadata.ImporterVersion = JsonReadUInt32Or(root, "importerVersion", 1);

        if (!metadata.IsValid())
            return MakeError(XJAssetMetadataLoadStatus::InvalidData, "Metadata fields are invalid.");

        XJAssetMetadataLoadResult result;
        result.Status = XJAssetMetadataLoadStatus::Success;
        result.Metadata = metadata;
        return result;

    }

    bool XJAssetMetadataSerializer::Save(const std::filesystem::path& assetPath, const XJAssetMetadata& metadata, std::string* outError)
    {
        auto fail = [outError]( const std::string& message)
        {
            if (outError)
                *outError = message;

            return false;
        };

        if (assetPath.empty())
            return fail("Asset path is empty.");

        if (!metadata.IsValid())
            return fail("Metadata is invalid.");

        nlohmann::json root;
        root["version"] = metadata.Version;

        // 使用字符串避免其他 JSON 工具读取 uint64 时丢失精度。
        root["handle"] =
            std::to_string(metadata.Handle);

        root["type"] =  AssetTypeToString(metadata.Type);

        if (!metadata.Importer.empty())
        {
            root["importer"] = metadata.Importer;

            root["importerVersion"] = metadata.ImporterVersion;
        }

        const std::filesystem::path metadataPath =
            BuildAssetMetadataPath(assetPath);

        try
        {
            if (!WriteJsonFileAtomic(metadataPath, root))
            {
                return fail("Atomic metadata write failed.");
            }
        }
        catch (const std::exception& e)
        {
            return fail(e.what());
        }

        return true;
    }
}