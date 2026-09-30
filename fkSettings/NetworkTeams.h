#pragma once
#include <array>
#include <cstdint>

namespace NetworkTeams
{
    // Native start packet (type + GUID), followed by six name/skill records.
    constexpr uint32_t NativeStartPacketSize = 20;
    using StartPacket = std::array<unsigned char, NativeStartPacketSize + 4 + 6 * 18>;

    bool Install(); // Install after the shared lobby packet hooks in SecretWeapons.
    bool ExtendStartPacket(const void* packet, uint32_t length, StartPacket& extended);
    void ReceiveStartPacket(uint32_t sender, uint32_t host, const void* packet, uint32_t length);
}
