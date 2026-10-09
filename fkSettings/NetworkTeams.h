#pragma once
#include <array>
#include <cstdint>

namespace NetworkTeams
{
    // Native start packet (type + GUID), followed by six name/skill records.
    constexpr uint32_t NativeStartPacketSize = 20;
    using StartPacket = std::array<unsigned char, NativeStartPacketSize + 4 + 6 * 18>;
    constexpr uint32_t NativeSnapshotPacketSize = 0x10de;
    constexpr uint32_t NativeAddTeamPacketSize = 0xc8;
    using LobbyPacket = std::array<unsigned char, NativeSnapshotPacketSize + 4 + 6 * 18>;

    bool Install(); // Install after the shared lobby packet hooks in FrontendNetwork.
    bool ExtendStartPacket(const void* packet, uint32_t length, StartPacket& extended);
    void ReceiveStartPacket(uint32_t sender, uint32_t host, const void* packet, uint32_t length);
    uint32_t LobbyPacketSize(uint32_t type);
    // Returns the extended length, or zero when the native packet should be sent.
    uint32_t ExtendLobbyPacket(const void* packet, uint32_t length, LobbyPacket& extended);
    // Mark the joining tree before native dispatch; refresh icons afterwards,
    // once the initial snapshot has established the host.
    void SetJoiningLobby(void* object);
    void ReceiveLobbyPacket(void* object, uint32_t sender, const void* packet, uint32_t length);
}
