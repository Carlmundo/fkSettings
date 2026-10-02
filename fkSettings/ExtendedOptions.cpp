#define NOMINMAX
typedef struct IUnknown IUnknown;
#include <Windows.h>
#include <commctrl.h>
#include <algorithm>
#include <cstring>
#include <new>
#include <string>
#include <vector>
#include "include/MinHook.h"
#include "ExtendedOptions.h"
#include "ExtendedOptionsStrings.h"

#pragma comment(lib, "comctl32.lib")

namespace ExtendedOptions
{
namespace
{
    constexpr size_t OptionsRva = 0x1863d8;
    constexpr size_t NativePayloadSize = 128;
    constexpr int FirstCheckId = 51000;
    constexpr wchar_t ContextProperty[] = L"fkSettings.ExtendedOptions";
    OptionStrings strings = MakeOptionStrings("en");
    struct Option
    {
        OptionIndex index;
        const wchar_t* const* label;
        unsigned char maximum;
        int row;
        UINT zeroStringId;
        int column = 0;
        UINT maximumStringId = 0;
        int blankLines = 0;
        bool IsSlider() const { return maximum > 1; }
    };
    // Flow down the left column, then the right. Sliders use two logical rows
    // plus padding to match the native 31-DLU slider pitch. Blank lines use
    // half a checkbox row. blankLines counts preceding gaps in each column.
    // This array sets visual/tab order. Each descriptor carries its fixed byte
    // index so moving it or changing its label cannot move the stored value.
    constexpr Option Options[OptionCount] = {
        { OptionIndex::GodMode, &strings.strGodMode, 1, 0, 0 },
        { OptionIndex::HighJump, &strings.strHighJump, 1, 1, 0 },
        { OptionIndex::SuicideBomber, &strings.strSuicideBomber, 1, 2, 0 },
        { OptionIndex::SheepHeaven, &strings.strSheepHeaven, 1, 3, 0 },
        { OptionIndex::SuperShopperCrates, &strings.strSuperShopperCrates, 100, 4, 141, 0, 4950 },
        { OptionIndex::ExtendedFusesHerds, &strings.strExtendedFusesHerds, 1, 6, 0, 0, 0, 1 },
        { OptionIndex::UtilitiesDontEndTurn, &strings.strUtilitiesDontEndTurn, 1, 7, 0, 0, 0, 1 },
        { OptionIndex::WeaponsDontEndTurn, &strings.strWeaponsDontEndTurn, 1, 8, 0, 0, 0, 1 },
        { OptionIndex::LossOfControlDoesntEndTurn, &strings.strLossOfControlDoesntEndTurn, 1, 9, 0, 0, 0, 1 },
        { OptionIndex::WormSelectAfterMovement, &strings.strWormSelectAfterMovement, 1, 10, 0, 0, 0, 1 },
        { OptionIndex::LowGravity, &strings.strLowGravity, 1, 11, 0, 0, 0, 1 },
        { OptionIndex::PersistentRope, &strings.strPersistentRope, 1, 12, 0, 0, 0, 1 },
        { OptionIndex::RapidPlay, &strings.strRapidPlay, 1, 13, 0, 0, 0, 1 },
        { OptionIndex::IndestructibleTerrain, &strings.strIndestructibleTerrain, 1, 14, 0, 0, 0, 2 },
        { OptionIndex::InvisibleTerrain, &strings.strInvisibleTerrain, 1, 15, 0, 0, 0, 2 },
        { OptionIndex::FastCrates, &strings.strFastCrates, 1, 0, 0, 1 },
        { OptionIndex::CrateSpy, &strings.strCrateSpy, 1, 1, 0, 1 },
        { OptionIndex::CrateLimit, &strings.strCrateLimit, 100, 2, 99, 1 },
        { OptionIndex::CrateRate, &strings.strCrateRate, 100, 4, 99, 1 },
        { OptionIndex::AquaSheep, &strings.strAquaSheep, 1, 6, 0, 1, 0, 1 },
        { OptionIndex::InstantMines, &strings.strInstantMines, 1, 7, 0, 1, 0, 1 },
        { OptionIndex::HerdDynamite, &strings.strHerdDynamite, 1, 8, 0, 1, 0, 1 },
        { OptionIndex::HerdMine, &strings.strHerdMine, 1, 9, 0, 1, 0, 1 },
        { OptionIndex::HerdMingVase, &strings.strHerdMingVase, 1, 10, 0, 1, 0, 1 },
        { OptionIndex::HerdSheep, &strings.strHerdSheep, 1, 11, 0, 1, 0, 1 },
        { OptionIndex::DisableBackflip, &strings.strDisableBackflip, 1, 12, 0, 1, 0, 2 },
        { OptionIndex::DisableUnlockedAim, &strings.strDisableUnlockedAim, 1, 13, 0, 1, 0, 2 },
    };
    constexpr bool UniqueStorageIndexes()
    {
        bool seen[OptionCount]{};
        for (const auto& option : Options)
        {
            const size_t index = ToIndex(option.index);
            if (index >= OptionCount || seen[index]) return false;
            seen[index] = true;
        }
        return true;
    }
    static_assert(UniqueStorageIndexes(), "Every option must have a unique fixed storage index");
    const Option* FindOption(size_t index)
    {
        const auto option = std::find_if(Options, Options + OptionCount,
            [=](const Option& item) { return ToIndex(item.index) == index; });
        return option == Options + OptionCount ? nullptr : option;
    }
    // Dialog 154: checkboxes are 10 DLU high, 11 DLU apart. Slider captions
    // and readouts are 8 DLU high; sliders start 10 DLU below their captions.
    constexpr int RowHeight = 11;
    constexpr int CheckboxHeight = 10;
    constexpr int LabelHeight = 8;
    constexpr int SliderTop = 10;
    constexpr int SliderHeight = 19;
    constexpr int SliderReadoutTop = 16;
    constexpr int SliderExtraHeight = 31 - 2 * RowHeight;
    constexpr int FirstRowTop = 490;
    // Both columns have two half-row gaps, added after DLU-to-pixel conversion.
    constexpr int GroupBottom = FirstRowTop + 16 * RowHeight + SliderExtraHeight + 6;
    constexpr int ExtraHeight = GroupBottom + 8 - 468;
    struct Extension
    {
        char magic[4];
        std::array<unsigned char, OptionCount> values;
    };
    static_assert(sizeof(Extension) == 31, "File contract: PLUS followed by twenty-seven bytes");
    BYTE* image = nullptr;
    bool enabled = false;
    // Storage order is OptionIndex, independent of the Options layout order.
    std::array<unsigned char, OptionCount> values{};
    std::vector<HWND> editors;

    using CreatePage = void* (__thiscall*)(void*, void*, const RECT*);
    using SetScrollSizes = void (__thiscall*)(void*, int, SIZE, const SIZE&, const SIZE&);
    using MarkEdited = void (__thiscall*)(void*);
    using Open = void* (__cdecl*)(const char*, const char*);
    using Close = int (__cdecl*)(void*);
    CreatePage originalCreatePage = nullptr;
    SetScrollSizes setScrollSizes = nullptr;
    MarkEdited markEdited = nullptr;
    Open originalOpen = nullptr;
    Close originalClose = nullptr;
    void* defaultTrampolines[9]{};

    struct LaunchStream { void* stream; std::string sidecar; };
    std::vector<LaunchStream> launchStreams;
    struct Editor
    {
        void* object;
        HWND group = nullptr;
        // Values, control IDs and HWND arrays all use permanent storage indexes.
        std::array<HWND, OptionCount> controls{};
        std::array<HWND, OptionCount> labels{};
        std::array<HWND, OptionCount> readouts{};
        std::vector<HWND> navigationParents;
    };

    bool Valid(const Extension& extension)
    {
        if (memcmp(extension.magic, "PLUS", 4) != 0) return false;
        for (const auto& option : Options)
            if (extension.values[ToIndex(option.index)] > option.maximum) return false;
        return true;
    }

    Extension CurrentExtension() { return Extension{ { 'P', 'L', 'U', 'S' }, values }; }

    std::wstring ValueText(const Option& option, HMODULE resourceModule = GetModuleHandleW(nullptr))
    {
        const size_t index = ToIndex(option.index);
        const UINT stringId = values[index] == 0 ? option.zeroStringId :
            values[index] == option.maximum ? option.maximumStringId : 0;
        if (stringId)
        {
            wchar_t text[128]{};
            if (LoadStringW(resourceModule, stringId, text, 128)) return text;
            return stringId == 141 ? L"No" : stringId == 4950 ? L"Unlimited" : L"Default";
        }
        return std::to_wstring(values[index]);
    }

    void RefreshEditors()
    {
        for (HWND window : editors)
            if (auto editor = static_cast<Editor*>(GetPropW(window, ContextProperty)))
                for (const auto& option : Options)
                {
                    const size_t i = ToIndex(option.index);
                    if (option.IsSlider())
                    {
                        SendMessageW(editor->controls[i], TBM_SETPOS, TRUE, values[i]);
                        SetWindowTextW(editor->readouts[i], ValueText(option).c_str());
                    }
                    else
                        SendMessageW(editor->controls[i], BM_SETCHECK, values[i] ? BST_CHECKED : BST_UNCHECKED, 0);
                }
    }

    void __cdecl ResetDefault()
    {
        values.fill(0);
        RefreshEditors();
    }

    // Native defaults are inlined copies at nine verified sites, including the
    // dropdown, Default button, editor construction and local/network setup.
    // Preserve every register and flags before continuing the original copy.
#define DEFAULT_DETOUR(N, OFFSET) \
    __declspec(naked) void Default##N() \
    { \
        __asm { pushfd } \
        __asm { pushad } \
        __asm { call ResetDefault } \
        __asm { popad } \
        __asm { popfd } \
        __asm { jmp dword ptr [defaultTrampolines + OFFSET] } \
    }
    DEFAULT_DETOUR(0, 0)
    DEFAULT_DETOUR(1, 4)
    DEFAULT_DETOUR(2, 8)
    DEFAULT_DETOUR(3, 12)
    DEFAULT_DETOUR(4, 16)
    DEFAULT_DETOUR(5, 20)
    DEFAULT_DETOUR(6, 24)
    DEFAULT_DETOUR(7, 28)
    DEFAULT_DETOUR(8, 32)
#undef DEFAULT_DETOUR

    void RestoreNavigation(Editor& editor)
    {
        for (HWND parent : editor.navigationParents)
            if (IsWindow(parent))
                SetWindowLongPtrW(parent, GWL_EXSTYLE, GetWindowLongPtrW(parent, GWL_EXSTYLE) & ~WS_EX_CONTROLPARENT);
    }

    void SetValue(Editor& editor, const Option& option, unsigned char value)
    {
        const size_t index = ToIndex(option.index);
        if (value > option.maximum || values[index] == value) return;
        values[index] = value;
        // Use the native CString/Save As/Default/lobby notification path.
        markEdited(editor.object);
        RefreshEditors();
    }

    LRESULT CALLBACK EditorProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam,
        UINT_PTR id, DWORD_PTR reference)
    {
        auto editor = reinterpret_cast<Editor*>(reference);
        const int index = static_cast<int>(LOWORD(wParam)) - FirstCheckId;
        const Option* option = index >= 0 && index < static_cast<int>(OptionCount) ? FindOption(index) : nullptr;
        if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && index >= 0 &&
            option && !option->IsSlider() &&
            reinterpret_cast<HWND>(lParam) == editor->controls[index])
        {
            const unsigned char value = SendMessageW(editor->controls[index], BM_GETCHECK, 0, 0) == BST_CHECKED ? 1 : 0;
            SetValue(*editor, *option, value);
            return 0;
        }
        if (message == WM_HSCROLL && lParam)
            for (const auto& slider : Options)
            {
                const size_t i = ToIndex(slider.index);
                if (slider.IsSlider() && reinterpret_cast<HWND>(lParam) == editor->controls[i])
                {
                    const auto value = SendMessageW(editor->controls[i], TBM_GETPOS, 0, 0);
                    if (value >= 0 && value <= slider.maximum)
                        SetValue(*editor, slider, static_cast<unsigned char>(value));
                    return 0;
                }
            }
        if (message == WM_NCDESTROY)
        {
            RemoveWindowSubclass(window, EditorProc, id);
            RemovePropW(window, ContextProperty);
            editors.erase(std::remove(editors.begin(), editors.end(), window), editors.end());
            RestoreNavigation(*editor);
            delete editor;
        }
        return DefSubclassProc(window, message, wParam, lParam);
    }

    bool AttachEditor(void* object, const Option* layout = Options)
    {
        HWND window = *reinterpret_cast<HWND*>(static_cast<BYTE*>(object) + 0x1c);
        if (!IsWindow(window) || GetPropW(window, ContextProperty)) return false;
        auto editor = new (std::nothrow) Editor{};
        if (!editor) return false;
        editor->object = object;
        // Append below the 468-DLU native form; direct children participate in
        // CScrollView's existing ScrollWindow and dialog keyboard traversal.
        HFONT font = reinterpret_cast<HFONT>(SendMessageW(GetDlgItem(window, 2087), WM_GETFONT, 0, 0));
        if (!font) font = reinterpret_cast<HFONT>(SendMessageW(window, WM_GETFONT, 0, 0));
        DWORD sliderBorder = WS_BORDER;
        DWORD sliderBorderEx = 0;
        for (HWND child = GetWindow(window, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
        {
            wchar_t type[64]{};
            GetClassNameW(child, type, 64);
            if (_wcsicmp(type, TRACKBAR_CLASSW) != 0) continue;
            sliderBorder = static_cast<DWORD>(GetWindowLongPtrW(child, GWL_STYLE)) & WS_BORDER;
            sliderBorderEx = static_cast<DWORD>(GetWindowLongPtrW(child, GWL_EXSTYLE)) & (WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
            break;
        }
        RECT rowPitch{ 0, FirstRowTop, 0, FirstRowTop + RowHeight };
        MapDialogRect(window, &rowPitch);
        const int blankHeight = (rowPitch.bottom - rowPitch.top + 1) / 2;
        const auto create = [&](const wchar_t* type, const wchar_t* text, DWORD style, int controlId, RECT rect,
            DWORD extendedStyle = 0, int blankLines = 0, int bottomBlankLines = 0) {
            if (!MapDialogRect(window, &rect)) return static_cast<HWND>(nullptr);
            rect.top += blankLines * blankHeight;
            rect.bottom += (blankLines + bottomBlankLines) * blankHeight;
            HWND control = CreateWindowExW(extendedStyle, type, text, WS_CHILD | WS_VISIBLE | style,
                rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, window,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(controlId)), GetModuleHandleW(nullptr), nullptr);
            if (control) SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            return control;
        };
        editor->group = create(L"BUTTON", strings.strExtendedOptions, BS_GROUPBOX, FirstCheckId - 1,
            RECT{ 8, 476, 383, GroupBottom }, 0, 0, 2);
        bool success = editor->group != nullptr;
        std::array<int, 2> sliderSpacing{};
        for (size_t position = 0; position < OptionCount && success; ++position)
        {
            const auto& option = layout[position];
            const size_t i = ToIndex(option.index);
            const int y = FirstRowTop + option.row * RowHeight + sliderSpacing[option.column];
            const int x = option.column == 0 ? 14 : 205;
            const int id = FirstCheckId + static_cast<int>(i);
            if (option.IsSlider())
            {
                editor->labels[i] = create(L"STATIC", *option.label, SS_LEFT, FirstCheckId + 100 + static_cast<int>(i),
                    RECT{ x, y, x + 171, y + LabelHeight }, 0, option.blankLines);
                editor->controls[i] = create(TRACKBAR_CLASSW, L"", WS_TABSTOP | TBS_AUTOTICKS | TBS_TOP | sliderBorder, id,
                    RECT{ x, y + SliderTop, x + 100, y + SliderTop + SliderHeight }, sliderBorderEx, option.blankLines);
                editor->readouts[i] = create(L"STATIC", L"", SS_LEFT, FirstCheckId + 200 + static_cast<int>(i),
                    RECT{ x + 104, y + SliderReadoutTop, x + 171, y + SliderReadoutTop + LabelHeight }, 0, option.blankLines);
                success = editor->labels[i] && editor->controls[i] && editor->readouts[i];
                if (success)
                {
                    SendMessageW(editor->controls[i], TBM_SETRANGE, TRUE, MAKELPARAM(0, option.maximum));
                    SendMessageW(editor->controls[i], TBM_SETTICFREQ, 10, 0);
                    SendMessageW(editor->controls[i], TBM_SETPAGESIZE, 0, 10);
                }
                sliderSpacing[option.column] += SliderExtraHeight;
            }
            else
            {
                editor->controls[i] = create(L"BUTTON", *option.label, BS_AUTOCHECKBOX | WS_TABSTOP, id,
                    RECT{ x, y, x + 171, y + CheckboxHeight }, 0, option.blankLines);
                success = editor->controls[i] != nullptr;
            }
        }
        for (HWND parent = window; success && parent && (GetWindowLongPtrW(parent, GWL_STYLE) & WS_CHILD);
            parent = GetParent(parent))
        {
            const LONG_PTR style = GetWindowLongPtrW(parent, GWL_EXSTYLE);
            if (style & WS_EX_CONTROLPARENT) continue;
            SetWindowLongPtrW(parent, GWL_EXSTYLE, style | WS_EX_CONTROLPARENT);
            success = (GetWindowLongPtrW(parent, GWL_EXSTYLE) & WS_EX_CONTROLPARENT) != 0;
            if (success) editor->navigationParents.push_back(parent);
        }
        if (success) success = SetPropW(window, ContextProperty, editor) != FALSE;
        if (success) success = SetWindowSubclass(window, EditorProc, 1, reinterpret_cast<DWORD_PTR>(editor)) != FALSE;
        if (!success)
        {
            RemovePropW(window, ContextProperty);
            for (HWND control : editor->controls) if (control) DestroyWindow(control);
            for (HWND label : editor->labels) if (label) DestroyWindow(label);
            for (HWND readout : editor->readouts) if (readout) DestroyWindow(readout);
            if (editor->group) DestroyWindow(editor->group);
            RestoreNavigation(*editor);
            delete editor;
            return false;
        }
        editors.push_back(window);
        RefreshEditors();
        RECT extra{ 0, 0, 0, ExtraHeight };
        MapDialogRect(window, &extra);
        SIZE total = *reinterpret_cast<SIZE*>(static_cast<BYTE*>(object) + 0x44);
        total.cy += extra.bottom + 2 * blankHeight;
        const SIZE automatic{};
        setScrollSizes(object, MM_TEXT, total, automatic, automatic);
        return true;
    }

    void* __fastcall CreateOptionsPage(void* object, void*, void* parent, const RECT* rect)
    {
        void* result = originalCreatePage(object, parent, rect);
        if (!AttachEditor(object)) OutputDebugStringA("fkSettings: unable to attach Extended Options controls.\n");
        return result;
    }

    std::string SidecarPath(const char* path)
    {
        if (!path) return {};
        const std::string filename(path);
        const auto slash = filename.find_last_of("\\/");
        const auto name = slash == std::string::npos ? filename : filename.substr(slash + 1);
        if (_stricmp(name.c_str(), "game.dat") != 0) return {};
        return (slash == std::string::npos ? std::string{} : filename.substr(0, slash + 1)) + "extended.dat";
    }

    void ForgetStream(void* stream)
    {
        launchStreams.erase(std::remove_if(launchStreams.begin(), launchStreams.end(),
            [=](const LaunchStream& launch) { return launch.stream == stream; }), launchStreams.end());
    }

    void* __cdecl OpenFile(const char* filename, const char* mode)
    {
        const auto sidecar = mode && mode[0] == 'w' ? SidecarPath(filename) : std::string{};
        // Remove previous match settings even when this attempt cannot open game.dat.
        if (!sidecar.empty()) DeleteFileA(sidecar.c_str());
        void* stream = originalOpen(filename, mode);
        if (stream)
        {
            ForgetStream(stream);
            if (!sidecar.empty()) launchStreams.push_back({ stream, sidecar });
        }
        return stream;
    }

    int __cdecl CloseFile(void* stream)
    {
        ForgetStream(stream);
        return originalClose(stream);
    }

    bool SaveSidecar(const std::string& path)
    {
        Extension extension = CurrentExtension();
        // An unset launch has no active extension signature. Schemes and lobby
        // packets retain PLUS so they can restore and transmit zero values.
        if (std::none_of(extension.values.begin(), extension.values.end(), [](unsigned char value) { return value != 0; }))
            memset(extension.magic, 0, sizeof(extension.magic));
        // The original is removed when game.dat opens. Publish only a complete
        // closed file; a failed write cannot leave previous or partial settings.
        const std::string temporary = path + ".tmp";
        HANDLE file = CreateFileA(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return false;
        DWORD written = 0;
        bool success = ::WriteFile(file, &extension, sizeof(extension), &written, nullptr) && written == sizeof(extension);
        if (!CloseHandle(file)) success = false;
        if (success) success = MoveFileExA(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
        if (!success) DeleteFileA(temporary.c_str());
        return success;
    }

    bool InstallInImage(BYTE* frontend)
    {
        enabled = false;
        image = frontend;
        auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
        auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(image + dos->e_lfanew);
        if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
            nt->FileHeader.TimeDateStamp != 0x3587be19 || nt->OptionalHeader.SizeOfImage != 0x5b8000) return false;
        struct Hook { size_t rva; const char* signature; size_t length; void* detour; void** original; };
        const Hook hooks[] = {
            { 0x57d20, "\x55\x8b\xec\x6a\xff\x68", 6, reinterpret_cast<void*>(CreateOptionsPage), reinterpret_cast<void**>(&originalCreatePage) },
            { 0x97780, "\x8b\x44\x24\x08\x8b\x4c\x24\x04", 8, reinterpret_cast<void*>(OpenFile), reinterpret_cast<void**>(&originalOpen) },
            { 0x97500, "\x56\x8b\x74\x24\x08\x57\x83\xcf\xff", 9, reinterpret_cast<void*>(CloseFile), reinterpret_cast<void**>(&originalClose) },
        };
        const size_t defaultSites[] = { 0x9cdc, 0x9ff8, 0xa5d4, 0x31cd9, 0x392c0, 0x586a3, 0x5899e, 0x59611, 0x59e3d };
        void* detours[] = { Default0, Default1, Default2, Default3, Default4, Default5, Default6, Default7, Default8 };
        BYTE copy[] = { 0xb9, 0x20, 0, 0, 0, 0xbe, 0, 0, 0, 0, 0xbf, 0, 0, 0, 0, 0xf3, 0xa5 };
        const uint32_t source = reinterpret_cast<uint32_t>(image + 0x186358);
        const uint32_t destination = reinterpret_cast<uint32_t>(image + OptionsRva);
        memcpy(copy + 6, &source, 4);
        memcpy(copy + 11, &destination, 4);
        for (const auto& hook : hooks)
            if (memcmp(image + hook.rva, hook.signature, hook.length) != 0) return false;
        for (const auto rva : defaultSites)
            if (memcmp(image + rva, copy, sizeof(copy)) != 0) return false;
        if (memcmp(image + 0xcf5b5, "\xb8\x44\x1b", 3) != 0 ||
            memcmp(image + 0x57e6f, "\x55\x8b\xec\x83\xec\x08", 6) != 0) return false;
        std::vector<void*> created;
        bool success = true;
        for (const auto& hook : hooks)
        {
            if (MH_CreateHook(image + hook.rva, hook.detour, hook.original) != MH_OK) { success = false; break; }
            created.push_back(image + hook.rva);
        }
        for (size_t i = 0; i < 9 && success; ++i)
        {
            if (MH_CreateHook(image + defaultSites[i], detours[i], &defaultTrampolines[i]) != MH_OK) { success = false; break; }
            created.push_back(image + defaultSites[i]);
        }
        setScrollSizes = reinterpret_cast<SetScrollSizes>(image + 0xcf5b5);
        markEdited = reinterpret_cast<MarkEdited>(image + 0x57e6f);
        if (success)
            for (void* target : created)
                if (MH_EnableHook(target) != MH_OK) { success = false; break; }
        if (!success)
            for (void* target : created) { MH_DisableHook(target); MH_RemoveHook(target); }
        enabled = success;
        return success;
    }
}

void SetLanguage(const std::string& language)
{
    strings = MakeOptionStrings(language);
    for (HWND window : editors)
        if (auto editor = static_cast<Editor*>(GetPropW(window, ContextProperty)))
        {
            SetWindowTextW(editor->group, strings.strExtendedOptions);
            for (const auto& option : Options)
            {
                const size_t index = ToIndex(option.index);
                SetWindowTextW(option.IsSlider() ? editor->labels[index] : editor->controls[index], *option.label);
            }
        }
}

bool Install() { return InstallInImage(reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr))); }

void ReadScheme(void* buffer, size_t size, size_t count, void* stream, size_t result, Read read)
{
    if (!enabled || buffer != image + OptionsRva || size != NativePayloadSize || count != 1) return;
    values.fill(0);
    Extension extension{};
    if (result == count && read(&extension, 1, sizeof(extension), stream) == sizeof(extension) && Valid(extension))
        values = extension.values;
    RefreshEditors();
}

bool WriteScheme(const void* buffer, size_t size, size_t count, void* stream, size_t result, Write write)
{
    if (!enabled || buffer != image + OptionsRva || size != NativePayloadSize || count != 1 || result != count) return true;
    const Extension extension = CurrentExtension();
    return write(&extension, 1, sizeof(extension), stream) == sizeof(extension);
}

bool WriteLaunchData(void* stream, size_t size, size_t count, size_t result)
{
    if (!enabled || size != 1 || count != 0xcd4 || result != count) return true;
    for (const auto& launch : launchStreams)
        if (launch.stream == stream && !SaveSidecar(launch.sidecar))
        {
            OutputDebugStringA("fkSettings: failed to write extended.dat; game data write reported as failed.\n");
            return false;
        }
    return true;
}

bool ExtendPacket(const void* packet, uint32_t length, Packet& extended)
{
    uint32_t type = 0;
    if (!enabled || !packet || length != NativePacketSize) return false;
    memcpy(&type, packet, sizeof(type));
    if (type != 0x19) return false;
    memcpy(extended.data(), packet, length);
    const Extension extension = CurrentExtension();
    memcpy(extended.data() + length, &extension, sizeof(extension));
    return true;
}

bool ReceivePacket(uint32_t sender, uint32_t host, const void* packet, uint32_t length)
{
    uint32_t type = 0;
    if (!enabled || !packet || length < sizeof(type)) return true;
    memcpy(&type, packet, sizeof(type));
    if (type != 0x19) return true;
    if (length < NativePacketSize) return false;
    if (sender == host)
    {
        values.fill(0);
        if (length == Packet{}.size())
        {
            Extension extension;
            memcpy(&extension, static_cast<const BYTE*>(packet) + NativePacketSize, sizeof(extension));
            if (Valid(extension)) values = extension.values;
        }
        RefreshEditors();
    }
    return true;
}
}
