#ifndef XJ_MATERIAL_UPLOAD_STATE_H
#define XJ_MATERIAL_UPLOAD_STATE_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace XJ
{
    struct XJMaterialUploadRequirements
    {
        bool Parameters = false;
        bool Resources = false;
    };

    class XJMaterialUploadState
    {
        private:
            /* data */
            struct XJMaterialUploadStamp
            {
                uint64_t OwnerId = 0;
                uint64_t ParameterRevision = 0;
                uint64_t ResourceRevision = 0;
            };

            XJMaterialUploadStamp* FindStamp(uint32_t frameSlot, uint32_t materialIndex)
            {
                if(frameSlot >= mFrameSlotCount || materialIndex >= mMaterialCapacity){return nullptr;}

                return &mSlots[static_cast<size_t>(frameSlot)* mMaterialCapacity + materialIndex];
            }

            const XJMaterialUploadStamp* FindStamp(uint32_t frameSlot, uint32_t materialIndex)const
            {
                if(frameSlot >= mFrameSlotCount || materialIndex >= mMaterialCapacity){return nullptr;}

                return &mSlots[static_cast<size_t>(frameSlot)* mMaterialCapacity + materialIndex];
            }
            
            uint32_t mMaterialCapacity = 0;
            uint32_t mFrameSlotCount = 0;
            std::vector<XJMaterialUploadStamp> mSlots;
        public:
            void Configure(uint32_t materialCapacity, uint32_t frameSlotCount)//配置
            {
                const size_t slotCount = static_cast<size_t>(materialCapacity) * frameSlotCount;

                if(mMaterialCapacity == materialCapacity && mFrameSlotCount == frameSlotCount && mSlots.size() == slotCount){return;}

                mSlots.assign(slotCount,{});
                mMaterialCapacity = materialCapacity;
                mFrameSlotCount = frameSlotCount;
            }

            std::optional<XJMaterialUploadRequirements> Prepare(uint32_t frameSlot, uint32_t materialIndex,
                 uint64_t ownerId, uint64_t parameterRevision, uint64_t resourceRevision)//准备
            {
                XJMaterialUploadStamp* stamp = FindStamp(frameSlot, materialIndex);
                if (!stamp || ownerId == 0 || parameterRevision == 0 || resourceRevision == 0)
                    return std::nullopt;

                if(stamp->OwnerId != ownerId)
                    *stamp = {ownerId, 0, 0};

                return XJMaterialUploadRequirements
                {
                    stamp->ParameterRevision != parameterRevision,
                    stamp->ResourceRevision != resourceRevision
                };
            }

            bool CommitParameters(uint32_t frameSlot, uint32_t materialIndex, uint64_t ownerId, uint64_t revision)//提交参数
            {
                XJMaterialUploadStamp* stamp = FindStamp(frameSlot, materialIndex);
                if (!stamp || ownerId == 0 || revision == 0 || stamp->OwnerId != ownerId)
                    return false;

                stamp-> ParameterRevision = revision;
                return true;
            }

            bool CommitResources(uint32_t frameSlot, uint32_t materialIndex, uint64_t ownerId, uint64_t revision)//提交资源
            {
                XJMaterialUploadStamp* stamp = FindStamp(frameSlot, materialIndex);
                if (!stamp || ownerId == 0 || revision == 0 || stamp->OwnerId != ownerId)
                    return false;

                stamp->ResourceRevision = revision;
                return true;
            }

            bool IsCurrent(uint32_t frameSlot, uint32_t materialIndex, uint64_t ownerId, uint64_t parameterRevision, uint64_t resourceRevision) const//是否是当前
            {
                const XJMaterialUploadStamp* stamp = FindStamp(frameSlot, materialIndex);

                return stamp && ownerId != 0 && parameterRevision != 0 && resourceRevision != 0 &&
                       stamp->OwnerId == ownerId &&
                       stamp->ParameterRevision == parameterRevision &&
                       stamp->ResourceRevision == resourceRevision;
            }  
            
            size_t GetSlotCount() const{return mSlots.size();}
    };
    

}
#endif
