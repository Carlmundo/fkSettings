#define NOMINMAX
typedef struct IUnknown IUnknown;
#include <windows.h>
#include <cstddef>
#include <cwchar>
#include <cstring>
#include "include/MinHook.h"
#include "DirectPlayCompat.h"

namespace DirectPlayCompat
{
namespace
{
    // Prefixes of DPSP_SPCALLBACKS and SPINITDATA, documented in
    // https://github.com/wine-mirror/wine/blob/master/include/wine/dplaysp.h.
    // These are provider callbacks, not IDirectPlay COM vtable entries.
    struct DeletePlayerData { DWORD player, flags; void* directPlaySp; };
    using DeletePlayer = HRESULT (WINAPI*)(DeletePlayerData*);
    struct ProviderCallbacks
    {
        DWORD size, version;
        FARPROC enumSessions, reply, send, addPlayerToGroup, close;
        FARPROC createGroup, createPlayer, deleteGroup;
        DeletePlayer deletePlayer;
    };
    struct ProviderInitData { ProviderCallbacks* callbacks; };
    using ProviderInit = HRESULT (WINAPI*)(ProviderInitData*);
    using ResolveExport = FARPROC (WINAPI*)(HMODULE, LPCSTR);
    ResolveExport originalResolveExport = nullptr;
    PVOID volatile originalProviderInit = nullptr;

    HRESULT WINAPI DeletePlayerWithoutResources(DeletePlayerData*)
    {
        // This provider has no DeletePlayer implementation or per-player
        // cleanup. DirectPlay still owns its copied SP player data. XP calls
        // this optional callback without a null check when updating a player.
        return S_OK;
    }
    HRESULT WINAPI InitializeProvider(ProviderInitData* data)
    {
        const auto initialize = reinterpret_cast<ProviderInit>(InterlockedCompareExchangePointer(&originalProviderInit, nullptr, nullptr));
        if (!initialize) return E_FAIL;
        const HRESULT result = initialize(data);
        if (SUCCEEDED(result) && data && data->callbacks &&
            data->callbacks->size >= sizeof(ProviderCallbacks) && !data->callbacks->deletePlayer)
        {
            data->callbacks->deletePlayer = DeletePlayerWithoutResources;
        }
        return result;
    }
    bool IsIpxProvider(HMODULE module)
    {
        wchar_t path[MAX_PATH]{};
        const DWORD length = GetModuleFileNameW(module, path, MAX_PATH);
        if (!length || length >= MAX_PATH) return false;
        const wchar_t* name = wcsrchr(path, L'\\');
        return _wcsicmp(name ? name + 1 : path, L"dpwsockx_ipx.dll") == 0;
    }
    FARPROC AdaptProviderExport(FARPROC address, bool ipxProvider)
    {
        if (!ipxProvider || !address || address == reinterpret_cast<FARPROC>(InitializeProvider)) return address;
        InterlockedExchangePointer(&originalProviderInit, reinterpret_cast<void*>(address));
        return reinterpret_cast<FARPROC>(InitializeProvider);
    }
    FARPROC WINAPI GetProviderExport(HMODULE module, LPCSTR name)
    {
        const FARPROC address = originalResolveExport(module, name);
        // Ordinals are not strings. All other modules and exports pass through.
        if (!address || reinterpret_cast<ULONG_PTR>(name) <= 0xffff || strcmp(name, "SPInit")) return address;
        const DWORD error = GetLastError();
        const FARPROC adapted = AdaptProviderExport(address, IsIpxProvider(module));
        SetLastError(error);
        return adapted;
    }
}
bool Install()
{
#ifdef _M_IX86
    static_assert(offsetof(ProviderCallbacks, deletePlayer) == 0x28 && sizeof(ProviderCallbacks) == 0x2c, "DirectPlay provider ABI");
    void* target = nullptr;
    if (MH_CreateHookApiEx(L"kernel32", "GetProcAddress", reinterpret_cast<void*>(GetProviderExport),
        reinterpret_cast<void**>(&originalResolveExport), &target) != MH_OK) return false;
    if (MH_EnableHook(target) == MH_OK) return true;
    MH_RemoveHook(target);
#endif
    return false;
}
}
