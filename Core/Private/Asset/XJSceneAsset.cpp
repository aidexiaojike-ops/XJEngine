#include "Asset/XJSceneAsset.h"

namespace XJ
{
    XJSceneEntityData* XJSceneAsset::FindEntity(XJUUID id)
    {
        for(auto& e: Entities)
           if (e.UUID == id) return &e;
        return nullptr;
    }
    const XJSceneEntityData* XJSceneAsset::FindEntity(XJUUID id) const
    {
        for (const auto& e : Entities)
            if (e.UUID == id) return &e;
        return nullptr;
    }
}