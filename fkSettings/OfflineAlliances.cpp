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
#include "include/MinHook.h"
#include "OfflineAlliances.h"
#include "Alliances.h"

#pragma comment(lib, "comctl32.lib")

namespace OfflineAlliances
{
namespace
{
    constexpr wchar_t ContextProperty[] = L"fkSettings.OfflineAlliances";
    struct Team
    {
        LPARAM savedIndex;
        std::string name;
        BYTE colour;
    };
    struct Roster
    {
        // In insertion order, independent of the native list's sort order.
        std::vector<Team> teams;

        bool Enabled() const { return teams.size() > 1; }

        BYTE LeastUsed() const
        {
            std::array<unsigned, 6> counts{};
            for (const auto& team : teams)
                if (team.colour >= 1 && team.colour <= 6) ++counts[team.colour - 1];
            return static_cast<BYTE>(std::min_element(counts.begin(), counts.end()) - counts.begin() + 1);
        }

        void Sync(const std::vector<Team>& current)
        {
            const bool wasEnabled = Enabled();
            teams.erase(std::remove_if(teams.begin(), teams.end(), [&](const Team& team) {
                return std::none_of(current.begin(), current.end(), [&](const Team& item) {
                    return item.savedIndex == team.savedIndex;
                });
            }), teams.end());
            for (const auto& item : current)
            {
                auto found = std::find_if(teams.begin(), teams.end(), [&](const Team& team) {
                    return team.savedIndex == item.savedIndex;
                });
                if (found != teams.end()) found->name = item.name;
                else if (teams.size() < 6)
                    teams.push_back({ item.savedIndex, item.name, wasEnabled ? LeastUsed() : BYTE(0) });
            }
            // Crossing from one team to two has the same effect as enabling
            // alliances: assign colours in addition order. Keep choices while
            // two or more teams remain; zero/one team clears them.
            if (!wasEnabled || !Enabled())
                for (size_t i = 0; i < teams.size(); ++i)
                    teams[i].colour = Enabled() ? static_cast<BYTE>(i + 1) : 0;
        }

        void Cycle(LPARAM savedIndex, bool reverse = false)
        {
            if (!Enabled()) return;
            for (auto& team : teams)
                if (team.savedIndex == savedIndex)
                    team.colour = reverse ? (team.colour == 1 ? 6 : team.colour - 1) : team.colour % 6 + 1;
        }
    } roster;

    struct Editor
    {
        HWND list;
        HIMAGELIST images, nativeImages;
        HIMAGELIST alignedTeamImages, nativeTeamImages;
        int imageSize;
    };
    bool localLaunch = false;
    bool firstRoundLaunch = false;
    using NativeVoid = void (__thiscall*)(void*);
    NativeVoid originalRefresh = nullptr, originalPlay = nullptr, originalNextRound = nullptr;

    std::vector<Team> ReadRows(HWND list)
    {
        std::vector<Team> rows;
        const int count = ListView_GetItemCount(list);
        for (int row = 0; row < count && row < 6; ++row)
        {
            char name[18]{};
            LVITEMA item{};
            item.mask = LVIF_PARAM | LVIF_TEXT;
            item.iItem = row;
            item.pszText = name;
            item.cchTextMax = sizeof(name);
            if (SendMessageA(list, LVM_GETITEMA, 0, reinterpret_cast<LPARAM>(&item)))
                rows.push_back({ item.lParam, name, 0 });
        }
        return rows;
    }

    void RefreshImages(Editor& editor)
    {
        ListView_SetImageList(editor.list, roster.Enabled() ? editor.images : editor.nativeImages, LVSIL_STATE);
        for (int row = 0; row < ListView_GetItemCount(editor.list); ++row)
        {
            LVITEMA item{};
            item.mask = LVIF_PARAM;
            item.iItem = row;
            if (!SendMessageA(editor.list, LVM_GETITEMA, 0, reinterpret_cast<LPARAM>(&item))) continue;
            BYTE colour = 0;
            if (roster.Enabled())
                for (const auto& team : roster.teams)
                    if (team.savedIndex == item.lParam) colour = team.colour;
            ListView_SetItemState(editor.list, row, INDEXTOSTATEIMAGEMASK(colour), LVIS_STATEIMAGEMASK);
        }
        InvalidateRect(editor.list, nullptr, TRUE);
    }

    void CycleRow(Editor& editor, int row, bool reverse = false)
    {
        LVITEMA item{};
        item.mask = LVIF_PARAM;
        item.iItem = row;
        if (row >= 0 && SendMessageA(editor.list, LVM_GETITEMA, 0, reinterpret_cast<LPARAM>(&item)))
        {
            roster.Cycle(item.lParam, reverse);
            RefreshImages(editor);
        }
    }

    bool UpdateImageLayout(Editor& editor);

    void RestoreImageLists(Editor& editor)
    {
        ListView_SetImageList(editor.list, editor.nativeImages, LVSIL_STATE);
        if (editor.alignedTeamImages)
            ListView_SetImageList(editor.list, editor.nativeTeamImages, LVSIL_SMALL);
    }

    LRESULT CALLBACK ListProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam,
        UINT_PTR id, DWORD_PTR reference)
    {
        auto& editor = *reinterpret_cast<Editor*>(reference);
        const bool leftButton = message == WM_LBUTTONDOWN || message == WM_LBUTTONDBLCLK || message == WM_LBUTTONUP;
        const bool rightButton = message == WM_RBUTTONDOWN || message == WM_RBUTTONDBLCLK || message == WM_RBUTTONUP;
        if (roster.Enabled() && (leftButton || rightButton))
        {
            LVHITTESTINFO hit{};
            hit.pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            const int row = ListView_HitTest(window, &hit);
            if (row >= 0 && (hit.flags & LVHT_ONITEMSTATEICON))
            {
                if (message != WM_LBUTTONUP && message != WM_RBUTTONUP)
                {
                    SetFocus(window);
                    CycleRow(editor, row, rightButton);
                }
                return 0;
            }
        }
        if (message == WM_NCDESTROY)
        {
            RestoreImageLists(editor);
            RemoveWindowSubclass(window, ListProc, id);
            editor.list = nullptr;
        }
        const LRESULT result = DefSubclassProc(window, message, wParam, lParam);
        if (message == WM_SETFONT && !UpdateImageLayout(editor))
            OutputDebugStringA("fkSettings: unable to update offline roster image layout.\n");
        if (message == LVM_INSERTITEMA || message == LVM_INSERTITEMW ||
            message == LVM_DELETEITEM || message == LVM_DELETEALLITEMS)
        {
            roster.Sync(ReadRows(window));
            RefreshImages(editor);
        }
        return result;
    }

    LRESULT CALLBACK EditorProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam,
        UINT_PTR id, DWORD_PTR reference)
    {
        auto editor = reinterpret_cast<Editor*>(reference);
        if (message == WM_NCDESTROY)
        {
            if (IsWindow(editor->list))
            {
                RestoreImageLists(*editor);
                RemoveWindowSubclass(editor->list, ListProc, 1);
            }
            RemoveWindowSubclass(window, EditorProc, id);
            RemovePropW(window, ContextProperty);
            ImageList_Destroy(editor->images);
            if (editor->alignedTeamImages) ImageList_Destroy(editor->alignedTeamImages);
            delete editor;
            // Keep the match's assignments for the results screen's next round.
        }
        return DefSubclassProc(window, message, wParam, lParam);
    }

    HIMAGELIST PadTeamImages(HIMAGELIST source, int height)
    {
        int width = 0, sourceHeight = 0;
        if (!source || !ImageList_GetIconSize(source, &width, &sourceHeight) || width <= 0 || sourceHeight > height)
            return nullptr;
        const int count = ImageList_GetImageCount(source);
        if (count <= 0) return nullptr;
        HIMAGELIST images = ImageList_Create(width, height, ILC_COLOR32 | ILC_MASK, count, 0);
        HDC screen = GetDC(nullptr), dc = CreateCompatibleDC(screen);
        HBITMAP bitmap = CreateCompatibleBitmap(screen, width, height);
        ReleaseDC(nullptr, screen);
        HBRUSH background = CreateSolidBrush(RGB(255, 0, 255));
        bool success = images && dc && bitmap && background;
        if (success)
        {
            HGDIOBJ old = SelectObject(dc, bitmap);
            RECT area{ 0, 0, width, height };
            for (int index = 0; success && index < count; ++index)
            {
                success = FillRect(dc, &area, background) &&
                    ImageList_Draw(source, index, dc, 0, (height - sourceHeight) / 2, ILD_TRANSPARENT);
                SelectObject(dc, old);
                success = success && ImageList_AddMasked(images, bitmap, RGB(255, 0, 255)) == index;
                SelectObject(dc, bitmap);
            }
            SelectObject(dc, old);
        }
        if (background) DeleteObject(background);
        if (bitmap) DeleteObject(bitmap);
        if (dc) DeleteDC(dc);
        if (!success && images) { ImageList_Destroy(images); images = nullptr; }
        return images;
    }

    bool UpdateImageLayout(Editor& editor)
    {
        HFONT font = reinterpret_cast<HFONT>(SendMessageW(editor.list, WM_GETFONT, 0, 0));
        HDC dc = GetDC(editor.list);
        if (!dc) return false;
        HGDIOBJ old = font ? SelectObject(dc, font) : nullptr;
        TEXTMETRICW metrics{};
        const bool measured = GetTextMetricsW(dc, &metrics) != FALSE;
        if (old) SelectObject(dc, old);
        ReleaseDC(editor.list, dc);
        if (!measured) return false;
        // The control's font is already DPI-scaled. Classic list views align
        // state images to the SMALL image cell, not the taller text row. Pad
        // only this roster's sprites so both image lists share that height.
        const int size = (std::max<LONG>)(16, metrics.tmHeight);
        HIMAGELIST source = ListView_GetImageList(editor.list, LVSIL_SMALL);
        if (editor.alignedTeamImages && source == editor.alignedTeamImages) source = editor.nativeTeamImages;
        if (editor.images && editor.imageSize == size && source == editor.nativeTeamImages) return true;
        HIMAGELIST images = static_cast<HIMAGELIST>(Alliances::CreateColourImages(size)), aligned = nullptr;
        if (!images) return false;
        int nativeWidth = 0, nativeHeight = 0;
        bool success = !source || ImageList_GetIconSize(source, &nativeWidth, &nativeHeight) != FALSE;
        if (success && source && nativeHeight < size)
        {
            aligned = PadTeamImages(source, size);
            success = aligned != nullptr;
        }
        if (!success)
        {
            ImageList_Destroy(images);
            return false;
        }
        HIMAGELIST previousImages = editor.images, previousAligned = editor.alignedTeamImages;
        editor.images = images;
        editor.imageSize = size;
        editor.nativeTeamImages = source;
        editor.alignedTeamImages = aligned;
        ListView_SetImageList(editor.list, aligned ? aligned : source, LVSIL_SMALL);
        RefreshImages(editor);
        if (previousImages) ImageList_Destroy(previousImages);
        if (previousAligned) ImageList_Destroy(previousAligned);
        return true;
    }

    bool Attach(HWND window)
    {
        if (!IsWindow(window)) return false;
        if (auto editor = static_cast<Editor*>(GetPropW(window, ContextProperty))) return UpdateImageLayout(*editor);
        HWND list = GetDlgItem(window, 1033);
        if (!IsWindow(list)) return false;
        auto editor = new (std::nothrow) Editor{};
        if (!editor) return false;
        editor->list = list;
        editor->nativeImages = ListView_GetImageList(list, LVSIL_STATE);
        bool success = SetPropW(window, ContextProperty, editor) != FALSE;
        if (success) success = SetWindowSubclass(window, EditorProc, 1, reinterpret_cast<DWORD_PTR>(editor)) != FALSE;
        if (success) success = SetWindowSubclass(list, ListProc, 1, reinterpret_cast<DWORD_PTR>(editor)) != FALSE;
        if (!success)
        {
            RemoveWindowSubclass(window, EditorProc, 1);
            RemovePropW(window, ContextProperty);
            if (editor->images) ImageList_Destroy(editor->images);
            delete editor;
            return false;
        }
        roster = {};
        roster.Sync(ReadRows(list));
        if (!UpdateImageLayout(*editor))
        {
            RemoveWindowSubclass(list, ListProc, 1);
            RemoveWindowSubclass(window, EditorProc, 1);
            RemovePropW(window, ContextProperty);
            delete editor;
            return false;
        }
        return true;
    }

    void __fastcall RefreshControls(void* object, void*)
    {
        originalRefresh(object);
        if (!Attach(*reinterpret_cast<HWND*>(static_cast<BYTE*>(object) + 0x1c)))
            OutputDebugStringA("fkSettings: unable to attach offline alliance controls.\n");
    }

    void RunLocal(void* object, NativeVoid run, bool firstRound = false)
    {
        struct Restore
        {
            bool local, first;
            ~Restore() { localLaunch = local; firstRoundLaunch = first; }
        } restore{ localLaunch, firstRoundLaunch };
        localLaunch = true;
        firstRoundLaunch = firstRound;
        run(object);
    }
    void __fastcall Play(void* object, void*) { RunLocal(object, originalPlay, true); }
    void __fastcall NextRound(void* object, void*) { RunLocal(object, originalNextRound); }

    bool LaunchData(const BYTE* game, std::array<BYTE, 7>& data)
    {
        data = {};
        if (!localLaunch || !roster.Enabled()) return true;
        data[0] = 1;
        for (size_t slot = 0; slot < 6; ++slot)
        {
            const char* name = reinterpret_cast<const char*>(game + 0x486 + slot * 0x108);
            if (!*name) continue;
            if (!memchr(name, 0, 17)) return false;
            auto team = std::find_if(roster.teams.begin(), roster.teams.end(), [&](const Team& item) {
                return item.name == name;
            });
            if (team == roster.teams.end() || team->colour < 1 || team->colour > 6) return false;
            data[slot + 1] = team->colour;
        }
        return true;
    }

    Alliances::Teams MatchTeams()
    {
        Alliances::Teams teams;
        for (const auto& team : roster.teams) teams.push_back({ team.name, team.colour });
        return teams;
    }

    bool PrepareLaunch(const void* game, std::array<BYTE, 7>& data, bool& local)
    {
        local = localLaunch;
        return LaunchData(static_cast<const BYTE*>(game), data);
    }

    bool ShuffleLocal(void* game)
    {
        if (!localLaunch) return false;
        if (firstRoundLaunch) Alliances::InterleaveTeams(game, MatchTeams());
        return true;
    }

    void RegisterLocalHandlers()
    {
        Alliances::LocalCallbacks().launch = PrepareLaunch;
        Alliances::LocalCallbacks().shuffle = ShuffleLocal;
    }

    bool InstallInImage(BYTE* frontend)
    {
        if (!frontend) return false;
        auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(frontend);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 0x1000) return false;
        auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(frontend + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
            nt->FileHeader.TimeDateStamp != 0x3587be19 || nt->OptionalHeader.SizeOfImage < 0x1862e0) return false;
        struct Hook { size_t rva; const char* signature; size_t length; void* detour; void** original; };
        const Hook hooks[] = {
            { 0x78f7b, "\x55\x8b\xec\x6a\xff\x68", 6, reinterpret_cast<void*>(RefreshControls), reinterpret_cast<void**>(&originalRefresh) },
            { 0x79343, "\x55\x8b\xec\x83\xec\x0c\x89\x4d\xf4", 9, reinterpret_cast<void*>(Play), reinterpret_cast<void**>(&originalPlay) },
            { 0x77da2, "\x55\x8b\xec\x83\xec\x24\x89\x4d\xe4", 9, reinterpret_cast<void*>(NextRound), reinterpret_cast<void**>(&originalNextRound) },
        };
        for (const auto& hook : hooks)
            if (memcmp(frontend + hook.rva, hook.signature, hook.length)) return false;
        std::vector<void*> created;
        bool success = true;
        for (const auto& hook : hooks)
        {
            void* target = frontend + hook.rva;
            if (MH_CreateHook(target, hook.detour, hook.original) != MH_OK) { success = false; break; }
            created.push_back(target);
        }
        if (success)
            for (void* target : created)
                if (MH_QueueEnableHook(target) != MH_OK) { success = false; break; }
        if (success) success = MH_ApplyQueued() == MH_OK;
        if (!success)
            for (void* target : created) { MH_DisableHook(target); MH_RemoveHook(target); }
        if (success) RegisterLocalHandlers();
        return success;
    }
}

bool Install() { return InstallInImage(reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr))); }
}
