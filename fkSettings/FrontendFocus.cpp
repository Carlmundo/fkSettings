#define NOMINMAX
typedef struct IUnknown IUnknown;
#include <windows.h>
#include <intrin.h>
#include <cstring>
#include "include/MinHook.h"
#include "FrontendFocus.h"

namespace FrontendFocus
{
namespace
{
    using NativeWindow = void (__thiscall*)(void*);
    using NativeWait = DWORD (WINAPI*)(HANDLE, DWORD);
    NativeWindow originalMinimize = nullptr, originalRestore = nullptr;
    NativeWait originalWait = nullptr;
    BYTE* image = nullptr;
    volatile LONG launchThread = 0;
    void* launchWindow = nullptr;

    // The local ShellExecute engine-exit event, four DirectPlay engine-exit
    // events, and the CRT's _P_WAIT process wait used by IPX launches.
    const size_t EngineWaitSites[] = { 0x27828, 0x35b66, 0x3b43c, 0x61d27, 0x640d1, 0xa739e };

    DWORD WaitForEngine(HANDLE handle)
    {
        for (;;)
        {
            const DWORD result = MsgWaitForMultipleObjects(1, &handle, FALSE, INFINITE, QS_SENDMESSAGE);
            if (result != WAIT_OBJECT_0 + 1) return result;
            // PeekMessage services incoming SendMessage calls even when it
            // does not retrieve a posted message. Leave commands, timers,
            // input and WM_QUIT for the frontend's normal MFC message loop:
            // dispatching them here could start another match or destroy UI
            // while the native launch stack still owns it.
            MSG message;
            PeekMessageA(&message, nullptr, 0, 0, PM_NOREMOVE);
        }
    }

    DWORD WaitForCaller(HANDLE handle, DWORD timeout, const void* caller)
    {
        if (timeout == INFINITE && GetCurrentThreadId() ==
            static_cast<DWORD>(InterlockedCompareExchange(&launchThread, 0, 0)) && launchWindow)
            for (size_t site : EngineWaitSites)
                if (caller == image + site + 6) return WaitForEngine(handle);
        return originalWait(handle, timeout);
    }

    DWORD WINAPI Wait(HANDLE handle, DWORD timeout)
    {
        return WaitForCaller(handle, timeout, _ReturnAddress());
    }

    void __fastcall Minimize(void* object, void*)
    {
        launchWindow = object;
        InterlockedExchange(&launchThread, static_cast<LONG>(GetCurrentThreadId()));
        originalMinimize(object);
    }

    void __fastcall Restore(void* object, void*)
    {
        const bool returningFromGame = launchWindow == object;
        if (returningFromGame)
        {
            launchWindow = nullptr;
            InterlockedExchange(&launchThread, 0);
        }
        originalRestore(object);
        if (!returningFromGame) return;
        const HWND window = *reinterpret_cast<HWND*>(static_cast<BYTE*>(object) + 0x1c);
        if (IsWindow(window))
        {
            if (IsIconic(window)) ShowWindow(window, SW_RESTORE);
            SetForegroundWindow(window);
        }
    }

    bool InstallInImage(BYTE* frontend)
    {
        if (!frontend) return false;
        const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(frontend);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
        const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(frontend + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
            nt->FileHeader.TimeDateStamp != 0x3587be19 || nt->OptionalHeader.SizeOfImage < 0x1bc008) return false;
        if (memcmp(frontend + 0x25b80, "\x55\x8b\xec\x51\x89\x4d\xfc\x8b\x45\xfc\x05\xa4\x00\x00\x00", 15) ||
            memcmp(frontend + 0x25ba6, "\x55\x8b\xec\x51\x89\x4d\xfc\x6a\x09\x8b\x4d\xfc", 12)) return false;
        for (size_t site : EngineWaitSites)
        {
            if (memcmp(frontend + site, "\xff\x15", 2)) return false;
            DWORD operand;
            memcpy(&operand, frontend + site + 2, sizeof(operand));
            // SEC_IMAGE fixtures retain preferred-base operands; a loaded
            // frontend has relocated operands. Accept either representation.
            if (operand != reinterpret_cast<DWORD>(frontend + 0x1bc004) &&
                operand != nt->OptionalHeader.ImageBase + 0x1bc004) return false;
        }

        void* waitTarget = nullptr;
        if (MH_CreateHookApiEx(L"kernel32", "WaitForSingleObject", reinterpret_cast<void*>(Wait),
            reinterpret_cast<void**>(&originalWait), &waitTarget) != MH_OK) return false;
        struct Hook { void* target; void* detour; void** original; };
        const Hook hooks[] = {
            { frontend + 0x25b80, reinterpret_cast<void*>(Minimize), reinterpret_cast<void**>(&originalMinimize) },
            { frontend + 0x25ba6, reinterpret_cast<void*>(Restore), reinterpret_cast<void**>(&originalRestore) }
        };
        size_t created = 0;
        for (const auto& hook : hooks)
        {
            if (MH_CreateHook(hook.target, hook.detour, hook.original) != MH_OK) break;
            ++created;
        }
        image = frontend;
        launchWindow = nullptr;
        InterlockedExchange(&launchThread, 0);
        bool ready = created == 2 && MH_QueueEnableHook(waitTarget) == MH_OK;
        for (const auto& hook : hooks)
            if (ready) ready = MH_QueueEnableHook(hook.target) == MH_OK;
        if (ready && MH_ApplyQueued() == MH_OK) return true;
        MH_DisableHook(waitTarget);
        MH_RemoveHook(waitTarget);
        for (size_t i = 0; i < created; ++i)
        {
            MH_DisableHook(hooks[i].target);
            MH_RemoveHook(hooks[i].target);
        }
        return false;
    }
}

bool Install()
{
    return InstallInImage(reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr)));
}
}
