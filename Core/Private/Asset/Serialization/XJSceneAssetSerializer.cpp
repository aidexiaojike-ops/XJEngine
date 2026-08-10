#include "Asset/Serialization/XJSceneAssetSerializer.h"

#include "ECS/Component/Material/XJUnlitMaterialComponent.h"
#include "ECS/Component/XJCameraComponent.h"
#include "ECS/Component/XJSceneAssetComponents.h"
#include "ECS/Component/XJTransformComponent.h"
#include "ECS/XJEntity.h"
#include "ECS/XJScene.h"
#include "Asset/Serialization/XJJsonIO.h"
#include "ECS/XJReservedUUID.h"

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
        return JsonReadUInt64Or(j, key, fallback);
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
        return JsonReadVec3Or(j, fallback);
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
        if (j.is_object())
        {
            if (j.contains("pos"))
                t.Position = DeserializeVec3(j["pos"], t.Position);

            if (j.contains("rot"))
                t.Rotation = DeserializeVec3(j["rot"], t.Rotation);

            if (j.contains("scale"))
                t.Scale = DeserializeVec3(j["scale"], t.Scale);
        }
        return t;
    }

    static XJSceneMeshRendererData DeserializeMeshRenderer(const nlohmann::json& j)
    {
        XJSceneMeshRendererData mr;
        mr.UUID = ReadUUID(j, "uuid");
        mr.Mesh = XJAssetRef::FromUri(JsonReadStringOr(j, "mesh"), XJAssetType::Mesh);

        if (j.contains("materials") && j["materials"].is_array())
        {
            for (const auto& mat : j["materials"])
            {
                if (mat.is_string())
                    mr.Materials.push_back(XJAssetRef::FromUri(mat.get<std::string>(), XJAssetType::Material));
            }
        }

        return mr;
    }

    static XJSceneCameraData DeserializeCamera(const nlohmann::json& j)
    {
        XJSceneCameraData c;
        c.UUID = ReadUUID(j, "uuid");
        c.Enabled = JsonReadBoolOr(j, "enabled", true);
        c.Fov = JsonReadFloatOr(j, "fov", c.Fov);
        c.NearClip = JsonReadFloatOr(j, "near", c.NearClip);
        c.FarClip = JsonReadFloatOr(j, "far", c.FarClip);
        c.Primary = JsonReadBoolOr(j, "primary", false);
        return c;
    }

    static XJSceneLightData DeserializeLight(const nlohmann::json& j)
    {
        XJSceneLightData l;
        l.UUID = ReadUUID(j, "uuid");
        l.Enabled = JsonReadBoolOr(j, "enabled", true);
        l.Type = JsonReadIntOr(j, "lightType", JsonReadIntOr(j, "type", l.Type));
        if (j.is_object() && j.contains("color"))
            l.Color = DeserializeVec3(j["color"], l.Color);
        l.Intensity = JsonReadFloatOr(j, "intensity", l.Intensity);
        return l;
    }

    static XJSceneEntityData DeserializeEntity(const nlohmann::json& j)
    {
        XJSceneEntityData e;
        e.UUID = ReadUUID(j, "uuid");
        e.Type = JsonReadStringOr(j, "type", std::string{"Entity"});
        e.Name = JsonReadStringOr(j, "name");

        if (j.contains("parent") && !j["parent"].is_null())
            e.Parent = XJUUID(ReadUInt64(j, "parent", 0));

        if (j.contains("children") && j["children"].is_array())
        {
            for (const auto& child : j["children"])
            {
                uint64_t childId = 0;
                if (JsonReadUInt64(child, childId))
                    e.Children.push_back(XJUUID(childId));
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

        return WriteJsonFileAtomic(path, root);
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
    
        if (JsonReadIntOr(j, "version", 0) != 2)
            return nullptr;
    
        auto asset = std::make_shared<XJSceneAsset>();
        if (j.contains("asset"))
        {
            asset->mHandle = ReadUInt64(j["asset"], "handle", 0);
            asset->mName = JsonReadStringOr(j["asset"], "name");
        }
    
        if (!j.contains("objects") || !j["objects"].is_array())
            return asset;
    
        for (const auto& objectJson : j["objects"])
        {
            if (!objectJson.is_object() || JsonReadStringOr(objectJson, "type") != "Entity")
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
            if (entity.XJGetUUID() == XJUUID(XJ_PREVIEW_CAMERA_UUID))
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
            data.Camera.Enabled = c.XJGetEnabled();
            data.Camera.Fov = c.XJGetFov();
            data.Camera.NearClip = c.XJGetNear();
            data.Camera.FarClip = c.XJGetFar();
        }

        return data;
    }
}
