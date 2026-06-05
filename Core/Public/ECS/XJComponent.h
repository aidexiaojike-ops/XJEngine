#ifndef XJ_COMPONENT_H
#define XJ_COMPONENT_H

#include "ECS/XJEntity.h"
#include "ECS/XJUUID.h"

namespace XJ
{
    class XJComponent
    {
        public:
            //归属  谁拥有
            void SetOwner(XJEntity *owner){mOwner = owner;}
            XJEntity *XJGetOwner() const {return mOwner;}

            XJUUID XJGetUUID() const {return mUUID;}

            void XJSetUUID(const XJUUID& uuid) {mUUID = uuid;}

        private:
            XJUUID mUUID;
            XJEntity *mOwner = nullptr;
    };
    
}

#endif