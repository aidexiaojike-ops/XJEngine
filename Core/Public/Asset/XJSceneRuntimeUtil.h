#ifndef XJ_SCENE_RUNTIME_UTIL_H
#define XJ_SCENE_RUNTIME_UTIL_H
//运行时工具 (查找主摄像机)
namespace XJ
{
    class XJScene;
    class XJEntity;

    class XJSceneRuntimeUtil
    {
        private:
            /* data */
        public:
            static XJEntity* FindPrimaryCameraEntity(XJScene& scene);//找到摄像机
    };
    
  
    
}

#endif

