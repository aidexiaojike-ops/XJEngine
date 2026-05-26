#ifndef XJ_UUID_H
#define XJ_UUID_H

#include "Edit/EditIncludes.h"

namespace XJ
{
    class XJUUID
    {
        private:
            /* data */
        public:
            XJUUID(/* args */);
            XJUUID(uint64_t uuid);
            XJUUID(const XJUUID&);
            ~XJUUID();
            operator uint64_t() const{return mUUID;}

            uint64_t mUUID;
    };
    

}
namespace std
{
    template<>
    struct  hash<XJ::XJUUID>
    {
        std::size_t operator()(const XJ::XJUUID &uuid) const
        {
            if(!uuid)
            {
                return 0;
            }
            return static_cast<std::size_t>(static_cast<uint64_t>(uuid));
        }
        /* data */
    };
    
}




#endif
