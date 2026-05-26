#include "Asset/Serialization/XJSceneAssetSerializer.h"

#include "ECS/Component/Material/XJUnlitMaterialComponent.h"
#include "ECS/Component/XJCameraComponent.h"
#include "ECS/Component/XJSceneAssetComponents.h"
#include "ECS/Component/XJTransformComponent.h"
#include "ECS/XJEntity.h"
#include "ECS/XJScene.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace XJ
{
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
            {"pos", SerializeVec3(t.Position)},
            {"rot", SerializeVec3(t.Rotation)},
            {"scale", SerializeVec3(t.Scale)}
        };
    }

    static nlohmann::json SerializeAssetRefArray(const std::vector<XJAssetRef>& refs)
    {
        auto j = nlohmann::json::array();
        for (const auto& ref : refs)
            j.push_back(ref.ToUri());
        return j;
    }

    static nlohmann::json SerializeEntity(const XJSceneEntityData& e)
    {
        nlohmann::json j;
        j["id"] = static_cast<uint64_t>(e.Id);
        j["name"] = e.Name;
        j["parent"] = static_cast<uint64_t>(e.Parent);

        auto children = nlohmann::json::array();
        for (auto c : e.Children)
            children.push_back(static_cast<uint64_t>(c));
        j["children"] = children;

        j["transform"] = SerializeTransform(e.Transform);

        nlohmann::json mats = nlohmann::json::array();
        for (const auto& mat : e.MeshRenderer.Materials)
            mats.push_back(mat.ToUri());

        j["meshRenderer"] = {
            {"mesh",   e.MeshRenderer.Mesh.ToUri()},
            {"materials", mats}
        };

        j["camera"] = {
            {"enabled", e.Camera.Enabled},
            {"fov", e.Camera.Fov},
            {"near", e.Camera.NearClip},
            {"far", e.Camera.FarClip},
            {"primary", e.Camera.Primary}
        };
        j["light"] = {
            {"enabled", e.Light.Enabled},
            {"type", e.Light.Type},
            {"color", SerializeVec3(e.Light.Color)},
            {"intensity", e.Light.Intensity}
        };

        return j;
    }

    static XJSceneTransformData DeserializeTransform(const nlohmann::json& j)
    {
        XJSceneTransformData t;
        t.Position = DeserializeVec3(j.value("pos", nlohmann::json::array()), t.Position);
        t.Rotation = DeserializeVec3(j.value("rot", nlohmann::json::array()), t.Rotation);
        t.Scale = DeserializeVec3(j.value("scale", nlohmann::json::array()), t.Scale);
        return t;
    }

    static XJSceneEntityData DeserializeEntity(const nlohmann::json& j)
    {
        XJSceneEntityData e;
        e.Id = XJUUID(j.value("id", 0ull));
        e.Name = j.value("name", std::string{});
        e.Parent = XJUUID(j.value("parent", 0ull));

        if (j.contains("children") && j["children"].is_array())
        {
            for (auto& c : j["children"])
                e.Children.push_back(XJUUID(c.get<uint64_t>()));
        }

        if (j.contains("transform"))
            e.Transform = DeserializeTransform(j["transform"]);

        if (j.contains("meshRenderer"))
        {
            const auto& mr = j["meshRenderer"];
            e.MeshRenderer.Mesh = XJAssetRef::FromUri(mr.value("mesh", std::string{}), XJAssetType::Mesh);
            if (mr.contains("materials") && mr["materials"].is_array())
            {
                for (const auto& mat : mr["materials"])
                    e.MeshRenderer.Materials.push_back(
                        XJAssetRef::FromUri(mat.get<std::string>(), XJAssetType::Material));
            }
        }

        if (j.contains("camera"))
        {
            const auto& c = j["camera"];
            e.Camera.Enabled = c.value("enabled", false);
            e.Camera.Fov = c.value("fov", e.Camera.Fov);
            e.Camera.NearClip = c.value("near", e.Camera.NearClip);
            e.Camera.FarClip = c.value("far", e.Camera.FarClip);
            e.Camera.Primary = c.value("primary", false);
        }

        if (j.contains("light"))
        {
            const auto& l = j["light"];
            e.Light.Enabled = l.value("enabled", false);
            e.Light.Type = l.value("type", e.Light.Type);
            e.Light.Color = DeserializeVec3(l.value("color", nlohmann::json::array()), e.Light.Color);
            e.Light.Intensity = l.value("intensity", e.Light.Intensity);
        }
        
        for (const auto& mj : j["meshRenderer"]["materials"])
            e.MeshRenderer.Materials.push_back(XJAssetRef::FromUri(mj.get<std::string>()));

        return e;
    }

    bool XJSceneAssetSerializer::SaveToFile(const XJSceneAsset& sceneAsset, const std::filesystem::path& path)
    {
        if (path.has_parent_path())
            std::filesystem::create_directories(path.parent_path());

        nlohmann::json root;
        root["version"] = 1;
        root["asset"] = {
            {"handle", sceneAsset.mHandle},
            {"type", static_cast<int>(sceneAsset.mType)},
            {"name", sceneAsset.mName}
        };
        root["entities"] = nlohmann::json::array();
        for (const auto& e : sceneAsset.Entities)
            root["entities"].push_back(SerializeEntity(e));

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

        auto asset = std::make_shared<XJSceneAsset>();
        if (j.contains("asset"))
        {
            asset->mHandle = j["asset"].value("handle", 0ull);
            asset->mName = j["asset"].value("name", std::string{});
        }

        if (j.contains("entities") && j["entities"].is_array())
        {
            for (const auto& ej : j["entities"])
                asset->Entities.push_back(DeserializeEntity(ej));
        }

        return asset;
    }

    std::shared_ptr<XJSceneAsset> XJSceneAssetSerializer::BuildFromScene(const XJScene& scene)
    {
        auto asset = std::make_shared<XJSceneAsset>();
        auto& reg = const_cast<XJScene&>(scene).XJGetEcsRegistry();
        auto view = reg.view<XJTransformComponent>();
        for (auto e : view)
        {
            auto* xjEntity = scene.XJGetEntities(e);
            if (!xjEntity)
                continue;

            XJSceneEntityData data = BuildEntityData(*xjEntity);
            asset->Entities.push_back(data);
        }
        return asset;
    }

    XJSceneEntityData XJSceneAssetSerializer::BuildEntityData(const XJEntity& entity)
    {
        XJSceneEntityData data;
        data.Id = entity.XJGetId();
        data.Name = entity.XJGetName();

        if (entity.HasParent())
            data.Parent = entity.XJGetParent()->XJGetId();
        for (auto* child : entity.XJGetChildren())
            data.Children.push_back(child->XJGetId());

        if (entity.HasComponent<XJTransformComponent>())
        {
            auto& t = entity.GetComponent<XJTransformComponent>();
            data.Transform.Position = t.position;
            data.Transform.Rotation = t.rotation;
            data.Transform.Scale = t.scale;
        }

        if (entity.HasComponent<XJMeshAssetRefComponent>())
            data.MeshRenderer.Mesh = entity.GetComponent<XJMeshAssetRefComponent>().Mesh;

        if (entity.HasComponent<XJMaterialAssetRefComponent>())
            data.MeshRenderer.Materials = entity.GetComponent<XJMaterialAssetRefComponent>().Materials;

        if (entity.HasComponent<XJCameraComponent>())
        {
            auto& c = entity.GetComponent<XJCameraComponent>();
            data.Camera.Enabled = true;
            data.Camera.Fov = c.XJGetFov();
            data.Camera.NearClip = c.XJGetNear();
            data.Camera.FarClip = c.XJGetFar();
        }

        return data;
    }
}
