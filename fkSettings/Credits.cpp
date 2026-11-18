#define NOMINMAX
typedef struct IUnknown IUnknown;
#include <windows.h>
#include <commctrl.h>
#include <cstring>
#include "include/MinHook.h"
#include "Credits.h"

#pragma comment(lib, "comctl32.lib")

namespace Credits
{
namespace
{
    using NativeInitialize = void (__thiscall*)(void*);
    NativeInitialize originalInitialize = nullptr;
    const size_t InitializeRva = 0x71ba;

    LRESULT CALLBACK TextProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
        UINT_PTR id, DWORD_PTR)
    {
        if (message == WM_NCDESTROY)
            RemoveWindowSubclass(window, TextProc, id);
        const LRESULT result = DefSubclassProc(window, message, wparam, lparam);
        // The dialog manager uses this flag to select all text when assigning
        // focus, including after WM_INITDIALOG. Keep all other keyboard behavior
        // and explicit selections (mouse, Shift+arrows, Ctrl+A) unchanged.
        return message == WM_GETDLGCODE ? result & ~DLGC_HASSETSEL : result;
    }

    bool Attach(HWND text)
    {
        if (!text || !SetWindowSubclass(text, TextProc, 1, 0)) return false;
        SendMessageA(text, EM_SETSEL, 0, 0);
        return true;
    }

    void __fastcall Initialize(void* object, void*)
    {
        originalInitialize(object);
        const HWND dialog = *reinterpret_cast<HWND*>(static_cast<BYTE*>(object) + 0x1c);
        // Native initialization creates control 1000 and streams About.rtf.
        // Subclass before OnInitDialog returns and Windows assigns focus.
        if (!Attach(GetDlgItem(dialog, 1000)))
            OutputDebugStringA("fkSettings: could not prevent credits automatic selection.\n");
    }

    bool InstallInImage(BYTE* frontend)
    {
        if (!frontend) return false;
        const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(frontend);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
        const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(frontend + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
            nt->FileHeader.TimeDateStamp != 0x3587be19 || nt->OptionalHeader.SizeOfImage < 0x73ab) return false;
        // Validate the function prologue and its credits-specific control setup.
        // Skip the relocated SEH address in the prologue.
        if (memcmp(frontend + InitializeRva, "\x55\x8b\xec\x6a\xff\x68", 6) ||
            memcmp(frontend + 0x71d2, "\x81\xec\xac\x00\x00\x00\x89\x8d\x50\xff\xff\xff", 12) ||
            memcmp(frontend + 0x7234, "\x68\xe8\x03\x00\x00", 5) ||
            memcmp(frontend + 0x7244, "\x68\x04\x00\xa0\x40", 5)) return false;

        void* target = frontend + InitializeRva;
        if (MH_CreateHook(target, reinterpret_cast<void*>(Initialize),
            reinterpret_cast<void**>(&originalInitialize)) != MH_OK) return false;
        if (MH_QueueEnableHook(target) == MH_OK && MH_ApplyQueued() == MH_OK) return true;
        MH_DisableHook(target);
        MH_RemoveHook(target);
        return false;
    }
}

bool Install()
{
    return InstallInImage(reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr)));
}
}
