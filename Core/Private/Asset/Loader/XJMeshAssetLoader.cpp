#include "Asset/Loader/XJMeshAssetLoader.h"

#include "Asset/Importer/XJModelImporter.h"
#include "Asset/XJAssetRegistry.h"
#include "Render/Resource/XJMeshFactory.h"

#include <spdlog/spdlog.h>


namespace XJ
{

    namespace
    {
        constexpr const char* kBuiltinTJCubeSource = "builtin://mesh/TJCube";//注册一个自带的TJCube

        bool IsBuiltinTJCube(const std::filesystem::path& sourcePath)
        {
            const std::string source = sourcePath.generic_string();
            return source == kBuiltinTJCubeSource || source == "builtin:/mesh/TJCube";
        }
    }

    std::shared_ptr<XJMesh> XJMeshAssetLoader::LoadMesh(XJAssetHandle handle, XJMeshAssetLoadContext& context)
    {
        if(handle == 0)return nullptr;

        if(context.MeshCache)
        {
            auto cacheIt = context.MeshCache->find(handle);//先查缓存
            if(cacheIt != context.MeshCache->end())
                return cacheIt->second;
        }

        std::shared_ptr<XJMesh> gpuMesh;

        if(context.Registry && context.Registry->Contains(handle))//再查注册表
        {
            auto metaOpt = context.Registry->GetMeta(handle);
            if (metaOpt.has_value())
            {
                if (metaOpt->Type != XJAssetType::Mesh)
                    return nullptr;
                if (IsBuiltinTJCube(metaOpt->SourcePath))
                {
                    gpuMesh = XJMeshFactory::CreateCubeMesh();

                    if (!gpuMesh)
                        spdlog::error("Create builtin mesh TJCube failed");
                }
                else
                {
                    XJGltfImporter importer;
                    if(importer.LoadMeshAsset(metaOpt->SourcePath.string()))//从文件加载网格数据
                    {
                        auto meshAsset = importer.ExtractMesh(0);//提取第一个网格
                        if(meshAsset && !meshAsset->mVertices.empty())
                            gpuMesh = XJMeshFactory::CreateFromAsset(*meshAsset);//创建 GPU 网格资源

                    }
                }
            }
        }

        if (context.MeshCache && gpuMesh)
            (*context.MeshCache)[handle] = gpuMesh; //放入缓存

        return gpuMesh; 
            
    }
}