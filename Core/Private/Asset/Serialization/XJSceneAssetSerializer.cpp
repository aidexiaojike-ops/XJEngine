#include "Asset/Serialization/XJSceneAssetSerializer.h"

#include "ECS/Component/Material/XJUnlitMaterialComponent.h"
#include "ECS/Component/XJCameraComponent.h"
#include "ECS/Component/XJSceneAssetComponents.h"
#include "ECS/Component/XJTransformComponent.h"
#include "ECS/XJEntity.h"
#include "ECS/XJScene.h"

#include <fstream>
#include <nlohmann/json.hpp>
#include <string>

namespace XJ
{
    static std::string UUIDToString(XJUUID uuid)
    {
        return std::to_string(static_cast<uint64_t>(uuid));
    }

    static uint64_t ReadUInt64(const nlohmann::json& j, const char* key, uint64_t fallback = 0)
    {
        if (!j.contains(key))
            return fallback;

        const auto& value = j[key];
        if (value.is_number_unsigned())
            return value.get<uint64_t>();

        if (value.is_number_integer())
            return static_cast<uint64_t>(value.get<int64_t>());

        if (value.is_string())
        {
            const std::string text = value.get<std::string>();
            if (text.empty())
                return fallback;

            int base = 10;
            std::string number = text;

            if (number.rfind("0x", 0) == 0 || number.rfind("0X", 0) == 0)
            {
                base = 16;
                number = number.substr(2);
            }
            else if (number.find_first_of("abcdefABCDEF") != std::string::npos)
            {
                base = 16;
            }

            try
            {
                return std::stoull(number, nullptr, base);
            }
            catch (...)
            {
                return fallback;
            }
        }

        return fallback;
    }

    static XJUUID ReadUUID(const nlohmann::json& j, const char* key)
    {
        return XJUUID(ReadUInt64(j, key, 0));
    }

    static nlohmann::json SerializeVec3(const glm::vec3& v)
    {
        return {v.x, v.y, v.z};
    }

    static glm::vec3 DeserializeVec3(const nlohmann::json& j, const glm::vec3& fallback)
    {
        if (!j.is_array() || j.size() < 3)
            return fallback;

        return glm::vec3(j[0].get<float>(), j[1].get<float>(), j[2].get<float>());
    }

    static nlohmann::json SerializeTransform(const XJSceneTransformData& t)
    {
        return {
            {"uuid", UUIDToString(t.UUID)},
            {"type", "TransformComponent"},
            {"pos", SerializeVec3(t.Position)},
            {"rot", SerializeVec3(t.Rotation)},
            {"scale", SerializeVec3(t.Scale)}
        };
    }

    static nlohmann::json SerializeMeshRenderer(const XJSceneMeshRendererData& mr)
    {
        auto materials = nlohmann::json::array();
        for (const auto& mat : mr.Materials)
            materials.push_back(mat.ToUri());

        return {
            {"uuid", UUIDToString(mr.UUID)},
            {"type", "MeshRendererComponent"},
            {"mesh", mr.Mesh.ToUri()},
            {"materials", materials}
        };
    }

    static nlohmann::json SerializeCamera(const XJSceneCameraData& c)
    {
        return {
            {"uuid", UUIDToString(c.UUID)},
            {"type", "CameraComponent"},
            {"enabled", c.Enabled},
            {"fov", c.Fov},
            {"near", c.NearClip},
            {"far", c.FarClip},
            {"primary", c.Primary}
        };
    }

    static nlohmann::json SerializeLight(const XJSceneLightData& l)
    {
        return {
            {"uuid", UUIDToString(l.UUID)},
            {"type", "LightComponent"},
            {"enabled", l.Enabled},
            {"lightType", l.Type},
            {"color", SerializeVec3(l.Color)},
            {"intensity", l.Intensity}
        };
    }

    static nlohmann::json SerializeEntity(const XJSceneEntityData& e)
    {
        nlohmann::json j;
        j["uuid"] = UUIDToString(e.UUID);
        j["type"] = e.Type.empty() ? "Entity" : e.Type;
        j["name"] = e.Name;

        if (e.Parent == 0)
            j["parent"] = nullptr;
        else
            j["parent"] = UUIDToString(e.Parent);

        auto children = nlohmann::json::array();
        for (auto child : e.Children)
            children.push_back(UUIDToString(child));
        j["children"] = children;

        j["components"] = nlohmann::json::object();

        if (e.HasTransform)
            j["components"]["transform"] = SerializeTransform(e.Transform);

        if (e.HasMeshRenderer)
            j["components"]["meshRenderer"] = SerializeMeshRenderer(e.MeshRenderer);

        if (e.HasCamera)
            j["components"]["camera"] = SerializeCamera(e.Camera);

        if (e.HasLight)
            j["components"]["light"] = SerializeLight(e.Light);

        return j;
    }

    static XJSceneTransformData DeserializeTransform(const nlohmann::json& j)
    {
        XJSceneTransformData t;
        t.UUID = ReadUUID(j, "uuid");
        t.Position = DeserializeVec3(j.value("pos", nlohmann::json::array()), t.Position);
        t.Rotation = DeserializeVec3(j.value("rot", nlohmann::json::array()), t.Rotation);
        t.Scale = DeserializeVec3(j.value("scale", nlohmann::json::array()), t.Scale);
        return t;
    }

    static XJSceneMeshRendererData DeserializeMeshRenderer(const nlohmann::json& j)
    {
        XJSceneMeshRendererData mr;
        mr.UUID = ReadUUID(j, "uuid");
        mr.Mesh = XJAssetRef::FromUri(j.value("mesh", std::string{}), XJAssetType::Mesh);

        if (j.contains("materials") && j["materials"].is_array())
        {
            for (const auto& mat : j["materials"])
                mr.Materials.push_back(XJAssetRef::FromUri(mat.get<std::string>(), XJAssetType::Material));
        }

        return mr;
    }

    static XJSceneCameraData DeserializeCamera(const nlohmann::json& j)
    {
        XJSceneCameraData c;
        c.UUID = ReadUUID(j, "uuid");
        c.Enabled = j.value("enabled", true);
        c.Fov = j.value("fov", c.Fov);
        c.NearClip = j.value("near", c.NearClip);
        c.FarClip = j.value("far", c.FarClip);
        c.Primary = j.value("primary", false);
        return c;
    }

    static XJSceneLightData DeserializeLight(const nlohmann::json& j)
    {
        XJSceneLightData l;
        l.UUID = ReadUUID(j, "uuid");
        l.Enabled = j.value("enabled", true);
        l.Type = j.value("lightType", j.value("type", l.Type));
        l.Color = DeserializeVec3(j.value("color", nlohmann::json::array()), l.Color);
        l.Intensity = j.value("intensity", l.Intensity);
        return l;
    }

    static XJSceneEntityData DeserializeEntity(const nlohmann::json& j)
    {
        XJSceneEntityData e;
        e.UUID = ReadUUID(j, "uuid");
        e.Type = j.value("type", std::string{"Entity"});
        e.Name = j.value("name", std::string{});

        if (j.contains("parent") && !j["parent"].is_null())
            e.Parent = XJUUID(ReadUInt64(j, "parent", 0));

        if (j.contains("children") && j["children"].is_array())
        {
            for (const auto& child : j["children"])
            {
                if (child.is_string())
                    e.Children.push_back(XJUUID(ReadUInt64(nlohmann::json{{"value", child}}, "value", 0)));
                else
                    e.Children.push_back(XJUUID(child.get<uint64_t>()));
            }
        }

        if (!j.contains("components") || !j["components"].is_object())
            return e;

        const auto& components = j["components"];

        if (components.contains("transform"))
        {
            e.HasTransform = true;
            e.Transform = DeserializeTransform(components["transform"]);
        }

        if (components.contains("meshRenderer"))
        {
            e.HasMeshRenderer = true;
            e.MeshRenderer = DeserializeMeshRenderer(components["meshRenderer"]);
        }

        if (components.contains("camera"))
        {
            e.HasCamera = true;
            e.Camera = DeserializeCamera(components["camera"]);
        }

        if (components.contains("light"))
        {
            e.HasLight = true;
            e.Light = DeserializeLight(components["light"]);
        }

        return e;
    }

    bool XJSceneAssetSerializer::SaveToFile(const XJSceneAsset& sceneAsset, const std::filesystem::path& path)
    {
        if (path.has_parent_path())
            std::filesystem::create_directories(path.parent_path());

        nlohmann::json root;
        root["version"] = 2;
        root["asset"] = {
            {"handle", std::to_string(sceneAsset.mHandle)},
            {"type", "Scene"},
            {"name", sceneAsset.mName}
        };

        root["objects"] = nlohmann::json::array();
        for (const auto& e : sceneAsset.Entities)
            root["objects"].push_back(SerializeEntity(e));

        std::ofstream out(path);
        if (!out)
            return false;

        out << root.dump(2);
        return out.good();
    }

    std::shared_ptr<XJSceneAsset> XJSceneAssetSerializer::LoadFromFile(const std::filesystem::path& path)
    {
        std::ifstream in(path);
        if (!in)
            return nullptr;
        
        nlohmann::json j;
        try
        {
            j = nlohmann::json::parse(in);
        }
        catch (const nlohmann::json::exception&)
        {
            return nullptr;
        }
    
        if (j.value("version", 0) != 2)
            return nullptr;
    
        auto asset = std::make_shared<XJSceneAsset>();
        if (j.contains("asset"))
        {
            asset->mHandle = ReadUInt64(j["asset"], "handle", 0);
            asset->mName = j["asset"].value("name", std::string{});
        }
    
        if (!j.contains("objects") || !j["objects"].is_array())
            return asset;
    
        for (const auto& objectJson : j["objects"])
        {
            if (objectJson.value("type", std::string{}) != "Entity")
                continue;
        
            asset->Entities.push_back(DeserializeEntity(objectJson));
        }
    
        return asset;
    }

    std::shared_ptr<XJSceneAsset> XJSceneAssetSerializer::BuildFromScene(const XJScene& scene)
    {
       
        auto asset = std::make_shared<XJSceneAsset>();

        for (const auto& [enttEntity, entityPtr] : scene.GetEntities())
        {
            if (!entityPtr)
                continue;
        
            const XJEntity& entity = *entityPtr;
        
            // 不保存 editor preview camera。
            if (entity.XJGetUUID() == XJUUID(static_cast<uint64_t>(0x30000001ull)))
                continue;
        
            XJSceneEntityData data = BuildEntityData(entity);
            asset->Entities.push_back(data);
        }
    
        return asset;
    }

    XJSceneEntityData XJSceneAssetSerializer::BuildEntityData(const XJEntity& entity)
    {
        XJSceneEntityData data;
        data.UUID = entity.XJGetUUID();
        data.Type = "Entity";
        data.Name = entity.XJGetName();

        if (entity.HasParent())
            data.Parent = entity.XJGetParent()->XJGetUUID();

        for (auto* child : entity.XJGetChildren())
            data.Children.push_back(child->XJGetUUID());

        if (entity.HasComponent<XJTransformComponent>())
        {
            auto& t = entity.GetComponent<XJTransformComponent>();
            data.HasTransform = true;
            data.Transform.UUID = t.XJGetUUID();
            data.Transform.Position = t.position;
            data.Transform.Rotation = t.rotation;
            data.Transform.Scale = t.scale;
        }

        if (entity.HasComponent<XJMeshAssetRefComponent>())
        {
            auto& mr = entity.GetComponent<XJMeshAssetRefComponent>();
            data.HasMeshRenderer = true;
            data.MeshRenderer.UUID = mr.XJGetUUID();
            data.MeshRenderer.Mesh = mr.Mesh;
        }

        if (entity.HasComponent<XJMaterialAssetRefComponent>())
        {
            data.HasMeshRenderer = true;
            data.MeshRenderer.Materials = entity.GetComponent<XJMaterialAssetRefComponent>().Materials;
        }

        if (entity.HasComponent<XJCameraComponent>())
        {
            auto& c = entity.GetComponent<XJCameraComponent>();
            data.HasCamera = true;
            data.Camera.UUID = c.XJGetUUID();
            data.Camera.Enabled = true;
            data.Camera.Fov = c.XJGetFov();
            data.Camera.NearClip = c.XJGetNear();
            data.Camera.FarClip = c.XJGetFar();
        }

        return data;
    }
}