#ifndef XJ_SCRIPT_COMPONENT_H
#define XJ_SCRIPT_COMPONENT_H

#include "Asset/XJAssetRef.h"
#include "ECS/XJComponent.h"
#include "Script/Bytecode/XJScriptBytecode.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace XJ
{
    struct XJScriptSlot
    {
        XJUUID SlotId{0};
        XJAssetRef Script;
        bool Enabled = true;

        // key 是脚本源码 [[FieldId("...")]] 中的稳定 ID。
        std::unordered_map<uint64_t, XJScriptValue> FieldOverrides;
    };

    class XJScriptComponent final : public XJComponent
    {
    public:
        XJScriptSlot* AddSlot(const XJAssetRef& script, bool enabled = true)
        {
            if (!IsValidScriptRef(script))
                return nullptr;

            XJUUID slotId;
            while (FindSlot(slotId) != nullptr)
                slotId = XJUUID();
            return AddSlotWithId(slotId, script, enabled);
        }

        XJScriptSlot* AddSlotWithId(
            XJUUID slotId,
            const XJAssetRef& script,
            bool enabled = true)
        {
            if (static_cast<uint64_t>(slotId) == 0 ||
                !IsValidScriptRef(script) ||
                FindSlot(slotId) != nullptr)
            {
                return nullptr;
            }

            XJScriptSlot slot;
            slot.SlotId = slotId;
            slot.Script = script;
            slot.Enabled = enabled;
            mSlots.push_back(std::move(slot));
            return &mSlots.back();
        }

        bool RemoveSlot(XJUUID slotId)
        {
            const auto it = std::find_if(
                mSlots.begin(), mSlots.end(),
                [slotId](const XJScriptSlot& slot)
                {
                    return static_cast<uint64_t>(slot.SlotId) ==
                           static_cast<uint64_t>(slotId);
                });
            if (it == mSlots.end())
                return false;
            mSlots.erase(it);
            return true;
        }

        XJScriptSlot* FindSlot(XJUUID slotId)
        {
            for (auto& slot : mSlots)
            {
                if (static_cast<uint64_t>(slot.SlotId) == static_cast<uint64_t>(slotId))
                    return &slot;
            }
            return nullptr;
        }

        const XJScriptSlot* FindSlot(XJUUID slotId) const
        {
            for (const auto& slot : mSlots)
            {
                if (static_cast<uint64_t>(slot.SlotId) == static_cast<uint64_t>(slotId))
                    return &slot;
            }
            return nullptr;
        }

        bool SetSlotEnabled(XJUUID slotId, bool enabled)
        {
            XJScriptSlot* slot = FindSlot(slotId);
            if (!slot)
                return false;
            slot->Enabled = enabled;
            return true;
        }

        bool SetSlotScript(XJUUID slotId, const XJAssetRef& script)
        {
            XJScriptSlot* slot = FindSlot(slotId);
            if (!slot || !IsValidScriptRef(script))
                return false;
            slot->Script = script;
            slot->FieldOverrides.clear();
            return true;
        }

        bool SetFieldOverride(XJUUID slotId, uint64_t fieldId, XJScriptValue value)
        {
            XJScriptSlot* slot = FindSlot(slotId);
            if (!slot || fieldId == 0)
                return false;
            if (const auto* real = std::get_if<double>(&value); real && !std::isfinite(*real))
                return false;
            slot->FieldOverrides[fieldId] = std::move(value);
            return true;
        }

        bool ClearFieldOverride(XJUUID slotId, uint64_t fieldId)
        {
            XJScriptSlot* slot = FindSlot(slotId);
            return slot && slot->FieldOverrides.erase(fieldId) > 0;
        }

        const XJScriptValue* GetFieldOverride(XJUUID slotId, uint64_t fieldId) const
        {
            const XJScriptSlot* slot = FindSlot(slotId);
            if (!slot)
                return nullptr;
            const auto it = slot->FieldOverrides.find(fieldId);
            return it == slot->FieldOverrides.end() ? nullptr : &it->second;
        }

        const std::vector<XJScriptSlot>& GetSlots() const { return mSlots; }
        size_t GetSlotCount() const { return mSlots.size(); }
        void ClearSlots() { mSlots.clear(); }

    private:
        static bool IsValidScriptRef(const XJAssetRef& script)
        {
            return script.IsValid() && script.Type == XJAssetType::Script;
        }

        std::vector<XJScriptSlot> mSlots;
    };
}

#endif
