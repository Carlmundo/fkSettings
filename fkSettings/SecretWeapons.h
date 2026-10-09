#pragma once
#include <string>
#include <array>
#include <cstdint>

namespace SecretWeapons
{
    // MinHook must already be initialized. Unsupported frontends are left alone.
    bool Install();
    void SetLanguage(const std::string& language);

    // Weapon-specific serialization used by FrontendNetwork's shared hooks.
    using NetworkPacket = std::array<unsigned char, 0x664>;
    using NetworkReceive = void (*)(void*, uint32_t, const void*, uint32_t);
    bool ExtendNetworkPacket(const void* packet, uint32_t length, NetworkPacket& extended);
    bool IsNetworkPacket(const void* packet, uint32_t length);
    void ReceiveNetworkPacket(void* object, uint32_t sender, uint32_t host,
        const void* packet, uint32_t length, NetworkReceive receive);
}
