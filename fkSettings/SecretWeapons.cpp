#define NOMINMAX
typedef struct IUnknown IUnknown;
#include <Windows.h>
#include <commctrl.h>
#include <array>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <new>
#include <vector>
#include "include/MinHook.h"
#include "SecretWeapons.h"
#include "NetworkTeams.h"

#pragma comment(lib, "comctl32.lib")

namespace SecretWeapons
{
namespace
{
    constexpr int NativeWeaponCount = 38;
    constexpr size_t SecretWeaponCount = 8;
    constexpr size_t WeaponRecordSize = 0x8c;
    constexpr size_t NativeSchemePayloadSize = NativeWeaponCount * WeaponRecordSize;
    struct Weapon
    {
        size_t stockOffset;
        UINT stringId;
        const char* fallbackName;
    };
    constexpr std::array<Weapon, SecretWeaponCount> Weapons{{
        { 0x49e, 4938, "Salvation Army" },
        { 0x49f, 4939, "MB Bomb" },
        { 0x4a2, 4940, "Sheep Strike" },
        { 0x4a3, 4941, "Carpet Bomb" },
        { 0x4a6, 4942, "Cloned Sheep" },
        { 0x4a7, 4943, "Concrete Donkey" },
        { 0x4aa, 4944, "Nuclear Bomb" },
        { 0x4ab, 4945, "Magic Bullet" },
    }};
    constexpr size_t GameObjectHeaderSize = 0x1c;
    constexpr size_t TeamStride = 0x108;
    constexpr int TeamCount = 6;
    // The native editor's cursor handler loads control ID + 500 as the hint.
    // Reuse the normal stock trackbar ID so it displays string 5501 too.
    constexpr int StockControlId = 5001;
    constexpr uint32_t MaximumStock = 100;
    constexpr wchar_t ContextProperty[] = L"fkSettings.SecretWeapons";
    BYTE* image = nullptr;
    std::array<uint32_t, SecretWeaponCount> secretStocks{}; // 0..100; 10 is unlimited.
    std::vector<HWND> editorWindows;

    bool IsSecretSelection(int index)
    {
        return index >= NativeWeaponCount && index < NativeWeaponCount + static_cast<int>(Weapons.size());
    }

    // Use the frontend's CRT with its own FILE objects, never this DLL's CRT.
    using Read = size_t (__cdecl*)(void*, size_t, size_t, void*);
    using Write = size_t (__cdecl*)(const void*, size_t, size_t, void*);
    using Init = BOOL (__thiscall*)(void*);
    using Select = void (__thiscall*)(void*);
    using Selection = int (__thiscall*)(void*);
    using PrepareStocks = void (__thiscall*)(void*, int);
    using LoadNativeString = BOOL (__thiscall*)(void*, UINT);
    using SendToPlayer = void (__thiscall*)(void*, uint32_t, uint32_t, const void*, uint32_t);
    using SendToAll = void (__thiscall*)(void*, uint32_t, const void*, uint32_t);
    using ReceivePacket = void (__thiscall*)(void*, uint32_t, const void*, uint32_t);
    Read originalRead = nullptr;
    Write originalWrite = nullptr;
    Init originalInit = nullptr;
    Select originalSelect = nullptr;
    Select originalDefault = nullptr;
    Selection originalSelection = nullptr;
    PrepareStocks originalPrepareStocks = nullptr;
    LoadNativeString loadNativeString = nullptr;
    SendToPlayer originalSendToPlayer = nullptr;
    SendToAll originalSendToAll = nullptr;
    ReceivePacket originalReceivePacket = nullptr;

    struct Editor
    {
        void* object;
        HWND window;
        HWND list;
        HWND panel;
        HWND slider;
        HWND value;
        int nativeSelection = 0;
        bool secretSelected = false;
        size_t secretIndex = 0;
        std::vector<HWND> navigationParents;
    };

    void RestoreNavigationParents(Editor& editor)
    {
        for (HWND parent : editor.navigationParents)
            if (IsWindow(parent))
                SetWindowLongPtrW(parent, GWL_EXSTYLE,
                    GetWindowLongPtrW(parent, GWL_EXSTYLE) & ~WS_EX_CONTROLPARENT);
        editor.navigationParents.clear();
    }

    bool ConfigureNavigationParents(Editor& editor)
    {
        // The stock slider must be reachable from the outer dialog. Mark every
        // intervening child container, not just our panel. Otherwise the dialog
        // manager loops forever when deactivation walks back to the focused slider.
        for (HWND parent = editor.window; parent &&
            (GetWindowLongPtrW(parent, GWL_STYLE) & WS_CHILD); parent = GetParent(parent))
        {
            const LONG_PTR style = GetWindowLongPtrW(parent, GWL_EXSTYLE);
            if (style & WS_EX_CONTROLPARENT)
                continue;
            SetWindowLongPtrW(parent, GWL_EXSTYLE, style | WS_EX_CONTROLPARENT);
            // Style notifications run window procedures which may change last
            // error even after a successful write. Check the resulting style.
            if (!(GetWindowLongPtrW(parent, GWL_EXSTYLE) & WS_EX_CONTROLPARENT))
            {
                RestoreNavigationParents(editor);
                return false;
            }
            editor.navigationParents.push_back(parent);
        }
        return true;
    }

    HWND ObjectWindow(void* object)
    {
        return *reinterpret_cast<HWND*>(static_cast<BYTE*>(object) + 0x1c);
    }

    Editor* GetEditor(HWND window)
    {
        return static_cast<Editor*>(GetPropW(window, ContextProperty));
    }

    void Refresh(Editor& editor)
    {
        const auto stock = secretStocks[editor.secretIndex];
        SendMessageW(editor.slider, TBM_SETPOS, TRUE, stock);
        char text[128] = {};
        if (stock == 10)
        {
            if (!LoadStringA(GetModuleHandleW(nullptr), 4950, text, sizeof(text)))
                strcpy_s(text, "Unlimited");
        }
        else
            sprintf_s(text, "%u", stock);
        SetWindowTextA(editor.value, text);
    }

    void ShowNativePage(Editor& editor, bool show)
    {
        const int index = *reinterpret_cast<int*>(static_cast<BYTE*>(editor.object) + 0x9c);
        if (index < 0 || index >= NativeWeaponCount)
            return;
        auto page = *reinterpret_cast<void**>(static_cast<BYTE*>(editor.object) + 0xa4 + index * 4);
        if (page && IsWindow(ObjectWindow(page)))
            ShowWindow(ObjectWindow(page), show ? SW_SHOW : SW_HIDE);
    }

    void RefreshEditors()
    {
        // Scheme loading can originate on Game controls while this editor is hidden.
        for (HWND window : editorWindows)
            if (auto editor = GetEditor(window))
            {
                Refresh(*editor);
                if (editor->secretSelected)
                    ShowNativePage(*editor, false);
            }
    }

    void MarkUserDefined(Editor& editor)
    {
        // The scheme name is a CString owned by the frontend's MFC runtime.
        // Use its string loader so the native game-controls page sees the edit too.
        loadNativeString(image + 0x187644, 138);
        // Native edits retain the editor's selected scheme as the Save As source.
        EnableWindow(GetDlgItem(editor.window, 1002), TRUE); // Save As
        EnableWindow(GetDlgItem(editor.window, 1003), TRUE); // Default
        SendMessageW(GetAncestor(editor.window, GA_ROOT), WM_USER + 23, 0, 0);
    }

    LRESULT CALLBACK PanelProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam,
        UINT_PTR, DWORD_PTR reference)
    {
        auto& editor = *reinterpret_cast<Editor*>(reference);
        if (message == WM_ERASEBKGND)
        {
            RECT rect;
            GetClientRect(window, &rect);
            FillRect(reinterpret_cast<HDC>(wParam), &rect, GetSysColorBrush(COLOR_BTNFACE));
            return 1;
        }
        if (message == WM_CTLCOLORSTATIC)
        {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetBkColor(dc, GetSysColor(COLOR_BTNFACE));
            SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
            SetBkMode(dc, TRANSPARENT);
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_BTNFACE));
        }
        if (message == WM_HSCROLL && reinterpret_cast<HWND>(lParam) == editor.slider)
        {
            const auto stock = static_cast<uint32_t>(SendMessageW(editor.slider, TBM_GETPOS, 0, 0));
            if (stock != secretStocks[editor.secretIndex])
            {
                secretStocks[editor.secretIndex] = stock;
                Refresh(editor);
                // Stock is already in our backing state. The native dirty flag
                // means a native page has controls to commit on WM_SHOWWINDOW
                // (including Alt+Tab); it must not be set for this Win32 panel.
                MarkUserDefined(editor);
            }
            return 0;
        }
        return DefSubclassProc(window, message, wParam, lParam);
    }

    LRESULT CALLBACK EditorProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam,
        UINT_PTR subclassId, DWORD_PTR reference)
    {
        auto editor = reinterpret_cast<Editor*>(reference);
        if (message == WM_CTLCOLORSTATIC && reinterpret_cast<HWND>(lParam) == editor->panel)
        {
            SetBkColor(reinterpret_cast<HDC>(wParam), GetSysColor(COLOR_BTNFACE));
            return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_BTNFACE));
        }
        if (message == WM_NCDESTROY)
        {
            RemovePropW(window, ContextProperty);
            editorWindows.erase(std::remove(editorWindows.begin(), editorWindows.end(), window), editorWindows.end());
            RemoveWindowSubclass(window, EditorProc, subclassId);
            RestoreNavigationParents(*editor);
            const LRESULT result = DefSubclassProc(window, message, wParam, lParam);
            delete editor;
            return result;
        }
        const LRESULT result = DefSubclassProc(window, message, wParam, lParam);
        // Scheme changes/defaults can run from nested messages. Read current state.
        if (message == WM_COMMAND && GetEditor(window) == editor)
        {
            Refresh(*editor);
            if (editor->secretSelected)
                ShowNativePage(*editor, false);
        }
        return result;
    }

    void Attach(void* object)
    {
        HWND window = ObjectWindow(object);
        HWND list = GetDlgItem(window, 2014);
        HWND frame = GetDlgItem(window, 1004);
        // The native object has exactly 38 page pointers. Never enlarge that array.
        if (!list || !frame || GetEditor(window) || SendMessageW(list, LB_GETCOUNT, 0, 0) != NativeWeaponCount)
            return;
        Editor* editor = new (std::nothrow) Editor();
        if (!editor)
            return;
        editor->object = object;
        editor->window = window;
        editor->list = list;
        editor->nativeSelection = static_cast<int>(SendMessageW(list, LB_GETCURSEL, 0, 0));
        RECT rect;
        GetWindowRect(frame, &rect);
        MapWindowPoints(nullptr, window, reinterpret_cast<POINT*>(&rect), 2);
        HFONT font = reinterpret_cast<HFONT>(SendMessageW(list, WM_GETFONT, 0, 0));
        HINSTANCE instance = GetModuleHandleW(nullptr);
        editor->panel = CreateWindowExW(WS_EX_CONTROLPARENT, L"STATIC", L"",
            WS_CHILD | WS_CLIPCHILDREN, rect.left, rect.top, rect.right - rect.left,
            rect.bottom - rect.top, window, nullptr, instance, nullptr);
        // These fallback rectangles are the native stock row's dialog units.
        // Prefer the live native controls: their page origin and font scaling
        // have already been applied by the frontend's layout code.
        std::array<RECT, 3> stockRects{{ { 7, 0, 129, 8 }, { 7, 12, 169, 34 }, { 175, 14, 225, 22 } }};
        for (auto& stockRect : stockRects) MapDialogRect(window, &stockRect);
        for (int i = 0; i < NativeWeaponCount; ++i)
        {
            auto page = *reinterpret_cast<void**>(static_cast<BYTE*>(object) + 0xa4 + i * 4);
            if (!page) continue;
            HWND pageWindow = ObjectWindow(page);
            const std::array<HWND, 3> controls{
                GetDlgItem(pageWindow, 5000), GetDlgItem(pageWindow, 5001), GetDlgItem(pageWindow, 5002) };
            if (std::any_of(controls.begin(), controls.end(), [](HWND control) { return !control; })) continue;
            for (size_t control = 0; control < controls.size(); ++control)
            {
                GetWindowRect(controls[control], &stockRects[control]);
                MapWindowPoints(nullptr, editor->panel, reinterpret_cast<POINT*>(&stockRects[control]), 2);
            }
            break;
        }
        auto create = [&](const char* type, const char* text, DWORD style,
            const RECT& bounds, int id)
        {
            HWND control = CreateWindowExA(0, type, text,
                WS_CHILD | WS_VISIBLE | style, bounds.left, bounds.top,
                bounds.right - bounds.left, bounds.bottom - bounds.top, editor->panel,
                reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
            if (control)
                SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            return control;
        };
        char stockLabel[128] = {};
        if (!LoadStringA(instance, 5000, stockLabel, sizeof(stockLabel)))
            strcpy_s(stockLabel, "Initial stock");
        create("STATIC", stockLabel, 0, stockRects[0], 5000);
        editor->slider = create(TRACKBAR_CLASSA, "", WS_TABSTOP | TBS_AUTOTICKS | TBS_TOP,
            stockRects[1], StockControlId);
        editor->value = create("STATIC", "", 0, stockRects[2], 5002);
        if (!editor->panel || !editor->slider || !editor->value ||
            !SetPropW(window, ContextProperty, editor) ||
            !SetWindowSubclass(editor->panel, PanelProc, 1, reinterpret_cast<DWORD_PTR>(editor)) ||
            !SetWindowSubclass(window, EditorProc, 1, reinterpret_cast<DWORD_PTR>(editor)))
        {
            RemovePropW(window, ContextProperty);
            if (editor->panel) DestroyWindow(editor->panel);
            delete editor;
            return;
        }
        SendMessageW(editor->slider, TBM_SETRANGE, TRUE, MAKELPARAM(0, MaximumStock));
        SendMessageW(editor->slider, TBM_SETTICFREQ, 10, 0);
        Refresh(*editor);
        bool appended = true;
        for (size_t i = 0; i < Weapons.size(); ++i)
        {
            char name[128] = {};
            if (!LoadStringA(instance, Weapons[i].stringId, name, sizeof(name)))
                strcpy_s(name, Weapons[i].fallbackName);
            if (SendMessageA(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name)) !=
                NativeWeaponCount + static_cast<LRESULT>(i))
            {
                appended = false;
                break;
            }
        }
        if (!appended || !ConfigureNavigationParents(*editor))
        {
            while (SendMessageW(list, LB_GETCOUNT, 0, 0) > NativeWeaponCount)
                SendMessageW(list, LB_DELETESTRING, NativeWeaponCount, 0);
            RemoveWindowSubclass(window, EditorProc, 1);
            RemovePropW(window, ContextProperty);
            DestroyWindow(editor->panel);
            delete editor;
            return;
        }
        editorWindows.push_back(window);
    }

    BOOL __fastcall InitEditor(void* object, void*)
    {
        const BOOL result = originalInit(object);
        Attach(object);
        return result;
    }

    int __fastcall GetSelection(void* listObject, void*)
    {
        HWND list = ObjectWindow(listObject);
        auto editor = GetEditor(GetParent(list));
        // Save, load, Apply, and keyboard handlers also index the native page array.
        if (editor && editor->list == list &&
            IsSecretSelection(static_cast<int>(SendMessageW(list, LB_GETCURSEL, 0, 0))))
            return editor->nativeSelection;
        return originalSelection(listObject);
    }

    void __fastcall SelectWeapon(void* object, void*)
    {
        auto editor = GetEditor(ObjectWindow(object));
        if (!editor)
        {
            originalSelect(object);
            return;
        }
        const int index = static_cast<int>(SendMessageW(editor->list, LB_GETCURSEL, 0, 0));
        if (IsSecretSelection(index))
        {
            if (!editor->secretSelected)
            {
                // Keep the native handler's last valid page selection intact.
                originalSelect(object);
                ShowNativePage(*editor, false);
            }
            editor->secretSelected = true;
            editor->secretIndex = index - NativeWeaponCount;
            Refresh(*editor);
            ShowWindow(editor->panel, SW_SHOW);
            return;
        }
        if (editor->secretSelected)
        {
            editor->secretSelected = false;
            if (IsChild(editor->panel, GetFocus()))
                SetFocus(editor->list);
            ShowWindow(editor->panel, SW_HIDE);
            ShowNativePage(*editor, true);
        }
        if (index >= 0 && index < NativeWeaponCount)
            editor->nativeSelection = index;
        originalSelect(object);
    }

    void __fastcall LoadDefault(void* object, void*)
    {
        originalDefault(object);
        secretStocks.fill(0);
        RefreshEditors();
    }

    bool IsSchemePayload(const void* buffer, size_t size, size_t count)
    {
        return buffer == image + 0x187648 &&
            ((size == WeaponRecordSize && count == NativeWeaponCount) ||
                (size == 1 && count == NativeSchemePayloadSize));
    }

    // Little-endian trailer at file offset 0x14e0. Native readers stop before it.
    struct SchemeExtension
    {
        char magic[4];
        std::array<uint32_t, SecretWeaponCount> stocks;
    };
    static_assert(sizeof(SchemeExtension) == 36, "SchemeExtension must be 36 bytes");

    bool ValidExtension(const SchemeExtension& extension)
    {
        return memcmp(extension.magic, "PLUS", 4) == 0 &&
            std::all_of(extension.stocks.begin(), extension.stocks.end(), [](uint32_t stock) { return stock <= MaximumStock; });
    }

    constexpr uint32_t WeaponSchemePacketType = 0x18;
    constexpr size_t NativeWeaponPacketSize = 4 + NativeWeaponCount * 0x2a; // 0x640
    using ExtendedWeaponPacket = std::array<BYTE, NativeWeaponPacketSize + sizeof(SchemeExtension)>;

    bool IsWeaponPacket(const void* packet, uint32_t length)
    {
        uint32_t type = 0;
        if (!packet || length < sizeof(type)) return false;
        memcpy(&type, packet, sizeof(type));
        return type == WeaponSchemePacketType;
    }

    bool ExtendWeaponPacket(const void* packet, uint32_t length, ExtendedWeaponPacket& extended)
    {
        if (length != NativeWeaponPacketSize || !IsWeaponPacket(packet, length)) return false;
        memcpy(extended.data(), packet, NativeWeaponPacketSize);
        SchemeExtension extension = {};
        memcpy(extension.magic, "PLUS", 4);
        extension.stocks = secretStocks;
        memcpy(extended.data() + NativeWeaponPacketSize, &extension, sizeof(extension));
        return true;
    }

    // Both transports copy the packet before returning, just as they do with
    // the native serializer's stack buffer. Share these detours with network
    // computer teams so weapon schemes, lobby icons and AI launch settings coexist.
    void __fastcall SendWeaponPacketToPlayer(void* object, void*, uint32_t session,
        uint32_t player, const void* packet, uint32_t length)
    {
        ExtendedWeaponPacket extended;
        NetworkTeams::StartPacket start;
        NetworkTeams::LobbyPacket lobby;
        if (ExtendWeaponPacket(packet, length, extended))
            originalSendToPlayer(object, session, player, extended.data(), static_cast<uint32_t>(extended.size()));
        else if (NetworkTeams::ExtendStartPacket(packet, length, start))
            originalSendToPlayer(object, session, player, start.data(), static_cast<uint32_t>(start.size()));
        else if (const auto lobbyLength = NetworkTeams::ExtendLobbyPacket(packet, length, lobby))
            originalSendToPlayer(object, session, player, lobby.data(), lobbyLength);
        else
            originalSendToPlayer(object, session, player, packet, length);
    }

    void __fastcall SendWeaponPacketToAll(void* object, void*, uint32_t session,
        const void* packet, uint32_t length)
    {
        ExtendedWeaponPacket extended;
        NetworkTeams::StartPacket start;
        NetworkTeams::LobbyPacket lobby;
        if (ExtendWeaponPacket(packet, length, extended))
            originalSendToAll(object, session, extended.data(), static_cast<uint32_t>(extended.size()));
        else if (NetworkTeams::ExtendStartPacket(packet, length, start))
            originalSendToAll(object, session, start.data(), static_cast<uint32_t>(start.size()));
        else if (const auto lobbyLength = NetworkTeams::ExtendLobbyPacket(packet, length, lobby))
            originalSendToAll(object, session, lobby.data(), lobbyLength);
        else
            originalSendToAll(object, session, packet, length);
    }

    void __fastcall ReceiveWeaponPacket(void* object, void*, uint32_t sender,
        const void* packet, uint32_t length)
    {
        // Reliable receive (0x12bc2) advances past its sequence DWORD without
        // subtracting it from the reported length. Raw receive (0x12b6c) does
        // neither. Bound all extension reads by the actual payload length.
        const bool reliable = *reinterpret_cast<uint32_t*>(image + 0x188b14) == 1;
        const uint32_t payloadLength = reliable ? (length >= 4 ? length - 4 : 0) : length;
        const uint32_t host = *reinterpret_cast<uint32_t*>(static_cast<BYTE*>(object) + 0x163c);
        NetworkTeams::ReceiveStartPacket(sender, host, packet, payloadLength);
        uint32_t packetType = 0;
        if (packet && payloadLength >= sizeof(packetType)) memcpy(&packetType, packet, sizeof(packetType));
        if (packetType == 14 && payloadLength < NetworkTeams::NativeStartPacketSize) return;
        const auto lobbySize = NetworkTeams::LobbyPacketSize(packetType);
        if (lobbySize && payloadLength < lobbySize) return;
        if (!IsWeaponPacket(packet, payloadLength))
        {
            if (lobbySize) NetworkTeams::SetJoiningLobby(object);
            originalReceivePacket(object, sender, packet, length);
            if (lobbySize) NetworkTeams::ReceiveLobbyPacket(object, sender, packet, payloadLength);
            return;
        }
        // This lobby dispatcher runs on the UI thread and receives the real
        // transport length. The native weapon decoder itself has no length.
        if (payloadLength < NativeWeaponPacketSize) return;
        if (sender == host)
        {
            secretStocks.fill(0);
            if (payloadLength == std::tuple_size<ExtendedWeaponPacket>::value)
            {
                SchemeExtension extension;
                memcpy(&extension, static_cast<const BYTE*>(packet) + NativeWeaponPacketSize, sizeof(extension));
                if (ValidExtension(extension)) secretStocks = extension.stocks;
            }
        }
        originalReceivePacket(object, sender, packet, static_cast<uint32_t>(NativeWeaponPacketSize));
        if (sender == host) RefreshEditors();
    }

    size_t __cdecl ReadFile(void* buffer, size_t size, size_t count, void* stream)
    {
        const size_t result = originalRead(buffer, size, count, stream);
        if (IsSchemePayload(buffer, size, count))
        {
            secretStocks.fill(0); // Legacy, truncated, or unsupported extensions never inherit stock.
            if (result == count)
            {
                SchemeExtension extension{};
                if (originalRead(&extension, 1, sizeof(extension), stream) == sizeof(extension) &&
                    ValidExtension(extension))
                    secretStocks = extension.stocks;
            }
            RefreshEditors();
        }
        return result;
    }

    size_t __cdecl WriteFile(const void* buffer, size_t size, size_t count, void* stream)
    {
        const size_t result = originalWrite(buffer, size, count, stream);
        if (result == count && IsSchemePayload(buffer, size, count))
        {
            SchemeExtension extension = {};
            memcpy(extension.magic, "PLUS", 4);
            extension.stocks = secretStocks;
            if (originalWrite(&extension, 1, sizeof(extension), stream) != sizeof(extension))
            {
                OutputDebugStringA("fkSettings: failed to save secret weapon scheme extension.\n");
                return 0;
            }
        }
        return result;
    }

    void __fastcall PrepareWeaponStocks(void* object, void*, int mode)
    {
        originalPrepareStocks(object, mode);
        for (size_t weapon = 0; weapon < Weapons.size(); ++weapon)
        {
            const BYTE stock = secretStocks[weapon] == 10 ? 0xff : static_cast<BYTE>(secretStocks[weapon]);
            for (int team = 0; team < TeamCount; ++team)
            {
                BYTE& current = *(static_cast<BYTE*>(object) + GameObjectHeaderSize + Weapons[weapon].stockOffset + team * TeamStride);
                // The native all-weapons cheat fills every slot with unlimited stock.
                if (mode == 0 || mode == -23)
                {
                    if (current != 0xff)
                        current = stock;
                }
                else if (current == 0xff || stock == 0xff)
                    current = 0xff;
                else
                    current = static_cast<BYTE>(current + stock); // Native replenishment semantics.
            }
        }
    }

bool InstallInImage(BYTE* frontendImage)
{
    image = frontendImage;
    auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE)
        return false;
    auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(image + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt->FileHeader.TimeDateStamp != 0x3587be19 || nt->OptionalHeader.SizeOfImage != 0x5b8000)
        return false;

    struct Hook
    {
        size_t rva;
        const char* signature;
        size_t signatureLength;
        void* detour;
        void** original;
    };
    const Hook hooks[] = {
        { 0x89243, "\x55\x8b\xec\x6a\xff\x68\x3a\x04\x4f\x00", 6, reinterpret_cast<void*>(InitEditor), reinterpret_cast<void**>(&originalInit) },
        { 0x89475, "\x55\x8b\xec\x6a\xff\x68\x4e\x04\x4f\x00", 6, reinterpret_cast<void*>(SelectWeapon), reinterpret_cast<void**>(&originalSelect) },
        { 0xd760, "\x55\x8b\xec\x51\x89\x4d\xfc\x6a\x00\x6a\x00\x68\x88\x01\x00\x00", 16, reinterpret_cast<void*>(GetSelection), reinterpret_cast<void**>(&originalSelection) },
        { 0x1e698, "\x55\x8b\xec\x83\xec\x0c\x89\x4d\xf4", 9, reinterpret_cast<void*>(LoadDefault), reinterpret_cast<void**>(&originalDefault) },
        { 0x97bd0, "\x56\x8b\x74\x24\x14\x57\x56\xe8\x54\x6b\x00\x00", 12, reinterpret_cast<void*>(ReadFile), reinterpret_cast<void**>(&originalRead) },
        { 0x975b0, "\x56\x8b\x74\x24\x14\x57\x56\xe8\x74\x71\x00\x00", 12, reinterpret_cast<void*>(WriteFile), reinterpret_cast<void**>(&originalWrite) },
        { 0x2882f, "\x55\x8b\xec\x6a\xff\x68", 6, reinterpret_cast<void*>(PrepareWeaponStocks), reinterpret_cast<void**>(&originalPrepareStocks) },
        { 0x1e29a, "\x55\x8b\xec\x83\xec\x0c\x89\x4d\xf4", 9, reinterpret_cast<void*>(SendWeaponPacketToPlayer), reinterpret_cast<void**>(&originalSendToPlayer) },
        { 0x1e3c4, "\x55\x8b\xec\x83\xec\x0c\x89\x4d\xf4", 9, reinterpret_cast<void*>(SendWeaponPacketToAll), reinterpret_cast<void**>(&originalSendToAll) },
        { 0x39e41, "\x55\x8b\xec\x6a\xff\x68\x6d\xa0\x4e\x00", 10, reinterpret_cast<void*>(ReceiveWeaponPacket), reinterpret_cast<void**>(&originalReceivePacket) },
    };
    if (memcmp(image + 0xc5b02, "\x55\x8b\xec\x81\xec\x04\x01\x00\x00", 9) != 0)
        return false;
    // Check every site before creating any hook; addresses are relative to this executable.
    for (const auto& hook : hooks)
        if (memcmp(image + hook.rva, hook.signature, hook.signatureLength) != 0)
            return false;
    loadNativeString = reinterpret_cast<LoadNativeString>(image + 0xc5b02);
    size_t created = 0;
    for (const auto& hook : hooks)
    {
        if (MH_CreateHook(image + hook.rva, hook.detour, hook.original) != MH_OK)
            break;
        ++created;
    }
    const size_t hookCount = sizeof(hooks) / sizeof(hooks[0]);
    bool success = created == hookCount;
    if (success)
        for (const auto& hook : hooks)
            if (MH_EnableHook(image + hook.rva) != MH_OK)
            {
                success = false;
                break;
            }
    if (!success)
        for (size_t i = 0; i < created; ++i)
        {
            MH_DisableHook(image + hooks[i].rva);
            MH_RemoveHook(image + hooks[i].rva);
        }
    return success;
}
}

bool Install()
{
    return InstallInImage(reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr)));
}
}
