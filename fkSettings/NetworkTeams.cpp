#define NOMINMAX
typedef struct IUnknown IUnknown;
#include <Windows.h>
#include <intrin.h>
#include <algorithm>
#include <array>
#include <cstring>
#include "include/MinHook.h"
#include "NetworkTeams.h"

namespace NetworkTeams
{
namespace
{
    constexpr size_t GameRva = 0x1a7698;
    constexpr size_t SavedTeamsRva = 0x19d720;
    constexpr size_t SavedTeamStride = 0x19c;
    constexpr size_t TeamStride = 0x108;
    constexpr size_t TeamOffset = 0x484;
    constexpr size_t HostListModeRva = 0x3264a;
    constexpr size_t WriteGameRva = 0x26e38;
    constexpr uint32_t StartType = 14;
    struct ComputerTeam
    {
        char name[17];
        unsigned char skill;
    };
    struct Extension
    {
        char magic[4];
        std::array<ComputerTeam, 6> teams;
    };
    static_assert(sizeof(Extension) == 112);
    BYTE* image = nullptr;
    bool enabled = false;
    std::array<ComputerTeam, 6> receivedTeams{};
    using WriteGame = int (__thiscall*)(void*, const char*);
    WriteGame originalWriteGame = nullptr;

    bool IsStartPacket(const void* packet, uint32_t length)
    {
        uint32_t type = 0;
        if (!packet || length < sizeof(type)) return false;
        memcpy(&type, packet, sizeof(type));
        return type == StartType;
    }

    std::array<ComputerTeam, 6> HostComputerTeams(BYTE* game)
    {
        std::array<ComputerTeam, 6> teams{};
        for (size_t slot = 0; slot < teams.size(); ++slot)
        {
            const BYTE* team = game + TeamOffset + slot * TeamStride;
            int savedIndex;
            memcpy(&savedIndex, game + 4 + slot * 4, sizeof(savedIndex));
            // The host retains a saved index only for its own teams. Remote
            // human teams have -1, even if their name matches a local AI team.
            if (!team[2] || savedIndex < 0 || savedIndex >= 100) continue;
            int skill;
            memcpy(&skill, image + SavedTeamsRva + savedIndex * SavedTeamStride, sizeof(skill));
            if (skill < 1 || skill > 100 || !memchr(team + 2, 0, 17)) continue;
            memcpy(teams[slot].name, team + 2, 17);
            teams[slot].skill = static_cast<unsigned char>(skill);
        }
        return teams;
    }

    bool ValidExtension(const Extension& extension)
    {
        if (memcmp(extension.magic, "FKA1", 4) != 0) return false;
        for (size_t i = 0; i < extension.teams.size(); ++i)
        {
            const auto& team = extension.teams[i];
            if (team.skill > 100 || !memchr(team.name, 0, sizeof(team.name)) ||
                (team.skill && !team.name[0])) return false;
            for (size_t j = 0; j < i; ++j)
                if (team.skill && extension.teams[j].skill &&
                    strcmp(team.name, extension.teams[j].name) == 0) return false;
        }
        return true;
    }

    int WriteNetworkGame(BYTE* game, const char* filename, bool host)
    {
        const auto teams = host ? HostComputerTeams(game) : receivedTeams;
        std::array<BYTE, 6> owners{};
        for (size_t slot = 0; slot < owners.size(); ++slot)
        {
            BYTE* team = game + TeamOffset + slot * TeamStride;
            owners[slot] = team[0];
            if (!team[2] || !memchr(team + 2, 0, 17)) continue;
            // Team order is randomized independently at launch. Match by name,
            // not slot, and use the engine's existing negative skill encoding.
            for (const auto& computer : teams)
                if (computer.skill && strcmp(reinterpret_cast<char*>(team + 2), computer.name) == 0)
                    team[0] = static_cast<BYTE>(-static_cast<int>(computer.skill));
        }
        // The native writer does not consult team ownership. Restore the lobby
        // owners even on a failed write or an exception during serialization.
        struct RestoreOwners
        {
            BYTE* game;
            const std::array<BYTE, 6>& owners;
            ~RestoreOwners()
            {
                for (size_t slot = 0; slot < owners.size(); ++slot)
                    game[TeamOffset + slot * TeamStride] = owners[slot];
            }
        } restore{ game, owners };
        return originalWriteGame(game, filename);
    }

    int __fastcall SaveGame(void* object, void*, const char* filename)
    {
        const auto caller = static_cast<BYTE*>(_ReturnAddress());
        if (object == image + GameRva && (caller == image + 0x35950 || caller == image + 0x3b3d5))
            return WriteNetworkGame(static_cast<BYTE*>(object), filename, caller == image + 0x35950);
        return originalWriteGame(object, filename);
    }

    bool SetHostListMode(BYTE mode)
    {
        BYTE* operand = image + HostListModeRva + 1;
        DWORD previous;
        if (!VirtualProtect(operand, 1, PAGE_EXECUTE_READWRITE, &previous)) return false;
        *operand = mode;
        FlushInstructionCache(GetCurrentProcess(), operand, 1);
        DWORD ignored;
        VirtualProtect(operand, 1, previous, &ignored);
        return true;
    }

    bool InstallInImage(BYTE* frontend)
    {
        auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(frontend);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 0x1000) return false;
        auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(frontend + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
            nt->FileHeader.TimeDateStamp != 0x3587be19 || nt->OptionalHeader.SizeOfImage != 0x5b8000) return false;
        // Validate both network launch callers too. Local games keep their
        // native controller handling and joiners keep the human-only team list.
        if (memcmp(frontend + HostListModeRva, "\x6a\x00\x8b\x4d\xb4\x81\xc1\xe8\x1c\x00\x00\xe8\x2c\x13\xfd\xff", 16) ||
            memcmp(frontend + WriteGameRva, "\x55\x8b\xec\x6a\xff\x68\x1b\x91\x4e\x00", 10) ||
            memcmp(frontend + 0x3594b, "\xe8\x53\xd2\xfc\xff", 5) ||
            memcmp(frontend + 0x3b3d0, "\xe8\xce\x77\xfc\xff", 5)) return false;
        image = frontend;
        if (MH_CreateHook(image + WriteGameRva, reinterpret_cast<void*>(SaveGame),
            reinterpret_cast<void**>(&originalWriteGame)) != MH_OK) return false;
        if (MH_EnableHook(image + WriteGameRva) != MH_OK || !SetHostListMode(1))
        {
            MH_DisableHook(image + WriteGameRva);
            MH_RemoveHook(image + WriteGameRva);
            return false;
        }
        receivedTeams = {};
        enabled = true;
        return true;
    }
}

bool Install()
{
    return InstallInImage(reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr)));
}

bool ExtendStartPacket(const void* packet, uint32_t length, StartPacket& extended)
{
    if (!enabled || length != NativeStartPacketSize || !IsStartPacket(packet, length)) return false;
    const Extension extension{ { 'F', 'K', 'A', '1' }, HostComputerTeams(image + GameRva) };
    // Ordinary human-only matches retain the native packet format.
    if (std::none_of(extension.teams.begin(), extension.teams.end(),
        [](const ComputerTeam& team) { return team.skill != 0; })) return false;
    memcpy(extended.data(), packet, NativeStartPacketSize);
    memcpy(extended.data() + NativeStartPacketSize, &extension, sizeof(extension));
    return true;
}

void ReceiveStartPacket(uint32_t sender, uint32_t host, const void* packet, uint32_t length)
{
    if (!enabled || sender != host || !IsStartPacket(packet, length)) return;
    receivedTeams = {}; // Never reuse AI settings from a previous start attempt.
    if (length != StartPacket{}.size()) return;
    Extension extension;
    memcpy(&extension, static_cast<const BYTE*>(packet) + NativeStartPacketSize, sizeof(extension));
    if (ValidExtension(extension)) receivedTeams = extension.teams;
}
}
