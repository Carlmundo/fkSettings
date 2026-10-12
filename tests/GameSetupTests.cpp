#include "../fkSettings/GameSetup.cpp"
#include <cstdio>
#include <stdexcept>
#include <vector>

static void Check(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}

static void Patch(BYTE* target, const void* bytes, size_t length)
{
    DWORD previous, ignored;
    Check(VirtualProtect(target, length, PAGE_EXECUTE_READWRITE, &previous) != FALSE, "prepare mapped test code");
    memcpy(target, bytes, length);
    FlushInstructionCache(GetCurrentProcess(), target, length);
    VirtualProtect(target, length, previous, &ignored);
}

// Supply the native setup control's stack local, then execute its actual
// add/remove recalculation through the eight-worm clamp in the mapped image.
__declspec(naked) static void __fastcall Recalculate(void*, void*, BYTE*)
{
    __asm {
        push ebp
        mov ebp, esp
        sub esp, 0ch
        mov [ebp-0ch], ecx
        call dword ptr [ebp+8]
        mov esp, ebp
        pop ebp
        ret 4
    }
}

__declspec(naked) static size_t __fastcall Serialize(void*, void*, BYTE*)
{
    __asm {
        push ebp
        mov ebp, esp
        sub esp, 48h
        mov [ebp-48h], ecx
        mov [ebp-38h], edx
        call dword ptr [ebp+8]
        mov esp, ebp
        pop ebp
        ret 4
    }
}

static size_t __cdecl Write(const void* bytes, size_t size, size_t count, FILE* file)
{
    return fwrite(bytes, size, count, file);
}

static void SetupTests(BYTE* image)
{
    using Maximum = int (__thiscall*)(void*);
    using SetWorms = void (__thiscall*)(void*, int);
    const auto maximum = reinterpret_cast<Maximum>(image + 0x2764c);
    const auto setWorms = reinterpret_cast<SetWorms>(image + 0x2769f);
    std::array<BYTE, 0xcf0> game{};
    std::array<BYTE, 0x640> control{};
    game[0x483] = 6;
    Check(maximum(game.data()) == 3, "native six-team limit starts at three worms each");
    for (size_t site = 0; site < 3; ++site)
    {
        const auto& corrupt = GameSetup::LimitSites[site];
        const BYTE invalid = 0x90, restore = image[corrupt.rva + 5];
        Patch(image + corrupt.rva + 5, &invalid, 1);
        Check(!GameSetup::InstallInImage(image), "reject modified calculation signature");
        for (const auto& other : GameSetup::LimitSites)
            Check(image[other.rva + 1] == 18, "signature rejection changes no limits");
        Patch(image + corrupt.rva + 5, &restore, 1);
    }
    Check(GameSetup::InstallInImage(image), "install 48-worm setup");
    Check(GameSetup::InstallInImage(image), "installation is idempotent");
    Check(!memcmp(image + 0x2728a, "\x83\x7d\xfc\x06", 4), "game still has six team slots");
    const size_t ends[] = { 0x78e1a, 0x7927b };
    const BYTE ret = 0xc3;
    for (auto end : ends) Patch(image + end, &ret, 1);
    for (int teams = 0; teams <= 6; ++teams)
    {
        game[0x483] = static_cast<BYTE>(teams);
        Check(maximum(game.data()) == 8, "zero through six selected teams allow eight worms each");
        for (int requested : { 1, 4, 8, 9 })
        {
            setWorms(game.data(), requested);
            for (int team = 0; team < 6; ++team)
                Check(game[0x484 + team * 0x108 + 0x106] == (requested > 8 ? 8 : requested),
                    "native setter retains requested count and caps every team at eight");
        }
        if (!teams) continue; // Native control skips division when no teams remain.
        memcpy(control.data() + 0xb4, &teams, sizeof(teams));
        for (const auto& site : { GameSetup::LimitSites[1], GameSetup::LimitSites[2] })
        {
            Recalculate(control.data(), nullptr, image + site.rva);
            int limit = 0;
            memcpy(&limit, control.data() + 0x62c, sizeof(limit));
            Check(limit == 8, "adding/removing teams retains eight-worm setup control range");
        }
    }

    // Exercise the native writer's fwrite call and exact size/pointer operands.
    // End this isolated block before it enters the MFC error/cleanup paths.
    const auto writeDisplacement = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(Write) -
        reinterpret_cast<uintptr_t>(image + 0x26f8f));
    Patch(image + 0x26f8b, &writeDisplacement, sizeof(writeDisplacement));
    Patch(image + 0x26f92, &ret, 1); // Return after native add esp,10h.
    game[0x483] = 6;
    for (int team = 0; team < 6; ++team)
    {
        BYTE* record = game.data() + 0x484 + team * 0x108;
        sprintf_s(reinterpret_cast<char*>(record + 2), 17, "Team %d", team + 1);
        record[0] = team == 5 ? static_cast<BYTE>(-100) : static_cast<BYTE>(team % 3);
        for (int worm = 0; worm < 8; ++worm)
        {
            BYTE* target = record + 0x66 + worm * 20;
            const unsigned short health = static_cast<unsigned short>(100 + team * 8 + worm);
            memcpy(target, &health, 2);
            sprintf_s(reinterpret_cast<char*>(target + 2), 18, "Team %d Worm %d", team + 1, worm + 1);
        }
    }
    FILE* file = nullptr;
    Check(tmpfile_s(&file) == 0 && file, "open temporary game.dat fixture");
    Check(Serialize(game.data(), file, image + 0x26f78) == 0xcd4, "native writer serializes full game.dat payload");
    Check(ftell(file) == 0xcd4, "48 worms retain the native file size");
    rewind(file);
    std::array<BYTE, 0xcd4> saved{};
    Check(fread(saved.data(), 1, saved.size(), file) == saved.size(), "read serialized game.dat");
    fclose(file);
    Check(!memcmp(saved.data(), game.data() + 0x1c, saved.size()), "all 48 names, health, counts and controllers survive serialization");
    Check(saved[0x467] == 6 && saved[0x468 + 5 * 0x108 + 0x106] == 8,
        "sixth team and eighth worm fit in native game.dat");
    puts("PASS: native setup ranges, add/remove recalculation, six-team/eight-worm caps, 48-worm serialization, signatures and repeated installation");
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 2, "Usage: GameSetupTests.exe frontend.exe");
        HANDLE file = CreateFileA(argv[1], GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        Check(file != INVALID_HANDLE_VALUE, "open frontend");
        HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr);
        Check(mapping != nullptr, "map frontend without running it");
        BYTE* image = static_cast<BYTE*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));
        Check(image != nullptr, "view frontend image");
        SetupTests(image);
        UnmapViewOfFile(image);
        CloseHandle(mapping);
        CloseHandle(file);
        return 0;
    }
    catch (const std::exception& error)
    {
        fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
