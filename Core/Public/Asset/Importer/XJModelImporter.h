#ifndef XJ_MODEL_IMPORTER_H   
#define XJ_MODEL_IMPORTER_H

#include "Asset/XJMeshAsset.h"
#include <memory>
#include <tiny_gltf.h>

namespace XJ
{
    class XJGltfImporter
    {
        public:
             // ── Mesh 提取（静态，接受任意 Model）──
            static std::shared_ptr<XJMeshAsset> ExtractMesh(const tinygltf::Model& model, int meshIndex);
           
            bool LoadMeshAsset(const std::string& path);
            const tinygltf::Model& XJGetModel() const { return mModel; }
            const tinygltf::Scene& XJGetDefaultScene() const
            {  
                int idx = mModel.defaultScene >= 0 ? mModel.defaultScene : 0;
                return mModel.scenes[idx];
            };

            std::shared_ptr<XJMeshAsset> ExtractMesh(int meshIndex)
            {
                return ExtractMesh(mModel, meshIndex);
            }
        private:
            tinygltf::Model mModel;
            std::string mFilePath;
    };
    //class XJOBJImporter{};
    //class XJFBXImporter{};
}

#endif