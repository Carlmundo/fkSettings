#define NOMINMAX
typedef struct IUnknown IUnknown;
#include <Windows.h>
#include <array>
#include <cstring>
#include "GameSetup.h"

namespace GameSetup
{
namespace
{
    constexpr BYTE MaxWorms = 6 * 8;
    struct LimitSite
    {
        size_t rva;
        const char* signature;
        size_t length;
    };
    // Each native calculation is min(8, totalWorms / selectedTeams).
    // The setup control has separate recalculations when adding/removing teams.
    const LimitSite LimitSites[] = {
        { 0x27678, "\xb8\x12\x00\x00\x00\x99\xf7\xf9\x89\x45\xfc\x83\x7d\xfc\x08", 15 },
        { 0x78dec, "\xb8\x12\x00\x00\x00\x99\xf7\xb9\xb4\x00\x00\x00\x8b\x55\xf4\x89\x82\x2c\x06\x00\x00", 21 },
        { 0x7924d, "\xb8\x12\x00\x00\x00\x99\xf7\xb9\xb4\x00\x00\x00\x8b\x55\xf4\x89\x82\x2c\x06\x00\x00", 21 },
    };

    bool InstallInImage(BYTE* image)
    {
        if (!image) return false;
        const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 0x1000) return false;
        const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(image + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
            nt->FileHeader.TimeDateStamp != 0x3587be19 || nt->OptionalHeader.SizeOfImage < 0x79262) return false;
        // Validate every site before changing any code. Accept our own patch so
        // repeated initialization is harmless, but reject other modifications.
        for (const auto& site : LimitSites)
            if (image[site.rva] != 0xb8 || (image[site.rva + 1] != 18 && image[site.rva + 1] != MaxWorms) ||
                memcmp(image + site.rva + 2, site.signature + 2, site.length - 2)) return false;

        std::array<DWORD, 3> protections{};
        size_t prepared = 0;
        for (const auto& site : LimitSites)
        {
            if (!VirtualProtect(image + site.rva + 1, 1, PAGE_EXECUTE_READWRITE, &protections[prepared])) break;
            ++prepared;
        }
        const bool success = prepared == protections.size();
        if (success)
            for (const auto& site : LimitSites)
            {
                image[site.rva + 1] = MaxWorms;
                FlushInstructionCache(GetCurrentProcess(), image + site.rva + 1, 1);
            }
        // On a protection failure no operands have changed.
        while (prepared)
        {
            --prepared;
            DWORD ignored;
            VirtualProtect(image + LimitSites[prepared].rva + 1, 1, protections[prepared], &ignored);
        }
        return success;
    }
}

bool Install()
{
    return InstallInImage(reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr)));
}
}
