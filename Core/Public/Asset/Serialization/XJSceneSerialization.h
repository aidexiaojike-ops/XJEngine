#ifndef XJ_SCENE_SERIALIZATION_H
#define XJ_SCENE_SERIALIZATION_H

#include "Asset/XJAsset.h"
#include <memory>
#include <unordered_map>
#include <glm/glm.hpp>


namespace tinygltf { class Model; }  

namespace XJ
{
    class XJScene;
    class XJMesh;
    class XJTexture;
    class XJSampler;
    class XJGltfImporter;
    class XJSceneAsset;
    
    class XJSceneSerialization
    {
        public:
            // glTF → ECS Scene
            static bool InstantiateGltfScene(XJScene* outScene,
                                         XJGltfImporter& importer,  
                                         const std::shared_ptr<XJTexture>&  defaultTex,
                                         const std::shared_ptr<XJSampler>&  defaultSampler
                                         );
            static std::shared_ptr<XJSceneAsset> CreateDefaultSceneAsset();
        private:
            // 递归处理一个 glTF node（包括子节点）
            static void ProcessNode(XJScene* scene,
                                const tinygltf::Model& model, int nodeIdx,
                                const glm::mat4& parentTransform,
                                const std::shared_ptr<XJTexture>& defaultTex,
                                const std::shared_ptr<XJSampler>& defaultSampler);
                                
            static std::unordered_map<std::string, std::shared_ptr<XJMesh>> mMeshCache;  
            
            
            
    };
}

#endif