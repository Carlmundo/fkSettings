#define NOMINMAX
#include "../fkSettings/WaterFix.cpp"
#include "../fkSettings/SecretWeapons.cpp"
#include "../fkSettings/FrontendNetwork.cpp"
#include "../fkSettings/ColourMaps.cpp"
#include "../fkSettings/ExtendedOptions.cpp"
#include "../fkSettings/NetworkTeams.cpp"
#include "../fkSettings/WeaponTabOrder.h"
#include <algorithm>
#include <cstdio>
#include <stdexcept>

static void Check(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

static double Milliseconds(LARGE_INTEGER start, LARGE_INTEGER end, LARGE_INTEGER frequency)
{
    return 1000.0 * (end.QuadPart - start.QuadPart) / frequency.QuadPart;
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 2, "Usage: StartupTests.exe frontend.exe");
        LARGE_INTEGER frequency;
        Check(QueryPerformanceFrequency(&frequency) != FALSE, "performance counter");
        std::vector<double> scans, hooks, colourMaps;
        const size_t colourMapSites[] = {
            0x105f7, 0x1378d, 0x42a44, 0x7896a, 0x18841, 0x77c0e, 0xa349, 0x4468b,
            0x5c51d, 0x46f09, 0x277c4, 0x3233d, 0x611a6, 0x634c7, 0x336f1, 0x62214,
            0x34294, 0x61865, 0x1e32f, 0x358a4, 0x61a81, 0x144ed, 0xa0b70
        };
        for (int run = 0; run < 9; ++run)
        {
            HANDLE file = CreateFileA(argv[1], GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
            Check(file != INVALID_HANDLE_VALUE, "open frontend");
            HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr);
            Check(mapping != nullptr, "map frontend without running it");
            BYTE* image = static_cast<BYTE*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));
            Check(image != nullptr, "view frontend");
            const HMODULE module = reinterpret_cast<HMODULE>(image);
            // SEC_IMAGE leaves these absolute operands at their preferred base.
            // Match the operands the loader supplies in a relocated frontend.
            const size_t defaults[] = { 0x9cdc, 0x9ff8, 0xa5d4, 0x31cd9, 0x392c0, 0x586a3, 0x5899e, 0x59611, 0x59e3d };
            for (size_t site : defaults)
            {
                DWORD previous;
                Check(VirtualProtect(image + site, 17, PAGE_EXECUTE_READWRITE, &previous) != FALSE, "relocate default operands");
                *reinterpret_cast<uint32_t*>(image + site + 6) = reinterpret_cast<uint32_t>(image + 0x186358);
                *reinterpret_cast<uint32_t*>(image + site + 11) = reinterpret_cast<uint32_t>(image + ExtendedOptions::OptionsRva);
                VirtualProtect(image + site, 17, previous, &previous);
            }
            DWORD previous;
            Check(VirtualProtect(image + 0x592aa, 4, PAGE_EXECUTE_READWRITE, &previous) != FALSE, "relocate repeat swings operand");
            *reinterpret_cast<uint32_t*>(image + 0x592aa) = reinterpret_cast<uint32_t>(image + 0x18640c);
            VirtualProtect(image + 0x592aa, 4, previous, &previous);
            std::array<std::array<BYTE, 8>, sizeof(colourMapSites) / sizeof(colourMapSites[0])> originalColourMaps{};
            for (size_t i = 0; i < originalColourMaps.size(); ++i)
                memcpy(originalColourMaps[i].data(), image + colourMapSites[i], originalColourMaps[i].size());
            LARGE_INTEGER start, afterScans, beforeColourMaps, afterHooks;
            QueryPerformanceCounter(&start);
            Check(Hooks::scanPattern2("Trackbar", "E8 55 4E 09 00 8B 45 F0 83 C0", 0, module) != 0, "trackbar signature");
            Check(Hooks::scanPattern2("TrackbarLabel", "E8 06 4E 09 00 8D 4D A4 E8 13 3C", 0, module) != 0, "trackbar label signature");
            Check(Hooks::scanPattern2("CheckboxLabel", "E8 E6 4E 09 00 6A 00 8D 4D A4 E8 F1", 0, module) != 0, "checkbox label signature");
            Check(Hooks::scanPattern2("Checkbox", "E8 BF 4E 09 00 8D 4D A4 E8 CC", 0, module) != 0, "checkbox signature");
            const DWORD weaponLayoutEnd = Hooks::scanPattern2("WeaponLayoutEnd", TabOrder::LayoutEndPattern, 0, module);
            Check(weaponLayoutEnd != 0 && weaponLayoutEnd + 5 + *reinterpret_cast<int32_t*>(weaponLayoutEnd + 1) ==
                reinterpret_cast<DWORD>(image + 0xcf5b5), "weapon layout completion calls native SetScrollSizes");
            WaterFix::installed = false;
            Check(WaterFix::InstallInImage(module), "water signatures and jump");
            QueryPerformanceCounter(&afterScans);
            Check(MH_Initialize() == MH_OK, "initialize MinHook");
            Check(SecretWeapons::InstallInImage(image), "install secret weapon hooks");
            Check(FrontendNetwork::InstallInImage(image), "install shared frontend packet hooks");
            Check(ExtendedOptions::InstallInImage(image), "install extended option hooks");
            Check(NetworkTeams::InstallInImage(image), "install network team hooks");
            QueryPerformanceCounter(&beforeColourMaps);
            Check(ColourMaps::InstallInImage(image), "install colour map hooks");
            QueryPerformanceCounter(&afterHooks);
            Check(image[0x975b0] == 0xe9 && image[0x97780] == 0xe9 && image[NetworkTeams::WriteGameRva] == 0xe9,
                "all feature groups activated");
            Check(ColourMaps::enabled && ColourMaps::directSend != nullptr, "colour map networking enabled");
            for (const auto site : colourMapSites)
                Check(image[site] == 0xe9, "all colour map hooks activated");
            Check(MH_Uninitialize() == MH_OK, "remove test hooks");
            Check(image[0x975b0] == 0x56 && image[0x97780] == 0x8b && image[NetworkTeams::WriteGameRva] == 0x55,
                "all feature groups restored");
            for (size_t i = 0; i < originalColourMaps.size(); ++i)
                Check(memcmp(image + colourMapSites[i], originalColourMaps[i].data(), originalColourMaps[i].size()) == 0,
                    "all colour map hook bytes restored");
            if (run > 0)
            {
                scans.push_back(Milliseconds(start, afterScans, frequency));
                hooks.push_back(Milliseconds(afterScans, afterHooks, frequency));
                colourMaps.push_back(Milliseconds(beforeColourMaps, afterHooks, frequency));
            }
            UnmapViewOfFile(image);
            CloseHandle(mapping);
            CloseHandle(file);
        }
        std::sort(scans.begin(), scans.end());
        std::sort(hooks.begin(), hooks.end());
        std::sort(colourMaps.begin(), colourMaps.end());
        printf("PASS: combined startup installation/restoration; median scans/water %.3f ms, hooks %.3f ms, colour maps %.3f ms (8 warm runs)\n",
            scans[scans.size() / 2], hooks[hooks.size() / 2], colourMaps[colourMaps.size() / 2]);
        return 0;
    }
    catch (const std::exception& error)
    {
        fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
