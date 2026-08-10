#ifndef XJ_RESERVED_UUID_H
#define XJ_RESERVED_UUID_H

#include <cstdint>

namespace XJ
{
    // Engine/editor-owned UUIDs live in this reserved range. User-created UUIDs are
    // random 64-bit values, so centralizing reserved IDs avoids scattered literals.
    inline constexpr uint64_t XJ_RESERVED_UUID_BASE = 0x30000000ull;
    inline constexpr uint64_t XJ_RESERVED_UUID_END = 0x3000FFFFull;
    inline constexpr uint64_t XJ_PREVIEW_CAMERA_UUID = XJ_RESERVED_UUID_BASE + 0x1ull;

    inline constexpr bool XJIsReservedUUID(uint64_t uuid)
    {
        return uuid >= XJ_RESERVED_UUID_BASE && uuid <= XJ_RESERVED_UUID_END;
    }
}

#endif
