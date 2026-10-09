#define NOMINMAX
typedef struct IUnknown IUnknown;
#include <Windows.h>
#include <cstdint>
#include <cstring>
#include "include/MinHook.h"
#include "FrontendNetwork.h"
#include "SecretWeapons.h"
#include "NetworkTeams.h"
#include "ExtendedOptions.h"
#include "ColourMaps.h"

namespace FrontendNetwork
{
namespace
{
    BYTE* image = nullptr;
    using SendToPlayer = void (__thiscall*)(void*, uint32_t, uint32_t, const void*, uint32_t);
    using SendToAll = void (__thiscall*)(void*, uint32_t, const void*, uint32_t);
    using ReceivePacket = void (__thiscall*)(void*, uint32_t, const void*, uint32_t);
    SendToPlayer originalSendToPlayer = nullptr;
    SendToAll originalSendToAll = nullptr;
    ReceivePacket originalReceivePacket = nullptr;

    void SendFinalPacket(void* object, uint32_t source, uint32_t target, bool broadcast, const void* packet, uint32_t length)
    {
        if (broadcast) originalSendToAll(object, source, packet, length);
        else originalSendToPlayer(object, source, target, packet, length);
    }
    void SendPlayerPacket(void* object, uint32_t source, uint32_t target, const void* packet, uint32_t length)
    { ColourMaps::SendNetworkPacket(object, source, target, false, packet, length, SendFinalPacket); }
    void SendAllPacket(void* object, uint32_t source, const void* packet, uint32_t length)
    { ColourMaps::SendNetworkPacket(object, source, 0, true, packet, length, SendFinalPacket); }

    // Both transports copy the packet before returning, just as they do with
    // the native serializer's stack buffer. Share these detours with network
    // computer teams so weapon schemes, lobby icons and AI launch settings coexist.
    void __fastcall SendPacketToPlayer(void* object, void*, uint32_t session,
        uint32_t player, const void* packet, uint32_t length)
    {
        SecretWeapons::NetworkPacket extended;
        NetworkTeams::StartPacket start;
        NetworkTeams::LobbyPacket lobby;
        ExtendedOptions::Packet options;
        if (SecretWeapons::ExtendNetworkPacket(packet, length, extended))
            SendPlayerPacket(object, session, player, extended.data(), static_cast<uint32_t>(extended.size()));
        else if (ExtendedOptions::ExtendPacket(packet, length, options))
            SendPlayerPacket(object, session, player, options.data(), static_cast<uint32_t>(options.size()));
        else if (NetworkTeams::ExtendStartPacket(packet, length, start))
            SendPlayerPacket(object, session, player, start.data(), static_cast<uint32_t>(start.size()));
        else if (const auto lobbyLength = NetworkTeams::ExtendLobbyPacket(packet, length, lobby))
            SendPlayerPacket(object, session, player, lobby.data(), lobbyLength);
        else
            SendPlayerPacket(object, session, player, packet, length);
    }

    void __fastcall SendPacketToAll(void* object, void*, uint32_t session,
        const void* packet, uint32_t length)
    {
        SecretWeapons::NetworkPacket extended;
        NetworkTeams::StartPacket start;
        NetworkTeams::LobbyPacket lobby;
        ExtendedOptions::Packet options;
        if (SecretWeapons::ExtendNetworkPacket(packet, length, extended))
            SendAllPacket(object, session, extended.data(), static_cast<uint32_t>(extended.size()));
        else if (ExtendedOptions::ExtendPacket(packet, length, options))
            SendAllPacket(object, session, options.data(), static_cast<uint32_t>(options.size()));
        else if (NetworkTeams::ExtendStartPacket(packet, length, start))
            SendAllPacket(object, session, start.data(), static_cast<uint32_t>(start.size()));
        else if (const auto lobbyLength = NetworkTeams::ExtendLobbyPacket(packet, length, lobby))
            SendAllPacket(object, session, lobby.data(), lobbyLength);
        else
            SendAllPacket(object, session, packet, length);
    }

    void ReceiveNativePacket(void* object, uint32_t sender, const void* packet, uint32_t length)
    { originalReceivePacket(object, sender, packet, length); }

    void __fastcall ReceiveLobbyPacket(void* object, void*, uint32_t sender,
        const void* packet, uint32_t length)
    {
        // Reliable receive (0x12bc2) advances past its sequence DWORD without
        // subtracting it from the reported length. Raw receive (0x12b6c) does
        // neither. Bound all extension reads by the actual payload length.
        const bool reliable = *reinterpret_cast<uint32_t*>(image + 0x188b14) == 1;
        uint32_t payloadLength = reliable ? (length >= 4 ? length - 4 : 0) : length;
        const uint32_t host = *reinterpret_cast<uint32_t*>(static_cast<BYTE*>(object) + 0x163c);
        if (!ColourMaps::ReceiveNetworkPacket(sender, host, packet, payloadLength))
        {
            // A consumed map reference can be the first packet. Attach its
            // preview after decoding; accepted native startup packets retain
            // their original receive path without preview setup in the middle.
            if (host && sender == host && payloadLength >= 4) ColourMaps::JoiningLobby(object);
            return;
        }
        length = payloadLength + (reliable ? 4 : 0);
        if (!ExtendedOptions::ReceivePacket(sender, host, packet, payloadLength)) return;
        NetworkTeams::ReceiveStartPacket(sender, host, packet, payloadLength);
        uint32_t packetType = 0;
        if (packet && payloadLength >= sizeof(packetType)) memcpy(&packetType, packet, sizeof(packetType));
        if (packetType == 14 && payloadLength < NetworkTeams::NativeStartPacketSize) return;
        const auto lobbySize = NetworkTeams::LobbyPacketSize(packetType);
        if (lobbySize && payloadLength < lobbySize) return;
        if (!SecretWeapons::IsNetworkPacket(packet, payloadLength))
        {
            if (lobbySize) NetworkTeams::SetJoiningLobby(object);
            // The native decoder consumes only the original option records.
            originalReceivePacket(object, sender, packet, packetType == 0x19 &&
                payloadLength >= ExtendedOptions::NativePacketSize ?
                static_cast<uint32_t>(ExtendedOptions::NativePacketSize) : length);
            if (lobbySize) NetworkTeams::ReceiveLobbyPacket(object, sender, packet, payloadLength);
            if (packetType == 5) ColourMaps::JoiningLobby(object);
            return;
        }
        SecretWeapons::ReceiveNetworkPacket(object, sender, host, packet, payloadLength, ReceiveNativePacket);
    }

bool InstallInImage(BYTE* frontendImage)
{
    image = frontendImage;
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return false;
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(image + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt->FileHeader.TimeDateStamp != 0x3587be19)
        return false;

    struct Hook
    {
        size_t rva;
        const char* signature;
        size_t signatureLength;
        void* detour;
        void** original;
    };
    const Hook hooks[] = {
        { 0x1e29a, "\x55\x8b\xec\x83\xec\x0c\x89\x4d\xf4", 9, reinterpret_cast<void*>(SendPacketToPlayer), reinterpret_cast<void**>(&originalSendToPlayer) },
        { 0x1e3c4, "\x55\x8b\xec\x83\xec\x0c\x89\x4d\xf4", 9, reinterpret_cast<void*>(SendPacketToAll), reinterpret_cast<void**>(&originalSendToAll) },
        { 0x39e41, "\x55\x8b\xec\x6a\xff\x68\x6d\xa0\x4e\x00", 10, reinterpret_cast<void*>(ReceiveLobbyPacket), reinterpret_cast<void**>(&originalReceivePacket) },
    };
    // Check every site before creating any hook; addresses are relative to this executable.
    for (const auto& hook : hooks)
        if (memcmp(image + hook.rva, hook.signature, hook.signatureLength) != 0)
            return false;
    size_t created = 0;
    for (const auto& hook : hooks)
    {
        if (MH_CreateHook(image + hook.rva, hook.detour, hook.original) != MH_OK)
            break;
        ++created;
    }
    const size_t hookCount = sizeof(hooks) / sizeof(hooks[0]);
    bool success = created == hookCount;
    if (success)
        for (const auto& hook : hooks)
            if (MH_QueueEnableHook(image + hook.rva) != MH_OK)
            {
                success = false;
                break;
            }
    if (success)
        success = MH_ApplyQueued() == MH_OK;
    if (!success)
        for (size_t i = 0; i < created; ++i)
        {
            MH_DisableHook(image + hooks[i].rva);
            MH_RemoveHook(image + hooks[i].rva);
        }
    return success;
}
}

bool Install()
{
    return InstallInImage(reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr)));
}
}
