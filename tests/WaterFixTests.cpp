#include <stdexcept>
#include "../fkSettings/WaterFix.cpp"
#include <cstdio>
#include <string>

static void Check(bool value, const char* message)
{
    if (!value)
        throw std::runtime_error(message);
}

static DWORD testJumpSite;
static DWORD registers[7];
static DWORD flagsBefore, flagsAfter;

// Run the production naked hook with live registers/flags, then return through
// a RET temporarily placed at the real frontend continuation in a private view.
__declspec(naked) static void InvokeJump()
{
    __asm
    {
        pushfd
        pushad
        mov eax, 11111111h
        mov ebx, 22222222h
        mov ecx, 33333333h
        mov edx, 44444444h
        mov esi, 55555555h
        mov edi, 66666666h
        mov ebp, 77777777h
        stc
        pushfd
        pop dword ptr [flagsBefore]
        call dword ptr [testJumpSite]
        mov dword ptr [registers], eax
        mov dword ptr [registers + 4], ebx
        mov dword ptr [registers + 8], ecx
        mov dword ptr [registers + 12], edx
        mov dword ptr [registers + 16], esi
        mov dword ptr [registers + 20], edi
        mov dword ptr [registers + 24], ebp
        pushfd
        pop dword ptr [flagsAfter]
        popad
        popfd
        ret
    }
}

static std::string CurrentDirectory()
{
    std::vector<char> path(GetCurrentDirectoryA(0, nullptr));
    Check(!path.empty() && GetCurrentDirectoryA(static_cast<DWORD>(path.size()), path.data()), "read current directory");
    return path.data();
}

static void WaterHookTests(const char* path)
{
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    Check(file != INVALID_HANDLE_VALUE, "open frontend");
    HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr);
    Check(mapping != nullptr, "map frontend without running it");
    BYTE* frontend = static_cast<BYTE*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));
    Check(frontend != nullptr, "view frontend");
    BYTE* site = reinterpret_cast<BYTE*>(Hooks::scanPattern2("WaterFixTestSite",
        "E9 38 FE FF FF 68 40 51 5B 00 B9 B8 4F 5B 00", 0, reinterpret_cast<HMODULE>(frontend)));
    Check(site != nullptr, "locate native water jump");
    BYTE original[5];
    memcpy(original, site, sizeof(original));
    DWORD displacement;
    memcpy(&displacement, original + 1, sizeof(displacement));
    BYTE* continuation = reinterpret_cast<BYTE*>(reinterpret_cast<DWORD>(site) + 5 + displacement);
    DWORD previous, ignored;

    // Check rejection before installation: the production hook stays installed
    // for the process lifetime and has no removal path.
    for (BYTE* corrupt : { site + 5, continuation })
    {
        Check(VirtualProtect(corrupt, 1, PAGE_EXECUTE_READWRITE, &previous) != FALSE, "prepare unsupported signature fixture");
        const BYTE before = *corrupt;
        *corrupt ^= 1;
        Check(!WaterFix::InstallInImage(reinterpret_cast<HMODULE>(frontend)), "modified water signature rejected");
        Check(memcmp(site, original, sizeof(original)) == 0, "rejection does not modify code");
        *corrupt = before;
        VirtualProtect(corrupt, 1, previous, &ignored);
    }

    Check(WaterFix::InstallInImage(reinterpret_cast<HMODULE>(frontend)), "install merged water hook on supplied frontend");
    Check(WaterFix::returnAddress == reinterpret_cast<DWORD>(continuation), "hook retains native jump destination");
    Check(memcmp(site, original, sizeof(original)) != 0, "water hook changes original jump");
    Check(WaterFix::InstallInImage(reinterpret_cast<HMODULE>(frontend)), "repeated install is harmless");

    Check(VirtualProtect(continuation, 1, PAGE_EXECUTE_READWRITE, &previous) != FALSE, "prepare continuation fixture");
    const BYTE originalContinuation = *continuation;
    *continuation = 0xc3;
    FlushInstructionCache(GetCurrentProcess(), continuation, 1);
    testJumpSite = reinterpret_cast<DWORD>(site);
    const std::string start = CurrentDirectory();
    for (const char* alternate : { "Release", "tests", "fkSettings" })
    {
        Check(SetCurrentDirectoryA(alternate) != FALSE, "simulate terrain editor changing directory");
        InvokeJump();
        Check(CurrentDirectory() == start, "production hook restores startup directory");
        for (size_t i = 0; i < _countof(registers); ++i)
            Check(registers[i] == 0x11111111u * (i + 1), "water hook preserves native registers");
        Check(flagsBefore == flagsAfter, "water hook preserves native flags");
    }
    *continuation = originalContinuation;
    FlushInstructionCache(GetCurrentProcess(), continuation, 1);
    VirtualProtect(continuation, 1, previous, &ignored);
    Check(memcmp(site, original, sizeof(original)) != 0, "water hook remains installed");
    UnmapViewOfFile(frontend);
    CloseHandle(mapping);
    CloseHandle(file);
    puts("PASS: real frontend signatures, directory restoration, registers/flags, safe rejection and persistent installation");
}

int main(int argc, char** argv)
{
    const std::string start = CurrentDirectory();
    try
    {
        Check(argc > 1, "supply frontend.exe path");
        WaterHookTests(argv[1]);
        return 0;
    }
    catch (const std::exception& error)
    {
        SetCurrentDirectoryA(start.c_str());
        fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
