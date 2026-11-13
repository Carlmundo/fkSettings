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

#pragma comment(lib, "comctl32.lib")

namespace OfflineAlliances
{
namespace
{
    constexpr wchar_t ContextProperty[] = L"fkSettings.OfflineAlliances";
    constexpr COLORREF Colours[] = {
        RGB(255, 128, 128), RGB(128, 128, 255), RGB(128, 255, 128),
        RGB(255, 255, 128), RGB(255, 128, 255), RGB(128, 255, 255)
    };
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
    struct MatchTeam
    {
        std::string name;
        BYTE colour;
        BYTE wins = 0; // Score before this round, matched by name after sorting.
    };
    // Freeze the successfully written match, independent of later UI sorting
    // or roster edits. Non-alliance launches clear this context.
    std::vector<MatchTeam> launchedTeams;
    const BYTE* statistics = nullptr;
    constexpr size_t ResultsStride = 0x644;
    constexpr size_t TotalWinOffset = 0x60;
    constexpr size_t TotalNameOffset = 0x68c;
    constexpr size_t RoundWinOffset = 0x25f8;
    constexpr size_t RoundExcludedOffset = 0x2b84;
    constexpr size_t RoundSurvivorsOffset = 0x2bb8;
    constexpr size_t RoundNameOffset = 0x2c24;
    bool localLaunch = false;
    bool firstRoundLaunch = false;
    using NativeVoid = void (__thiscall*)(void*);
    using NativeWrite = int (__thiscall*)(void*, const char*);
    using NativeReadResults = int (__thiscall*)(void*);
    using NativeDrawString = void (__thiscall*)(void*, UINT, int, int, int, COLORREF);
    using NativeSwapTeams = void (__thiscall*)(void*, int, int);
    using NativeRandom = int (__cdecl*)();
    NativeVoid originalRefresh = nullptr, originalPlay = nullptr, originalNextRound = nullptr, originalAward = nullptr;
    NativeVoid originalShuffle = nullptr;
    NativeSwapTeams swapTeams = nullptr;
    NativeRandom random = nullptr;
    NativeWrite originalWrite = nullptr;
    NativeReadResults originalReadResults = nullptr;
    NativeDrawString originalDrawString = nullptr;

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

    HIMAGELIST MakeImages(int size)
    {
        // State index 0 hides the image; state indices 1..6 display images 0..5.
        HIMAGELIST images = ImageList_Create(size, size, ILC_COLOR32 | ILC_MASK, 6, 0);
        HDC screen = GetDC(nullptr), dc = CreateCompatibleDC(screen);
        HBITMAP bitmap = CreateCompatibleBitmap(screen, size, size);
        ReleaseDC(nullptr, screen);
        bool success = images && dc && bitmap;
        if (success)
        {
            HGDIOBJ old = SelectObject(dc, bitmap);
            for (int index = 0; success && index < 6; ++index)
            {
                RECT area{ 0, 0, size, size };
                HBRUSH transparent = CreateSolidBrush(RGB(255, 0, 255));
                success = transparent && FillRect(dc, &area, transparent);
                if (transparent) DeleteObject(transparent);
                InflateRect(&area, -2, -2);
                HBRUSH brush = CreateSolidBrush(Colours[index]);
                success = success && brush && FillRect(dc, &area, brush);
                if (brush) DeleteObject(brush);
                FrameRect(dc, &area, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
                // ImageList_AddMasked selects the source into its own DC. A
                // bitmap still selected here is copied as black on this path.
                SelectObject(dc, old);
                success = success && ImageList_AddMasked(images, bitmap, RGB(255, 0, 255)) == index;
                SelectObject(dc, bitmap);
            }
            SelectObject(dc, old);
        }
        if (bitmap) DeleteObject(bitmap);
        if (dc) DeleteDC(dc);
        if (!success && images) { ImageList_Destroy(images); images = nullptr; }
        return images;
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
        HIMAGELIST images = MakeImages(size), aligned = nullptr;
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

    bool AllianceTurnOrder(const BYTE* game, std::array<int, 6>& order)
    {
        if (!game || !roster.Enabled()) return false;
        const int count = game[0x483];
        if (count < 2 || count > 6 || count != static_cast<int>(roster.teams.size())) return false;
        std::array<std::array<int, 6>, 6> groups{};
        std::array<int, 6> sizes{}, colours{}, next{};
        unsigned seen = 0;
        for (int slot = 0; slot < count; ++slot)
        {
            const char* name = reinterpret_cast<const char*>(game + 0x486 + slot * 0x108);
            if (!*name || !memchr(name, 0, 17)) return false;
            const auto team = std::find_if(roster.teams.begin(), roster.teams.end(), [&](const Team& item) { return item.name == name; });
            if (team == roster.teams.end() || team->colour < 1 || team->colour > 6) return false;
            const unsigned bit = 1u << (team - roster.teams.begin());
            if (seen & bit) return false;
            seen |= bit;
            const int colour = team->colour - 1;
            groups[colour][sizes[colour]++] = slot;
        }
        int colourCount = 0;
        bool shared = false;
        for (int colour = 0; colour < 6; ++colour)
        {
            if (sizes[colour]) colours[colourCount++] = colour;
            if (sizes[colour] > 1) shared = true;
        }
        if (!shared || colourCount < 2) return false;
        // Shuffle colours equally, independent of the number of teams in each.
        // The native shuffle has already randomized members within each group.
        for (int index = colourCount - 1; index > 0; --index)
            std::swap(colours[index], colours[random() % (index + 1)]);
        int output = 0;
        while (output < count)
            for (int index = 0; index < colourCount; ++index)
            {
                const int colour = colours[index];
                if (next[colour] < sizes[colour]) order[output++] = groups[colour][next[colour]++];
            }
        return true;
    }

    void __fastcall ShuffleTeams(void* object, void*)
    {
        originalShuffle(object);
        if (!localLaunch || !firstRoundLaunch) return;
        std::array<int, 6> order{};
        if (!AllianceTurnOrder(static_cast<const BYTE*>(object), order)) return;
        std::array<int, 6> current{{ 0, 1, 2, 3, 4, 5 }};
        const int count = static_cast<const BYTE*>(object)[0x483];
        for (int slot = 0; slot < count; ++slot)
        {
            int source = slot;
            while (current[source] != order[slot]) ++source;
            if (source == slot) continue;
            // Move the complete team record and its native companion pointer.
            swapTeams(object, slot, source);
            std::swap(current[slot], current[source]);
        }
    }

    bool MultipleGameWinners(const BYTE* results)
    {
        if (!results) return false;
        const int count = *reinterpret_cast<const int*>(results + 4);
        const int required = *reinterpret_cast<const int*>(results + 0x0c);
        if (count < 2 || count > 6 || required <= 0) return false;
        int winners = 0;
        for (int row = 0; row < count; ++row)
            if (*reinterpret_cast<const int*>(results + TotalWinOffset + row * ResultsStride) >= required)
                ++winners;
        return winners > 1;
    }

    void __fastcall DrawStatisticsString(void* object, void*, UINT stringId, int x, int y, int size, COLORREF colour)
    {
        // 340 is the current Game Over winner sentence; also cover the legacy
        // 338 template if a caller requests it. Key by ID, preserving languages.
        if ((stringId == 338 || stringId == 340) && MultipleGameWinners(statistics)) return;
        originalDrawString(object, stringId, x, y, size, colour);
    }

    bool UpdateMatchScores(BYTE* game, int status, const std::vector<MatchTeam>& teams)
    {
        if (!game || status != 3 || teams.size() < 2 || teams.size() > 6 || game[0x483] != teams.size()) return false;
        std::array<size_t, 6> matches{};
        unsigned seen = 0;
        BYTE winner = 0;
        for (size_t row = 0; row < teams.size(); ++row)
        {
            const char* name = reinterpret_cast<const char*>(game + 0x486 + row * 0x108);
            if (!*name || !memchr(name, 0, 17)) return false;
            const auto team = std::find_if(teams.begin(), teams.end(), [&](const MatchTeam& item) { return item.name == name; });
            if (team == teams.end() || team->colour < 1 || team->colour > 6) return false;
            const size_t index = team - teams.begin();
            if (seen & (1u << index)) return false;
            seen |= 1u << index;
            matches[row] = index;
            const BYTE score = game[0x485 + row * 0x108];
            if (score == team->wins) continue;
            // The native result reader has already incremented its one winner.
            // Identify that alliance before changing any scores. Accept an
            // already repaired table too, so another refresh cannot add a win.
            if (score != static_cast<BYTE>(team->wins + 1) || (winner && winner != team->colour)) return false;
            winner = team->colour;
        }
        if (!winner) return false;
        for (size_t row = 0; row < teams.size(); ++row)
        {
            const auto& team = teams[matches[row]];
            game[0x485 + row * 0x108] = static_cast<BYTE>(team.wins + (team.colour == winner ? 1 : 0));
        }
        return true;
    }

    int __fastcall ReadResults(void* object, void*)
    {
        const int result = originalReadResults(object);
        if (result == 0 && !launchedTeams.empty())
        {
            BYTE* controller = static_cast<BYTE*>(object);
            // This reader updates the live game before the local results page
            // tests its victory limit or the statistics page builds its tables.
            UpdateMatchScores(controller + 0x30c58, *reinterpret_cast<const int*>(controller + 0xfa2c), launchedTeams);
        }
        return result;
    }

    bool AwardWinningAlliance(BYTE* results, const std::vector<MatchTeam>& teams)
    {
        if (!results || teams.size() < 2 || teams.size() > 6) return false;
        const int count = *reinterpret_cast<const int*>(results + 4);
        // The native routine awards wins only for a completed round (status 3).
        if (count != static_cast<int>(teams.size()) || *reinterpret_cast<const int*>(results + 0x1c) != 3)
            return false;
        std::array<BYTE, 6> colours{};
        std::array<int, 6> totalRows{};
        unsigned seen = 0;
        BYTE winner = 0;
        for (int row = 0; row < count; ++row)
        {
            const char* name = reinterpret_cast<const char*>(results + RoundNameOffset + row * ResultsStride);
            if (!*name || !memchr(name, 0, 17)) return false;
            const auto team = std::find_if(teams.begin(), teams.end(), [&](const MatchTeam& item) { return item.name == name; });
            if (team == teams.end() || team->colour < 1 || team->colour > 6) return false;
            const unsigned bit = 1u << (team - teams.begin());
            if (seen & bit) return false;
            seen |= bit;
            colours[row] = team->colour;
            // The accumulated table can have a different sort order from both
            // the launch roster and the round results. Validate every match
            // before changing either table, including duplicate/missing names.
            int totalRow = -1;
            for (int candidate = 0; candidate < count; ++candidate)
            {
                const char* totalName = reinterpret_cast<const char*>(results + TotalNameOffset + candidate * ResultsStride);
                if (!*totalName || !memchr(totalName, 0, 17)) return false;
                if (strcmp(totalName, name) != 0) continue;
                if (totalRow != -1) return false;
                totalRow = candidate;
            }
            if (totalRow == -1) return false;
            totalRows[row] = totalRow;
            const bool survivor = *reinterpret_cast<const int*>(results + RoundSurvivorsOffset + row * ResultsStride) != 0 &&
                *reinterpret_cast<const int*>(results + RoundExcludedOffset + row * ResultsStride) == 0;
            if (survivor)
            {
                if (winner && winner != team->colour) return false;
                winner = team->colour;
            }
        }
        if (!winner) return false; // No survivors: retain the native draw result.
        for (int row = 0; row < count; ++row)
            if (colours[row] == winner)
            {
                // These are per-round win flags, not counters. The existing
                // aggregation updates scores and saved statistics afterward.
                *reinterpret_cast<int*>(results + RoundWinOffset + row * ResultsStride) = 1;
                *reinterpret_cast<int*>(results + TotalWinOffset + totalRows[row] * ResultsStride) = 1;
            }
        return true;
    }

    void __fastcall AwardResults(void* object, void*)
    {
        originalAward(object);
        AwardWinningAlliance(static_cast<BYTE*>(object), launchedTeams);
    }

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

    std::string SidecarPath(const char* filename)
    {
        if (!filename) return {};
        const std::string path(filename);
        const auto slash = path.find_last_of("\\/");
        const auto name = slash == std::string::npos ? path : path.substr(slash + 1);
        if (_stricmp(name.c_str(), "game.dat")) return {};
        return (slash == std::string::npos ? std::string{} : path.substr(0, slash + 1)) + "teams.dat";
    }

    bool SaveSidecar(const std::string& path, const std::array<BYTE, 7>& data)
    {
        const std::string temporary = path + ".tmp";
        HANDLE file = CreateFileA(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return false;
        DWORD written = 0;
        bool success = WriteFile(file, data.data(), static_cast<DWORD>(data.size()), &written, nullptr) && written == data.size();
        if (!CloseHandle(file)) success = false;
        if (success) success = MoveFileExA(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
        if (!success) DeleteFileA(temporary.c_str());
        return success;
    }

    int __fastcall WriteGame(void* object, void*, const char* filename)
    {
        const std::string path = SidecarPath(filename);
        if (path.empty()) return originalWrite(object, filename);
        launchedTeams.clear();
        // Publish before the native writer: failure stops its local launch path.
        // Quick Play and online writers explicitly clear any previous alliance file.
        DeleteFileA(path.c_str());
        std::array<BYTE, 7> data{};
        if (!LaunchData(static_cast<const BYTE*>(object), data) || !SaveSidecar(path, data))
        {
            OutputDebugStringA("fkSettings: failed to write teams.dat; game data write reported as failed.\n");
            return 0;
        }
        std::vector<MatchTeam> pendingTeams;
        if (data[0])
            for (size_t slot = 0; slot < 6; ++slot)
                if (data[slot + 1])
                    pendingTeams.push_back({ reinterpret_cast<const char*>(static_cast<const BYTE*>(object) + 0x486 + slot * 0x108),
                        data[slot + 1], static_cast<const BYTE*>(object)[0x485 + slot * 0x108] });
        const int result = originalWrite(object, filename);
        if (result != 1) DeleteFileA(path.c_str());
        else launchedTeams.swap(pendingTeams);
        return result;
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
            { 0x2bcfc, "\x55\x8b\xec\x83\xec\x14\x89\x4d\xec", 9, reinterpret_cast<void*>(ShuffleTeams), reinterpret_cast<void**>(&originalShuffle) },
            // Hook the shared jump thunk, leaving NetworkTeams' writer hook free.
            { 0x2ba3, "\xe9\x90\x42\x02\x00", 5, reinterpret_cast<void*>(WriteGame), reinterpret_cast<void**>(&originalWrite) },
            { 0x2df64, "\x55\x8b\xec\x83\xec\x0c\x89\x4d\xf4", 9, reinterpret_cast<void*>(AwardResults), reinterpret_cast<void**>(&originalAward) },
            { 0x1e9bc, "\x55\x8b\xec\x6a\xff\x68", 6, reinterpret_cast<void*>(ReadResults), reinterpret_cast<void**>(&originalReadResults) },
            { 0x7d917, "\x55\x8b\xec\x6a\xff\x68", 6, reinterpret_cast<void*>(DrawStatisticsString), reinterpret_cast<void**>(&originalDrawString) },
        };
        for (const auto& hook : hooks)
            if (memcmp(frontend + hook.rva, hook.signature, hook.length)) return false;
        if (memcmp(frontend + 0x2bd78, "\x55\x8b\xec\x81\xec\x10\x01\x00\x00", 9) ||
            memcmp(frontend + 0x96770, "\xe8\x0b\x6f\x00\x00", 5)) return false;
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
        if (success)
        {
            statistics = frontend + 0x17eb60;
            swapTeams = reinterpret_cast<NativeSwapTeams>(frontend + 0x2bd78);
            random = reinterpret_cast<NativeRandom>(frontend + 0x96770);
        }
        return success;
    }
}

bool Install() { return InstallInImage(reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr))); }
}
