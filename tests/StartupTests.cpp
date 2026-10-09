#define NOMINMAX
#include "../fkSettings/WaterFix.cpp"
#include "../fkSettings/SecretWeapons.cpp"
#include "../fkSettings/FrontendNetwork.cpp"
#include "../fkSettings/ColourMaps.cpp"
#include "../fkSettings/ExtendedOptions.cpp"
#include "../fkSettings/NetworkTeams.cpp"
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
        std::vector<double> scans, hooks;
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
            LARGE_INTEGER start, afterScans, afterHooks;
            QueryPerformanceCounter(&start);
            Check(Hooks::scanPattern2("Trackbar", "E8 55 4E 09 00 8B 45 F0 83 C0", 0, module) != 0, "trackbar signature");
            Check(Hooks::scanPattern2("TrackbarLabel", "E8 06 4E 09 00 8D 4D A4 E8 13 3C", 0, module) != 0, "trackbar label signature");
            Check(Hooks::scanPattern2("CheckboxLabel", "E8 E6 4E 09 00 6A 00 8D 4D A4 E8 F1", 0, module) != 0, "checkbox label signature");
            Check(Hooks::scanPattern2("Checkbox", "E8 BF 4E 09 00 8D 4D A4 E8 CC", 0, module) != 0, "checkbox signature");
            WaterFix::installed = false;
            Check(WaterFix::InstallInImage(module), "water signatures and jump");
            QueryPerformanceCounter(&afterScans);
            Check(MH_Initialize() == MH_OK, "initialize MinHook");
            Check(SecretWeapons::InstallInImage(image), "install secret weapon hooks");
            Check(FrontendNetwork::InstallInImage(image), "install shared frontend packet hooks");
            Check(ExtendedOptions::InstallInImage(image), "install extended option hooks");
            Check(NetworkTeams::InstallInImage(image), "install network team hooks");
            QueryPerformanceCounter(&afterHooks);
            Check(image[0x975b0] == 0xe9 && image[0x97780] == 0xe9 && image[NetworkTeams::WriteGameRva] == 0xe9,
                "all feature groups activated");
            Check(MH_Uninitialize() == MH_OK, "remove test hooks");
            Check(image[0x975b0] == 0x56 && image[0x97780] == 0x8b && image[NetworkTeams::WriteGameRva] == 0x55,
                "all feature groups restored");
            if (run > 0)
            {
                scans.push_back(Milliseconds(start, afterScans, frequency));
                hooks.push_back(Milliseconds(afterScans, afterHooks, frequency));
            }
            UnmapViewOfFile(image);
            CloseHandle(mapping);
            CloseHandle(file);
        }
        std::sort(scans.begin(), scans.end());
        std::sort(hooks.begin(), hooks.end());
        printf("PASS: combined startup installation/restoration; median scans/water %.3f ms, hooks %.3f ms (8 warm runs)\n",
            scans[scans.size() / 2], hooks[hooks.size() / 2]);
        return 0;
    }
    catch (const std::exception& error)
    {
        fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
