#define NOMINMAX
typedef struct IUnknown IUnknown;
#include <Windows.h>
#include <commctrl.h>
#include <intrin.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <new>
#include "include/MinHook.h"
#include "NetworkTeams.h"

namespace NetworkTeams
{
namespace
{
    constexpr size_t GameRva = 0x1a7698;
    constexpr size_t SavedTeamsRva = 0x19d720;
    constexpr size_t SavedTeamStride = 0x19c;
    constexpr size_t TeamStride = 0x108;
    constexpr size_t TeamOffset = 0x484;
    constexpr size_t HostListModeRva = 0x3264a;
    constexpr size_t WriteGameRva = 0x26e38;
    constexpr size_t AddTreeTeamRva = 0x4b233;
    constexpr size_t ReceiveRoundPacketRva = 0x63ac6;
    constexpr size_t HostLaunchCallerRva = 0x35950;
    constexpr size_t JoinLaunchCallerRva = 0x3b3d5;
    constexpr size_t HostNextRoundCallerRva = 0x61b13;
    constexpr size_t JoinNextRoundCallerRva = 0x6406a;
    constexpr size_t InitResultsRva = 0x881d6;
    constexpr size_t RefreshResultsRva = 0x88336;
    constexpr size_t HostResultsCallerRva = 0x61240;
    constexpr size_t JoinResultsCallerRva = 0x6351e;
    constexpr uint32_t StartType = 14;
    struct ComputerTeam
    {
        char name[17];
        unsigned char skill;
    };
    struct Extension
    {
        char magic[4];
        std::array<ComputerTeam, 6> teams;
    };
    static_assert(sizeof(Extension) == 112, "Extension must be 112 bytes");
    BYTE* image = nullptr;
    bool enabled = false;
    std::array<ComputerTeam, 6> receivedTeams{};
    std::array<ComputerTeam, 6> lobbyTeams{};
    BYTE* joiningTree = nullptr;
    HWND joiningWindow = nullptr;
    using WriteGame = int (__thiscall*)(void*, const char*);
    WriteGame originalWriteGame = nullptr;
    using AddTreeTeam = void (__thiscall*)(void*, const BYTE*, int);
    AddTreeTeam originalAddTreeTeam = nullptr;
    using ReceiveRoundPacket = void (__thiscall*)(void*, uint32_t, const void*, uint32_t);
    ReceiveRoundPacket originalReceiveRoundPacket = nullptr;
    using ResultsControl = void (__thiscall*)(void*);
    ResultsControl originalInitResults = nullptr;
    ResultsControl originalRefreshResults = nullptr;

    struct TeamImages
    {
        HIMAGELIST native;
        HIMAGELIST expanded;
        int firstComputer;
    };

    LRESULT CALLBACK ReleaseTeamImages(HWND window, UINT message, WPARAM wParam, LPARAM lParam,
        UINT_PTR id, DWORD_PTR reference)
    {
        if (message == WM_NCDESTROY)
        {
            if (window == joiningWindow)
            {
                lobbyTeams = {};
                joiningWindow = nullptr;
                joiningTree = nullptr;
            }
            auto images = reinterpret_cast<TeamImages*>(reference);
            TreeView_SetImageList(window, images->native, TVSIL_NORMAL);
            RemoveWindowSubclass(window, ReleaseTeamImages, id);
            ImageList_Destroy(images->expanded);
            delete images;
        }
        return DefSubclassProc(window, message, wParam, lParam);
    }

    TeamImages* GetTeamImages(HWND window)
    {
        DWORD_PTR reference = 0;
        return GetWindowSubclass(window, ReleaseTeamImages, 1, &reference) ?
            reinterpret_cast<TeamImages*>(reference) : nullptr;
    }

    TeamImages* PrepareTeamImages(HWND window, HBITMAP computers)
    {
        HIMAGELIST native = TreeView_GetImageList(window, TVSIL_NORMAL);
        int nativeWidth, nativeHeight;
        BITMAP bitmap{};
        if (!native || !computers || !ImageList_GetIconSize(native, &nativeWidth, &nativeHeight) ||
            !GetObject(computers, sizeof(bitmap), &bitmap) || bitmap.bmWidth < 128 || bitmap.bmHeight != 16)
            return nullptr;
        const int count = ImageList_GetImageCount(native);
        const int width = (std::max)(32, nativeWidth), height = (std::max)(16, nativeHeight);
        HIMAGELIST expanded = ImageList_Create(width, height, ILC_COLOR32 | ILC_MASK, count + 3, 0);
        HDC screen = GetDC(nullptr);
        HDC destination = CreateCompatibleDC(screen), source = CreateCompatibleDC(screen);
        HBITMAP strip = CreateCompatibleBitmap(screen, width * (count + 3), height);
        ReleaseDC(nullptr, screen);
        bool success = expanded && destination && source && strip;
        if (success)
        {
            HGDIOBJ oldDestination = SelectObject(destination, strip), oldSource = SelectObject(source, computers);
            const COLORREF transparent = RGB(255, 0, 255); // Bitmap 244's native colour key.
            RECT area{ 0, 0, width * (count + 3), height };
            HBRUSH background = CreateSolidBrush(transparent);
            success = background && FillRect(destination, &area, background);
            if (background) DeleteObject(background);
            // Preserve every native sprite and its index. Only this tree gets
            // wider cells; the frontend's shared bitmap 340 list stays intact.
            for (int icon = 0; success && icon < count; ++icon)
                success = ImageList_Draw(native, icon, destination, icon * width + (width - nativeWidth) / 2,
                    (height - nativeHeight) / 2, ILD_TRANSPARENT) != FALSE;
            for (int level = 0; success && level < 3; ++level)
                success = BitBlt(destination, (count + level) * width + (width - 32) / 2,
                    (height - 16) / 2, 32, 16, source, (level + 1) * 32, 0, SRCCOPY) != FALSE;
            SelectObject(source, oldSource);
            SelectObject(destination, oldDestination);
            if (success) success = ImageList_AddMasked(expanded, strip, transparent) == 0;
        }
        if (strip) DeleteObject(strip);
        if (source) DeleteDC(source);
        if (destination) DeleteDC(destination);
        auto images = success ? new (std::nothrow) TeamImages{ native, expanded, count } : nullptr;
        if (!images || !SetWindowSubclass(window, ReleaseTeamImages, 1, reinterpret_cast<DWORD_PTR>(images)))
        {
            delete images;
            if (expanded) ImageList_Destroy(expanded);
            return nullptr;
        }
        TreeView_SetImageList(window, expanded, TVSIL_NORMAL);
        return images;
    }

    int JoiningTeamSkill(const char* name)
    {
        for (const auto& computer : lobbyTeams)
            if (computer.skill && strcmp(name, computer.name) == 0) return computer.skill;
        return 0;
    }

    void UpdateTeamIcon(BYTE* tree, size_t slot, int skill)
    {
        HWND window = *reinterpret_cast<HWND*>(tree + 0x1c);
        TeamImages* images = GetTeamImages(window);
        if (!images && skill)
        {
            HBITMAP computers = static_cast<HBITMAP>(LoadImageA(reinterpret_cast<HINSTANCE>(image),
                MAKEINTRESOURCEA(244), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
            images = PrepareTeamImages(window, computers);
            if (computers) DeleteObject(computers);
        }
        if (!images) return;
        TVITEMA item{};
        item.mask = TVIF_IMAGE | TVIF_SELECTEDIMAGE;
        item.hItem = *reinterpret_cast<HTREEITEM*>(tree + 0x90 + slot * 4);
        if (!item.hItem) return;
        if (!skill)
        {
            if (!SendMessageA(window, TVM_GETITEMA, 0, reinterpret_cast<LPARAM>(&item))) return;
            // A withdrawn/invalid CPU setting must not leave a stale CPU icon.
            // Preserve native human images that were never replaced.
            if (item.iImage < images->firstComputer || item.iImage >= images->firstComputer + 3) return;
            item.iImage = item.iSelectedImage = 5;
        }
        else item.iImage = item.iSelectedImage = images->firstComputer + (skill > 66 ? 2 : skill > 33 ? 1 : 0);
        SendMessageA(window, TVM_SETITEMA, 0, reinterpret_cast<LPARAM>(&item));
    }

    void __fastcall AddComputerTeamRow(void* object, void*, const BYTE* team, int savedIndex)
    {
        originalAddTreeTeam(object, team, savedIndex);
        if (!team || !team[2] || !memchr(team + 2, 0, 17)) return;
        BYTE* tree = static_cast<BYTE*>(object);
        int skill = 0;
        if (tree == joiningTree)
            skill = JoiningTeamSkill(reinterpret_cast<const char*>(team + 2));
        else if (savedIndex >= 0 && savedIndex < 100)
            memcpy(&skill, image + SavedTeamsRva + savedIndex * SavedTeamStride, sizeof(skill));
        if (skill < 1 || skill > 100) return;
        for (size_t slot = 0; slot < 6; ++slot)
        {
            const char* name = reinterpret_cast<char*>(tree + 0x40e + slot * TeamStride);
            // The native builder chooses its own free row, independent of the
            // game slot. Update only the row it actually created for this team.
            if (*reinterpret_cast<int*>(tree + 0x40 + slot * 4) != savedIndex ||
                !memchr(name, 0, 17) || strcmp(name, reinterpret_cast<const char*>(team + 2)) != 0) continue;
            UpdateTeamIcon(tree, slot, skill);
            break;
        }
    }

    bool IsStartPacket(const void* packet, uint32_t length)
    {
        uint32_t type = 0;
        if (!packet || length < sizeof(type)) return false;
        memcpy(&type, packet, sizeof(type));
        return type == StartType;
    }

    std::array<ComputerTeam, 6> HostComputerTeams(BYTE* game)
    {
        std::array<ComputerTeam, 6> teams{};
        for (size_t slot = 0; slot < teams.size(); ++slot)
        {
            const BYTE* team = game + TeamOffset + slot * TeamStride;
            int savedIndex;
            memcpy(&savedIndex, game + 4 + slot * 4, sizeof(savedIndex));
            // The host retains a saved index only for its own teams. Remote
            // human teams have -1, even if their name matches a local AI team.
            if (!team[2] || savedIndex < 0 || savedIndex >= 100) continue;
            int skill;
            memcpy(&skill, image + SavedTeamsRva + savedIndex * SavedTeamStride, sizeof(skill));
            if (skill < 1 || skill > 100 || !memchr(team + 2, 0, 17)) continue;
            memcpy(teams[slot].name, team + 2, 17);
            teams[slot].skill = static_cast<unsigned char>(skill);
        }
        return teams;
    }

    bool ValidExtension(const Extension& extension)
    {
        if (memcmp(extension.magic, "FKA1", 4) != 0) return false;
        for (size_t i = 0; i < extension.teams.size(); ++i)
        {
            const auto& team = extension.teams[i];
            if (team.skill > 100 || !memchr(team.name, 0, sizeof(team.name)) ||
                (team.skill && !team.name[0])) return false;
            for (size_t j = 0; j < i; ++j)
                if (team.skill && extension.teams[j].skill &&
                    strcmp(team.name, extension.teams[j].name) == 0) return false;
        }
        return true;
    }

    LRESULT CALLBACK ReleaseResultsContext(HWND window, UINT message, WPARAM wParam, LPARAM lParam,
        UINT_PTR id, DWORD_PTR)
    {
        if (message == WM_NCDESTROY) RemoveWindowSubclass(window, ReleaseResultsContext, id);
        return DefSubclassProc(window, message, wParam, lParam);
    }

    void __fastcall RefreshComputerResults(void* object, void*)
    {
        originalRefreshResults(object);
        HWND window = *reinterpret_cast<HWND*>(static_cast<BYTE*>(object) + 0x1c);
        DWORD_PTR mode = 0;
        if (!GetWindowSubclass(window, ReleaseResultsContext, 2, &mode)) return;
        const auto teams = mode == 1 ? HostComputerTeams(image + GameRva) : receivedTeams;
        const int count = ListView_GetItemCount(window);
        for (int row = 0; row < count; ++row)
        {
            LVITEMA item{};
            item.mask = LVIF_PARAM;
            item.iItem = row;
            if (!SendMessageA(window, LVM_GETITEMA, 0, reinterpret_cast<LPARAM>(&item)) ||
                item.lParam < 0 || item.lParam >= 6) continue;
            const char* name = reinterpret_cast<char*>(image + GameRva + TeamOffset + item.lParam * TeamStride + 2);
            if (!name[0] || !memchr(name, 0, 17)) continue;
            char rowName[18]{};
            LVITEMA text{};
            text.pszText = rowName;
            text.cchTextMax = sizeof(rowName);
            SendMessageA(window, LVM_GETITEMTEXTA, row, reinterpret_cast<LPARAM>(&text));
            if (strcmp(rowName, name) != 0) continue;
            for (const auto& computer : teams)
                if (computer.skill >= 1 && computer.skill <= 100 && strcmp(computer.name, name) == 0)
                {
                    // This list already contains bitmap 244's 32x16 sprites:
                    // image 0 is human, and images 1/2/3 are the CPU levels.
                    item.mask = LVIF_IMAGE;
                    item.iImage = 1 + (computer.skill > 66 ? 2 : computer.skill > 33 ? 1 : 0);
                    SendMessageA(window, LVM_SETITEMA, 0, reinterpret_cast<LPARAM>(&item));
                    break;
                }
        }
    }

    void InitializeResults(void* object, DWORD_PTR mode)
    {
        HWND window = *reinterpret_cast<HWND*>(static_cast<BYTE*>(object) + 0x1c);
        // Record the source on this control before native initialization calls
        // its refresh method. Later timer refreshes use the same context.
        if (mode) SetWindowSubclass(window, ReleaseResultsContext, 2, mode);
        else RemoveWindowSubclass(window, ReleaseResultsContext, 2);
        originalInitResults(object);
    }

    void __fastcall InitComputerResults(void* object, void*)
    {
        const auto caller = static_cast<BYTE*>(_ReturnAddress());
        const DWORD_PTR mode = caller == image + HostResultsCallerRva ? 1 :
            caller == image + JoinResultsCallerRva ? 2 : 0;
        InitializeResults(object, mode);
    }

    int WriteNetworkGame(BYTE* game, const char* filename, bool host)
    {
        const auto teams = host ? HostComputerTeams(game) : receivedTeams;
        std::array<BYTE, 6> owners{};
        for (size_t slot = 0; slot < owners.size(); ++slot)
        {
            BYTE* team = game + TeamOffset + slot * TeamStride;
            owners[slot] = team[0];
            if (!team[2] || !memchr(team + 2, 0, 17)) continue;
            // Team order is randomized independently at launch. Match by name,
            // not slot, and use the engine's existing negative skill encoding.
            for (const auto& computer : teams)
                if (computer.skill && strcmp(reinterpret_cast<char*>(team + 2), computer.name) == 0)
                    team[0] = static_cast<BYTE>(-static_cast<int>(computer.skill));
        }
        // The native writer does not consult team ownership. Restore the lobby
        // owners even on a failed write or an exception during serialization.
        struct RestoreOwners
        {
            BYTE* game;
            const std::array<BYTE, 6>& owners;
            ~RestoreOwners()
            {
                for (size_t slot = 0; slot < owners.size(); ++slot)
                    game[TeamOffset + slot * TeamStride] = owners[slot];
            }
        } restore{ game, owners };
        return originalWriteGame(game, filename);
    }

    int __fastcall SaveGame(void* object, void*, const char* filename)
    {
        const auto caller = static_cast<BYTE*>(_ReturnAddress());
        const bool host = caller == image + HostLaunchCallerRva || caller == image + HostNextRoundCallerRva;
        const bool join = caller == image + JoinLaunchCallerRva || caller == image + JoinNextRoundCallerRva;
        if (object == image + GameRva && (host || join))
            return WriteNetworkGame(static_cast<BYTE*>(object), filename, host);
        return originalWriteGame(object, filename);
    }

    void __fastcall ReceiveNextRoundPacket(void* object, void*, uint32_t sender,
        const void* packet, uint32_t length)
    {
        // Subsequent rounds use the results dialog's separate dispatcher. Its
        // constructor resolves the host player ID into +0xa8. Refresh the AI
        // settings before the native start handler writes the next game.dat.
        const bool reliable = *reinterpret_cast<uint32_t*>(image + 0x188b14) == 1;
        const uint32_t payloadLength = reliable ? (length >= 4 ? length - 4 : 0) : length;
        const uint32_t host = *reinterpret_cast<uint32_t*>(static_cast<BYTE*>(object) + 0xa8);
        ReceiveStartPacket(sender, host, packet, payloadLength);
        if (IsStartPacket(packet, payloadLength) && payloadLength < NativeStartPacketSize) return;
        originalReceiveRoundPacket(object, sender, packet, length);
    }

    bool SetHostListMode(BYTE mode)
    {
        BYTE* operand = image + HostListModeRva + 1;
        DWORD previous;
        if (!VirtualProtect(operand, 1, PAGE_EXECUTE_READWRITE, &previous)) return false;
        *operand = mode;
        FlushInstructionCache(GetCurrentProcess(), operand, 1);
        DWORD ignored;
        VirtualProtect(operand, 1, previous, &ignored);
        return true;
    }

    bool InstallInImage(BYTE* frontend)
    {
        auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(frontend);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 0x1000) return false;
        auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(frontend + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
            nt->FileHeader.TimeDateStamp != 0x3587be19) return false;
        // Validate first-round and subsequent network launch callers. Local games keep their
        // native controller handling and joiners keep the human-only team list.
        if (memcmp(frontend + HostListModeRva, "\x6a\x00\x8b\x4d\xb4\x81\xc1\xe8\x1c\x00\x00\xe8\x2c\x13\xfd\xff", 16) ||
            memcmp(frontend + WriteGameRva, "\x55\x8b\xec\x6a\xff\x68\x1b\x91\x4e\x00", 10) ||
            memcmp(frontend + AddTreeTeamRva, "\x55\x8b\xec\x83\xec\x0c\x89\x4d\xf4", 9) ||
            memcmp(frontend + ReceiveRoundPacketRva, "\x55\x8b\xec\x6a\xff\x68\x17\xcf\x4e\x00", 10) ||
            memcmp(frontend + InitResultsRva, "\x55\x8b\xec\x6a\xff\x68\xfb\x02\x4f\x00", 10) ||
            memcmp(frontend + RefreshResultsRva, "\x55\x8b\xec\x6a\xff\x68\x0e\x03\x4f\x00", 10) ||
            memcmp(frontend + 0x6123b, "\xe8\xec\x1c\xfa\xff", 5) ||
            memcmp(frontend + 0x63519, "\xe8\x0e\xfa\xf9\xff", 5) ||
            memcmp(frontend + 0x3594b, "\xe8\x53\xd2\xfc\xff", 5) ||
            memcmp(frontend + 0x3b3d0, "\xe8\xce\x77\xfc\xff", 5) ||
            memcmp(frontend + 0x61b0e, "\xe8\x90\x10\xfa\xff", 5) ||
            memcmp(frontend + 0x64065, "\xe8\x39\xeb\xf9\xff", 5) ||
            memcmp(frontend + 0x6319d, "\x68\xd2\x77\x5a\x00\xb9\x98\x76\x5a\x00\xe8\xbc\x02\xfa\xff\x8b\x4d\xf0\x89\x81\xa8\x00\x00\x00", 24)) return false;
        image = frontend;
        const struct Hook
        {
            size_t rva;
            void* detour;
            void** original;
        } hooks[] = {
            { WriteGameRva, reinterpret_cast<void*>(SaveGame), reinterpret_cast<void**>(&originalWriteGame) },
            { AddTreeTeamRva, reinterpret_cast<void*>(AddComputerTeamRow), reinterpret_cast<void**>(&originalAddTreeTeam) },
            { ReceiveRoundPacketRva, reinterpret_cast<void*>(ReceiveNextRoundPacket), reinterpret_cast<void**>(&originalReceiveRoundPacket) },
            { InitResultsRva, reinterpret_cast<void*>(InitComputerResults), reinterpret_cast<void**>(&originalInitResults) },
            { RefreshResultsRva, reinterpret_cast<void*>(RefreshComputerResults), reinterpret_cast<void**>(&originalRefreshResults) },
        };
        size_t created = 0;
        for (const auto& hook : hooks)
        {
            if (MH_CreateHook(image + hook.rva, hook.detour, hook.original) != MH_OK) break;
            ++created;
        }
        bool success = created == sizeof(hooks) / sizeof(hooks[0]);
        if (success)
            for (const auto& hook : hooks)
                if (MH_EnableHook(image + hook.rva) != MH_OK)
                {
                    success = false;
                    break;
                }
        if (!success || !SetHostListMode(1))
        {
            for (size_t i = 0; i < created; ++i)
            {
                MH_DisableHook(image + hooks[i].rva);
                MH_RemoveHook(image + hooks[i].rva);
            }
            return false;
        }
        receivedTeams = {};
        lobbyTeams = {};
        joiningTree = nullptr;
        joiningWindow = nullptr;
        enabled = true;
        return true;
    }
}

bool Install()
{
    return InstallInImage(reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr)));
}

bool ExtendStartPacket(const void* packet, uint32_t length, StartPacket& extended)
{
    if (!enabled || length != NativeStartPacketSize || !IsStartPacket(packet, length)) return false;
    const Extension extension{ { 'F', 'K', 'A', '1' }, HostComputerTeams(image + GameRva) };
    // Ordinary human-only matches retain the native packet format.
    if (std::none_of(extension.teams.begin(), extension.teams.end(),
        [](const ComputerTeam& team) { return team.skill != 0; })) return false;
    memcpy(extended.data(), packet, NativeStartPacketSize);
    memcpy(extended.data() + NativeStartPacketSize, &extension, sizeof(extension));
    return true;
}

void ReceiveStartPacket(uint32_t sender, uint32_t host, const void* packet, uint32_t length)
{
    if (!enabled || sender != host || !IsStartPacket(packet, length)) return;
    receivedTeams = {}; // Never reuse AI settings from a previous start attempt.
    if (length != StartPacket{}.size()) return;
    Extension extension;
    memcpy(&extension, static_cast<const BYTE*>(packet) + NativeStartPacketSize, sizeof(extension));
    if (ValidExtension(extension)) receivedTeams = extension.teams;
}

uint32_t LobbyPacketSize(uint32_t type)
{
    switch (type)
    {
    case 5: return NativeSnapshotPacketSize;
    case 10: return NativeAddTeamPacketSize;
    case 11: return 0x1c;
    default: return 0;
    }
}

uint32_t ExtendLobbyPacket(const void* packet, uint32_t length, LobbyPacket& extended)
{
    uint32_t type = 0;
    if (!enabled || !packet || length < sizeof(type)) return 0;
    memcpy(&type, packet, sizeof(type));
    if ((type != 5 && type != 10) || length != LobbyPacketSize(type)) return 0;
    const Extension extension{ { 'F', 'K', 'A', '1' }, HostComputerTeams(image + GameRva) };
    if (std::none_of(extension.teams.begin(), extension.teams.end(),
        [](const ComputerTeam& team) { return team.skill != 0; })) return 0;
    memcpy(extended.data(), packet, length);
    memcpy(extended.data() + length, &extension, sizeof(extension));
    return length + sizeof(extension);
}

void SetJoiningLobby(void* object)
{
    if (!enabled) return;
    BYTE* tree = static_cast<BYTE*>(object) + 0x3648;
    if (tree != joiningTree)
    {
        lobbyTeams = {};
        joiningWindow = nullptr;
        joiningTree = tree;
    }
}

void ReceiveLobbyPacket(void* object, uint32_t sender, const void* packet, uint32_t length)
{
    if (!enabled || !packet || length < sizeof(uint32_t)) return;
    uint32_t type;
    memcpy(&type, packet, sizeof(type));
    const uint32_t nativeSize = LobbyPacketSize(type);
    if (!nativeSize || length < nativeSize) return;
    // Run after the native dispatcher: the initial type 5 snapshot establishes
    // the host at +0x163c before rebuilding the joining dialog's Teams tree.
    if (sender != *reinterpret_cast<uint32_t*>(static_cast<BYTE*>(object) + 0x163c)) return;
    SetJoiningLobby(object);
    if (type == 11)
    {
        const char* name = static_cast<const char*>(packet) + 8;
        if (!memchr(name, 0, 17)) return;
        for (auto& computer : lobbyTeams)
            if (strcmp(computer.name, name) == 0) computer = {};
    }
    else
    {
        lobbyTeams = {};
        if (length == nativeSize + sizeof(Extension))
        {
            Extension extension;
            memcpy(&extension, static_cast<const BYTE*>(packet) + nativeSize, sizeof(extension));
            if (ValidExtension(extension)) lobbyTeams = extension.teams;
        }
    }
    joiningWindow = *reinterpret_cast<HWND*>(joiningTree + 0x1c);
    for (size_t slot = 0; slot < 6; ++slot)
    {
        const char* name = reinterpret_cast<const char*>(joiningTree + 0x40e + slot * TeamStride);
        if (name[0] && memchr(name, 0, 17)) UpdateTeamIcon(joiningTree, slot, JoiningTeamSkill(name));
    }
}
}
