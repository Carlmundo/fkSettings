#include <cstdio>
#include <filesystem>
#include "../fkSettings/ColourMaps.cpp"
#include "../fkSettings/SecretWeapons.cpp"
#include "../fkSettings/ExtendedOptions.cpp"
#include "../fkSettings/NetworkTeams.cpp"

namespace CM = ColourMaps;
static void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
static void Put(std::vector<BYTE>& bytes, size_t offset, uint32_t value) { memcpy(bytes.data() + offset, &value, 4); }
static bool Rejected(std::vector<BYTE> bytes)
{
    try { CM::Parse(std::move(bytes)); return false; }
    catch (const std::exception&) { return true; }
}
static void ParserTests(const CM::Map& map)
{
    Check(map.pixels.size() == 1920 * 696, "full indexed preview");
    Check(map.waterOffset + 1 + map.waterPath.size() == map.bytes.size() &&
        map.bytes[map.waterOffset] == map.waterPath.size(), "water path is the final length-prefixed field");
    for (size_t size : { 0u, 4u, 8u, 24u, 28u, 283u, 300u, 10000u })
        Check(Rejected(std::vector<BYTE>(map.bytes.begin(), map.bytes.begin() + size)), "truncated map rejected");
    auto bytes = map.bytes;
    bytes[0] = 'G'; Check(Rejected(bytes), "wrong signature rejected");
    bytes = map.bytes; Put(bytes, 4, 0); Check(Rejected(bytes), "wrong declared file size rejected");
    bytes = map.bytes; Put(bytes, 8, 4096); Check(Rejected(bytes), "unsupported dimensions rejected");
    bytes = map.bytes; Put(bytes, 16, 2); Check(Rejected(bytes), "invalid cavern flag rejected");
    bytes = map.bytes; Put(bytes, 20, 0xffffffff); Check(Rejected(bytes), "unbounded location count rejected");
    bytes = map.bytes; Put(bytes, 24, 1920); Check(Rejected(bytes), "invalid object coordinate rejected");
    const size_t imageStart = 28 + map.spawns * 8;
    bytes = map.bytes; bytes[imageStart] = 'X'; Check(Rejected(bytes), "wrong image signature rejected");
    bytes = map.bytes; bytes[imageStart + 9] = 24; Check(Rejected(bytes), "truecolour foreground rejected");
    bytes = map.bytes; bytes[imageStart + 11] = 255; bytes[imageStart + 12] = 255;
    Check(Rejected(bytes), "unbounded palette rejected");
    bytes = map.bytes; bytes.pop_back(); Put(bytes, 4, static_cast<uint32_t>(bytes.size()));
    Check(Rejected(bytes), "truncated path rejected despite valid outer size");
    bytes = map.bytes; bytes.push_back(0); Put(bytes, 4, static_cast<uint32_t>(bytes.size()));
    Check(Rejected(bytes), "trailing bytes rejected");
    bytes = map.bytes; bytes[map.waterOffset] = 255;
    Check(Rejected(bytes), "truncated water resource path rejected");
    if (!map.waterPath.empty())
    {
        bytes = map.bytes; bytes[map.waterOffset + 1] = 0;
        Check(Rejected(bytes), "embedded control characters in water path rejected");
    }
    auto waterMap = map;
    for (const wchar_t* name : { L"Blue", L"Yellow", L"Red", L"Black" })
    {
        Check(CM::ChangeWater(waterMap, name), "change encoded water resource");
        auto parsed = CM::Parse(waterMap.bytes);
        Check(_wcsicmp(CM::WaterName(parsed).c_str(), name) == 0 && parsed.pixels == map.pixels && parsed.waterOffset == map.waterOffset,
            "variable-length water change reparses without terrain changes");
        auto originalPrefix = std::vector<BYTE>(map.bytes.begin(), map.bytes.begin() + map.waterOffset);
        auto changedPrefix = std::vector<BYTE>(waterMap.bytes.begin(), waterMap.bytes.begin() + waterMap.waterOffset);
        Put(changedPrefix, 4, static_cast<uint32_t>(map.bytes.size()));
        Check(originalPrefix == changedPrefix, "water edit changes only final path and outer length");
    }
    const auto originalWaterBytes = waterMap.bytes;
    Check(CM::ChangeWater(waterMap, L"bLaCk") && waterMap.bytes == originalWaterBytes,
        "choosing same water preserves original path case and bytes");
    for (const auto& name : { std::wstring{}, std::wstring(L"Blue/other"), std::wstring(L"Blue\\other"),
        std::wstring(L"Blue\n"), std::wstring(256, L'x') })
        Check(!CM::ChangeWater(waterMap, name) && waterMap.bytes == originalWaterBytes, "invalid water edit leaves selected bytes intact");
    // A compressed literal/back-reference fixture exercises the bounded
    // decoder without relying on any shipped compressed map.
    CM::Map compressed;
    std::vector<BYTE> data{ 'I','M','G',0x1a,0,0,0,0,0,8,0xc0,1,0,255,0,0,0x80,7,0xb8,2,1 };
    size_t remaining = 1920 * 696 - 1;
    while (remaining >= 18)
    {
        unsigned count = static_cast<unsigned>(std::min<size_t>(273, remaining));
        data.insert(data.end(), { 0x80, 1, static_cast<BYTE>(count - 18) }); remaining -= count;
    }
    while (remaining--) data.push_back(1);
    data.insert(data.end(), { 0x80, 0 });
    Put(data, 4, static_cast<uint32_t>(data.size()));
    CM::Reader reader{ data }; CM::ReadImage(reader, compressed, 0);
    Check(compressed.pixels.size() == 1920 * 696 && compressed.pixels.back() == 1, "compressed overlapping copies decoded");
    data[20] = 0x80; data[21] = 1; // Back-reference before any output.
    bool invalid = false;
    try { CM::Reader bad{ data }; CM::ReadImage(bad, compressed, 0); } catch (...) { invalid = true; }
    Check(invalid, "invalid compressed back-reference rejected");
    puts("PASS: bounded LND/image parsing, malformed data and compressed previews");
}

static int nativeCalls = 0;
static void __fastcall NativeGenerate(void*, void*) { ++nativeCalls; }
static const CM::Map* launchExpectedMap = nullptr;
static int __fastcall NativeLaunch(void*, void*, const char*)
{
    ++nativeCalls;
    if (launchExpectedMap)
        Check(CM::Load((CM::GameDirectory() + L"\\Data\\land.dat").c_str()).bytes == launchExpectedMap->bytes,
            "engine launcher sees selected terrain after all preparation");
    return 1;
}
static void __fastcall NativePrepareTerrain(void* object, void*, const void*) { CM::Generate(object, nullptr); }
static void __fastcall NativeNetworkLaunch(void*, void*)
{
    ++nativeCalls;
    Check(launchExpectedMap && CM::Load((CM::GameDirectory() + L"\\Data\\land.dat").c_str()).bytes == launchExpectedMap->bytes,
        "native network launcher sees the frozen imported terrain");
}
static void PublishTests(const CM::Map& map)
{
    const auto directory = std::filesystem::absolute("Release/ColourMapsFixture");
    std::filesystem::create_directory(directory);
    const std::wstring target = (directory / "land.dat").wstring();
    Check(CM::Publish(map, target), "publish complete map");
    auto loaded = CM::Load(target.c_str());
    Check(loaded.bytes == map.bytes, "published map is byte-identical");
    Check(!(GetFileAttributesW(target.c_str()) & FILE_ATTRIBUTE_READONLY), "land.dat remains writable");
    SetFileAttributesW(target.c_str(), FILE_ATTRIBUTE_READONLY);
    Check(!CM::Publish(map, target), "read-only target reports failure");
    Check(CM::Load(target.c_str()).bytes == map.bytes, "failed publication preserves previous map");
    Check(GetFileAttributesW(target.c_str()) & FILE_ATTRIBUTE_READONLY, "CTerrain lock preserved");
    Check(std::distance(std::filesystem::directory_iterator(directory), std::filesystem::directory_iterator{}) == 1,
        "temporary file cleaned up on failure");
    SetFileAttributesW(target.c_str(), FILE_ATTRIBUTE_NORMAL);
    std::filesystem::remove(target); std::filesystem::remove(directory);
    CM::selected.reset(new CM::Map(map));
    CM::originalGenerate = reinterpret_cast<CM::NativeVoid>(NativeGenerate);
    CM::originalLaunch = reinterpret_cast<CM::NativeLaunch>(NativeLaunch);
    CM::preparingLocal = false; CM::launchFailed = true;
    std::array<BYTE, 0x44> object{};
    CM::Generate(object.data(), nullptr);
    Check(nativeCalls == 1 && !CM::launchFailed, "network and other native generation remains unchanged");
    const auto launchDirectory = std::filesystem::absolute("Release/Data");
    const auto launchMap = launchDirectory / "land.dat";
    Check(!std::filesystem::exists(launchDirectory), "isolated launch fixture directory");
    std::filesystem::create_directory(launchDirectory);
    CM::preparingLocal = true;
    object[0x38] = 1;
    CM::Generate(object.data(), nullptr);
    Check(nativeCalls == 1 && !CM::launchFailed && object[0x38] == 0, "local generation bypassed and completion byte set");
    Check(CM::Load(launchMap.c_str()).bytes == map.bytes, "local launch publishes exact selected bytes");
    auto changed = map; Put(changed.bytes, 16, !map.cavern);
    CM::selected.reset(new CM::Map(changed));
    CM::Generate(object.data(), nullptr);
    Check(CM::Load(launchMap.c_str()).bytes == changed.bytes, "next local launch replaces previous map");
    CM::preparingLocal = false;
    CM::launchFailed = true;
    Check(CM::LaunchForCaller(nullptr, "worms2.exe", CM::image + 0x79591) == 0 && nativeCalls == 1, "failed local import stops native engine launcher");
    launchExpectedMap = CM::selected.get();
    Check(CM::LaunchForCaller(nullptr, "worms2.exe", CM::image + 0x79591) == 1 && nativeCalls == 2, "later native launches not blocked");
    launchExpectedMap = nullptr;
    std::filesystem::remove(launchMap); std::filesystem::remove(launchDirectory);
    CM::launchFailed = false;
    CM::selected.reset();
    puts("PASS: atomic byte-preserving publication, CTerrain locks and local launch failure gate");
}

static INT_PTR CALLBACK DialogProc(HWND, UINT, WPARAM, LPARAM) { return FALSE; }
static void EditorTests(const char* frontend, const CM::Map& map)
{
    HMODULE resources = LoadLibraryExA(frontend, nullptr, LOAD_LIBRARY_AS_DATAFILE);
    Check(resources != nullptr, "load frontend resources");
    HWND parent = CreateWindowW(L"STATIC", L"Colour maps fixture", WS_OVERLAPPEDWINDOW, 0, 0, 900, 500, nullptr, nullptr, nullptr, nullptr);
    HWND window = CreateDialogParamW(resources, MAKEINTRESOURCEW(288), parent, DialogProc, 0);
    HWND gameControls = CreateDialogParamW(resources, MAKEINTRESOURCEW(149), parent, DialogProc, 0);
    Check(window != nullptr, "create actual Select Level dialog");
    Check(gameControls && CM::AttachGamePreview(gameControls) && CM::AttachGamePreview(gameControls), "attach actual Game controls preview idempotently");
    Check(CM::gamePreviews.size() == 1, "one shared preview per Game controls page");
    const LONG initialStyle = GetWindowLongW(GetDlgItem(window, 1018), GWL_STYLE);
    HWND levelStyle = GetDlgItem(window, 1021);
    auto label = [&](unsigned stringId) {
        wchar_t text[256]{};
        Check(LoadStringW(resources, stringId, text, 256) != 0, "load native style resource caption");
        return std::wstring(text);
    };
    for (unsigned stringId : { 501u, 502u, 510u })
    {
        const std::wstring text = label(stringId);
        const LRESULT index = SendMessageW(levelStyle, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(text.c_str()));
        SendMessageW(levelStyle, CB_SETITEMDATA, static_cast<WPARAM>(index), stringId);
    }
    const LRESULT originalStyleSelection = CM::FindStyle(window, 510);
    Check(originalStyleSelection != CB_ERR, "find Random in native style dropdown");
    SendMessageW(levelStyle, CB_SETCURSEL, static_cast<WPARAM>(originalStyleSelection), 0);
    EnableWindow(levelStyle, FALSE);
    HWND water = GetDlgItem(window, 1020);
    struct WaterNode { WaterNode* next; WaterNode* previous; const char* text; };
    const std::array<const char*, 7> waterNames{ "Black", "Blue", "Green", "Purple", "Red", "Yellow", "< Random >" };
    std::array<WaterNode, 7> waterNodes{};
    for (size_t i = 0; i < waterNodes.size(); ++i)
    {
        waterNodes[i] = { i + 1 < waterNodes.size() ? &waterNodes[i + 1] : nullptr, i ? &waterNodes[i - 1] : nullptr, waterNames[i] };
        SendMessageW(water, CB_INSERTSTRING, static_cast<WPARAM>(-1), i);
    }
    *reinterpret_cast<WaterNode**>(CM::image + 0x1b5124) = waterNodes.data();
    *reinterpret_cast<int*>(CM::image + 0x1b512c) = static_cast<int>(waterNodes.size());
    const LRESULT randomWater = CM::FindCaption(window, 1020, 510);
    Check(randomWater != CB_ERR, "resolve owner-drawn water item data through native list layout");
    SendMessageW(water, CB_SETCURSEL, static_cast<WPARAM>(randomWater), 0);
    CM::ComboState originalWater; CM::SaveCombo(water, originalWater);
    EnableWindow(water, FALSE);
    HWND levelList = GetDlgItem(window, 1022);
    SendMessageW(levelList, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Original level"));
    SendMessageW(levelList, CB_SETITEMDATA, 0, 1234);
    SendMessageW(levelList, CB_SETCURSEL, 0, 0);
    RECT originalGameBounds{}; GetWindowRect(GetDlgItem(gameControls, 1262), &originalGameBounds);
    const LONG originalGameStyle = GetWindowLongW(GetDlgItem(gameControls, 1262), GWL_STYLE);
    const LONG originalGameExtendedStyle = GetWindowLongW(GetDlgItem(gameControls, 1262), GWL_EXSTYLE);
    EnableWindow(GetDlgItem(window, 1024), FALSE);
    Check(CM::Attach(window) && CM::Attach(window), "attach importer idempotently");
    Check(CM::pages.size() == 1, "only one page state");
    RECT button{}, picture{};
    GetWindowRect(GetDlgItem(window, CM::ImportId), &button);
    GetWindowRect(GetDlgItem(window, 1033), &picture);
    RECT intersection{};
    Check(!IntersectRect(&intersection, &button, &picture), "import button fits beside native preview");
    CM::selected.reset(new CM::Map(map));
    CM::RefreshAll();
    Check(!(GetWindowLongW(GetDlgItem(window, 1018), GWL_STYLE) & WS_VISIBLE), "generated pictures hidden when imported");
    Check(!IsWindowEnabled(GetDlgItem(window, 1019)), "generator options disabled when imported");
    const LRESULT importedWater = SendMessageW(water, CB_GETCURSEL, 0, 0);
    const auto importedWaterName = CM::WaterName(map);
    bool knownWater = false;
    for (int i = 0; i < 6; ++i) knownWater |= _wcsicmp(CM::ComboText(water, i).c_str(), importedWaterName.c_str()) == 0;
    Check(IsWindowEnabled(water) && (knownWater ? importedWater != CB_ERR &&
        _wcsicmp(CM::ComboText(water, importedWater).c_str(), importedWaterName.c_str()) == 0 : importedWater == CB_ERR),
        "water control 1020 matches installed colours and leaves unknown resources unselected");
    Check(CM::FindCaption(window, 1020, 510) == CB_ERR && CM::selected->bytes == map.bytes,
        "initial water synchronization removes Random without rewriting imported bytes");
    Check(IsWindowEnabled(levelStyle) && SendMessageW(levelStyle, CB_GETCOUNT, 0, 0) == 2,
        "imported Level style stays enabled with Open and Cavern choices");
    Check(CM::FindStyle(window, 510) == CB_ERR, "Random is removed only while an import is active");
    for (unsigned stringId : { 501u, 502u })
    {
        const LRESULT index = CM::FindStyle(window, stringId);
        Check(index != CB_ERR && SendMessageW(levelStyle, CB_GETITEMDATA, static_cast<WPARAM>(index), 0) == static_cast<LRESULT>(stringId),
            "existing native Open/Cavern captions and item data preserved");
    }
    Check(SendMessageW(levelStyle, CB_GETCURSEL, 0, 0) == CM::FindStyle(window, map.cavern ? 502 : 501), "style reflects imported header");
    wchar_t styleText[32]{}; SendMessageW(levelList, CB_GETLBTEXT, 0, reinterpret_cast<LPARAM>(styleText));
    Check(!IsWindowEnabled(levelList) && SendMessageW(levelList, CB_GETCOUNT, 0, 0) == 1 &&
        SendMessageW(levelList, CB_GETCURSEL, 0, 0) == 0 && SendMessageW(levelList, CB_GETITEMDATA, 0, 0) == 1234 &&
        wcscmp(styleText, L"Original level") == 0, "control 1022 keeps its native level list without imported styles");
    const auto styleDirectory = std::filesystem::absolute("Release/ColourMapsStyleFixture");
    std::filesystem::create_directory(styleDirectory);
    for (int choice : { 1, 0 })
    {
        SendMessageW(levelStyle, CB_SETCURSEL, static_cast<WPARAM>(CM::FindStyle(window, choice ? 502 : 501)), 0);
        SendMessageW(window, WM_COMMAND, MAKEWPARAM(1021, CBN_SELCHANGE), reinterpret_cast<LPARAM>(levelStyle));
        auto expected = map.bytes; Put(expected, 16, choice);
        Check(CM::selected->cavern == (choice == 1) && CM::selected->bytes == expected,
            "switching Open/Cavern changes only the LND border flag");
        Check(CM::Publish(*CM::selected, (styleDirectory / "land.dat").wstring()), "publish selected style");
        auto published = CM::Load((styleDirectory / "land.dat").c_str());
        Check(published.cavern == (choice == 1) && published.bytes == expected, "published map carries selected style");
    }
    std::filesystem::remove(styleDirectory / "land.dat"); std::filesystem::remove(styleDirectory);
    auto beforeWater = *CM::selected;
    std::filesystem::create_directory(styleDirectory);
    for (const wchar_t* name : { L"Yellow", L"Red", L"Blue" })
    {
        LRESULT index = CB_ERR;
        for (int i = 0; i < 6; ++i) if (CM::ComboText(water, i) == name) index = i;
        Check(index != CB_ERR, "native water colour remains available");
        SendMessageW(water, CB_SETCURSEL, static_cast<WPARAM>(index), 0);
        SendMessageW(window, WM_COMMAND, MAKEWPARAM(1020, CBN_SELCHANGE), reinterpret_cast<LPARAM>(water));
        Check(CM::WaterName(*CM::selected) == name && CM::selected->pixels == beforeWater.pixels &&
            CM::selected->cavern == beforeWater.cavern, "water dropdown edits selected map without regeneration");
        Check(CM::Publish(*CM::selected, (styleDirectory / "land.dat").wstring()), "publish edited water colour");
        Check(CM::Load((styleDirectory / "land.dat").c_str()).bytes == CM::selected->bytes, "published terrain retains selected water path");
    }
    auto prefixedWater = beforeWater;
    Check(CM::ChangeWater(prefixedWater, L"Green"), "prepare water path normalization fixture");
    // Match case and slash/prefix variants without rewriting the import.
    const std::string variant = ".\\data/water/gReEn";
    prefixedWater.bytes.resize(prefixedWater.waterOffset);
    prefixedWater.bytes.push_back(static_cast<BYTE>(variant.size()));
    prefixedWater.bytes.insert(prefixedWater.bytes.end(), variant.begin(), variant.end());
    Put(prefixedWater.bytes, 4, static_cast<uint32_t>(prefixedWater.bytes.size()));
    CM::selected.reset(new CM::Map(CM::Parse(prefixedWater.bytes))); CM::RefreshAll();
    Check(CM::ComboText(water, SendMessageW(water, CB_GETCURSEL, 0, 0)) == L"Green" && CM::selected->bytes == prefixedWater.bytes,
        "case-insensitive colour matching accepts relative prefix and slash variants");
    std::filesystem::remove(styleDirectory / "land.dat"); std::filesystem::remove(styleDirectory);
    Check(GetWindowLongW(GetDlgItem(window, CM::ResetId), GWL_STYLE) & WS_VISIBLE, "generated map reset available");
    HDC dc = CreateCompatibleDC(nullptr);
    BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth = 600;
    info.bmiHeader.biHeight = -300; info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    void* pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    HGDIOBJ previous = SelectObject(dc, bitmap);
    CM::PaintPreview(CM::pages[0]->preview, dc);
    auto realPixels = CM::selected->pixels;
    CM::selected->pixels.assign(1920 * 696, 1);
    CM::PaintPreview(CM::pages[0]->preview, dc);
    RECT previewRect{}; GetClientRect(CM::pages[0]->preview, &previewRect);
    const auto color = map.palette[1];
    auto checkPicture = [&](HWND pictureWindow) {
        RECT size{}; GetClientRect(pictureWindow, &size);
        Check(abs(size.bottom - MulDiv(size.right, 696, 1920)) <= 1, "preview control follows map aspect ratio");
        const COLORREF expected = RGB(CM::selected->palette[1].rgbRed, CM::selected->palette[1].rgbGreen, CM::selected->palette[1].rgbBlue);
        for (int y = 0; y < size.bottom; ++y)
            for (int x = 0; x < size.right; ++x)
                Check(GetPixel(dc, x, y) == expected, "entire preview is image, without text or letterbox borders");
    };
    checkPicture(CM::pages[0]->preview);
    Check(GetPixel(dc, previewRect.right / 2, previewRect.bottom / 2) == RGB(color.rgbRed, color.rgbGreen, color.rgbBlue),
        "indexed preview uses the imported RGB palette");
    HWND gamePreview = GetDlgItem(gameControls, 1262);
    ShowWindow(window, SW_HIDE); ShowWindow(gameControls, SW_SHOW);
    memset(pixels, 0, 600 * 300 * 4);
    SendMessageW(gamePreview, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT);
    GetClientRect(gamePreview, &previewRect);
    checkPicture(gamePreview);
    Check(GetPixel(dc, previewRect.right / 2, previewRect.bottom / 2) == RGB(color.rgbRed, color.rgbGreen, color.rgbBlue),
        "switching to Game controls paints the current imported palette");
    CM::selected->palette[1].rgbRed ^= 0xff;
    CM::RefreshAll();
    SendMessageW(gamePreview, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT);
    const auto nextColor = CM::selected->palette[1];
    Check(GetPixel(dc, previewRect.right / 2, previewRect.bottom / 2) == RGB(nextColor.rgbRed, nextColor.rgbGreen, nextColor.rgbBlue),
        "Game controls refreshes when the imported selection changes");
    const auto sourceBytes = CM::selected->bytes;
    const auto sourceAir = CM::selected->palette[0];
    CM::selected->palette[1] = {}; // Black land must remain black beside blue air.
    for (int y = 0; y < 696; ++y)
        std::fill_n(CM::selected->pixels.begin() + y * 1920, 960, BYTE{ 0 });
    for (HWND pictureWindow : { CM::pages[0]->preview, gamePreview })
    {
        SendMessageW(pictureWindow, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT);
        RECT size{}; GetClientRect(pictureWindow, &size);
        Check(GetPixel(dc, size.right / 4, size.bottom / 2) == RGB(0, 160, 255), "air matches native terrain preview blue on both tabs");
        Check(GetPixel(dc, size.right * 3 / 4, size.bottom / 2) == RGB(0, 0, 0), "black terrain colours remain unchanged");
    }
    Check(CM::selected->bytes == sourceBytes && memcmp(&CM::selected->palette[0], &sourceAir, sizeof(sourceAir)) == 0,
        "preview background does not modify source bytes or palette");
    CM::selected->pixels.swap(realPixels);
    SelectObject(dc, previous); DeleteObject(bitmap); DeleteDC(dc);
    SendMessageW(window, WM_COMMAND, MAKEWPARAM(CM::ResetId, BN_CLICKED), reinterpret_cast<LPARAM>(GetDlgItem(window, CM::ResetId)));
    Check(!CM::selected, "reset clears active import");
    Check((GetWindowLongW(GetDlgItem(window, 1018), GWL_STYLE) & WS_VISIBLE) == (initialStyle & WS_VISIBLE), "native visibility restored");
    Check(!IsWindowEnabled(GetDlgItem(window, 1024)), "preexisting disabled state preserved");
    Check(!IsWindowEnabled(levelStyle) && SendMessageW(levelStyle, CB_GETCOUNT, 0, 0) == 3 &&
        SendMessageW(levelStyle, CB_GETCURSEL, 0, 0) == originalStyleSelection, "native style selection and enabled state restored");
    for (unsigned stringId : { 501u, 502u, 510u })
    {
        const LRESULT index = CM::FindStyle(window, stringId);
        Check(index != CB_ERR && SendMessageW(levelStyle, CB_GETITEMDATA, static_cast<WPARAM>(index), 0) == static_cast<LRESULT>(stringId),
            "all native style captions and item data restored, including Random");
    }
    Check(IsWindowEnabled(levelList), "native level-list enabled state restored");
    Check(!IsWindowEnabled(water) && SendMessageW(water, CB_GETCOUNT, 0, 0) == static_cast<LRESULT>(originalWater.items.size()) &&
        SendMessageW(water, CB_GETCURSEL, 0, 0) == originalWater.selection, "native water selection, Random and enabled state restored");
    for (size_t i = 0; i < originalWater.items.size(); ++i)
        Check(CM::ComboText(water, i) == originalWater.items[i].text && SendMessageW(water, CB_GETITEMDATA, i, 0) == originalWater.items[i].data,
            "native water captions, ordering and item data restored");
    RECT restoredGameBounds{}; GetWindowRect(gamePreview, &restoredGameBounds);
    Check(EqualRect(&originalGameBounds, &restoredGameBounds) != FALSE, "native Game controls preview bounds restored");
    Check(GetWindowLongW(gamePreview, GWL_STYLE) == originalGameStyle &&
        GetWindowLongW(gamePreview, GWL_EXSTYLE) == originalGameExtendedStyle, "native preview frame styles restored");
    DestroyWindow(window); DestroyWindow(gameControls); DestroyWindow(parent); FreeLibrary(resources);
    Check(CM::pages.empty() && CM::gamePreviews.empty(), "both tabs clean up preview state");
    puts("PASS: actual Terrain/Game controls dialogs, borderless previews, Open/Cavern and water switching, and native reset lifecycle");
}

static void CallerTests(BYTE* frontend, const CM::Map& map)
{
    const auto directory = std::filesystem::absolute("Release/Data");
    Check(!std::filesystem::exists(directory), "isolated real-caller fixture");
    std::filesystem::create_directory(directory);
    CM::selected.reset(new CM::Map(map));
    CM::originalPrepareTerrain = reinterpret_cast<CM::NativePrepareTerrain>(NativePrepareTerrain);
    CM::originalGenerate = reinterpret_cast<CM::NativeVoid>(NativeGenerate);
    CM::originalLaunch = reinterpret_cast<CM::NativeLaunch>(NativeLaunch);
    std::array<BYTE, 0x44> terrain{};
    // Retain each real CALL instruction and its return address. Replace only
    // surrounding MFC setup/continuation in this private mapping with a tiny
    // fixture entry/return, so the production detours classify the real caller.
    auto runCall = [&](size_t call, void* object) {
        BYTE* entry = frontend + call - 7;
        std::array<BYTE, 13> saved{}; memcpy(saved.data(), entry, saved.size());
        DWORD old = 0; Check(VirtualProtect(entry, saved.size(), PAGE_EXECUTE_READWRITE, &old) != FALSE, "protect private caller fixture");
        entry[0] = 0x6a; entry[1] = 0; entry[2] = 0xb9;
        const uint32_t address = reinterpret_cast<uint32_t>(object); memcpy(entry + 3, &address, 4);
        entry[12] = 0xc3;
        FlushInstructionCache(GetCurrentProcess(), entry, saved.size());
        reinterpret_cast<void(__cdecl*)()>(entry)();
        memcpy(entry, saved.data(), saved.size());
        FlushInstructionCache(GetCurrentProcess(), entry, saved.size());
        VirtualProtect(entry, saved.size(), old, &old);
    };
    const std::wstring destination = (directory / "land.dat").wstring();
    bool yellowWater = true;
    for (const auto calls : { std::pair<size_t, size_t>{0x79567, 0x7958c}, {0x77dff, 0x77e46} })
    {
        Check(CM::ChangeWater(*CM::selected, yellowWater ? L"Yellow" : L"Red"), "choose water before real local launch");
        yellowWater = false;
        const int before = nativeCalls;
        CM::preparingLocal = false;
        terrain[0x38] = 1;
        runCall(calls.first, terrain.data());
        Check(!CM::preparingLocal && !CM::launchFailed && terrain[0x38] == 0 && nativeCalls == before,
            "real Go/next-round preparation bypasses native generation");
        Check(CM::Load(destination.c_str()).bytes == CM::selected->bytes, "real local preparation writes imported map with edited water");
        // Reproduce the reported overwrite between preparation and launch.
        auto stale = map; Put(stale.bytes, 16, !map.cavern);
        Check(CM::Publish(stale, destination), "stage stale generated terrain");
        launchExpectedMap = CM::selected.get();
        runCall(calls.second, nullptr);
        launchExpectedMap = nullptr;
        Check(nativeCalls == before + 1, "real engine launch proceeds with imported bytes");
    }
    const int before = nativeCalls;
    CM::PrepareTerrainForCaller(terrain.data(), nullptr, frontend + 0x35819);
    Check(nativeCalls == before + 1, "network host generation remains native with an active import");
    CM::network.hosting = true;
    CM::network.round.size = static_cast<uint32_t>(map.bytes.size());
    CM::network.roundMap.reset(new CM::Map(map));
    for (size_t call : { 0x35814u, 0x623e3u })
    {
        const int calls = nativeCalls;
        runCall(call, terrain.data());
        Check(nativeCalls == calls && CM::Load(destination.c_str()).bytes == map.bytes,
            "actual first-game and next-round host CALLs bypass native generation");
    }
    CM::network.hosting = false; CM::network.joining = true;
    for (size_t call : { 0x3e261u, 0x64457u })
    {
        BYTE* entry = frontend + call - 5;
        std::array<BYTE, 11> saved{}; memcpy(saved.data(), entry, saved.size());
        DWORD old = 0; Check(VirtualProtect(entry, saved.size(), PAGE_EXECUTE_READWRITE, &old) != FALSE, "protect real client generator CALL");
        entry[0] = 0xb9; const uint32_t address = reinterpret_cast<uint32_t>(terrain.data()); memcpy(entry + 1, &address, 4);
        entry[10] = 0xc3; FlushInstructionCache(GetCurrentProcess(), entry, saved.size());
        const int calls = nativeCalls;
        reinterpret_cast<void(__cdecl*)()>(entry)();
        memcpy(entry, saved.data(), saved.size()); VirtualProtect(entry, saved.size(), old, &old);
        Check(nativeCalls == calls && CM::Load(destination.c_str()).bytes == map.bytes,
            "actual first-game and next-round client CALLs publish host map");
    }
    CM::network.hosting = true; CM::network.joining = false;
    CM::originalHostLaunch = CM::originalHostRoundLaunch = reinterpret_cast<CM::NativeVoid>(NativeNetworkLaunch);
    launchExpectedMap = CM::NetworkMap();
    for (size_t launcher : { 0x358a4u, 0x61a81u })
    {
        auto stale = map; Put(stale.bytes, 16, !map.cavern); Check(CM::Publish(stale, destination), "stage stale terrain at native network launch boundary");
        const int calls = nativeCalls;
        reinterpret_cast<CM::NativeVoid>(frontend + launcher)(nullptr);
        Check(nativeCalls == calls + 1, "actual host/next-round launch detour republishes map before launching");
    }
    launchExpectedMap = nullptr; CM::network = CM::NetworkState{};
    Check(!CM::LocalLaunchCaller(frontend + 0x74f67), "mission launch cannot inherit the local import");
    CM::selected.reset();
    const int callsBeforeReset = nativeCalls;
    runCall(0x79567, terrain.data());
    Check(nativeCalls == callsBeforeReset + 1, "real Go preparation regenerates terrain after reset");
    std::filesystem::remove(directory / "land.dat"); std::filesystem::remove(directory);
    puts("PASS: supplied normal Go and next-round CALL instructions preserve imported terrain through the engine-launch boundary");
}

static std::vector<std::vector<BYTE>> mapPackets;
static std::vector<BYTE> readyPacket;
static int dispatched = 0;
static void CaptureMap(void*, uint32_t, uint32_t, bool, const void* packet, uint32_t length)
{ mapPackets.emplace_back(static_cast<const BYTE*>(packet), static_cast<const BYTE*>(packet) + length); }
static void __fastcall CaptureReady(void*, void*, uint32_t, uint32_t, const void* packet, uint32_t length)
{ readyPacket.assign(static_cast<const BYTE*>(packet), static_cast<const BYTE*>(packet) + length); }
static void __fastcall CaptureDispatch(void*, void*, uint32_t, const void*, uint32_t) { ++dispatched; }
static void __fastcall CaptureBroadcast(void* object, void*, uint32_t source, const void* packet, uint32_t length)
{ CaptureMap(object, source, 0, true, packet, length); }
static void __fastcall CaptureTarget(void* object, void*, uint32_t source, uint32_t target, const void* packet, uint32_t length)
{ CaptureMap(object, source, target, false, packet, length); }

static void NetworkTests(const CM::Map& sample)
{
    CM::enabled = true;
    const auto directory = std::filesystem::absolute("Release/Data");
    Check(!std::filesystem::exists(directory), "isolated network map directory");
    std::filesystem::create_directory(directory);
    const auto destination = directory / "land.dat";
    CM::selected.reset(new CM::Map(sample));
    CM::ChangeWater(*CM::selected, L"Green");
    const auto expected = CM::selected->bytes;
    std::array<uint32_t, 2> go{ 27, 0 }, prepare{ 29, 0 }, ready{ 28, 20 };
    std::array<BYTE, 132> start{};
    const uint32_t startType = 14; memcpy(start.data(), &startType, 4); memcpy(start.data() + 20, "FKA1", 4);
    CM::network = CM::NetworkState{};
    mapPackets.clear();
    CM::SendNetworkPacket(nullptr, 10, 0, true, go.data(), sizeof(go), CaptureMap);
    Check(mapPackets.size() > 2 && mapPackets.back().size() == 24, "Go transfers complete map before tagged native handshake");
    const auto firstPackets = mapPackets;
    for (size_t i = 0; i + 1 < firstPackets.size(); ++i)
        Check(firstPackets[i].size() <= CM::ChunkSize + sizeof(CM::MapChunk) &&
            CM::PacketType(firstPackets[i].data(), static_cast<uint32_t>(firstPackets[i].size())) == CM::MapPacketType,
            "map chunks fit the native transport buffer");
    const auto identity = CM::network.round;
    auto hostState = std::move(CM::network);
    CM::network = CM::NetworkState{};
    const auto receive = [&](const std::vector<BYTE>& packet, uint32_t sender = 10, uint32_t host = 10) {
        uint32_t length = static_cast<uint32_t>(packet.size());
        return CM::ReceiveNetworkPacket(sender, host, packet.data(), length);
    };
    Check(!receive(firstPackets.front(), 99) && !CM::network.remote, "non-host map chunks ignored");
    for (size_t i = 0; i + 2 < firstPackets.size(); ++i) Check(!receive(firstPackets[i]), "consume bounded map chunks");
    Check(!receive(firstPackets.back()) && !CM::NetworkMap(), "incomplete maps cannot enter ready handshake");
    Check(!receive(firstPackets[firstPackets.size() - 2]) && CM::network.remote && CM::network.remote->bytes == expected,
        "joining player reconstructs exact palette, collision, spawns, borders and water bytes");
    uint32_t goLength = static_cast<uint32_t>(firstPackets.back().size());
    Check(CM::ReceiveNetworkPacket(10, 10, firstPackets.back().data(), goLength) && goLength == 8 && CM::NetworkMap()->bytes == expected,
        "validated map enters native handshake with trailer stripped");
    Check(CM::Load(destination.c_str()).bytes == expected, "joining player publishes exact host map");
    Check(CM::PreviewMap() == CM::network.remote.get(), "joining preview uses host terrain instead of a local selection");
    CM::originalGenerate = reinterpret_cast<CM::NativeVoid>(NativeGenerate);
    CM::originalSendReady = reinterpret_cast<CM::NativeSendPlayer>(CaptureReady);
    std::array<BYTE, 0x44> terrain{};
    const int before = nativeCalls;
    for (size_t caller : { 0x3e266u, 0x6445cu })
    {
        terrain[0x38] = 1;
        CM::GenerateForCaller(terrain.data(), CM::image + caller);
        Check(nativeCalls == before && terrain[0x38] == 0 && !CM::network.failed, "first-game and next-round clients bypass native landgen");
    }
    CM::GenerateForCaller(terrain.data(), CM::image + 0x74f0b);
    Check(nativeCalls == before + 1, "mission generation remains native during network import");
    CM::SendReady(nullptr, nullptr, 20, 10, ready.data(), sizeof(ready));
    Check(readyPacket.size() == 24, "client readiness confirms exact map identity");
    auto firstReady = readyPacket;
    SetFileAttributesW(destination.c_str(), FILE_ATTRIBUTE_READONLY);
    readyPacket.clear();
    CM::SendReady(nullptr, nullptr, 20, 10, ready.data(), sizeof(ready));
    Check(readyPacket.empty(), "locked destination cannot acknowledge a network map");
    SetFileAttributesW(destination.c_str(), FILE_ATTRIBUTE_NORMAL);
    auto clientState = std::move(CM::network);
    CM::network = std::move(hostState);
    CM::originalPrepareTerrain = reinterpret_cast<CM::NativePrepareTerrain>(NativePrepareTerrain);
    for (size_t caller : { 0x35819u, 0x623e8u })
    {
        const int calls = nativeCalls;
        CM::PrepareTerrainForCaller(terrain.data(), nullptr, CM::image + caller);
        Check(nativeCalls == calls && !CM::network.generating && CM::Load(destination.c_str()).bytes == expected,
            "first-game and next-round hosts publish imported map instead of generating terrain");
    }
    uint32_t playerCount = 1, player = 20;
    memcpy(CM::image + 0x1a7698 + 0x10d4, &playerCount, 4);
    memcpy(CM::image + 0x1a7698 + 0x108c, &player, 4);
    CM::originalHostReceive = CM::originalHostRoundReceive = reinterpret_cast<CM::NativeReceive>(CaptureDispatch);
    for (uint32_t reliable : { 0u, 1u })
    {
        memcpy(CM::image + 0x188b14, &reliable, 4);
        for (auto dispatcher : { CM::HostReceive, CM::HostRoundReceive })
        {
            CM::network.acknowledgements = {}; dispatched = 0;
            dispatcher(nullptr, nullptr, 20, ready.data(), sizeof(ready) + reliable * 4);
            Check(dispatched == 0, "legacy clients cannot advance an imported-map start");
            auto invalid = firstReady; invalid.back() ^= 1;
            dispatcher(nullptr, nullptr, 20, invalid.data(), static_cast<uint32_t>(invalid.size()) + reliable * 4);
            dispatcher(nullptr, nullptr, 99, firstReady.data(), static_cast<uint32_t>(firstReady.size()) + reliable * 4);
            Check(dispatched == 0, "bad checksum, claimed sender and non-member acknowledgements rejected");
            dispatcher(nullptr, nullptr, 20, firstReady.data(), static_cast<uint32_t>(firstReady.size()) + reliable * 4);
            dispatcher(nullptr, nullptr, 20, firstReady.data(), static_cast<uint32_t>(firstReady.size()) + reliable * 4);
            Check(dispatched == 1, "valid readiness accepted once on both transports and round dispatchers");
            auto generatedReady = firstReady; const uint32_t generatedType = 30; memcpy(generatedReady.data(), &generatedType, 4);
            dispatcher(nullptr, nullptr, 20, generatedReady.data(), static_cast<uint32_t>(generatedReady.size()) + reliable * 4);
            Check(dispatched == 2, "validated generation readiness advances native handshake");
        }
    }
    mapPackets.clear();
    CM::SendNetworkPacket(nullptr, 10, 0, true, prepare.data(), sizeof(prepare), CaptureMap);
    const auto preparePacket = mapPackets.back();
    CM::SendNetworkPacket(nullptr, 10, 0, true, start.data(), static_cast<uint32_t>(start.size()), CaptureMap);
    const auto startPacket = mapPackets.back();
    Check(startPacket.size() == 148 && !memcmp(startPacket.data(), start.data(), start.size()), "colour map trailer preserves CPU-team start extension and native GUID");
    hostState = std::move(CM::network); CM::network = std::move(clientState);
    uint32_t length = static_cast<uint32_t>(preparePacket.size());
    Check(CM::ReceiveNetworkPacket(10, 10, preparePacket.data(), length) && length == 8, "prepare marker validated before native generation");
    length = static_cast<uint32_t>(startPacket.size());
    Check(CM::ReceiveNetworkPacket(10, 10, startPacket.data(), length) && length == start.size(), "start restores unchanged CPU packet length before shared decoder");
    Check(!receive(std::vector<BYTE>(start.begin(), start.end())), "missing start marker cannot launch an imported round");
    auto corruptStart = startPacket; corruptStart.back() ^= 1;
    Check(!receive(corruptStart), "wrong map cannot launch");
    SetFileAttributesW(destination.c_str(), FILE_ATTRIBUTE_READONLY);
    Check(!receive(startPacket), "client launch blocked when final publication fails");
    SetFileAttributesW(destination.c_str(), FILE_ATTRIBUTE_NORMAL);
    // Malformed terminal chunks must be bounded even for exact multiples of the chunk size.
    CM::MapChunk malformed; malformed.identity.revision = identity.revision + 1;
    malformed.identity.size = CM::ChunkSize; malformed.offset = CM::ChunkSize;
    Check(!CM::ReceiveMapChunk(&malformed, sizeof(malformed)), "zero-byte chunk at end of nonempty map rejected");
    auto truncated = firstPackets.front(); truncated.resize(sizeof(CM::MapChunk) - 1);
    Check(!CM::ReceiveMapChunk(truncated.data(), static_cast<uint32_t>(truncated.size())), "truncated chunk header rejected");
    auto oversize = malformed; oversize.identity.size = static_cast<uint32_t>(CM::MaximumFileSize) + 1;
    Check(!CM::ReceiveMapChunk(&oversize, sizeof(oversize)), "oversize allocation rejected");
    CM::network = CM::NetworkState{};
    for (size_t i = 0; i + 1 < firstPackets.size(); ++i)
    {
        auto corrupt = firstPackets[i]; if (i == 1) corrupt.back() ^= 1;
        receive(corrupt);
    }
    Check(!CM::network.remote && !receive(firstPackets.back()), "corrupt map never publishes or becomes ready");
    CM::network = std::move(hostState);
    CM::ChangeWater(*CM::selected, L"Red");
    Check(CM::NetworkMap()->bytes == expected, "editing selection cannot change a round already in handshake");
    mapPackets.clear(); CM::SendNetworkPacket(nullptr, 10, 0, true, go.data(), sizeof(go), CaptureMap);
    Check(CM::network.round.revision != identity.revision && CM::NetworkMap()->bytes == CM::selected->bytes,
        "next round transfers new selection with a fresh identity");
    // Native host snapshot is sent before its map, so a late join knows the sender.
    std::vector<BYTE> snapshot(0x10de); const uint32_t snapshotType = 5; memcpy(snapshot.data(), &snapshotType, 4);
    mapPackets.clear(); CM::SendNetworkPacket(nullptr, 10, 20, false, snapshot.data(), static_cast<uint32_t>(snapshot.size()), CaptureMap);
    Check(mapPackets.front() == snapshot && mapPackets.size() > 2, "late join receives native snapshot followed by selected map");
    CM::selected.reset(); mapPackets.clear(); CM::SelectionChanged();
    Check(mapPackets.size() == 1 && mapPackets[0].size() == sizeof(CM::MapChunk), "generated-map reset broadcast uses a bounded empty transfer");
    CM::network = CM::NetworkState{}; CM::SelectNetworkHost(10);
    receive(mapPackets[0]); Check(!CM::network.remote, "generated-map reset clears remote preview");
    length = sizeof(go); Check(CM::ReceiveNetworkPacket(10, 10, go.data(), length) && !CM::NetworkMap(), "native round clears imported state");
    HWND window = CreateWindowW(L"STATIC", L"Network lifecycle fixture", WS_POPUP, 0, 0, 100, 100, nullptr, nullptr, nullptr, nullptr);
    std::array<BYTE, 0x20> object{}; memcpy(object.data() + 28, &window, sizeof(window));
    CM::WatchNetworkWindow(object.data()); DestroyWindow(window);
    Check(!CM::network.hosting && !CM::network.joining && !CM::network.remote && !CM::network.transport, "lobby destruction releases transfer and role state");
    std::filesystem::remove(destination); std::filesystem::remove(directory);
    CM::enabled = false;
    puts("PASS: chunked network map transfer, exact terrain identity, first-game/next-round ready gates, late joins, reset and transport bounds");
}

static void SharedNetworkTests(const CM::Map& map)
{
    namespace SW = SecretWeapons; namespace NT = NetworkTeams; namespace EO = ExtendedOptions;
    SW::image = NT::image = EO::image = CM::image;
    NT::enabled = EO::enabled = CM::enabled = true;
    SW::originalSendToAll = reinterpret_cast<SW::SendToAll>(CaptureBroadcast);
    SW::originalSendToPlayer = reinterpret_cast<SW::SendToPlayer>(CaptureTarget);
    SW::originalReceivePacket = reinterpret_cast<SW::ReceivePacket>(CaptureDispatch);
    NT::originalReceiveRoundPacket = reinterpret_cast<NT::ReceiveRoundPacket>(CaptureDispatch);
    BYTE* game = CM::image + NT::GameRva;
    strcpy_s(reinterpret_cast<char*>(game + NT::TeamOffset + 2), 17, "Map CPU");
    const int savedIndex = 0, skill = 50;
    memcpy(game + 4, &savedIndex, 4); memcpy(CM::image + NT::SavedTeamsRva, &skill, 4);
    const auto directory = std::filesystem::absolute("Release/Data");
    Check(!std::filesystem::exists(directory), "isolated shared network fixture"); std::filesystem::create_directory(directory);
    CM::selected.reset(new CM::Map(map));
    std::array<uint32_t, 2> go{ 27, 0 };
    std::array<BYTE, 20> start{}; const uint32_t startType = 14; memcpy(start.data(), &startType, 4);
    for (uint32_t reliable : { 0u, 1u })
    {
        memcpy(CM::image + 0x188b14, &reliable, 4);
        for (bool round : { false, true })
        {
            CM::network = CM::NetworkState{}; mapPackets.clear();
            SW::SendWeaponPacketToAll(nullptr, nullptr, 10, go.data(), sizeof(go));
            SW::SendWeaponPacketToAll(nullptr, nullptr, 10, start.data(), sizeof(start));
            Check(mapPackets.back().size() == NT::StartPacket{}.size() + sizeof(CM::MapIdentity), "shared send combines CPU and map metadata");
            std::array<BYTE, SW::NativeWeaponPacketSize> weapons{};
            const uint32_t weaponType = SW::WeaponSchemePacketType; memcpy(weapons.data(), &weaponType, 4);
            SW::secretStocks.fill(7);
            SW::SendWeaponPacketToAll(nullptr, nullptr, 10, weapons.data(), static_cast<uint32_t>(weapons.size()));
            std::array<BYTE, EO::NativePacketSize> options{};
            const uint32_t optionType = 0x19; memcpy(options.data(), &optionType, 4);
            EO::values.fill(0); EO::values[0] = 1;
            SW::SendWeaponPacketToAll(nullptr, nullptr, 10, options.data(), static_cast<uint32_t>(options.size()));
            const auto packets = mapPackets;
            CM::network = CM::NetworkState{}; dispatched = 0;
            SW::secretStocks.fill(0); EO::values.fill(0);
            std::vector<BYTE> object(0x3700);
            const uint32_t host = 10; memcpy(object.data() + (round ? 0xa8 : 0x163c), &host, 4);
            HWND window = CreateWindowW(L"STATIC", L"Joining round fixture", WS_POPUP, 0, 0, 100, 100, nullptr, nullptr, nullptr, nullptr);
            memcpy(object.data() + 28, &window, sizeof(window));
            for (const auto& packet : packets)
            {
                const uint32_t length = static_cast<uint32_t>(packet.size()) + reliable * 4;
                if (round) NT::ReceiveNextRoundPacket(object.data(), nullptr, host, packet.data(), length);
                else SW::ReceiveWeaponPacket(object.data(), nullptr, host, packet.data(), length);
            }
            Check(dispatched == 4 && CM::NetworkMap() && CM::NetworkMap()->bytes == map.bytes,
                "shared lobby/results receiver consumes chunks and forwards native Go/start on both transports");
            Check(NT::receivedTeams[0].skill == 50 && strcmp(NT::receivedTeams[0].name, "Map CPU") == 0,
                "CPU-team metadata survives colour map trailer stripping");
            if (!round)
                Check(SW::secretStocks.front() == 7 && SW::secretStocks.back() == 7 && EO::values[0] == 1,
                    "weapon and option extensions still decode alongside map transfer");
            if (round)
            {
                Check(CM::network.window == window, "joining results dispatcher tracks its window");
                DestroyWindow(window);
                Check(!CM::network.joining && !CM::network.remote, "closing results releases remote map and client role");
            }
            else DestroyWindow(window);
        }
    }
    CM::network = CM::NetworkState{}; CM::selected.reset(); CM::enabled = false;
    NT::enabled = EO::enabled = false;
    std::filesystem::remove(directory / "land.dat"); std::filesystem::remove(directory);
    puts("PASS: actual shared send/lobby/results hooks combine maps, CPU teams, weapons and options on both transports; results cleanup");
}

static void HookTests(const char* path, const CM::Map& map)
{
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    Check(file != INVALID_HANDLE_VALUE, "open frontend");
    HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr);
    BYTE* frontend = static_cast<BYTE*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));
    Check(frontend != nullptr, "map frontend privately without running it");
    const size_t sites[] = { 0x42a44, 0x7896a, 0xa349, 0x4468b, 0x46f09, 0x277c4,
        0x3233d, 0x34294, 0x61865, 0x1e32f, 0x358a4, 0x61a81 };
    std::array<std::array<BYTE, 16>, 12> originals{};
    for (size_t i = 0; i < originals.size(); ++i) memcpy(originals[i].data(), frontend + sites[i], 16);
    Check(MH_Initialize() == MH_OK, "initialize MinHook");
    for (size_t site : sites)
    {
        DWORD old = 0; VirtualProtect(frontend + site, 1, PAGE_EXECUTE_READWRITE, &old);
        const BYTE saved = frontend[site]; frontend[site] = 0x90;
        Check(!CM::InstallInImage(frontend) && !CM::enabled, "unsupported signature rejected before hooking");
        frontend[site] = saved; VirtualProtect(frontend + site, 1, old, &old);
        for (size_t i = 0; i < originals.size(); ++i) Check(!memcmp(originals[i].data(), frontend + sites[i], 16), "rejection leaves code untouched");
    }
    Check(CM::InstallInImage(frontend), "install twelve local/network colour-map detours");
    for (size_t i = 0; i < originals.size(); ++i) Check(memcmp(originals[i].data(), frontend + sites[i], 16), "hook enabled");
    CallerTests(frontend, map);
    Check(MH_Uninitialize() == MH_OK, "remove hooks");
    for (size_t i = 0; i < originals.size(); ++i) Check(!memcmp(originals[i].data(), frontend + sites[i], 16), "native code restored");
    UnmapViewOfFile(frontend); CloseHandle(mapping); CloseHandle(file);
    puts("PASS: supported frontend hook installation, signature rejection and native code restoration");
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 3, "supply frontend path and map import directory");
        INITCOMMONCONTROLSEX controls{ sizeof(controls), ICC_WIN95_CLASSES };
        InitCommonControlsEx(&controls);
        std::vector<BYTE> testImage(0x1b6000); CM::image = testImage.data();
        unsigned count = 0;
        std::unique_ptr<CM::Map> sample;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(argv[2]))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".dat") continue;
            CM::Map map = CM::Load(entry.path().c_str());
            if (!sample) sample.reset(new CM::Map(map));
            ++count;
        }
        Check(sample != nullptr, "found sample terrain");
        printf("PASS: all %u supplied terrain files load with their palettes and object locations\n", count);
        ParserTests(*sample); PublishTests(*sample); EditorTests(argv[1], *sample); NetworkTests(*sample); SharedNetworkTests(*sample); HookTests(argv[1], *sample);
        return 0;
    }
    catch (const std::exception& error) { fprintf(stderr, "FAIL: %s\n", error.what()); return 1; }
}
