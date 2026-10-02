#include <cstdio>
#include <stdexcept>
#include "../fkSettings/ExtendedOptions.cpp"
#include "../fkSettings/SecretWeapons.cpp"

namespace EO = ExtendedOptions;
namespace SW = SecretWeapons;
static void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
static size_t __cdecl Read(void* b, size_t s, size_t n, void* f) { return fread(b, s, n, static_cast<FILE*>(f)); }
static size_t __cdecl Write(const void* b, size_t s, size_t n, void* f) { return fwrite(b, s, n, static_cast<FILE*>(f)); }
static void* __cdecl Open(const char* path, const char* mode)
{
    FILE* file = nullptr;
    fopen_s(&file, path, mode);
    return file;
}
static int __cdecl Close(void* stream) { return fclose(static_cast<FILE*>(stream)); }
static size_t __cdecl ShortWrite(const void* b, size_t s, size_t n, void* f)
{
    return n == sizeof(EO::Extension) ? 0 : Write(b, s, n, f);
}
static std::array<unsigned char, EO::OptionCount> Pattern(size_t enabledIndex)
{
    std::array<unsigned char, EO::OptionCount> pattern{};
    if (enabledIndex < pattern.size()) pattern[enabledIndex] = 1;
    else if (enabledIndex == pattern.size())
        for (const auto& option : EO::Options) pattern[EO::ToIndex(option.index)] = option.maximum;
    return pattern;
}

static void LanguageTests()
{
    const wchar_t* const english[] = {
        L"God Mode", L"High Jump", L"Change Kamikaze to Suicide Bomber", L"Sheep Heaven",
        L"Super Shopper Crates", L"Extended Fuses/Herds", L"Utilities don't end turn",
        L"Weapons don't end turn", L"Loss of control doesn't end turn", L"Worm select after movement",
        L"Low Gravity", L"Persistent Rope", L"Rapid Play", L"Indestructible Terrain", L"Invisible Terrain",
        L"Fast Crates", L"Crate Spy", L"Crate Limit", L"Crate Rate", L"Aqua Sheep", L"Instant Mines",
        L"Herd weapon: Dynamite", L"Herd weapon: Mine", L"Herd weapon: Ming Vase", L"Herd weapon: Sheep",
        L"Disable Backflip", L"Disable Unlocked Aim"
    };
    EO::values = Pattern(EO::OptionCount);
    const auto before = EO::values;
    for (const char* language : { "en", "", "unknown", " \r\n", " en\r\n", "\xEF\xBB\xBF" "en\r\n" })
    {
        EO::SetLanguage(language);
        for (const auto& option : EO::Options)
            Check(*option.label && wcscmp(*option.label, english[EO::ToIndex(option.index)]) == 0,
                "English and fallback supply every named option label");
        Check(wcscmp(EO::strings.strExtendedOptions, L"Extended Options") == 0, "English group title");
    }
    for (const char* language : { "cs", "de", "es", "es-419", "fr", "is", "it", "nl", "pl", "pt", "pt-br",
        "ru", "sv", "zh-Hans", "de\r\n", "\xEF\xBB\xBF" "zh-Hans\r\n" })
    {
        EO::SetLanguage(language);
        for (const auto& option : EO::Options)
            Check(*option.label && **option.label == L'\0', "recognized languages have editable blank placeholders");
        Check(EO::strings.strExtendedOptions && *EO::strings.strExtendedOptions == L'\0', "blank group title placeholder");
    }
    Check(EO::values == before, "language changes preserve every stored option value");
    EO::SetLanguage("en");
    puts("PASS: all language cases, English fallback and blank translation placeholders");
}

static void SchemeTests()
{
    auto payload = EO::image + EO::OptionsRva;
    for (size_t i = 0; i < EO::NativePayloadSize; ++i) payload[i] = static_cast<BYTE>(i);
    const std::vector<BYTE> expected(payload, payload + EO::NativePayloadSize);
    for (size_t index = 0; index < EO::OptionCount + 2; ++index)
    {
        auto file = static_cast<FILE*>(Open("Release/extended-options-test.opt", "w+b"));
        Check(file != nullptr, "open option scheme fixture");
        const char header[7] = { 'O', 'P', 'T', 'I', 'O', 'N', 'S' };
        fwrite(header, 1, sizeof(header), file);
        EO::values = Pattern(index);
        Check(SW::WriteFile(payload, 128, 1, file) == 1, "shared hook writes native scheme");
        Check(ftell(file) == 166, "135 native bytes plus thirty-one extension bytes");
        fflush(file);
        rewind(file);
        char restoredHeader[7];
        fread(restoredHeader, 1, sizeof(header), file);
        Check(memcmp(header, restoredHeader, 7) == 0, "native header unchanged");
        EO::values.fill(0);
        Check(SW::ReadFile(payload, 128, 1, file) == 1, "shared hook reads native scheme");
        Check(EO::values == Pattern(index), "all twenty-seven independent values round trip");
        Check(memcmp(payload, expected.data(), 128) == 0, "native option bytes unchanged");
        fseek(file, 135, SEEK_SET);
        EO::Extension extension{};
        Check(Read(&extension, 1, sizeof(extension), file) == sizeof(extension) && EO::Valid(extension), "PLUS directly followed by option bytes");
        Check(memcmp(extension.magic, "PLUS", 4) == 0, "scheme signature remains PLUS even when all values are zero");
        Check(extension.values == Pattern(index), "documented offsets contain correct bytes");
        fclose(file);
    }
    // Every truncation, unrecognized signature and each invalid option clears all settings.
    const size_t extent = sizeof(EO::Extension);
    const size_t badSignature = extent + EO::OptionCount;
    const size_t shortPayload = badSignature + 1;
    for (size_t variant = 0; variant <= shortPayload; ++variant)
    {
        auto file = static_cast<FILE*>(Open("Release/extended-options-test.opt", "w+b"));
        Write(expected.data(), 1, expected.size() - (variant == shortPayload ? 1 : 0), file);
        EO::Extension extension{ { 'P', 'L', 'U', 'S' }, Pattern(EO::OptionCount) };
        if (variant >= extent && variant < badSignature)
        {
            const auto& option = EO::Options[variant - extent];
            extension.values[EO::ToIndex(option.index)] = option.maximum + 1;
        }
        if (variant == badSignature) extension.magic[0] = 'X';
        if (variant != shortPayload) Write(&extension, 1, variant < extent ? variant : extent, file);
        fflush(file);
        rewind(file);
        EO::values.fill(1);
        SW::ReadFile(payload, 128, 1, file);
        Check(EO::values == Pattern(EO::OptionCount + 1), "legacy, short native data or invalid extension resets all values");
        fclose(file);
    }
    auto file = static_cast<FILE*>(Open("Release/extended-options-test.opt", "w+b"));
    SW::WriteFile(expected.data(), 128, 1, file);
    Check(ftell(file) == 128, "unrelated 128-byte writes are not extended");
    SW::originalWrite = ShortWrite;
    Check(SW::WriteFile(payload, 128, 1, file) == 0, "extension write failure propagates to native save");
    SW::originalWrite = Write;
    fclose(file);
    // Exercise the full numeric range through the same serialized scheme path.
    for (const auto& option : EO::Options)
        if (option.IsSlider())
        for (unsigned int value = 0; value <= option.maximum; ++value)
        {
            const size_t i = EO::ToIndex(option.index);
            file = static_cast<FILE*>(Open("Release/extended-options-test.opt", "w+b"));
            EO::values.fill(0);
            EO::values[i] = static_cast<unsigned char>(value);
            SW::WriteFile(payload, 128, 1, file);
            fflush(file);
            rewind(file);
            EO::values.fill(1);
            SW::ReadFile(payload, 128, 1, file);
            Check(EO::values[i] == value, "every numeric slider value survives scheme round trip");
            fclose(file);
        }
    puts("PASS: .opt contract, independent settings, full numeric ranges, native preservation, malformed files and save failure");
}

static void LaunchTests()
{
    EO::originalOpen = Open;
    EO::originalClose = Close;
    CreateDirectoryA("Release/extended-options-launch", nullptr);
    const char* gamePath = "Release/extended-options-launch/game.dat";
    const char* sidecarPath = "Release/extended-options-launch/extended.dat";
    Check(EO::SidecarPath("DATA\\GAME.DAT") == "DATA\\extended.dat", "case-insensitive launch name");
    Check(EO::SidecarPath("game.dat") == "extended.dat", "launch in current directory");
    Check(EO::SidecarPath("save.dat").empty(), "saved games have no launch sidecar");
    std::vector<BYTE> game(0xcd4, 0x5a);
    for (size_t index = 0; index < EO::OptionCount + 2; ++index)
    {
        EO::values = Pattern(index);
        void* file = EO::OpenFile(gamePath, "wb");
        Check(file != nullptr, "open launch game fixture");
        Check(GetFileAttributesA(sidecarPath) == INVALID_FILE_ATTRIBUTES, "old match sidecar removed before write");
        Check(SW::WriteFile(game.data(), 1, game.size(), file) == game.size(), "write native game and sidecar");
        EO::CloseFile(file);
        auto sidecar = static_cast<FILE*>(Open(sidecarPath, "rb"));
        Check(sidecar != nullptr, "sidecar exists before launch");
        EO::Extension extension{};
        Check(Read(&extension, 1, sizeof(extension), sidecar) == sizeof(extension), "sidecar format length");
        const char zeroMagic[4]{};
        const bool allUnset = index == EO::OptionCount + 1;
        Check(memcmp(extension.magic, allUnset ? zeroMagic : "PLUS", 4) == 0,
            "sidecar has PLUS for any enabled option and four zero bytes when all options are unset");
        Check(allUnset || EO::Valid(extension), "active sidecar values are valid");
        Check(extension.values == Pattern(index) && fgetc(sidecar) == EOF, "sidecar byte order and exact length");
        fclose(sidecar);
        auto native = static_cast<FILE*>(Open(gamePath, "rb"));
        std::vector<BYTE> restored(0xcd4);
        Check(Read(restored.data(), 1, restored.size(), native) == restored.size() && fgetc(native) == EOF,
            "native game.dat length remains 3284");
        Check(restored == game, "every native game.dat byte preserved");
        fclose(native);
        Check(EO::launchStreams.empty(), "stream tracking released on close");
    }
    auto unrelated = EO::OpenFile("Release/extended-options-launch/save.dat", "wb");
    SW::WriteFile(game.data(), 1, game.size(), unrelated);
    EO::CloseFile(unrelated);
    auto sidecar = static_cast<FILE*>(Open(sidecarPath, "rb"));
    EO::Extension extension{};
    Check(Read(&extension, 1, sizeof(extension), sidecar) == sizeof(extension) && extension.values == Pattern(EO::OptionCount + 1), "unrelated saved game leaves launch data unchanged");
    fclose(sidecar);
    DeleteFileA(sidecarPath);
    Check(CreateDirectoryA(sidecarPath, nullptr) != FALSE, "create sidecar failure fixture");
    auto file = EO::OpenFile(gamePath, "wb");
    Check(SW::WriteFile(game.data(), 1, game.size(), file) == 0, "sidecar publication failure propagates to launch writer");
    EO::CloseFile(file);
    Check(GetFileAttributesA("Release/extended-options-launch/extended.dat.tmp") == INVALID_FILE_ATTRIBUTES,
        "failed sidecar leaves no partial file");
    RemoveDirectoryA(sidecarPath);
    puts("PASS: launch sidecar PLUS/zero headers, independent enabled settings, native game.dat preservation, stream lifetime and write failure");
}

static std::vector<BYTE> sent;
static void __fastcall Send(void*, void*, uint32_t, uint32_t, const void* p, uint32_t n)
{ sent.assign(static_cast<const BYTE*>(p), static_cast<const BYTE*>(p) + n); }
static void __fastcall Broadcast(void*, void*, uint32_t, const void* p, uint32_t n)
{ sent.assign(static_cast<const BYTE*>(p), static_cast<const BYTE*>(p) + n); }
static int receiveCalls = 0;
static uint32_t receivedLength = 0;
static void __fastcall Receive(void*, void*, uint32_t, const void*, uint32_t length)
{ ++receiveCalls; receivedLength = length; }
static void NetworkTests()
{
    SW::originalSendToPlayer = reinterpret_cast<SW::SendToPlayer>(Send);
    SW::originalSendToAll = reinterpret_cast<SW::SendToAll>(Broadcast);
    SW::originalReceivePacket = reinterpret_cast<SW::ReceivePacket>(Receive);
    std::array<BYTE, 0x84> native{};
    native[0] = 0x19;
    for (size_t i = 4; i < native.size(); ++i) native[i] = static_cast<BYTE>(i);
    std::vector<BYTE> lobby(0x1640);
    *reinterpret_cast<uint32_t*>(lobby.data() + 0x163c) = 42;
    for (int transport : { 0, 1 })
    {
        *reinterpret_cast<uint32_t*>(SW::image + 0x188b14) = transport;
        for (size_t index = 0; index < EO::OptionCount + 2; ++index)
        {
            EO::values = Pattern(index);
            SW::SendWeaponPacketToPlayer(lobby.data(), nullptr, 1, 2, native.data(), static_cast<uint32_t>(native.size()));
            Check(sent.size() == 163 && memcmp(sent.data(), native.data(), native.size()) == 0, "targeted options transfer preserves native packet");
            Check(memcmp(sent.data() + EO::NativePacketSize, "PLUS", 4) == 0,
                "lobby signature remains PLUS including all-unset settings");
            auto targeted = sent;
            SW::SendWeaponPacketToAll(lobby.data(), nullptr, 1, native.data(), static_cast<uint32_t>(native.size()));
            Check(sent == targeted, "broadcast includes same host extension");
            EO::values.fill(1);
            SW::ReceiveWeaponPacket(lobby.data(), nullptr, 42, sent.data(), static_cast<uint32_t>(sent.size() + transport * 4));
            Check(EO::values == Pattern(index), "both transport lengths restore host values before launch");
            Check(receivedLength == EO::NativePacketSize, "native option decoder sees native packet length");
            EO::values = Pattern(0);
            SW::ReceiveWeaponPacket(lobby.data(), nullptr, 99, sent.data(), static_cast<uint32_t>(sent.size() + transport * 4));
            Check(EO::values == Pattern(0), "non-host cannot change extended options");
        }
        EO::values.fill(1);
        SW::ReceiveWeaponPacket(lobby.data(), nullptr, 42, native.data(), static_cast<uint32_t>(native.size() + transport * 4));
        Check(EO::values == Pattern(EO::OptionCount + 1), "legacy host clears extension");
        sent.back() = 2;
        EO::values.fill(1);
        SW::ReceiveWeaponPacket(lobby.data(), nullptr, 42, sent.data(), static_cast<uint32_t>(sent.size() + transport * 4));
        Check(EO::values == Pattern(EO::OptionCount + 1), "invalid host boolean clears extension");
        sent.back() = 0;
        sent[EO::NativePacketSize + 4 + EO::ToIndex(EO::OptionIndex::LowGravity)] = 2;
        EO::values.fill(1);
        SW::ReceiveWeaponPacket(lobby.data(), nullptr, 42, sent.data(), static_cast<uint32_t>(sent.size() + transport * 4));
        Check(EO::values == Pattern(EO::OptionCount + 1), "invalid host Low Gravity boolean clears extension");
        const int calls = receiveCalls;
        SW::ReceiveWeaponPacket(lobby.data(), nullptr, 42, native.data(), 4 + transport * 4);
        Check(receiveCalls == calls, "short native option packet dropped before unbounded native decoder");
    }
    auto pages = static_cast<BYTE*>(VirtualAlloc(nullptr, 8192, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    Check(pages != nullptr, "allocate guarded receive fixture");
    DWORD ignored;
    VirtualProtect(pages + 4096, 4096, PAGE_NOACCESS, &ignored);
    for (size_t length = 4; length < EO::Packet{}.size(); ++length)
    {
        BYTE* packet = pages + 4096 - length;
        memset(packet, 0, length);
        packet[0] = 0x19;
        EO::ReceivePacket(42, 42, packet, static_cast<uint32_t>(length));
    }
    VirtualFree(pages, 0, MEM_RELEASE);
    puts("PASS: targeted/broadcast transfer, both transport lengths, host authority and bounded malformed packets");
}

static SIZE scrollTotal{};
static int edits = 0;
static void __fastcall Scroll(void*, void*, int mapMode, SIZE size, const SIZE&, const SIZE&)
{ Check(mapMode == MM_TEXT, "native scrolling stays in pixels"); scrollTotal = size; }
static void __fastcall Mark(void*, void*) { ++edits; }
static INT_PTR CALLBACK DialogProc(HWND, UINT, WPARAM, LPARAM) { return FALSE; }
static void EditorTests(const char* path)
{
    InitCommonControls();
    HMODULE module = LoadLibraryExA(path, nullptr, LOAD_LIBRARY_AS_DATAFILE);
    Check(module != nullptr, "load supplied frontend resources without executing it");
    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(154), MAKEINTRESOURCEW(5));
    auto resourceTemplate = static_cast<const DLGTEMPLATE*>(LockResource(LoadResource(module, resource)));
    HWND root = CreateWindowW(L"STATIC", L"Fixture", WS_OVERLAPPEDWINDOW, 0, 0, 700, 500, nullptr, nullptr, nullptr, nullptr);
    HWND container = CreateWindowW(L"STATIC", L"Container", WS_CHILD | WS_VISIBLE, 0, 0, 650, 450, root, nullptr, nullptr, nullptr);
    HWND window = CreateDialogIndirectParamW(module, resourceTemplate, container, DialogProc, 0);
    Check(window != nullptr, "create actual Dialog 154 fixture");
    HWND nativeTrackbar = nullptr;
    for (HWND child = GetWindow(window, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
    {
        wchar_t type[64]{};
        GetClassNameW(child, type, 64);
        if (_wcsicmp(type, TRACKBAR_CLASSW) == 0) { nativeTrackbar = child; break; }
    }
    Check(nativeTrackbar != nullptr, "native dialog contains reference trackbar");
    const LONG_PTR nativeBorder = GetWindowLongPtrW(nativeTrackbar, GWL_STYLE) & WS_BORDER;
    const LONG_PTR nativeBorderEx = GetWindowLongPtrW(nativeTrackbar, GWL_EXSTYLE) & (WS_EX_CLIENTEDGE | WS_EX_STATICEDGE);
    Check(nativeBorder || nativeBorderEx, "native trackbar has border styling");
    RECT nativeCheckbox{}, nextNativeCheckbox{}, nativeLabel{}, nativeSlider{}, nativeReadout{},
        nativeFirstSliderLabel{}, nativeSecondSliderLabel{};
    Check(GetWindowRect(GetDlgItem(window, 2072), &nativeCheckbox) &&
        GetWindowRect(GetDlgItem(window, 2073), &nextNativeCheckbox) &&
        GetWindowRect(GetDlgItem(window, 2031), &nativeLabel) &&
        GetWindowRect(GetDlgItem(window, 2200), &nativeSlider) &&
        GetWindowRect(GetDlgItem(window, 2201), &nativeReadout) &&
        GetWindowRect(GetDlgItem(window, 2063), &nativeFirstSliderLabel) &&
        GetWindowRect(GetDlgItem(window, 2066), &nativeSecondSliderLabel), "measure native checkbox and slider spacing");
    // Dialog-unit conversion can round positions differently by one pixel.
    const auto sameSpacing = [](LONG actual, LONG native) { return abs(actual - native) <= 1; };
    ShowWindow(window, SW_SHOWNOACTIVATE); // The outer fixture remains hidden.
    std::array<BYTE, 0xa0> object{};
    *reinterpret_cast<HWND*>(object.data() + 0x1c) = window;
    RECT native{ 0, 0, 386, 468 };
    MapDialogRect(window, &native);
    *reinterpret_cast<SIZE*>(object.data() + 0x44) = SIZE{ native.right, native.bottom };
    EO::setScrollSizes = reinterpret_cast<EO::SetScrollSizes>(Scroll);
    EO::markEdited = reinterpret_cast<EO::MarkEdited>(Mark);
    EO::values = Pattern(0);
    Check(EO::AttachEditor(object.data()), "attach controls to actual native form");
    auto editor = static_cast<EO::Editor*>(GetPropW(window, EO::ContextProperty));
    Check(scrollTotal.cx == native.right && scrollTotal.cy > native.bottom, "extend native scroll height without changing width");
    const int expectedOffsets[] = { 0, 11, 22, 33, 44, 75, 86, 97, 108, 119, 130, 141, 152, 163, 174,
        0, 11, 22, 53, 84, 95, 106, 117, 128, 139, 150, 161 };
    const int expectedGaps[] = { 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2,
        0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2 };
    RECT firstCheckbox{}, secondCheckbox{}, firstSliderLabel{}, secondSliderLabel{};
    GetWindowRect(editor->controls[0], &firstCheckbox);
    GetWindowRect(editor->controls[1], &secondCheckbox);
    GetWindowRect(editor->labels[17], &firstSliderLabel);
    GetWindowRect(editor->labels[18], &secondSliderLabel);
    const LONG checkboxPitch = secondCheckbox.top - firstCheckbox.top;
    const int halfGap = (checkboxPitch + 1) / 2;
    Check(sameSpacing(secondCheckbox.top - firstCheckbox.top, nextNativeCheckbox.top - nativeCheckbox.top),
        "checkbox pitch matches original controls");
    RECT instantMines{}, herdDynamite{}, rapidPlay{}, terrain{}, aquaSheep{}, herdSheep{}, backflip{};
    GetWindowRect(editor->controls[20], &instantMines);
    GetWindowRect(editor->controls[21], &herdDynamite);
    Check(sameSpacing(herdDynamite.top - instantMines.top, checkboxPitch),
        "Aqua Sheep and Instant Mines precede herd weapons without an extra gap");
    GetWindowRect(editor->controls[12], &rapidPlay);
    GetWindowRect(editor->controls[13], &terrain);
    GetWindowRect(editor->controls[19], &aquaSheep);
    GetWindowRect(editor->controls[24], &herdSheep);
    GetWindowRect(editor->controls[25], &backflip);
    Check(sameSpacing(terrain.top - rapidPlay.top, checkboxPitch + halfGap) &&
        sameSpacing(aquaSheep.top - secondSliderLabel.top,
            nativeSecondSliderLabel.top - nativeFirstSliderLabel.top + halfGap) &&
        sameSpacing(backflip.top - herdSheep.top, checkboxPitch + halfGap),
        "blank-line gaps are half a checkbox row including the new terrain gap");
    Check(sameSpacing(secondSliderLabel.top - firstSliderLabel.top, nativeSecondSliderLabel.top - nativeFirstSliderLabel.top),
        "successive slider rows match original spacing");
    size_t extendedGroupCount = 0;
    for (HWND child = GetWindow(window, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
    {
        if (GetDlgCtrlID(child) < EO::FirstCheckId - 10) continue;
        wchar_t type[64]{}, text[100]{};
        GetClassNameW(child, type, 64);
        GetWindowTextW(child, text, 100);
        Check(wcscmp(text, L"Herd weapons") != 0, "Herd weapons label removed");
        if (_wcsicmp(type, L"BUTTON") == 0 && (GetWindowLongPtrW(child, GWL_STYLE) & BS_TYPEMASK) == BS_GROUPBOX)
            ++extendedGroupCount;
    }
    Check(extendedGroupCount == 1, "Extended Options has only the main group box");
    Check((GetWindowLongPtrW(editor->controls[10], GWL_STYLE) & BS_TYPEMASK) == BS_AUTOCHECKBOX &&
        !editor->labels[10] && !editor->readouts[10], "Low Gravity is a checkbox without slider label or readout");
    for (size_t i = 0; i < EO::OptionCount; ++i)
    {
        const bool slider = EO::Options[i].IsSlider();
        const size_t storageIndex = EO::ToIndex(EO::Options[i].index);
        const HWND control = editor->controls[i];
        const HWND labelWindow = slider ? editor->labels[i] : control;
        wchar_t label[100]{};
        GetWindowTextW(labelWindow, label, 100);
        Check(wcscmp(label, *EO::Options[i].label) == 0, "every requested option label present");
        RECT bounds{}, group{};
        GetWindowRect(control, &bounds);
        GetWindowRect(editor->group, &group);
        Check(bounds.left > group.left && bounds.right < group.right && bounds.top > group.top && bounds.bottom < group.bottom,
            "all controls contained inside new group");
        Check(GetWindowLongPtrW(control, GWL_STYLE) & WS_TABSTOP, "option is keyboard reachable");
        RECT labelBounds{};
        GetWindowRect(labelWindow, &labelBounds);
        RECT expected{ i < 15 ? 14 : 205, 490 + expectedOffsets[i], 0, 0 };
        MapDialogRect(window, &expected);
        expected.top += expectedGaps[i] * halfGap;
        MapWindowPoints(window, nullptr, reinterpret_cast<POINT*>(&expected), 2);
        Check(labelBounds.left == expected.left && labelBounds.top == expected.top,
            "requested two-column order and half-height blank rows");
        HDC dc = GetDC(labelWindow);
        auto font = reinterpret_cast<HFONT>(SendMessageW(labelWindow, WM_GETFONT, 0, 0));
        HGDIOBJ previous = SelectObject(dc, font);
        SIZE textSize{};
        GetTextExtentPoint32W(dc, label, static_cast<int>(wcslen(label)), &textSize);
        Check(textSize.cx + (slider ? 0 : GetSystemMetrics(SM_CXMENUCHECK) + 6) < labelBounds.right - labelBounds.left,
            "complete option label fits");
        SelectObject(dc, previous);
        ReleaseDC(labelWindow, dc);
        const auto before = EO::values;
        if (slider)
        {
            const int maximum = 100;
            const UINT zeroString = i == 4 ? 141 : 99;
            Check(SendMessageW(control, TBM_GETRANGEMIN, 0, 0) == 0 &&
                SendMessageW(control, TBM_GETRANGEMAX, 0, 0) == maximum, "requested slider range");
            Check(bounds.left == labelBounds.left && bounds.top >= labelBounds.bottom,
                "trackbar appears below its title");
            Check(sameSpacing(bounds.top - labelBounds.top, nativeSlider.top - nativeLabel.top) &&
                sameSpacing(labelBounds.bottom - labelBounds.top, nativeLabel.bottom - nativeLabel.top) &&
                sameSpacing(bounds.bottom - bounds.top, nativeSlider.bottom - nativeSlider.top),
                "caption height and label-to-trackbar distance match native controls");
            RECT readoutBounds{};
            GetWindowRect(editor->readouts[i], &readoutBounds);
            Check(sameSpacing(readoutBounds.top - bounds.top, nativeReadout.top - nativeSlider.top) &&
                sameSpacing(readoutBounds.bottom - readoutBounds.top, nativeReadout.bottom - nativeReadout.top),
                "numeric readout height and alignment match native controls");
            Check((GetWindowLongPtrW(control, GWL_STYLE) & WS_BORDER) == nativeBorder &&
                (GetWindowLongPtrW(control, GWL_EXSTYLE) & (WS_EX_CLIENTEDGE | WS_EX_STATICEDGE)) == nativeBorderEx,
                "extended trackbar border matches original trackbar");
            Check(SendMessageW(control, TBM_GETPOS, 0, 0) == 0, "initial slider zero restored");
            Check(EO::Options[i].zeroStringId == zeroString, "zero uses requested string ID");
            wchar_t readout[100]{};
            GetWindowTextW(editor->readouts[i], readout, 100);
            Check(wcscmp(readout, i == 4 ? L"No" : L"Default") == 0, "zero readout fallback");
            wchar_t localized[100]{};
            Check(LoadStringW(module, zeroString, localized, 100) != 0 && EO::ValueText(EO::Options[i], module) == localized,
                "zero text loads actual frontend string resource");
            SendMessageW(control, WM_KEYDOWN, VK_END, 0);
            SendMessageW(control, WM_KEYUP, VK_END, 0);
            Check(EO::values[storageIndex] == maximum, "keyboard End on actual trackbar writes maximum immediately");
            GetWindowTextW(editor->readouts[i], readout, 100);
            Check(std::wstring(readout) == (i == 4 ? L"Unlimited" : std::to_wstring(maximum)), "maximum readout");
            if (i == 4)
            {
                Check(LoadStringW(module, 4950, localized, 100) != 0 && EO::ValueText(EO::Options[i], module) == localized,
                    "Super Shopper Crates maximum loads frontend Unlimited string");
                EO::values[storageIndex] = 99;
                Check(EO::ValueText(EO::Options[i], module) == L"99", "intermediate crate count remains numeric");
                EO::values[storageIndex] = 100;
            }
        }
        else
        {
            Check(sameSpacing(bounds.bottom - bounds.top, nativeCheckbox.bottom - nativeCheckbox.top),
                "checkbox height matches original controls");
            Check(SendMessageW(control, BM_GETCHECK, 0, 0) == (i == 0 ? BST_CHECKED : BST_UNCHECKED), "restore loaded checkboxes on attachment");
            SendMessageW(control, BM_CLICK, 0, 0);
            Check(EO::values[storageIndex] == 1 - before[storageIndex], "actual checkbox click writes its boolean immediately");
        }
        for (size_t j = 0; j < EO::OptionCount; ++j)
            if (j != storageIndex) Check(EO::values[j] == before[j], "control does not modify another option");
        if (i + 1 < EO::OptionCount)
            Check(GetNextDlgTabItem(root, control, FALSE) == editor->controls[i + 1],
                "forward tab order follows list including sliders and spacing");
        if (i > 0)
        {
            const HWND previousControl = GetNextDlgTabItem(root, control, TRUE);
            if (previousControl != editor->controls[i - 1])
                fprintf(stderr, "Tab mismatch at option %zu: previous ID %d, expected ID %d\n", i,
                    GetDlgCtrlID(previousControl), GetDlgCtrlID(editor->controls[i - 1]));
            Check(previousControl == editor->controls[i - 1], "reverse tab order follows list");
        }
    }
    Check(edits == EO::OptionCount, "each edit uses native scheme edit handler once");
    wchar_t lastLabel[100]{};
    GetWindowTextW(editor->controls.back(), lastLabel, 100);
    Check(wcscmp(lastLabel, L"Disable Unlocked Aim") == 0, "Disable Unlocked Aim is last");
    const auto beforeLanguageChange = EO::values;
    const int editsBeforeLanguageChange = edits;
    for (const char* language : { "de", "en" })
    {
        EO::SetLanguage(language);
        wchar_t text[150]{};
        GetWindowTextW(editor->group, text, 150);
        Check(wcscmp(text, EO::strings.strExtendedOptions) == 0, "language updates attached group title");
        for (const auto& option : EO::Options)
        {
            const size_t index = EO::ToIndex(option.index);
            GetWindowTextW(option.IsSlider() ? editor->labels[index] : editor->controls[index], text, 150);
            Check(wcscmp(text, *option.label) == 0, "language updates attached checkbox and slider captions");
            Check(option.IsSlider() ? SendMessageW(editor->controls[index], TBM_GETPOS, 0, 0) == beforeLanguageChange[index] :
                SendMessageW(editor->controls[index], BM_GETCHECK, 0, 0) == (beforeLanguageChange[index] ? BST_CHECKED : BST_UNCHECKED),
                "language change preserves control state");
        }
    }
    Check(EO::values == beforeLanguageChange && edits == editsBeforeLanguageChange,
        "language refresh does not edit the scheme");
    auto saved = static_cast<FILE*>(Open("Release/extended-options-test.opt", "w+b"));
    EO::values = Pattern(EO::OptionCount);
    SW::WriteFile(EO::image + EO::OptionsRva, 128, 1, saved);
    fflush(saved);
    rewind(saved);
    EO::values.fill(1);
    SW::ReadFile(EO::image + EO::OptionsRva, 128, 1, saved);
    fclose(saved);
    for (size_t i = 0; i < EO::OptionCount; ++i)
        Check(EO::Options[i].IsSlider() ? SendMessageW(editor->controls[i], TBM_GETPOS, 0, 0) == EO::Options[i].maximum :
            SendMessageW(editor->controls[i], BM_GETCHECK, 0, 0) == BST_CHECKED,
            "loading another scheme refreshes all checks and sliders");
    Check(edits == EO::OptionCount, "loading scheme does not mark it User defined");
    EO::ResetDefault();
    for (size_t i = 0; i < EO::OptionCount; ++i)
        if (EO::Options[i].IsSlider())
        {
            Check(SendMessageW(editor->controls[i], TBM_GETPOS, 0, 0) == 0, "Default resets numeric slider");
            wchar_t text[100]{};
            GetWindowTextW(editor->readouts[i], text, 100);
            Check(wcscmp(text, i == 4 ? L"No" : L"Default") == 0, "Default restores special zero label");
        }
        else Check(SendMessageW(editor->controls[i], BM_GETCHECK, 0, 0) == BST_UNCHECKED, "Default clears visible checkbox");
    DestroyWindow(window);
    Check(EO::editors.empty() && !(GetWindowLongPtrW(container, GWL_EXSTYLE) & WS_EX_CONTROLPARENT), "editor cleanup restores ancestor styles");

    // Move every descriptor to a different visual position while preserving
    // its fixed index. Exercise actual control notifications in this layout.
    std::array<EO::Option, EO::OptionCount> moved{};
    std::copy(EO::Options, EO::Options + EO::OptionCount, moved.begin());
    std::rotate(moved.begin(), moved.begin() + 1, moved.begin() + 15);
    std::reverse(moved.begin() + 15, moved.end());
    int row[2]{};
    for (size_t position = 0; position < moved.size(); ++position)
    {
        auto& option = moved[position];
        option.column = position < 15 ? 0 : 1;
        option.row = row[option.column];
        option.blankLines = 0;
        row[option.column] += option.IsSlider() ? 2 : 1;
        Check(EO::ToIndex(option.index) != position, "every option moved to a different position");
    }
    window = CreateDialogIndirectParamW(module, resourceTemplate, container, DialogProc, 0);
    Check(window != nullptr, "create reordered dialog fixture");
    ShowWindow(window, SW_SHOWNOACTIVATE);
    *reinterpret_cast<HWND*>(object.data() + 0x1c) = window;
    *reinterpret_cast<SIZE*>(object.data() + 0x44) = SIZE{ native.right, native.bottom };
    EO::values.fill(0);
    Check(EO::AttachEditor(object.data(), moved.data()), "attach reordered descriptors");
    editor = static_cast<EO::Editor*>(GetPropW(window, EO::ContextProperty));
    std::array<BYTE, EO::NativePacketSize> nativePacket{};
    nativePacket[0] = 0x19;
    const char* movedSidecar = "Release/extended-options-reordered.dat";
    for (size_t position = 0; position < moved.size(); ++position)
    {
        const auto& option = moved[position];
        const size_t index = EO::ToIndex(option.index);
        const HWND control = editor->controls[index];
        Check(GetDlgCtrlID(control) == EO::FirstCheckId + static_cast<int>(index), "moved control retains fixed control ID");
        EO::values.fill(0);
        EO::RefreshEditors();
        if (option.IsSlider())
        {
            SendMessageW(control, WM_KEYDOWN, VK_END, 0);
            SendMessageW(control, WM_KEYUP, VK_END, 0);
        }
        else SendMessageW(control, BM_CLICK, 0, 0);
        std::array<unsigned char, EO::OptionCount> expected{};
        expected[index] = option.maximum;
        Check(EO::values == expected, "moved control updates only its permanent byte");
        if (position + 1 < moved.size())
            Check(GetNextDlgTabItem(root, control, FALSE) == editor->controls[EO::ToIndex(moved[position + 1].index)],
                "tab order follows changed layout while IDs stay fixed");
        if (position > 0)
            Check(GetNextDlgTabItem(root, control, TRUE) == editor->controls[EO::ToIndex(moved[position - 1].index)],
                "reverse tab order follows changed layout");

        auto savedMoved = static_cast<FILE*>(Open("Release/extended-options-test.opt", "w+b"));
        Check(savedMoved != nullptr, "open moved-option scheme fixture");
        fwrite("OPTIONS", 1, 7, savedMoved);
        Check(SW::WriteFile(EO::image + EO::OptionsRva, 128, 1, savedMoved) == 1, "save moved-option scheme");
        fflush(savedMoved);
        fseek(savedMoved, static_cast<long>(139 + index), SEEK_SET);
        Check(fgetc(savedMoved) == option.maximum, "moved option keeps its exact .opt byte offset");
        fseek(savedMoved, 7, SEEK_SET);
        EO::values.fill(1);
        SW::ReadFile(EO::image + EO::OptionsRva, 128, 1, savedMoved);
        fclose(savedMoved);
        Check(EO::values == expected, "moved option restores from original scheme byte");

        Check(EO::SaveSidecar(movedSidecar), "save moved-option launch sidecar");
        auto launch = static_cast<FILE*>(Open(movedSidecar, "rb"));
        Check(launch != nullptr, "open moved-option sidecar");
        EO::Extension extension{};
        Check(Read(&extension, 1, sizeof(extension), launch) == sizeof(extension) &&
            EO::Valid(extension) && extension.values == expected && fgetc(launch) == EOF,
            "moved option keeps its exact launch sidecar byte offset");
        fclose(launch);

        EO::Packet packet{};
        Check(EO::ExtendPacket(nativePacket.data(), static_cast<uint32_t>(nativePacket.size()), packet), "send moved option");
        Check(packet[EO::NativePacketSize + 4 + index] == option.maximum, "moved option keeps original lobby byte offset");
        EO::values.fill(1);
        Check(EO::ReceivePacket(42, 42, packet.data(), static_cast<uint32_t>(packet.size())) && EO::values == expected,
            "moved option restores original byte from lobby packet");
        Check(option.IsSlider() ? SendMessageW(control, TBM_GETPOS, 0, 0) == option.maximum :
            SendMessageW(control, BM_GETCHECK, 0, 0) == BST_CHECKED, "scheme/network refresh reaches moved control by fixed index");
    }
    DeleteFileA(movedSidecar);
    EO::ResetDefault();
    DestroyWindow(window);
    Check(EO::editors.empty(), "reordered editor cleanup");
    puts("PASS: all options moved in the dialog retain their fixed .opt, extended.dat and lobby indexes");
    DestroyWindow(root);
    FreeLibrary(module);
    puts("PASS: real Dialog 154 single group/two-column spacing, Low Gravity checkbox, native slider borders, keyboard sliders, localized endpoints, scheme reload, tab traversal and Default");
}

__declspec(naked) static void ReturnFromDefault() { __asm { ret } }
__declspec(naked) static void CaptureDefault(void (*)(), DWORD*)
{
    // A dedicated naked fixture avoids the compiler's aligned-stack frame
    // register when loading sentinels into every general-purpose register.
    __asm
    {
        push ebp
        mov ebp, esp
        pushfd
        pushad
        mov eax, 11111111h
        mov ecx, 22222222h
        mov edx, 33333333h
        mov ebx, 44444444h
        mov esi, 55555555h
        mov edi, 66666666h
        cmp eax, ecx
        pushfd
        call dword ptr [ebp + 8]
        pushfd
        pushad
        mov edx, [ebp + 12]
        mov eax, [esp + 28]
        mov [edx], eax
        mov eax, [esp + 24]
        mov [edx + 4], eax
        mov eax, [esp + 20]
        mov [edx + 8], eax
        mov eax, [esp + 16]
        mov [edx + 12], eax
        mov eax, [esp + 4]
        mov [edx + 16], eax
        mov eax, [esp]
        mov [edx + 20], eax
        mov eax, [esp + 36]
        mov [edx + 24], eax
        mov eax, [esp + 32]
        mov [edx + 28], eax
        popad
        add esp, 8
        popad
        popfd
        pop ebp
        ret
    }
}
static void DefaultTests()
{
    void (*detours[])() = { EO::Default0, EO::Default1, EO::Default2, EO::Default3, EO::Default4,
        EO::Default5, EO::Default6, EO::Default7, EO::Default8 };
    for (size_t i = 0; i < 9; ++i)
    {
        EO::defaultTrampolines[i] = reinterpret_cast<void*>(ReturnFromDefault);
        EO::values.fill(1);
        DWORD registers[8]{};
        CaptureDefault(detours[i], registers);
        Check(EO::values == Pattern(EO::OptionCount + 1), "every native default copy detour clears all extended options");
        Check(registers[6] == registers[7], "default detour preserves native flags");
        for (size_t r = 0; r < 6; ++r)
            Check(registers[r] == 0x11111111u * (r + 1), "default detour preserves native registers before trampoline");
    }
    puts("PASS: all nine default-copy detours reset options and preserve native registers/flags");
}

static void HookTests(const char* path)
{
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    Check(file != INVALID_HANDLE_VALUE, "open frontend for private hook validation");
    HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr);
    BYTE* frontend = static_cast<BYTE*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));
    Check(frontend != nullptr, "map frontend without running it");
    const std::array<size_t, 12> sites{ 0x57d20, 0x97780, 0x97500, 0x9cdc, 0x9ff8, 0xa5d4,
        0x31cd9, 0x392c0, 0x586a3, 0x5899e, 0x59611, 0x59e3d };
    // SEC_IMAGE does not run loader relocations; relocate the inspected operands
    // in this private mapping exactly as a relocated executable's loader would.
    for (size_t i = 3; i < sites.size(); ++i)
    {
        DWORD old;
        VirtualProtect(frontend + sites[i], 17, PAGE_EXECUTE_READWRITE, &old);
        *reinterpret_cast<uint32_t*>(frontend + sites[i] + 6) = reinterpret_cast<uint32_t>(frontend + 0x186358);
        *reinterpret_cast<uint32_t*>(frontend + sites[i] + 11) = reinterpret_cast<uint32_t>(frontend + EO::OptionsRva);
        VirtualProtect(frontend + sites[i], 17, old, &old);
    }
    std::array<std::array<BYTE, 17>, 12> before{};
    for (size_t i = 0; i < sites.size(); ++i) memcpy(before[i].data(), frontend + sites[i], before[i].size());
    Check(MH_Initialize() == MH_OK, "initialize MinHook");
    Check(SW::InstallInImage(frontend), "install shared CRT/network hooks");
    DWORD old;
    VirtualProtect(frontend + sites[11], 17, PAGE_EXECUTE_READWRITE, &old);
    frontend[sites[11]] = 0x90;
    Check(!EO::InstallInImage(frontend) && !EO::enabled, "unsupported default signature disables feature before hooks are created");
    frontend[sites[11]] = before[11][0];
    VirtualProtect(frontend + sites[11], 17, old, &old);
    for (size_t i = 0; i < sites.size(); ++i)
        Check(memcmp(before[i].data(), frontend + sites[i], before[i].size()) == 0, "signature rejection leaves all extended hook sites untouched");
    Check(EO::InstallInImage(frontend), "install all twelve extended option hooks on supplied frontend");
    for (size_t i = 0; i < sites.size(); ++i)
        Check(memcmp(before[i].data(), frontend + sites[i], before[i].size()) != 0, "extended hook enabled");
    Check(MH_Uninitialize() == MH_OK, "remove hooks");
    for (size_t i = 0; i < sites.size(); ++i)
        Check(memcmp(before[i].data(), frontend + sites[i], before[i].size()) == 0, "original code restored");
    UnmapViewOfFile(frontend);
    CloseHandle(mapping);
    CloseHandle(file);
    puts("PASS: shared and extended hooks coexist, install on supported frontend, and restore native code");
}

int main(int argc, char** argv)
{
    try
    {
        std::vector<BYTE> image(0x5b8000);
        EO::image = SW::image = image.data();
        EO::enabled = true;
        SW::originalRead = Read;
        SW::originalWrite = Write;
        LanguageTests();
        SchemeTests();
        LaunchTests();
        NetworkTests();
        DefaultTests();
        Check(argc > 1, "supply frontend path for real resource/hook validation");
        EditorTests(argv[1]);
        HookTests(argv[1]);
        return 0;
    }
    catch (const std::exception& error) { fprintf(stderr, "FAIL: %s\n", error.what()); return 1; }
}
