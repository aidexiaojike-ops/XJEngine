#include "Asset/Loader/XJMeshAssetLoader.h"

#include "Asset/Importer/XJModelImporter.h"
#include "Asset/XJAssetRegistry.h"
#include "Render/Resource/XJMeshFactory.h"


namespace XJ
{
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

                XJGltfImporter importer;
                if(importer.LoadMeshAsset(metaOpt->SourcePath.string()))//从文件加载网格数据
                {
                    auto meshAsset = importer.ExtractMesh(0);//提取第一个网格
                    if(meshAsset && !meshAsset->mVertices.empty())
                        gpuMesh = XJMeshFactory::CreateFromAsset(*meshAsset);//创建 GPU 网格资源
                    
                       
                }
            }
        }

        if(context.MeshCache)
            (*context.MeshCache)[handle] = gpuMesh;//放入缓存

        return gpuMesh; 
            
    }
}