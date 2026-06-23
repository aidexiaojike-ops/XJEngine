#include "Asset/Importer/XJMaterialImporter.h"

#include "Asset/Serialization/XJMaterialAssetSerializer.h"



namespace XJ
{
    /*
    std::shared_ptr<XJMaterialAsset> XJMaterialImporter::ImportMaterial(const std::string& path)
    {
        std::ifstream in(path);//输入文件流类（Input File Stream）。它专门用来从文件读取数据，就像 std::cin 从键盘读取一样，只不过数据源是磁盘上的文件。
        if(!in.is_open())
        {
            spdlog::error("Failed to open material asset: {}", path);
            return nullptr;
        }

        nlohmann::json j; // 创建一个空的 JSON 对象
        try
        {
            j = nlohmann::json::parse(in);//从已打开的文件流 in 中读取并解析 JSON 数据。
        }
        catch(const std::exception& e)
        {
            spdlog::error("Failed to parse material asset {}: {}", path, e.what());
            return nullptr;
        }
        //定义材质
        auto asset = std::make_shared<XJMaterialAsset>();
        asset->mType = XJAssetType::Material;
        asset->mPath = path;
        //json赋值给材质
        if (j.contains("baseColor") && j["baseColor"].is_array() && j["baseColor"].size() >= 4)
        {
            asset->BaseColorFactor = glm::vec4(
                j["baseColor"][0].get<float>(),
                j["baseColor"][1].get<float>(),
                j["baseColor"][2].get<float>(),
                j["baseColor"][3].get<float>());
        }

        asset->MetallicFactor = j.value("metallic", asset->MetallicFactor);
        asset->RoughnessFactor = j.value("roughness", asset->RoughnessFactor);
        asset->AlbedoTexture = j.value("albedoTexture", static_cast<XJAssetHandle>(0));


        return asset;
    }*/ //之前的文件

    std::shared_ptr<XJMaterialAsset> XJMaterialImporter::ImportMaterial(const std::string& path)
    {
        return XJMaterialAssetSerializer::LoadFromFile(path);
    }
}
