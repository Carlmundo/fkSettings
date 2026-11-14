#define NOMINMAX
typedef struct IUnknown IUnknown;
#include <Windows.h>
#include <commctrl.h>
#include <windowsx.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <new>
#include <string>
#include <vector>
#include "OnlineAlliances.h"
#include "Alliances.h"

namespace OnlineAlliances
{
namespace
{
    constexpr size_t GameRva = 0x1a7698, LocalIdRva = 0x1892ac, TransportRva = 0x176a40;
    constexpr size_t HostTreeOffset = 0x548, JoinTreeOffset = 0x3648;
    constexpr uint32_t StateType = 0x464b4131, ChangeType = 0x464b4132, Protocol = 1;
    constexpr UINT RefreshMessage = WM_APP + 0x4a1;
    constexpr wchar_t TreeProperty[] = L"fkSettings.OnlineAlliances";
    BYTE* image = nullptr;

#pragma pack(push, 1)
    struct WireTeam { char name[17]; BYTE colour; uint32_t owner; };
    struct State
    {
        char magic[4];
        uint32_t version, revision, count;
        std::array<WireTeam, 6> teams;
        std::array<BYTE, 16> footer;
    };
    struct StatePacket { uint32_t type; State state; };
    struct ChangePacket { uint32_t type, version; char name[17]; signed char direction; };
#pragma pack(pop)
    static_assert(sizeof(State) == 164 && sizeof(ChangePacket) == 26, "Alliance wire format");
    const std::array<BYTE, 16> StateFooter{{ 'A', 'L', 'N', 'D' }};

    struct Team { std::string name; BYTE colour; uint32_t owner; };
    struct Session
    {
        bool hosting = false, locked = false, launchAttempted = false, launchReady = false;
        uint32_t host = 0, revision = 0;
        void* lobby = nullptr;
        BYTE* tree = nullptr;
        std::vector<Team> teams;
        State round{}, pending{};
        uint32_t pendingSender = 0;
    } session;
    struct TreeImages
    {
        HWND window;
        BYTE* tree;
        HIMAGELIST images = nullptr, native = nullptr;
        int size = 0, nativeHeight = 0;
        bool posted = false;
    };

    uint32_t Dword(const void* pointer) { uint32_t value; memcpy(&value, pointer, sizeof(value)); return value; }
    uint32_t LocalId() { return image ? Dword(image + LocalIdRva) : 0; }
    uint32_t PacketType(const void* packet, uint32_t length) { return packet && length >= 4 ? Dword(packet) : 0; }
    uint32_t NativeSize(uint32_t type)
    {
        switch (type) { case 5: return 0x10de; case 10: return 0xc8; case 11: return 0x1c; case 14: return 20; default: return 0; }
    }
    bool SameTeam(const Team& first, const Team& second)
    { return first.name == second.name && first.colour == second.colour && first.owner == second.owner; }

    bool Valid(const State& state)
    {
        if (memcmp(state.magic, "FKAL", 4) || state.version != Protocol || !state.revision || state.count > 6 || state.footer != StateFooter) return false;
        const WireTeam empty{};
        for (size_t index = 0; index < state.teams.size(); ++index)
        {
            const auto& team = state.teams[index];
            if (index >= state.count) { if (memcmp(&team, &empty, sizeof(team))) return false; continue; }
            if (!team.name[0] || !memchr(team.name, 0, sizeof(team.name)) || !team.owner ||
                (state.count < 2 ? team.colour != 0 : team.colour < 1 || team.colour > 6)) return false;
            for (size_t previous = 0; previous < index; ++previous)
                if (!strcmp(team.name, state.teams[previous].name)) return false;
        }
        return true;
    }
    BYTE LeastUsed(const std::vector<Team>& teams)
    {
        std::array<unsigned, 6> counts{};
        for (const auto& team : teams) if (team.colour >= 1 && team.colour <= 6) ++counts[team.colour - 1];
        return static_cast<BYTE>(std::min_element(counts.begin(), counts.end()) - counts.begin() + 1);
    }
    bool SyncHost()
    {
        if (!image || !session.hosting) return false;
        const BYTE* game = image + GameRva;
        std::vector<Team> current;
        for (int slot = 0; slot < 6; ++slot)
        {
            const BYTE* record = game + 0x484 + slot * 0x108;
            const char* name = reinterpret_cast<const char*>(record + 2);
            if (!*name || !memchr(name, 0, 17)) continue;
            const uint32_t owner = record[0] < 14 ? Dword(game + 0x108c + record[0] * 4) : 0;
            if (!owner || std::any_of(current.begin(), current.end(), [&](const Team& team) { return team.name == name; })) continue;
            current.push_back({ name, 0, owner });
        }
        const auto before = session.teams;
        const bool wasEnabled = before.size() >= 2;
        session.teams.erase(std::remove_if(session.teams.begin(), session.teams.end(), [&](const Team& team) {
            return std::none_of(current.begin(), current.end(), [&](const Team& item) { return team.name == item.name; });
        }), session.teams.end());
        for (const auto& team : current)
        {
            const auto found = std::find_if(session.teams.begin(), session.teams.end(), [&](const Team& item) { return team.name == item.name; });
            if (found != session.teams.end()) found->owner = team.owner;
            else session.teams.push_back({ team.name, wasEnabled ? LeastUsed(session.teams) : BYTE(0), team.owner });
        }
        if (!wasEnabled || session.teams.size() < 2)
            for (size_t index = 0; index < session.teams.size(); ++index)
                session.teams[index].colour = session.teams.size() >= 2 ? static_cast<BYTE>(index + 1) : 0;
        const bool changed = before.size() != session.teams.size() || !std::equal(before.begin(), before.end(), session.teams.begin(), SameTeam);
        if (changed || !session.revision) { if (!++session.revision) ++session.revision; }
        return changed;
    }
    State CurrentState()
    {
        State state{};
        memcpy(state.magic, "FKAL", 4);
        state.version = Protocol; state.revision = session.revision; state.count = static_cast<uint32_t>(session.teams.size());
        // A fixed footer cannot be mistaken for ColourMaps' outer trailer,
        // regardless of a team name or player ID in the final record.
        state.footer = StateFooter;
        for (size_t index = 0; index < session.teams.size(); ++index)
        {
            strcpy_s(state.teams[index].name, sizeof(state.teams[index].name), session.teams[index].name.c_str());
            state.teams[index].colour = session.teams[index].colour; state.teams[index].owner = session.teams[index].owner;
        }
        return state;
    }
    void BroadcastState()
    {
        if (!Callbacks().send || !LocalId()) return;
        const StatePacket packet{ StateType, CurrentState() };
        Callbacks().send(image + TransportRva, LocalId(), 0, true, &packet, sizeof(packet));
    }
    Alliances::Teams Colours(const State& state)
    {
        Alliances::Teams colours;
        for (size_t index = 0; index < state.count && index < state.teams.size(); ++index)
            colours.push_back({ state.teams[index].name, state.teams[index].colour });
        return colours;
    }
    bool ColoursForGame(const BYTE* game, const State& state, std::array<BYTE, 7>& data)
    {
        data = {};
        if (!game || !Valid(state) || game[0x483] != state.count) return false;
        unsigned seen = 0;
        for (size_t slot = 0; slot < state.count; ++slot)
        {
            const char* name = reinterpret_cast<const char*>(game + 0x486 + slot * 0x108);
            if (!*name || !memchr(name, 0, 17)) return false;
            size_t match = 0;
            while (match < state.count && strcmp(name, state.teams[match].name)) ++match;
            if (match == state.count || (seen & (1u << match))) return false;
            seen |= 1u << match;
            if (state.count >= 2) data[slot + 1] = state.teams[match].colour;
        }
        data[0] = state.count >= 2 ? 1 : 0;
        return true;
    }

    void RefreshTree();
    void AttachTree();
    bool Change(uint32_t sender, const char* name, int direction)
    {
        if (!session.hosting || session.locked || !sender || (direction != 1 && direction != -1)) return false;
        SyncHost();
        if (session.teams.size() < 2) return false;
        const auto team = std::find_if(session.teams.begin(), session.teams.end(), [&](const Team& item) { return item.name == name; });
        if (team == session.teams.end() || (sender != LocalId() && sender != team->owner)) return false;
        team->colour = static_cast<BYTE>((team->colour + (direction > 0 ? 0 : 4)) % 6 + 1);
        if (!++session.revision) ++session.revision;
        RefreshTree(); BroadcastState();
        return true;
    }
    void Click(const char* name, int direction)
    {
        if (session.hosting) { Change(LocalId(), name, direction); return; }
        if (!Callbacks().send || !session.host || session.locked || !LocalId()) return;
        const auto team = std::find_if(session.teams.begin(), session.teams.end(), [&](const Team& item) { return item.name == name; });
        if (team == session.teams.end() || team->owner != LocalId()) return;
        ChangePacket packet{};
        packet.type = ChangeType; packet.version = Protocol; packet.direction = static_cast<signed char>(direction);
        strcpy_s(packet.name, sizeof(packet.name), name);
        Callbacks().send(image + TransportRva, LocalId(), session.host, false, &packet, sizeof(packet));
    }
    bool UpdateImages(TreeImages& tree)
    {
        HDC dc = GetDC(tree.window);
        if (!dc) return false;
        HFONT font = reinterpret_cast<HFONT>(SendMessageW(tree.window, WM_GETFONT, 0, 0));
        HGDIOBJ old = font ? SelectObject(dc, font) : nullptr;
        TEXTMETRICW metrics{};
        const bool measured = GetTextMetricsW(dc, &metrics) != FALSE;
        if (old) SelectObject(dc, old);
        ReleaseDC(tree.window, dc);
        if (!measured) return false;
        const int size = (std::max<LONG>)(16, metrics.tmHeight);
        if (tree.images && tree.size == size) return true;
        HIMAGELIST palette = static_cast<HIMAGELIST>(Alliances::CreateColourImages(size));
        HIMAGELIST images = ImageList_Create(size, size, ILC_COLOR32 | ILC_MASK, 7, 0);
        // Unlike the offline list view, the tree uses the state number as the
        // image index directly. Its index zero is reserved for no state image.
        bool success = palette && images && ImageList_SetImageCount(images, 1);
        for (int index = 0; success && index < 6; ++index)
        {
            HICON icon = ImageList_GetIcon(palette, index, ILD_TRANSPARENT);
            success = icon && ImageList_AddIcon(images, icon) == index + 1;
            if (icon) DestroyIcon(icon);
        }
        if (palette) ImageList_Destroy(palette);
        if (!success) { if (images) ImageList_Destroy(images); return false; }
        HIMAGELIST previous = tree.images;
        tree.images = images; tree.size = size;
        TreeView_SetImageList(tree.window, images, TVSIL_STATE);
        TreeView_SetItemHeight(tree.window, (std::max)(tree.nativeHeight, (size + 3) & ~1));
        if (previous) ImageList_Destroy(previous);
        return true;
    }
    void PaintTree(TreeImages& tree)
    {
        if (!UpdateImages(tree)) return;
        TreeView_SetImageList(tree.window, session.teams.size() >= 2 ? tree.images : tree.native, TVSIL_STATE);
        for (int slot = 0; slot < 6; ++slot)
        {
            TVITEMA item{};
            item.mask = TVIF_STATE;
            item.hItem = *reinterpret_cast<HTREEITEM*>(tree.tree + 0x90 + slot * 4);
            if (!item.hItem) continue;
            const char* name = reinterpret_cast<const char*>(tree.tree + 0x40e + slot * 0x108);
            if (!memchr(name, 0, 17)) continue;
            const auto team = std::find_if(session.teams.begin(), session.teams.end(), [&](const Team& entry) { return entry.name == name; });
            item.stateMask = TVIS_STATEIMAGEMASK;
            item.state = INDEXTOSTATEIMAGEMASK(session.teams.size() >= 2 && team != session.teams.end() ? team->colour : 0);
            SendMessageA(tree.window, TVM_SETITEMA, 0, reinterpret_cast<LPARAM>(&item));
        }
        InvalidateRect(tree.window, nullptr, TRUE);
    }
    void RefreshTree()
    {
        if (!session.tree) return;
        HWND window = *reinterpret_cast<HWND*>(session.tree + 0x1c);
        if (auto tree = static_cast<TreeImages*>(GetPropW(window, TreeProperty))) PaintTree(*tree);
    }
    LRESULT CALLBACK TreeProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR id, DWORD_PTR reference)
    {
        auto tree = reinterpret_cast<TreeImages*>(reference);
        const bool left = message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK || message == WM_LBUTTONUP;
        const bool right = message == WM_RBUTTONDOWN || message == WM_RBUTTONDBLCLK || message == WM_RBUTTONUP;
        if (session.tree == tree->tree && session.teams.size() >= 2 && (left || right))
        {
            TVHITTESTINFO hit{}; hit.pt = { GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
            TreeView_HitTest(window, &hit);
            if (hit.hItem && (hit.flags & TVHT_ONITEMSTATEICON))
            {
                for (int slot = 0; slot < 6; ++slot)
                    if (*reinterpret_cast<HTREEITEM*>(tree->tree + 0x90 + slot * 4) == hit.hItem)
                    {
                        if (message != WM_LBUTTONUP && message != WM_RBUTTONUP)
                        {
                            const char* name = reinterpret_cast<const char*>(tree->tree + 0x40e + slot * 0x108);
                            if (memchr(name, 0, 17)) { SetFocus(window); Click(name, right ? -1 : 1); }
                        }
                        return 0;
                    }
            }
        }
        if (message == WM_DESTROY)
        {
            TreeView_SetImageList(window, tree->native, TVSIL_STATE);
            TreeView_SetItemHeight(window, tree->nativeHeight);
        }
        if (message == WM_NCDESTROY)
        {
            RemoveWindowSubclass(window, TreeProc, id); RemovePropW(window, TreeProperty);
            if (session.tree == tree->tree) session.tree = nullptr;
            if (tree->images) ImageList_Destroy(tree->images);
            delete tree;
            return DefSubclassProc(window, message, wparam, lparam);
        }
        if (message == RefreshMessage)
        {
            tree->posted = false;
            if (session.tree == tree->tree)
            { const bool changed = session.hosting && SyncHost(); PaintTree(*tree); if (changed) BroadcastState(); }
            return 0;
        }
        const LRESULT result = DefSubclassProc(window, message, wparam, lparam);
        if (message == WM_SETFONT) PaintTree(*tree);
        if ((message == TVM_INSERTITEMA || message == TVM_INSERTITEMW || message == TVM_DELETEITEM) && !tree->posted)
        { tree->posted = PostMessageW(window, RefreshMessage, 0, 0) != FALSE; }
        return result;
    }
    void AttachTree()
    {
        if (!session.tree) return;
        HWND window = *reinterpret_cast<HWND*>(session.tree + 0x1c);
        if (!IsWindow(window)) return;
        if (GetPropW(window, TreeProperty)) { RefreshTree(); return; }
        auto tree = new (std::nothrow) TreeImages{};
        if (!tree) return;
        tree->window = window; tree->tree = session.tree;
        tree->native = TreeView_GetImageList(window, TVSIL_STATE);
        tree->nativeHeight = TreeView_GetItemHeight(window);
        if (!SetPropW(window, TreeProperty, tree) || !SetWindowSubclass(window, TreeProc, 3, reinterpret_cast<DWORD_PTR>(tree)))
        { RemovePropW(window, TreeProperty); delete tree; return; }
        PaintTree(*tree);
    }
    void Lobby(void* object, bool hosting, bool round, bool reset)
    {
        const uint32_t host = hosting ? LocalId() : Dword(static_cast<BYTE*>(object) + (round ? 0xa8 : 0x163c));
        if (reset || (!round && (session.hosting != hosting || (session.lobby && session.lobby != object) ||
            (session.host && session.host != host)))) session = Session{};
        session.hosting = hosting; session.lobby = object;
        session.host = host;
        if (!round)
        {
            session.tree = static_cast<BYTE*>(object) + (hosting ? HostTreeOffset : JoinTreeOffset);
            if (hosting) SyncHost();
            AttachTree();
        }
    }
    void TreeChanged(void* tree)
    {
        if (tree != session.tree) return;
        const bool changed = session.hosting && SyncHost();
        AttachTree();
        if (changed) BroadcastState();
    }
    bool Adopt(const State& state, uint32_t sender)
    {
        if (!Valid(state) || !session.host || sender != session.host || session.hosting ||
            (session.revision && static_cast<int32_t>(state.revision - session.revision) < 0)) return false;
        session.teams.clear();
        for (size_t index = 0; index < state.count; ++index)
            session.teams.push_back({ state.teams[index].name, state.teams[index].colour, state.teams[index].owner });
        session.revision = state.revision; RefreshTree();
        return true;
    }
    bool Extend(const void* packet, uint32_t length, std::vector<BYTE>& extended)
    {
        const uint32_t type = PacketType(packet, length), native = NativeSize(type);
        if (!session.hosting || !native || (length != native && (type == 11 || length != native + 112))) return false;
        SyncHost();
        const State state = CurrentState();
        if (!Valid(state)) return false;
        const BYTE* bytes = static_cast<const BYTE*>(packet);
        extended.assign(bytes, bytes + length);
        const BYTE* data = reinterpret_cast<const BYTE*>(&state);
        extended.insert(extended.end(), data, data + sizeof(state));
        return true;
    }
    bool Receive(void*, uint32_t sender, uint32_t host, const void* packet, uint32_t& length, bool)
    {
        if (!packet || length < 4) return false;
        const uint32_t type = PacketType(packet, length);
        if (type == ChangeType) return false;
        if (type == StateType)
        {
            if (host && sender == host && length == sizeof(StatePacket))
            { StatePacket state; memcpy(&state, packet, sizeof(state)); Adopt(state.state, sender); }
            return false;
        }
        const uint32_t native = NativeSize(type);
        if (!native) return true;
        if ((!host && type != 5) || (host && sender != host) ||
            (length != native + sizeof(State) && (type == 11 || length != native + 112 + sizeof(State)))) return false;
        State state;
        memcpy(&state, static_cast<const BYTE*>(packet) + length - sizeof(state), sizeof(state));
        const uint32_t stripped = length - sizeof(state);
        if (!Valid(state)) return false;
        if (host && session.host && host != session.host) return false;
        length = stripped;
        if (type == 14)
        {
            session.host = host;
            if (!Adopt(state, sender)) return false;
            session.round = state; session.locked = true;
        }
        else { session.pending = state; session.pendingSender = sender; }
        return true;
    }
    void AfterReceive(void* object, uint32_t sender, const void* packet, uint32_t length, bool round)
    {
        if (round || !NativeSize(PacketType(packet, length))) return;
        Lobby(object, false, false, false);
        if (session.pendingSender == sender)
        { Adopt(session.pending, sender); session.pendingSender = 0; session.pending = {}; }
        RefreshTree();
    }
    bool HostReceive(void*, uint32_t sender, const void* packet, uint32_t length, bool round)
    {
        const uint32_t type = PacketType(packet, length);
        if (type == StateType) return false;
        if (type != ChangeType) return true;
        if (!round && length == sizeof(ChangePacket))
        {
            ChangePacket request; memcpy(&request, packet, sizeof(request));
            if (request.version == Protocol && request.name[0] && memchr(request.name, 0, sizeof(request.name)))
                Change(sender, request.name, request.direction);
        }
        return false;
    }
    bool Launch(const void* object, const void* caller, bool local, std::array<BYTE, 7>& data)
    {
        session.launchAttempted = session.launchReady = false;
        if (local || object != image + GameRva) return true;
        const bool host = caller == image + 0x35950 || caller == image + 0x61b13;
        const bool join = caller == image + 0x3b3d5 || caller == image + 0x6406a;
        if (!host && !join) return true;
        session.launchAttempted = true;
        if (host)
        {
            if (!session.hosting) return false;
            SyncHost(); session.round = CurrentState();
        }
        else if (session.hosting || !session.host) return false;
        return ColoursForGame(static_cast<const BYTE*>(object), session.round, data);
    }
    void LaunchComplete(bool success)
    { if (session.launchAttempted) { session.launchReady = success; session.locked = success; } }
    bool CanRunEngine() { return !session.launchAttempted || session.launchReady; }
    void Shuffle(void* object, const void* caller)
    {
        if (object != image + GameRva) return;
        State state{};
        if (caller == image + 0x35937 && session.hosting) { SyncHost(); state = CurrentState(); }
        else if (caller == image + 0x3b3bc && !session.hosting) state = session.round;
        else return;
        if (Valid(state)) Alliances::InterleaveTeams(object, Colours(state));
    }

    bool InstallInImage(BYTE* frontend)
    {
        if (!frontend) return false;
        const auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(frontend);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 0x1000) return false;
        const auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(frontend + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
            nt->FileHeader.TimeDateStamp != 0x3587be19 || nt->OptionalHeader.SizeOfImage < GameRva + 0x10d8) return false;
        // Shared hooks own these call paths; validate their unmodified callers.
        const struct { size_t rva; const char* signature; size_t size; } sites[] = {
            { 0x35932, "\xe8\xc7\xbd\xfc\xff", 5 },
            { 0x3b3b7, "\xe8\x42\x63\xfc\xff", 5 },
            { 0x3594b, "\xe8\x53\xd2\xfc\xff", 5 },
            { 0x3b3d0, "\xe8\xce\x77\xfc\xff", 5 },
            { 0x61b0e, "\xe8\x90\x10\xfa\xff", 5 },
            { 0x64065, "\xe8\x39\xeb\xf9\xff", 5 },
        };
        for (const auto& site : sites) if (memcmp(frontend + site.rva, site.signature, site.size)) return false;
        image = frontend; session = Session{};
        auto& hooks = Callbacks();
        hooks.extend = Extend; hooks.receive = Receive; hooks.afterReceive = AfterReceive; hooks.hostReceive = HostReceive;
        hooks.lobby = Lobby; hooks.treeChanged = TreeChanged; hooks.launch = Launch; hooks.launchComplete = LaunchComplete;
        hooks.shuffle = Shuffle; hooks.canRunEngine = CanRunEngine;
        return true;
    }
}
bool Install() { return InstallInImage(reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr))); }
}
