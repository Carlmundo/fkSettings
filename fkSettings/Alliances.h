#pragma once
#include <array>
#include <string>
#include <vector>

namespace Alliances
{
    struct MatchTeam
    {
        std::string name;
        unsigned char colour;
        unsigned char wins = 0; // Score before this round, matched by name after sorting.
    };
    using Teams = std::vector<MatchTeam>;

    // Local controls provide their current launch context to the shared hooks.
    // Returning true from shuffle consumes a local launch, including later rounds.
    struct LocalIntegration
    {
        bool (*launch)(const void*, std::array<unsigned char, 7>&, bool&) = nullptr;
        bool (*shuffle)(void*) = nullptr;
    };
    inline LocalIntegration& LocalCallbacks() { static LocalIntegration callbacks; return callbacks; }
    // Preserve the native caller through the shared thunk and CPU writer hooks.
    inline const void*& WriterCaller() { static const void* caller = nullptr; return caller; }

    bool Install();
    void* CreateColourImages(int size);
    void InterleaveTeams(void* game, const Teams& teams);
}
