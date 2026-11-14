#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace OnlineAlliances
{
    using Send = void (*)(void*, uint32_t, uint32_t, bool, const void*, uint32_t);
    using SendToHost = void (*)(void*, uint32_t, uint32_t, const void*, uint32_t);
    // Optional callbacks let the existing shared hooks compose extensions and
    // lifecycle events without adding competing detours or test dependencies.
    struct Integration
    {
        Send send = nullptr;
        SendToHost sendToHost = nullptr;
        bool (*extend)(const void*, uint32_t, std::vector<unsigned char>&) = nullptr;
        bool (*receive)(void*, uint32_t, uint32_t, const void*, uint32_t&, bool) = nullptr;
        void (*afterReceive)(void*, uint32_t, const void*, uint32_t, bool) = nullptr;
        bool (*hostReceive)(void*, uint32_t, const void*, uint32_t, bool) = nullptr;
        void (*lobby)(void*, bool, bool, bool) = nullptr;
        void (*treeChanged)(void*) = nullptr;
        bool (*launch)(const void*, const void*, bool, std::array<unsigned char, 7>&) = nullptr;
        void (*launchComplete)(bool) = nullptr;
        void (*shuffle)(void*, const void*) = nullptr;
        bool (*canRunEngine)() = nullptr;
    };
    inline Integration& Callbacks() { static Integration callbacks; return callbacks; }
    bool Install();
}
