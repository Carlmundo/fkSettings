#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace ExtendedOptions
{
    constexpr size_t OptionCount = 27;
    // Explicit file/packet byte indexes. Renumber only for intentional format changes.
    enum class OptionIndex : size_t
    {
        GodMode = 0,
        HighJump = 1,
        SheepHeaven = 2,
        SuperShopperCrates = 3,
        ExtendedFusesHerds = 4,
        UtilitiesDontEndTurn = 5,
        WeaponsDontEndTurn = 6,
        LossOfControlDoesntEndTurn = 7,
        WormSelectAfterMovement = 8,
        LowGravity = 9,
        PersistentRope = 10,
        RapidPlay = 11,
        IndestructibleTerrain = 12,
        InvisibleTerrain = 13,
        FastCrates = 14,
        CrateSpy = 15,
        CrateLimit = 16,
        CrateRate = 17,
        SuicideBomber = 18,
        AquaSheep = 19,
        InstantMines = 20,
        HerdDynamite = 21,
        HerdMine = 22,
        HerdMingVase = 23,
        HerdSheep = 24,
        DisableBackflip = 25,
        DisableUnlockedAim = 26,
    };
    constexpr size_t ToIndex(OptionIndex index) { return static_cast<size_t>(index); }
    constexpr size_t NativePacketSize = 0x84;
    using Packet = std::array<unsigned char, NativePacketSize + 4 + OptionCount>;
    using Read = size_t (__cdecl*)(void*, size_t, size_t, void*);
    using Write = size_t (__cdecl*)(const void*, size_t, size_t, void*);

    // Install after SecretWeapons (shared CRT) and FrontendNetwork (lobby hooks).
    bool Install();
    void SetLanguage(const std::string& language);
    void ReadScheme(void* buffer, size_t size, size_t count, void* stream, size_t result, Read read);
    bool WriteScheme(const void* buffer, size_t size, size_t count, void* stream, size_t result, Write write);
    bool WriteLaunchData(void* stream, size_t size, size_t count, size_t result);
    bool ExtendPacket(const void* packet, uint32_t length, Packet& extended);
    // Returns false for a truncated native options packet, so it can be dropped.
    bool ReceivePacket(uint32_t sender, uint32_t host, const void* packet, uint32_t length);
}
