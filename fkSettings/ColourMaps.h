#pragma once
#include <cstdint>

namespace ColourMaps
{
    // MinHook must be initialized; online transfer uses the shared packet hooks.
    bool Install();
    using NetworkSend = void (*)(void*, uint32_t, uint32_t, bool, const void*, uint32_t);
    // Called by the shared packet hooks after other extensions are serialized.
    void SendNetworkPacket(void* object, uint32_t source, uint32_t target, bool broadcast,
        const void* packet, uint32_t length, NetworkSend send);
    // False consumes/rejects the packet. Strip our trailer before other decoders.
    bool ReceiveNetworkPacket(uint32_t sender, uint32_t host, const void* packet, uint32_t& length);
    void JoiningLobby(void* object);
    void JoiningRound(void* object);
}
