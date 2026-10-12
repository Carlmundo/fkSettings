#include "../fkSettings/DirectPlayCompat.cpp"
#include <cstdio>
#include <stdexcept>
#include "DirectPlayCompatTests.h"

namespace
{
    using DirectPlayCompatFixture::Require;

    void Patch(BYTE* target, const void* bytes, size_t length)
    {
        DWORD previous, ignored;
        Require(VirtualProtect(target, length, PAGE_EXECUTE_READWRITE, &previous) != FALSE, "prepare mapped test code");
        memcpy(target, bytes, length);
        FlushInstructionCache(GetCurrentProcess(), target, length);
        Require(VirtualProtect(target, length, previous, &ignored) != FALSE, "restore mapped test protection");
    }

    __declspec(naked) HRESULT __fastcall InvokeSend(void*, BYTE*)
    {
        __asm {
            push esi
            mov esi, ecx
            call edx
            pop esi
            ret
        }
    }

    HRESULT CatchNullTable(void* object, BYTE* entry)
    {
        __try { return InvokeSend(object, entry); }
        __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH)
        { return E_POINTER; }
    }

    void SendTests(BYTE* image)
    {
        namespace DP = DirectPlayCompat;
        const auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image);
        const auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(image + dos->e_lfanew);
        Require(nt->FileHeader.TimeDateStamp == 0x4802a0e1 && nt->OptionalHeader.SizeOfImage == 0x3b000,
            "fixture requires the reported XP SP3 DirectPlay build");
        BYTE* entry = image + DP::XpSendGuardRva;
        BYTE original[9], fallback[7], success[6], cleanup[3];
        memcpy(original, entry, sizeof(original));
        memcpy(fallback, image + 0x28f4b, sizeof(fallback));
        memcpy(success, image + 0x28f12, sizeof(success));
        memcpy(cleanup, image + 0x28f3b, sizeof(cleanup));

        // Isolate the actual table check and both branches from provider I/O.
        // The failure branch still executes XP's original mov esi,HRESULT/jmp.
        const BYTE returnSuccess[] = { 0xb8, 1, 0, 0, 0, 0xc3 }; // S_FALSE
        const BYTE returnFailure[] = { 0x8b, 0xc6, 0xc3 }; // mov eax,esi; ret
        Patch(image + 0x28f12, returnSuccess, sizeof(returnSuccess));
        Patch(image + 0x28f3b, returnFailure, sizeof(returnFailure));
        void* object[6]{};
        DWORD callbacks[21]{};
        Require(CatchNullTable(object, entry) == E_POINTER, "reproduce reported instruction's null-table access violation");
        object[5] = callbacks;
        Require(InvokeSend(object, entry) == HRESULT(0x887700fa), "native missing SendEx error");
        callbacks[20] = 1;
        Require(InvokeSend(object, entry) == S_FALSE, "native live SendEx continuation");

        Require(MH_Initialize() == MH_OK, "initialize send guard hooks");
        const BYTE invalid = 0x90;
        Patch(entry + 8, &invalid, 1);
        Require(!DP::InstallXpSendGuard(image) && entry[0] == original[0], "reject changed conditional branch without patching");
        Patch(entry + 8, original + 8, 1);
        Patch(image + 0x28f4b, &invalid, 1);
        Require(!DP::InstallXpSendGuard(image) && !memcmp(entry, original, sizeof(original)), "reject changed native error path without patching");
        Patch(image + 0x28f4b, fallback, sizeof(fallback));
        const DWORD timestamp = nt->FileHeader.TimeDateStamp;
        const DWORD unknownTimestamp = timestamp + 1;
        Patch(reinterpret_cast<BYTE*>(&nt->FileHeader.TimeDateStamp), &unknownTimestamp, sizeof(unknownTimestamp));
        Require(DP::InstallXpSendGuard(image) && !memcmp(entry, original, sizeof(original)), "other DirectPlay builds remain unchanged");
        Patch(reinterpret_cast<BYTE*>(&nt->FileHeader.TimeDateStamp), &timestamp, sizeof(timestamp));
        Require(DP::InstallXpSendGuard(nullptr), "absent DirectPlay module needs no guard");
        Require(DP::InstallXpSendGuard(image), "install detour on real XP instructions");
        object[5] = nullptr;
        Require(CatchNullTable(object, entry) == HRESULT(0x887700fa), "absent provider table follows native error branch");
        object[5] = callbacks;
        callbacks[20] = 0;
        Require(InvokeSend(object, entry) == HRESULT(0x887700fa), "guard preserves native missing callback behavior and flags");
        callbacks[20] = 1;
        Require(InvokeSend(object, entry) == S_FALSE, "guard preserves live provider continuation and flags");
        Require(MH_Uninitialize() == MH_OK && !memcmp(entry, original, sizeof(original)), "remove guard and restore native instructions");
        object[5] = nullptr;
        Require(CatchNullTable(object, entry) == E_POINTER, "restored instructions reproduce original null-table failure");
        Patch(image + 0x28f12, success, sizeof(success));
        Patch(image + 0x28f3b, cleanup, sizeof(cleanup));
        DP::xpSendContinue = DP::xpSendUnavailable = nullptr;
        puts("PASS: actual XP SendEx instructions, null-table failure and guard, original error/continuation branches, signature/version checks and restoration");
    }
}

int main(int argc, char** argv)
{
    try
    {
        Require(argc >= 2, "usage: DirectPlayCompatTests.exe <XP dplayx.dll> [dpwsockx_ipx.dll]");
        DirectPlayCompatFixture::Run(argc >= 3 ? argv[2] : nullptr);
        // Map code privately, without running DllMain, imports or any provider.
        const HMODULE module = LoadLibraryExA(argv[1], nullptr, DONT_RESOLVE_DLL_REFERENCES);
        Require(module != nullptr, "map XP DirectPlay without executing it");
        SendTests(reinterpret_cast<BYTE*>(module));
        FreeLibrary(module);
        return 0;
    }
    catch (const std::exception& error)
    {
        fprintf(stderr, "FAIL: %s\n", error.what());
        MH_Uninitialize();
        return 1;
    }
}
