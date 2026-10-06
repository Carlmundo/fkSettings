#include "Hooks.h"
#include "WaterFix.h"
#include <cstring>
#include <vector>

#if !defined(_M_IX86)
#error The Worms 2 water preview hook requires a Win32 build.
#endif

namespace WaterFix
{
namespace
{
    std::vector<char> mainDirectory;
    bool installed = false;
    DWORD returnAddress = 0;

    // fkWaterFix restores the working directory captured when the DLL loads.
    // GetCurrentDirectoryA is available on XP and needs no filesystem runtime.
    bool CaptureDirectory()
    {
        DWORD capacity = GetCurrentDirectoryA(0, nullptr);
        while (capacity)
        {
            mainDirectory.resize(capacity);
            const DWORD length = GetCurrentDirectoryA(capacity, mainDirectory.data());
            if (!length)
                break;
            if (length < capacity)
                return true;
            capacity = length;
        }
        mainDirectory.clear();
        return false;
    }

    void __stdcall RestoreDirectory()
    {
        if (!mainDirectory.empty() && !SetCurrentDirectoryA(mainDirectory.data()))
            OutputDebugStringA("fkSettings: could not restore the water preview directory.\n");
    }

    __declspec(naked) void AfterColorLoad()
    {
        __asm
        {
            pushfd
            pushad
            call RestoreDirectory
            popad
            popfd
            jmp dword ptr [returnAddress]
        }
    }

    bool WriteJump(BYTE* site, const BYTE* bytes)
    {
        DWORD previous;
        if (!VirtualProtect(site, 5, PAGE_EXECUTE_READWRITE, &previous))
            return false;
        memcpy(site, bytes, 5);
        FlushInstructionCache(GetCurrentProcess(), site, 5);
        DWORD ignored;
        VirtualProtect(site, 5, previous, &ignored);
        return true;
    }

    bool InstallInImage(HMODULE module)
    {
        if (installed)
            return true;
        if (!CaptureDirectory())
            return false;

        // Keep the two original fkWaterFix signatures. Both must match before
        // changing code, so unknown executables (or an existing fix) are skipped.
        const DWORD site = Hooks::scanPattern2("ColorTxtAfterLoad",
            "E9 38 FE FF FF 68 40 51 5B 00 B9 B8 4F 5B 00 E8 93 2E 00 00 6A 08 E8 B4 8B F7 FF 89 45 B4 C6 45 FC 04 83 7D B4 00 74 0D 8B 4D B4 E8 C4 8E F7 FF 89 45 94 EB 07 C7 45 94 00 00 00 00 8B 4D 94 89 4D B0 C6 45 FC 00 8B 55 A8 8B 45 B0",
            0, module);
        const DWORD continuation = Hooks::scanPattern2("ColorTxtAfterLoadReturn",
            "8B 4D E4 83 C1 01 89 4D E4 8B 55 E4 3B 55 DC 0F 8D B3 01 00 00 8B 45 E4 50 8B 4D A8 E8 31 87 F7 FF 89 45 D4 8B 4D D4 51",
            0, module);
        if (!site || !continuation)
            return false;

        BYTE* target = reinterpret_cast<BYTE*>(site);
        // The intercepted instruction is already a jump to the continuation.
        // Reject an unrelated pair of matches rather than changing control flow.
        DWORD displacement;
        memcpy(&displacement, target + 1, sizeof(displacement));
        if (site + 5 + displacement != continuation)
            return false;

        BYTE jump[5] = { 0xe9 };
        displacement = reinterpret_cast<DWORD>(&AfterColorLoad) - (site + 5);
        memcpy(jump + 1, &displacement, sizeof(displacement));
        returnAddress = continuation;
        if (!WriteJump(target, jump))
        {
            returnAddress = 0;
            return false;
        }
        installed = true;
        return true;
    }
}

bool Install()
{
    try
    {
        return InstallInImage(GetModuleHandleA(nullptr));
    }
    catch (...)
    {
        OutputDebugStringA("fkSettings: water preview hook installation failed.\n");
        return false;
    }
}

}
