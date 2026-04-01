#ifndef XJ_COMPONENT_H
#define XJ_COMPONENT_H

#include "ECS/XJEntity.h"

namespace XJ
{
    class XJComponent
    {
        public:
            //归属  谁拥有
            void SetOwner(XJEntity *owner){mOwner = owner;}
            XJEntity *XJGetOwner() const {return mOwner;}

        private:
            XJEntity *mOwner = nullptr;
    };
    
}

#endif