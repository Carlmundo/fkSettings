#include <cstdio>
#include <filesystem>
#include "../fkSettings/ColourMaps.cpp"
#include "../fkSettings/SecretWeapons.cpp"
#include "../fkSettings/ExtendedOptions.cpp"
#include "../fkSettings/NetworkTeams.cpp"

namespace CM = ColourMaps;
static void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
static void Put(std::vector<BYTE>& bytes, size_t offset, uint32_t value) { memcpy(bytes.data() + offset, &value, 4); }
static void ChecksumTests()
{
    const BYTE digits[] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
    Check(CM::Checksum(digits, sizeof(digits)) == 0xcbf43926, "CRC32 standard check vector");
    Check(CM::Checksum(nullptr, 0) == 0, "empty CRC32");
    std::vector<BYTE> bytes(256);
    for (size_t i = 0; i < bytes.size(); ++i) bytes[i] = static_cast<BYTE>(i);
    Check(CM::Checksum(bytes) == 0x29058c73, "CRC32 of all byte values matches an independent reference");
    bytes.assign(CM::ChunkSize, 0);
    Check(CM::Checksum(bytes) == 0x011ffca6, "CRC32 of a zero-filled network chunk matches an independent reference");
    puts("PASS: CRC32 independently verified known vectors");
}
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

static INT_PTR CALLBACK TransparentStatusDialogProc(HWND, UINT message, WPARAM wparam, LPARAM)
{
    if (message == WM_CTLCOLORSTATIC)
    {
        SetBkMode(reinterpret_cast<HDC>(wparam), TRANSPARENT);
        return reinterpret_cast<INT_PTR>(GetStockObject(HOLLOW_BRUSH));
    }
    return FALSE;
}
static void StatusRepaintTests(HWND page)
{
    HWND label = GetDlgItem(page, CM::StatusId), progress = GetDlgItem(page, CM::ProgressId);
    RECT rect{}; GetClientRect(label, &rect);
    BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = rect.right; info.bmiHeader.biHeight = -rect.bottom;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
    void* pixels = nullptr; HDC dc = CreateCompatibleDC(nullptr);
    HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    Check(dc && bitmap && pixels, "status repaint bitmap"); HGDIOBJ previous = SelectObject(dc, bitmap);
    const size_t size = size_t(rect.right) * rect.bottom * 4;
    memset(pixels, 0xa5, size);
    CM::TransferStatus status; status.visible = true;
    for (const wchar_t* caption : { L"Sending map: 1%", L"Sending map: 45%", L"Game queued; sending map: 99%", L"Map ready" })
    {
        status.text = status.compact = caption;
        CM::ShowTransferStatus(label, progress, status, false);
        SendMessageW(label, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT);
    }
    GdiFlush();
    const auto repeated = std::vector<BYTE>(static_cast<BYTE*>(pixels), static_cast<BYTE*>(pixels) + size);
    memset(pixels, 0x5a, size);
    SendMessageW(label, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT); GdiFlush();
    Check(!memcmp(repeated.data(), pixels, size), "changing status captions paints exactly the final caption without old text or background pixels");
    SelectObject(dc, previous); DeleteObject(bitmap); DeleteDC(dc);
}
static void TransferStatusTests(const char* frontend, const CM::Map& map)
{
    HMODULE resources = LoadLibraryExA(frontend, nullptr, LOAD_LIBRARY_AS_DATAFILE);
    Check(resources != nullptr, "load actual dialogs for transfer status");
    HWND parent = CreateWindowW(L"STATIC", L"Map status fixture", WS_OVERLAPPEDWINDOW, 0, 0, 900, 500, nullptr, nullptr, nullptr, nullptr);
    HWND terrain = CreateDialogParamW(resources, MAKEINTRESOURCEW(288), parent, TransparentStatusDialogProc, 0);
    HWND game = CreateDialogParamW(resources, MAKEINTRESOURCEW(149), parent, TransparentStatusDialogProc, 0);
    Check(CM::Attach(terrain) && CM::AttachGamePreview(game), "attach both status displays to native dialogs");
    auto text = [](HWND page) { wchar_t value[128]{}; GetWindowTextW(GetDlgItem(page, CM::StatusId), value, 128); return std::wstring(value); };
    auto visible = [](HWND page) { return (GetWindowLongW(GetDlgItem(page, CM::StatusId), GWL_STYLE) & WS_VISIBLE) != 0; };
    CM::ResetNetwork(); CM::selected.reset(new CM::Map(map)); CM::RefreshAll();
    Check(!visible(terrain) && !visible(game), "local imports do not show an online transfer status");
    CM::network.hosting = true;
    CM::MapTransfer transfer; transfer.map = std::make_shared<CM::Map>(map); transfer.players = { 10, 20 };
    transfer.identity.revision = 1; transfer.identity.size = static_cast<uint32_t>(map.bytes.size()); transfer.identity.crc = CM::Checksum(map.bytes);
    transfer.offset = static_cast<uint32_t>(map.bytes.size() / 2 / CM::ChunkSize) * CM::ChunkSize;
    transfer.confirmedBytes = static_cast<uint64_t>(transfer.offset) * transfer.players.size(); CM::network.transfers.push_back(transfer); CM::UpdateTransferStatus();
    Check(visible(terrain) && visible(game) && text(terrain).find(L"Sending map:") == 0 && text(game).find(L"Sending ") == 0,
        "host has visible sending percentages on Terrain and beside Go");
    const auto percent = SendDlgItemMessageW(game, CM::ProgressId, PBM_GETPOS, 0, 0);
    Check(percent > 0 && percent < 100, "status progress reports confirmed bytes rather than submitted bytes");
    CM::network.pendingGo.assign(8, 0); CM::UpdateTransferStatus();
    Check(text(terrain).find(L"Game queued;") == 0 && text(game).find(L"Queued ") == 0,
        "early Go visibly reports the game is queued while the transfer continues");
    CM::CancelTransfers();
    Check(text(game) == L"Retry Go", "failed transfer gives a visible retry instruction");
    CM::network.failed = false; CM::UpdateTransferStatus();
    Check(text(game) == L"Map ready" && SendDlgItemMessageW(game, CM::ProgressId, PBM_GETPOS, 0, 0) == 100,
        "completion remains visibly ready with full progress");
    CM::ResetNetwork(); CM::network.joining = true; CM::network.incoming = transfer.identity;
    CM::network.received = transfer.identity.size / 2; CM::UpdateTransferStatus();
    Check(text(terrain).find(L"Receiving map:") == 0 && text(game).find(L"Receiving ") == 0, "joiner sees receive progress before its preview is ready");
    CM::network.remote.reset(new CM::Map(map)); CM::network.complete = CM::network.incoming; CM::RefreshAll();
    Check(text(terrain) == L"Map ready" && text(game) == L"Map ready", "joiner reports readiness only after complete validation");
    for (HWND page : { terrain, game })
    {
        RECT label{}, bar{}, image{}, intersection{};
        GetWindowRect(GetDlgItem(page, CM::StatusId), &label); GetWindowRect(GetDlgItem(page, CM::ProgressId), &bar);
        GetWindowRect(GetDlgItem(page, page == game ? 1262 : CM::PreviewId), &image);
        Check(!IntersectRect(&intersection, &label, &image) && !IntersectRect(&intersection, &bar, &image), "transfer status and progress stay outside the map image");
    }
    RECT label{}, go{}, teams{}, intersection{};
    GetWindowRect(GetDlgItem(game, CM::StatusId), &label); GetWindowRect(GetDlgItem(game, 1066), &go); GetWindowRect(GetDlgItem(game, 1032), &teams);
    Check(!IntersectRect(&intersection, &label, &go) && !IntersectRect(&intersection, &label, &teams), "Go status fits the native gap without overlapping Go or teams");
    HDC dc = GetDC(GetDlgItem(game, CM::StatusId));
    HGDIOBJ previous = SelectObject(dc, reinterpret_cast<HFONT>(SendDlgItemMessageW(game, CM::StatusId, WM_GETFONT, 0, 0)));
    SIZE extent{}; GetTextExtentPoint32W(dc, L"Receiving 99%", 13, &extent); SelectObject(dc, previous); ReleaseDC(GetDlgItem(game, CM::StatusId), dc);
    Check(extent.cx <= label.right - label.left, "receive progress caption fits the native dialog font");
    StatusRepaintTests(terrain); StatusRepaintTests(game);
    CM::ResetNetwork(); Check(!visible(terrain) && !visible(game), "disconnect hides both transfer indicators");
    DestroyWindow(terrain); DestroyWindow(game); DestroyWindow(parent); FreeLibrary(resources); CM::selected.reset();
    Check(CM::pages.empty() && CM::gamePreviews.empty(), "status controls clean up with their pages");
    puts("PASS: native Terrain/Go transfer percentages, queued start, ready/failure status, transparent-parent repaint, layout and lifecycle");
}

static std::vector<std::vector<BYTE>> mapPackets;
static std::vector<uint32_t> mapTargets;
static std::vector<BYTE> readyPacket;
static int dispatched = 0;
static void CaptureMap(void*, uint32_t, uint32_t target, bool, const void* packet, uint32_t length)
{ mapPackets.emplace_back(static_cast<const BYTE*>(packet), static_cast<const BYTE*>(packet) + length); mapTargets.push_back(target); }
static void __fastcall CaptureReady(void*, void*, uint32_t, uint32_t, const void* packet, uint32_t length)
{ readyPacket.assign(static_cast<const BYTE*>(packet), static_cast<const BYTE*>(packet) + length); }
static void __fastcall CaptureDispatch(void*, void*, uint32_t, const void*, uint32_t) { ++dispatched; }
static void __fastcall CaptureBroadcast(void* object, void*, uint32_t source, const void* packet, uint32_t length)
{ CaptureMap(object, source, 0, true, packet, length); }
static void __fastcall CaptureTarget(void* object, void*, uint32_t source, uint32_t target, const void* packet, uint32_t length)
{ CaptureMap(object, source, target, false, packet, length); }
static bool confirmMap = true;
static bool CaptureMapAttempt(void* object, uint32_t source, uint32_t target, const void* packet, uint32_t length)
{
    CaptureMap(object, source, target, false, packet, length);
    if (confirmMap && length >= sizeof(CM::MapChunk))
    {
        CM::CheckedMapChunk ack; memcpy(&ack.header, packet, sizeof(ack.header)); ack.header.type = CM::MapAckPacketType;
        const uint32_t ackSize = ack.header.version >= 6 ? sizeof(ack) : sizeof(ack.header);
        if (ack.header.version >= 6) memcpy(&ack.crc, static_cast<const BYTE*>(packet) + sizeof(ack.header), 4);
        Check(CM::ReceiveMapAck(target, &ack, ackSize), "mock peer confirms the submitted chunk");
    }
    return true;
}
static bool CaptureControlAttempt(void* object, uint32_t source, uint32_t target, const void* packet, uint32_t length, bool ready)
{
    if (ready) CaptureReady(object, nullptr, source, target, packet, length);
    else CaptureMap(object, source, target, false, packet, length);
    return true;
}
static void DrainMapTransfers()
{
    for (size_t i = 0; (!CM::network.transfers.empty() || CM::network.deferredGo || !CM::network.controls.empty() || !CM::network.pendingGo.empty()) && i < 8192; ++i) CM::PumpTransfers();
    Check(CM::network.transfers.empty() && !CM::network.deferredGo && CM::network.controls.empty() && CM::network.pendingGo.empty() && !CM::transferTimer,
        "paced terrain and handshake sends drain and stop their timer");
}
static int busyAttempts = 0, heartbeats = 0;
static bool BusyMapAttempt(void*, uint32_t, uint32_t, const void*, uint32_t) { ++busyAttempts; return false; }
static bool BusyControlAttempt(void*, uint32_t, uint32_t, const void*, uint32_t, bool) { ++busyAttempts; return false; }
static int nativeHandshakeCalls = 0;
static void NativeHandshake(void*, uint32_t, uint32_t, bool, const void*, uint32_t) { ++nativeHandshakeCalls; }
static void __fastcall NativeReadyHandshake(void*, void*, uint32_t, uint32_t, const void*, uint32_t) { ++nativeHandshakeCalls; }
static LRESULT CALLBACK HeartbeatProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR)
{
    if (message == WM_APP + 1) { ++heartbeats; return 0; }
    return DefSubclassProc(window, message, wparam, lparam);
}
static void PacedNetworkTests(const CM::Map& sample)
{
    CM::enabled = true; CM::ResetNetwork(); CM::mapSendAttempt = CaptureMapAttempt;
    CM::selected.reset(new CM::Map(sample));
    const auto directory = std::filesystem::absolute("Release/Data");
    Check(!std::filesystem::exists(directory), "isolated paced transfer fixture"); std::filesystem::create_directory(directory);
    const uint32_t peer = 20, secondPeer = 30;
    memcpy(CM::image + 0x1a7698 + 0x108c, &peer, 4); memcpy(CM::image + 0x1a7698 + 0x1090, &secondPeer, 4);
    CM::network.hosting = true; CM::network.source = 10; CM::network.send = CaptureMap;
    mapPackets.clear(); mapTargets.clear(); CM::SelectionChanged();
    Check(mapPackets.empty() && CM::transferTimer && CM::network.transfers.size() == 1, "selection schedules terrain without sending inside the handler");
    const auto old = CM::network.outgoing;
    CM::PumpTransfers();
    Check(mapPackets.size() >= 2 && mapPackets.size() <= CM::TransferBurst && mapTargets[0] == peer && mapTargets[1] == secondPeer && mapPackets[0] == mapPackets[1],
        "bounded timer burst sends the same initial group to both peers");
    const size_t submitted = mapPackets.size();
    CM::mapSendAttempt = BusyMapAttempt; busyAttempts = 0;
    CM::PumpTransfers();
    Check(busyAttempts == 1 && mapPackets.size() == submitted,
        "busy transport ends its burst after one attempt without waiting");
    CM::mapSendAttempt = CaptureMapAttempt; CM::PumpTransfers();
    Check(mapPackets.size() > submitted && mapPackets.size() <= submitted + CM::TransferBurst,
        "the next tick resumes a bounded burst after backpressure");
    CM::ChangeWater(*CM::selected, L"Blue"); CM::SelectionChanged();
    CM::ChangeWater(*CM::selected, L"Red"); CM::SelectionChanged();
    Check(CM::network.transfers.size() == 1 && !CM::SameIdentity(old, CM::network.outgoing) &&
        CM::network.transfers.front().map->bytes == CM::selected->bytes, "rapid edits replace obsolete transfers with the latest immutable selection");
    std::array<uint32_t, 2> go{ 27, 0 };
    mapPackets.clear(); CM::SendNetworkPacket(nullptr, 10, 0, true, go.data(), sizeof(go), CaptureMap);
    Check(mapPackets.empty() && CM::network.pendingGo.size() == 24 && CM::network.transfers.size() == 1,
        "Go returns immediately and reuses the in-progress selection transfer");
    const auto frozen = CM::network.roundMap->bytes;
    CM::ChangeWater(*CM::selected, L"Yellow"); CM::SelectionChanged();
    Check(CM::network.transfers.front().map->bytes == frozen && CM::network.roundMap->bytes == frozen,
        "edits cannot replace the terrain of a queued Go");
    DrainMapTransfers();
    Check(CM::PacketType(mapPackets.back().data(), static_cast<uint32_t>(mapPackets.back().size())) == 27,
        "Go is sent only after every recipient's chunks");
    CM::selected.reset(new CM::Map(*CM::network.roundMap)); mapPackets.clear();
    const auto previousRound = CM::network.round;
    CM::SendNetworkPacket(nullptr, 10, 0, true, go.data(), sizeof(go), CaptureMap);
    Check(mapPackets.empty() && CM::network.transfers.size() == 1 && CM::network.transfers.front().reuse,
        "unchanged Go queues a small cache reference instead of the entire terrain");
    DrainMapTransfers();
    Check(mapPackets.size() == 4 && mapPackets[0].size() == sizeof(CM::MapChunk) && mapPackets.back().size() == 24 &&
        !CM::SameIdentity(previousRound, CM::network.round), "cached terrain uses a fresh round revision without pixel retransmission");
    const auto cachedPackets = mapPackets;
    auto host = std::move(CM::network); CM::network = CM::NetworkState{};
    CM::SelectNetworkHost(10);
    Check(!CM::ReceiveMapChunk(cachedPackets[0].data(), static_cast<uint32_t>(cachedPackets[0].size())), "cache reference cannot substitute for missing terrain");
    CM::network.remote.reset(new CM::Map(*CM::selected)); CM::network.incoming = CM::network.complete = previousRound;
    auto wrongReference = cachedPackets[0]; wrongReference[20] ^= 1;
    Check(!CM::ReceiveMapChunk(wrongReference.data(), static_cast<uint32_t>(wrongReference.size())), "wrong checksum cannot reuse a cached map");
    Check(CM::ReceiveMapChunk(cachedPackets[0].data(), static_cast<uint32_t>(cachedPackets[0].size())) &&
        CM::ReceiveMapChunk(cachedPackets[1].data(), static_cast<uint32_t>(cachedPackets[1].size())), "cached map accepts a fresh reference and its duplicate");
    // After the cache reference clears the assembly vectors, an unsolicited
    // full chunk with the same identity must remain bounded.
    CM::MapChunk duplicate; duplicate.identity = CM::network.complete; duplicate.size = CM::ChunkSize;
    std::vector<BYTE> duplicatePacket(sizeof(duplicate) + duplicate.size);
    memcpy(duplicatePacket.data(), &duplicate, sizeof(duplicate)); memcpy(duplicatePacket.data() + sizeof(duplicate), frozen.data(), duplicate.size);
    Check(CM::ReceiveMapChunk(duplicatePacket.data(), static_cast<uint32_t>(duplicatePacket.size())), "cached duplicate chunk validates against the map without indexing an empty assembly");
    uint32_t cachedLength = static_cast<uint32_t>(cachedPackets.back().size());
    Check(CM::ReceiveNetworkPacket(10, 10, cachedPackets.back().data(), cachedLength) && cachedLength == 8 &&
        CM::network.roundMap->bytes == frozen, "cached Go launches the same complete terrain with a fresh identity");
    CM::network = std::move(host);
    std::array<BYTE, 24> staleReady{}; const uint32_t readyType = 28;
    memcpy(staleReady.data(), &readyType, 4); memcpy(staleReady.data() + 4, &peer, 4); memcpy(staleReady.data() + 8, &previousRound, sizeof(previousRound));
    Check(!CM::AcceptReady(peer, staleReady.data(), static_cast<uint32_t>(staleReady.size())), "cached rounds reject stale ready replies from the previous round");
    // A late join during a broadcast must get its own transfer if that peer
    // was absent from the original recipient snapshot.
    CM::ChangeWater(*CM::selected, L"Green"); CM::SelectionChanged(); CM::QueueMap(40, false);
    Check(CM::network.transfers.size() == 2, "new late join is not mistaken for an existing broadcast recipient");
    CM::network.transfers.clear(); CM::StopTransferTimer();
    // Exercise an actual Windows timer and a UI message while a map is queued.
    HWND window = CreateWindowW(L"STATIC", L"Responsive transfer fixture", WS_POPUP, 0, 0, 100, 100, nullptr, nullptr, nullptr, nullptr);
    SetWindowSubclass(window, HeartbeatProc, 1, 0);
    std::array<BYTE, 32> object{}; memcpy(object.data() + 28, &window, sizeof(window)); CM::WatchNetworkWindow(object.data());
    mapPackets.clear(); heartbeats = 0; CM::SelectionChanged(); PostMessageW(window, WM_APP + 1, 0, 0);
    const DWORD started = GetTickCount();
    while (mapPackets.empty() && GetTickCount() - started < 1000)
    {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
        Sleep(1);
    }
    Check(heartbeats == 1 && !mapPackets.empty() && !CM::network.transfers.empty(),
        "UI messages dispatch during a real timer-driven terrain transfer");
    CM::SendNetworkPacket(nullptr, 10, 0, true, go.data(), sizeof(go), CaptureMap);
    std::array<BYTE, 24> prematureReady{};
    memcpy(prematureReady.data(), &readyType, 4); memcpy(prematureReady.data() + 4, &peer, 4);
    memcpy(prematureReady.data() + 8, &CM::network.round, sizeof(CM::network.round));
    Check(!CM::AcceptReady(peer, prematureReady.data(), static_cast<uint32_t>(prematureReady.size())), "ready replies cannot advance a Go that is still queued");
    SendMessageW(window, WM_TIMER, 8, 0);
    Check(CM::network.failed && CM::network.transfers.empty() && CM::network.pendingGo.empty() && !CM::transferTimer,
        "native Go timeout cancels deferred Go and all scheduled chunks");
    CM::SelectionChanged();
    DestroyWindow(window);
    Check(!CM::transferTimer && CM::network.transfers.empty() && !CM::network.send, "lobby destruction cancels scheduled sends before transport objects disappear");
    CM::network.hosting = true; CM::network.source = 10; CM::network.send = CaptureMap;
    CM::mapSendAttempt = BusyMapAttempt; CM::SelectionChanged();
    CM::network.transfers.front().lastProgress = GetTickCount() - 15000;
    CM::network.pendingGo.assign(reinterpret_cast<BYTE*>(go.data()), reinterpret_cast<BYTE*>(go.data()) + sizeof(go));
    CM::network.goStarted = GetTickCount();
    mapPackets.clear(); CM::PumpTransfers();
    Check(CM::network.failed && mapPackets.empty() && CM::network.pendingGo.empty() && !CM::transferTimer,
        "stalled transport cancels pending Go without spinning or launching a mismatched game");
    CM::mapSendAttempt = CaptureMapAttempt; CM::SelectionChanged();
    CM::network.pendingGo.assign(reinterpret_cast<BYTE*>(go.data()), reinterpret_cast<BYTE*>(go.data()) + sizeof(go));
    CM::network.goStarted = GetTickCount() - 25000; CM::PumpTransfers();
    Check(!CM::network.transfers.empty() && !CM::network.failed && !CM::network.pendingGo.empty(),
        "an active map transfer does not spend the queued-Go startup deadline");
    CM::network.transfers.clear(); mapPackets.clear(); CM::PumpTransfers();
    Check(CM::network.transfers.empty() && mapPackets.empty() && CM::network.pendingGo.empty() && !CM::transferTimer,
        "overall queued-Go deadline prevents sending a start after native cancellation");
    CM::ResetNetwork(); CM::mapSendAttempt = CaptureMapAttempt; CM::selected.reset(); CM::enabled = false;
    const uint32_t none = 0; memcpy(CM::image + 0x1a7698 + 0x1090, &none, 4);
    std::filesystem::remove(directory / "land.dat"); std::filesystem::remove(directory);
    puts("PASS: paced sends, UI timer responsiveness, bounded backpressure, edit coalescing, cached Go, multi-peer/late-join ordering and cancellation");
}

static DWORD windowNow = 0, windowRtt = 0;
static DWORD WINAPI WindowClock() { return windowNow; }
struct WindowEvent { DWORD due; uint32_t peer; bool data; std::vector<BYTE> packet; };
static std::deque<WindowEvent> windowEvents;
static unsigned windowAttempts = 0, windowSends = 0, windowGo = 0, windowDrops = 0;
static uint32_t windowCap = 0, corruptOffset = ~0u, lostOffset = ~0u;
static bool corruptData = false, loseData = false, loseReply = false, corruptReply = false;
static bool dropEverything = false;
static std::array<CM::NetworkState, 2>* windowClients = nullptr;
static bool WindowMapSend(void*, uint32_t, uint32_t target, const void* packet, uint32_t length)
{
    ++windowAttempts; ++windowSends;
    CM::MapChunk header; memcpy(&header, packet, sizeof(header));
    if (dropEverything || (target == 30 && windowCap && length + 4 > windowCap)) { ++windowDrops; return true; }
    if (loseData && target == 30 && header.offset && header.offset != corruptOffset)
    { loseData = false; lostOffset = header.offset; ++windowDrops; return true; }
    std::vector<BYTE> bytes(static_cast<const BYTE*>(packet), static_cast<const BYTE*>(packet) + length);
    if (corruptData && target == 20 && header.offset)
    {
        corruptData = false; corruptOffset = header.offset;
        auto raw = CM::UnpackChunk(bytes.data() + sizeof(CM::CheckedMapChunk), length - sizeof(CM::CheckedMapChunk),
            header.size, static_cast<uint32_t>(CM::MaximumFileSize), CM::TcpWireSize);
        raw[0] ^= 1;
        const auto packed = CM::PackChunk(raw.data(), static_cast<uint32_t>(raw.size()));
        bytes.resize(sizeof(CM::CheckedMapChunk) + packed.size()); memcpy(bytes.data() + sizeof(CM::CheckedMapChunk), packed.data(), packed.size());
    }
    windowEvents.push_back({ windowNow + windowRtt / 2 + (header.offset / CM::TransferBlockSize % 3) * 20, target, true, std::move(bytes) });
    return true;
}
static bool WindowControlSend(void*, uint32_t source, uint32_t, const void* packet, uint32_t length, bool ready)
{
    const uint32_t type = CM::PacketType(packet, length);
    if (type == 27)
    {
        Check(!ready && CM::network.transfers.empty() && windowClients && (*windowClients)[0].remote && (*windowClients)[1].remote,
            "native Go remains withheld until both pipelined receivers have validated a complete map");
        ++windowGo; return true;
    }
    Check(ready && type == CM::MapAckPacketType, "window test carries map receipts on the native ready channel");
    CM::MapChunk ack; memcpy(&ack, packet, sizeof(ack));
    if (loseReply && source == 20 && ack.offset && ack.offset != corruptOffset && ack.offset != lostOffset)
    { loseReply = false; ++windowDrops; return true; }
    windowEvents.push_back({ windowNow + windowRtt / 2, source, false,
        std::vector<BYTE>(static_cast<const BYTE*>(packet), static_cast<const BYTE*>(packet) + length) });
    return true;
}
static void TerminalReceiptTests(const CM::Map& sample)
{
    const uint32_t peer = 20, hostId = 10;
    memcpy(CM::image + 0x1892ac, &peer, 4);
    CM::controlSendAttempt = CaptureControlAttempt;
    CM::MapTransfer transfer; transfer.map = std::make_shared<CM::Map>(sample);
    transfer.identity.revision = 1; transfer.identity.size = static_cast<uint32_t>(sample.bytes.size()); transfer.identity.crc = CM::Checksum(sample.bytes);
    std::vector<CM::MapFlight> packets;
    for (uint32_t offset = 0; offset < transfer.identity.size;)
    { auto packet = CM::PrepareTransferPacket(transfer, offset); offset += packet.header.size; packets.push_back(std::move(packet)); }
    Check(packets.size() > 2, "multi-packet terminal-confirmation fixture");
    for (bool wrongFileCrc : { false, true })
    {
        CM::ResetNetwork();
        auto receive = [&](size_t index)
        {
            auto packet = packets[index].packet;
            if (wrongFileCrc) packet[20] ^= 1;
            uint32_t length = static_cast<uint32_t>(packet.size()); CM::ReceiveNetworkPacket(hostId, hostId, packet.data(), length);
        };
        receive(0); CM::PumpTransfers();
        receive(packets.size() - 1);
        Check(CM::network.waitingFinal && CM::network.controls.empty() && !CM::network.complete.revision && !CM::network.remote,
            "an out-of-order terminal packet has no receipt before all holes and complete-file validation");
        for (size_t i = 1; i + 1 < packets.size(); ++i)
        { receive(i); if (i + 2 < packets.size()) CM::PumpTransfers(); }
        if (wrongFileCrc)
        {
            Check(CM::network.rejectedAssembly && !CM::network.remote && !CM::network.complete.revision && CM::network.controls.empty(),
                "valid individual checksums never release a terminal receipt for a bad complete file");
            receive(packets.size() - 1);
            Check(CM::network.controls.empty(), "a terminal duplicate cannot confirm a rejected pipelined assembly");
        }
        else
        {
            Check(!CM::network.waitingFinal && CM::network.remote && CM::network.remote->bytes == sample.bytes && CM::network.controls.size() == 2,
                "filling the final hole queues its receipt and the stored terminal receipt after byte-for-byte validation");
            CM::CheckedMapChunk terminal; memcpy(&terminal, CM::network.controls.back().packet.data(), sizeof(terminal));
            Check(terminal.header.offset == packets.back().header.offset && CM::SameIdentity(terminal.header.identity, transfer.identity),
                "deferred terminal receipt confirms the original revision and exact packet");
        }
    }
    CM::ResetNetwork();
    puts("PASS: out-of-order terminal receipt waits for complete-file CRC/parsing; rejected full files cannot confirm through duplicates");
}
static void WindowTransferTests(const CM::Map& sample)
{
    const uint32_t hostId = 10, firstPeer = 20, secondPeer = 30, none = 0;
    BYTE* transport = CM::image + 0x176a40;
    uint32_t savedMode = 0; memcpy(&savedMode, transport + 0x120d4, 4);
    memcpy(CM::image + 0x1a7698 + 0x108c, &firstPeer, 4); memcpy(CM::image + 0x1a7698 + 0x1090, &secondPeer, 4);
    CM::enabled = true; CM::selected.reset(new CM::Map(sample));
    CM::originalHostReceive = CM::originalHostRoundReceive = reinterpret_cast<CM::NativeReceive>(CaptureDispatch);
    TerminalReceiptTests(sample);
    CM::transferClock = WindowClock;
    CM::mapSendAttempt = WindowMapSend; CM::controlSendAttempt = WindowControlSend;
    for (uint32_t mode : { 2u, 1u }) for (auto dispatcher : { CM::HostReceive, CM::HostRoundReceive }) for (unsigned scenario : { 0u, 1u, 2u })
    {
        windowNow = 100000; windowRtt = mode == 1 ? 400 : 160;
        windowEvents.clear(); windowAttempts = windowSends = windowGo = windowDrops = 0;
        windowCap = scenario == 2 ? 1000 : 0; corruptOffset = lostOffset = ~0u;
        corruptData = loseData = loseReply = corruptReply = scenario == 1; dropEverything = false;
        memcpy(transport + 0x120d4, &mode, 4);
        CM::ResetNetwork(); CM::network.hosting = true; CM::network.source = hostId; CM::network.transport = transport; CM::network.send = CaptureMap;
        CM::SelectionChanged();
        const uint32_t initialLimit = mode == 1 ? CM::IpxWireSize : CM::TcpWireSize;
        Check(CM::network.transfers.front().wireLimit == initialLimit, "real transport mode selects the TCP/IP or IPX payload cap");
        const auto identity = CM::network.outgoing;
        const std::array<uint32_t, 2> go{ 27, 0 };
        CM::network.pendingGo.assign(reinterpret_cast<const BYTE*>(go.data()), reinterpret_cast<const BYTE*>(go.data()) + sizeof(go));
        const BYTE* trailer = reinterpret_cast<const BYTE*>(&identity);
        CM::network.pendingGo.insert(CM::network.pendingGo.end(), trailer, trailer + sizeof(identity));
        CM::network.goStarted = windowNow - 60000;
        std::array<CM::NetworkState, 2> clients; windowClients = &clients;
        unsigned peakPackets = 0, reduced = 0, increased = 0;
        uint32_t previousCap = initialLimit; bool finished = false;
        auto host = std::move(CM::network); CM::StopTransferTimer();
        const DWORD started = windowNow;
        for (unsigned tick = 0; tick < 15000; ++tick, windowNow += 20)
        {
            CM::network = std::move(host); windowAttempts = 0; CM::PumpTransfers(); CM::StopTransferTimer();
            Check(windowAttempts <= CM::TransferBurst && !CM::network.failed, "window sends are bounded and active transfer preserves queued Go");
            if (!CM::network.transfers.empty())
            {
                const auto& t = CM::network.transfers.front();
                peakPackets = (std::max)(peakPackets, static_cast<unsigned>(t.flights.size()));
                size_t bytes = 0; for (const auto& flight : t.flights) bytes += flight.packet.size();
                Check(t.flights.size() <= CM::TransferWindowPackets && bytes <= CM::TransferWindowBytes,
                    "in-flight packet count and byte budget remain bounded despite delay or loss");
                if (t.wireLimit < previousCap) ++reduced;
                if (t.wireLimit > previousCap) ++increased;
                previousCap = t.wireLimit;
            }
            host = std::move(CM::network);
            for (size_t i = 0; i < clients.size(); ++i)
            {
                const uint32_t peer = i ? secondPeer : firstPeer; memcpy(CM::image + 0x1892ac, &peer, 4);
                CM::network = std::move(clients[i]); if (!CM::network.controls.empty()) CM::PumpTransfers();
                CM::StopTransferTimer(); clients[i] = std::move(CM::network);
            }
            // Deliver due packets in reverse order to exercise reordering.
            for (size_t i = windowEvents.size(); i-- > 0;)
            {
                if (static_cast<int32_t>(windowNow - windowEvents[i].due) < 0) continue;
                auto event = std::move(windowEvents[i]); windowEvents.erase(windowEvents.begin() + i);
                if (event.data)
                {
                    const size_t client = event.peer == firstPeer ? 0 : 1;
                    memcpy(CM::image + 0x1892ac, &event.peer, 4); CM::network = std::move(clients[client]);
                    uint32_t length = static_cast<uint32_t>(event.packet.size()); CM::ReceiveNetworkPacket(hostId, hostId, event.packet.data(), length);
                    Check(!CM::network.remote || CM::network.remote->bytes == sample.bytes, "receiver publishes only the exact complete map");
                    CM::StopTransferTimer(); clients[client] = std::move(CM::network);
                }
                else
                {
                    CM::network = std::move(host);
                    const uint32_t length = static_cast<uint32_t>(event.packet.size()) + (mode == 1 ? 4 : 0);
                    if (corruptReply && !CM::network.transfers.empty())
                    {
                        corruptReply = false; auto bad = event.packet; bad.back() ^= 1;
                        const uint64_t before = CM::network.transfers.front().confirmedBytes;
                        dispatched = 0; dispatcher(nullptr, nullptr, event.peer, bad.data(), length);
                        dispatcher(nullptr, nullptr, 99, event.packet.data(), length);
                        Check(!dispatched && CM::network.transfers.front().confirmedBytes == before,
                            "wrong checksums and non-recipient replies cannot advance either native host dispatcher");
                    }
                    dispatcher(nullptr, nullptr, event.peer, event.packet.data(), length);
                    const uint64_t before = CM::network.transfers.empty() ? 0 : CM::network.transfers.front().confirmedBytes;
                    dispatcher(nullptr, nullptr, event.peer, event.packet.data(), length);
                    Check(CM::network.transfers.empty() || CM::network.transfers.front().confirmedBytes == before,
                        "duplicate and stale pipelined receipts never count bytes twice");
                    host = std::move(CM::network);
                }
            }
            if (windowGo == 2 && windowEvents.empty() && host.transfers.empty() && host.controls.empty() && host.pendingGo.empty())
            { finished = true; break; }
        }
        Check(finished && peakPackets > 1 && clients[0].remote && clients[1].remote && clients[0].remote->bytes == sample.bytes &&
            clients[1].remote->bytes == sample.bytes && CM::SameIdentity(host.broadcastSent, identity),
            "two pipelined receivers reconstruct the exact map and release Go on both transports and host dispatchers");
        if (scenario == 0)
        {
            const uint64_t stopAndWait = static_cast<uint64_t>(windowSends) * windowRtt;
            Check(windowNow - started < stopAndWait / 2, "delayed pipelined transfer is more than twice as fast as waiting for each reply");
            printf("PASS: simulated %s: %lu ms with window, %llu ms with per-packet replies; peak %u packets\n",
                mode == 1 ? "IPX" : "TCP/IP", static_cast<unsigned long>(windowNow - started), stopAndWait, peakPackets);
        }
        if (scenario == 1) Check(!corruptData && !loseData && !loseReply && !corruptReply && !reduced,
            "transient corrupt/lost packets and replies recover without permanently shrinking packet sizes");
        if (scenario == 2) Check(windowDrops && reduced && increased, "persistent oversize loss shrinks the window and stable delivery grows its packet limit again");
        windowClients = nullptr;
    }
    // A dead peer still times out; Send success cannot release Go.
    windowNow = 100000; windowEvents.clear(); dropEverything = true;
    CM::ResetNetwork(); CM::network.hosting = true; CM::network.source = hostId; CM::network.transport = transport; CM::network.send = CaptureMap;
    CM::SelectionChanged(); CM::network.pendingGo.assign(8, 0);
    for (unsigned tick = 0; tick < 1000 && !CM::network.failed; ++tick, windowNow += 20) CM::PumpTransfers();
    Check(CM::network.failed && CM::network.transfers.empty() && CM::network.pendingGo.empty() && !CM::transferTimer,
        "a window with no real receipts times out and clears queued Go without a frontend freeze");
    CM::transferClock = GetTickCount; CM::ResetNetwork(); CM::selected.reset(); CM::enabled = false;
    CM::mapSendAttempt = CaptureMapAttempt; CM::controlSendAttempt = CaptureControlAttempt; confirmMap = true; dropEverything = false;
    memcpy(transport + 0x120d4, &savedMode, 4); memcpy(CM::image + 0x1892ac, &none, 4); memcpy(CM::image + 0x1a7698 + 0x1090, &none, 4);
    puts("PASS: bounded window/bursts, multi-peer delayed/reordered/lost/corrupt data and receipts, adaptive recovery, both dispatchers/envelopes and queued-Go timeout");
}

static void HandshakeBackpressureTests(const CM::Map& sample)
{
    const auto directory = std::filesystem::absolute("Release/Data");
    Check(!std::filesystem::exists(directory), "isolated handshake fixture"); std::filesystem::create_directory(directory);
    const uint32_t peer = 20, secondPeer = 30, count = 2, none = 0;
    memcpy(CM::image + 0x1a7698 + 0x108c, &peer, 4); memcpy(CM::image + 0x1a7698 + 0x1090, &secondPeer, 4);
    memcpy(CM::image + 0x1a7698 + 0x10d4, &count, 4);
    CM::enabled = true; CM::selected.reset(new CM::Map(sample));
    CM::mapSendAttempt = CaptureMapAttempt; CM::controlSendAttempt = BusyControlAttempt;
    std::array<uint32_t, 2> go{ 27, 0 }, prepare{ 29, 0 }, ready{ 28, peer };
    CM::ResetNetwork(); nativeHandshakeCalls = 0; mapPackets.clear();
    CM::SendNetworkPacket(nullptr, 10, 0, true, go.data(), sizeof(go), NativeHandshake);
    while (!CM::network.transfers.empty()) CM::PumpTransfers();
    CM::PumpTransfers(); // Move the frozen Go into the control queue.
    Check(CM::QueuedGo() && nativeHandshakeCalls == 0, "Go never calls the native blocking send wrapper");
    const auto identity = CM::network.round;
    busyAttempts = 0; CM::PumpTransfers();
    Check(busyAttempts == 1 && CM::QueuedGo() && !CM::network.failed, "busy Go returns after one attempt and stays queued");
    CM::controlSendAttempt = CaptureControlAttempt; mapPackets.clear(); CM::PumpTransfers();
    std::array<BYTE, 24> taggedReady{};
    memcpy(taggedReady.data(), ready.data(), 8); memcpy(taggedReady.data() + 8, &identity, sizeof(identity));
    Check(CM::QueuedGo() && CM::AcceptReady(peer, taggedReady.data(), sizeof(taggedReady)),
        "a peer can acknowledge its sent Go while another peer remains queued");
    memcpy(taggedReady.data() + 4, &secondPeer, 4);
    Check(!CM::AcceptReady(secondPeer, taggedReady.data(), sizeof(taggedReady)), "a peer cannot acknowledge an unsent Go");
    DrainMapTransfers();
    Check(CM::AcceptReady(secondPeer, taggedReady.data(), sizeof(taggedReady)) && mapPackets.size() == 2,
        "each Go recipient advances only after its own successful send");
    for (const auto& packet : { std::vector<BYTE>(reinterpret_cast<BYTE*>(prepare.data()), reinterpret_cast<BYTE*>(prepare.data()) + sizeof(prepare)),
        std::vector<BYTE>(20, 0) })
    {
        auto control = packet; const uint32_t type = control.size() == 20 ? 14 : 29; memcpy(control.data(), &type, 4);
        CM::controlSendAttempt = BusyControlAttempt;
        CM::SendNetworkPacket(nullptr, 10, 0, true, control.data(), static_cast<uint32_t>(control.size()), NativeHandshake);
        busyAttempts = 0; CM::PumpTransfers();
        Check(nativeHandshakeCalls == 0 && busyAttempts == 1 && CM::network.controls.size() == 1,
            "generation and start packets never enter the native retry loop");
        CM::controlSendAttempt = CaptureControlAttempt; DrainMapTransfers();
    }
    CM::ResetNetwork(); CM::network.joining = true; CM::network.host = 10;
    CM::network.round = identity; CM::network.roundMap.reset(new CM::Map(sample));
    CM::originalSendReady = reinterpret_cast<CM::NativeSendPlayer>(NativeReadyHandshake);
    for (uint32_t type : { 28u, 30u })
    {
        ready[0] = type; CM::controlSendAttempt = BusyControlAttempt; readyPacket.clear();
        CM::SendReady(nullptr, nullptr, peer, 10, ready.data(), sizeof(ready));
        busyAttempts = 0; CM::PumpTransfers();
        Check(nativeHandshakeCalls == 0 && busyAttempts == 1 && CM::network.controls.front().readyChannel,
            "client ready packets use their own channel without the native blocking wrapper");
        CM::controlSendAttempt = CaptureControlAttempt; DrainMapTransfers();
        Check(readyPacket.size() == 24 && CM::PacketType(readyPacket.data(), 24) == type,
            "queued client ready preserves its native type and map identity");
    }
    CM::controlSendAttempt = BusyControlAttempt;
    CM::SendReady(nullptr, nullptr, peer, 10, ready.data(), sizeof(ready));
    CM::network.controls.front().lastProgress = GetTickCount() - 15000; CM::PumpTransfers();
    Check(CM::network.failed && CM::network.controls.empty() && !CM::transferTimer && nativeHandshakeCalls == 0,
        "permanently busy readiness cancels without spinning or stale sends");
    CM::ResetNetwork(); CM::controlSendAttempt = CaptureControlAttempt; CM::selected.reset(); CM::enabled = false;
    memcpy(CM::image + 0x1a7698 + 0x1090, &none, 4);
    std::filesystem::remove(directory / "land.dat"); std::filesystem::remove(directory);
    puts("PASS: busy Go/generate/start/ready sends stay bounded, use the correct channels and acknowledge only sent recipients");
}

static std::vector<BYTE> directPacket;
static uint32_t directSource = 0, directTarget = 0, directFlags = 0;
static HRESULT __stdcall CaptureDirectPlay(void*, uint32_t source, uint32_t target, uint32_t flags, const void* packet, uint32_t length)
{
    directSource = source; directTarget = target; directFlags = flags;
    directPacket.assign(static_cast<const BYTE*>(packet), static_cast<const BYTE*>(packet) + length); return S_OK;
}
static HRESULT __fastcall BusyDirectPlay(void*, void*, uint32_t, uint32_t, uint32_t, const void*, uint32_t)
{ ++busyAttempts; return static_cast<HRESULT>(0x8877010e); }
static unsigned directReceiveCalls = 0;
static HRESULT directReceiveResult = S_OK;
static HRESULT __stdcall CaptureDirectReceive(void*, uint32_t* source, uint32_t* target, uint32_t flags, void* packet, uint32_t* length)
{
    ++directReceiveCalls;
    Check(flags == 1 && *length >= directPacket.size(), "native Receive preserves flags and buffer capacity");
    if (directReceiveResult != S_OK) return directReceiveResult;
    *source = 10; *target = 20; *length = static_cast<uint32_t>(directPacket.size());
    memcpy(packet, directPacket.data(), directPacket.size()); return S_OK;
}
static void DirectSendTests(BYTE* frontend)
{
    std::vector<BYTE> transport(0x13300);
    std::array<void*, 27> vtable{}; vtable[26] = reinterpret_cast<void*>(CaptureDirectPlay);
    void** com = vtable.data(); const void* comObject = &com;
    CM::MapChunk header;
    std::vector<BYTE> literal(CM::ChunkSize * 2); for (size_t i = 0; i < literal.size(); ++i) literal[i] = static_cast<BYTE>(i);
    const auto packed = CM::PackChunk(literal.data(), static_cast<uint32_t>(literal.size()));
    CM::CheckedMapChunk checked; checked.header.version = 7; checked.header.size = static_cast<uint32_t>(literal.size()); checked.crc = CM::Checksum(literal);
    std::vector<BYTE> maximum(sizeof(checked) + packed.size()); memcpy(maximum.data(), &checked, sizeof(checked));
    memcpy(maximum.data() + sizeof(checked), packed.data(), packed.size());
    for (bool ready : { false, true })
    {
    BYTE* channel = transport.data() + (ready ? 0x128c8 : 0x12d50);
    const uint32_t active = 1; memcpy(channel + 0x39c, &active, 4); memcpy(channel + 0x3a0, &comObject, 4);
    for (uint32_t reliable : { 0u, 1u })
    {
        memcpy(transport.data() + 0x120d4, &reliable, 4);
        const uint32_t sequence = 17; memcpy(channel + 0x438, &sequence, 4);
        CM::directSend = reinterpret_cast<CM::NativeDirectSend>(frontend + 0x1058b);
        Check(CM::TryPacketSend(transport.data(), 10, 20, &header, sizeof(header), ready), "actual native DirectPlay Send accepts a single queued packet on either channel");
        Check(directSource == 10 && directTarget == 20 && directFlags == 1 && directPacket.size() == sizeof(header) + reliable * 4 &&
            memcmp(directPacket.data() + reliable * 4, &header, sizeof(header)) == 0,
            "actual native Send preserves transport IDs, guaranteed flag and reliable envelope");
        Check(*reinterpret_cast<uint32_t*>(channel + 0x438) == sequence + reliable, "native reliable sequence advances exactly once after successful enqueue");
        if (reliable) Check(*reinterpret_cast<uint32_t*>(directPacket.data()) == sequence, "reliable prefix matches the native receiver's sequence");
        CM::directSend = reinterpret_cast<CM::NativeDirectSend>(BusyDirectPlay); busyAttempts = 0;
        Check(!CM::TryPacketSend(transport.data(), 10, 20, &header, sizeof(header), ready) && busyAttempts == 1 &&
            *reinterpret_cast<uint32_t*>(channel + 0x438) == sequence + reliable,
            "transport busy returns immediately without retrying or consuming another sequence");
        CM::directSend = reinterpret_cast<CM::NativeDirectSend>(frontend + 0x1058b);
        Check(CM::TryPacketSend(transport.data(), 10, 20, maximum.data(), static_cast<uint32_t>(maximum.size()), ready) &&
            directPacket.size() == maximum.size() + reliable * 4 && !memcmp(directPacket.data() + reliable * 4, maximum.data(), maximum.size()),
            "actual native Send accepts the complete worst-case checked chunk on both channels and envelopes");
    }
    }
    CM::directSend = reinterpret_cast<CM::NativeDirectSend>(frontend + 0x1058b);
    puts("PASS: actual native DirectPlay Send entry, both envelopes, shared sequence continuity and one-attempt busy handling");
}

static std::string ReadLog(const std::filesystem::path& path)
{
    FILE* file = nullptr; Check(_wfopen_s(&file, path.c_str(), L"rb") == 0 && file, "read isolated diagnostic log");
    Check(fseek(file, 0, SEEK_END) == 0, "seek diagnostic log");
    const long size = ftell(file); Check(size >= 0, "diagnostic log size"); rewind(file);
    std::string text(static_cast<size_t>(size), '\0');
    Check(fread(&text[0], 1, text.size(), file) == text.size(), "complete diagnostic log"); fclose(file); return text;
}
static void ReceiveDiagnosticsTests(BYTE* frontend)
{
    const auto directory = std::filesystem::absolute("Release/MapDiagnosticsFixture");
    Check(!std::filesystem::exists(directory), "isolated diagnostic fixture"); std::filesystem::create_directory(directory);
    const auto gamePath = directory / "Data.log", mirrorPath = directory / "TEMP.log";
    CM::traceGamePath = gamePath.wstring(); CM::traceMirrorPath = mirrorPath.wstring(); CM::traceEnabled = true;
    CM::BeginDiagnostics(GetModuleHandleW(nullptr));
    Check(ReadLog(gamePath) == ReadLog(mirrorPath) && ReadLog(gamePath).find("map-network diagnostics 13 protocol 9") != std::string::npos &&
        ReadLog(gamePath).find("CRC32 self-test cbf43926 cbf43926") != std::string::npos &&
        ReadLog(gamePath).find("frontend path") != std::string::npos && ReadLog(gamePath).find("pid=") != std::string::npos,
        "session marker, CRC32 self-test, build, executable path, UTC and PID reach both logs");
    SetFileAttributesW(gamePath.c_str(), FILE_ATTRIBUTE_READONLY);
    CM::Trace("read-only logging fixture", 1, 2);
    Check(ReadLog(mirrorPath).find("Data log append failed error=") != std::string::npos &&
        ReadLog(mirrorPath).find("read-only logging fixture") != std::string::npos,
        "TEMP mirror survives a read-only Data log and records the write failure");
    SetFileAttributesW(gamePath.c_str(), FILE_ATTRIBUTE_NORMAL);
    CM::Trace("logging restored"); Check(CM::traceGameError == ERROR_SUCCESS, "logging recovers after Data is unlocked");
    std::vector<BYTE> channel(0x500);
    std::array<void*, 27> vtable{}; vtable[25] = reinterpret_cast<void*>(CaptureDirectReceive);
    void** com = vtable.data(); const void* comObject = &com;
    const uint32_t active = 1; memcpy(channel.data() + 0x39c, &active, 4); memcpy(channel.data() + 0x3a0, &comObject, 4);
    auto receive = reinterpret_cast<CM::NativeDirectReceive>(frontend + 0x105f7);
    auto filter = reinterpret_cast<CM::NativeSequenceFilter>(frontend + 0x1378d);
    CM::MapChunk header; header.version = 5; header.identity.revision = 2; header.offset = 0x80000; header.size = CM::ChunkSize;
    for (bool reliable : { false, true })
    {
        directPacket.assign(sizeof(header) + (reliable ? 4 : 0) + 2, 0);
        const uint32_t sequence = 17; if (reliable) memcpy(directPacket.data(), &sequence, 4);
        memcpy(directPacket.data() + (reliable ? 4 : 0), &header, sizeof(header));
        std::array<BYTE, 128> buffer{}; uint32_t length = static_cast<uint32_t>(buffer.size()), source = 0, target = 0;
        directReceiveCalls = 0; directReceiveResult = S_OK;
        Check(receive(channel.data(), &source, &target, 1, buffer.data(), &length) == S_OK && directReceiveCalls == 1 &&
            source == 10 && target == 20 && length == directPacket.size() && !memcmp(buffer.data(), directPacket.data(), length),
            "actual Receive detour preserves HRESULT, pointers, lengths and both envelopes without consuming packets");
        const auto log = ReadLog(mirrorPath);
        Check(log.find("map wire received") != std::string::npos && log.find("offset=00080000") != std::string::npos &&
            log.find(reliable ? "seq=00000011" : "seq=00000000") != std::string::npos,
            "wire reception is logged before the native sequence filter for raw and reliable maps");
        directReceiveResult = static_cast<HRESULT>(0x887700be); directReceiveCalls = 0; length = static_cast<uint32_t>(buffer.size());
        Check(receive(channel.data(), &source, &target, 1, buffer.data(), &length) == directReceiveResult && directReceiveCalls == 1 &&
            length == buffer.size() && ReadLog(mirrorPath) == log, "idle Receive returns once and adds no diagnostic noise");
    }
    const uint32_t sender = 10, sequence = 17;
    memcpy(channel.data() + 0x3c8, &sender, 4); memcpy(channel.data() + 0x43c, &sequence, 4);
    Check(filter(channel.data(), sender, sequence) == 1 && filter(channel.data(), sender, sequence - 1) == 1 &&
        *reinterpret_cast<uint32_t*>(channel.data() + 0x43c) == sequence,
        "diagnostic detour preserves native duplicate/out-of-order drops and bookkeeping");
    Check(filter(channel.data(), sender, sequence + 1) == 0 && *reinterpret_cast<uint32_t*>(channel.data() + 0x43c) == sequence + 1,
        "diagnostic detour preserves acceptance and advancement of a new native sequence");
    memcpy(frontend + 0x1563c0, &sequence, 4); memcpy(frontend + 0x1563c4, &header, sizeof(header));
    CM::TraceSequenceFilter(sender, sequence, 1, frontend + 0x12cbd);
    Check(ReadLog(mirrorPath).find("native map sequence dropped") != std::string::npos, "custom packet dropped before dispatch has an explicit diagnostic");
    const auto log = ReadLog(mirrorPath);
    CM::TraceSequenceFilter(sender, sequence, 0, frontend + 0x12cbd);
    CM::TraceSequenceFilter(sender, sequence, 1, frontend + 0x12cbc);
    CM::TraceSequenceFilter(sender, sequence - 1, 1, frontend + 0x12cbd);
    Check(ReadLog(mirrorPath) == log, "accepted sequences and unrelated callers do not produce false discard diagnostics");
    CM::traceEnabled = false; CM::traceGamePath.clear(); CM::traceMirrorPath.clear(); CM::traceGameError = ERROR_SUCCESS;
    std::filesystem::remove(gamePath); std::filesystem::remove(mirrorPath); std::filesystem::remove(directory);
    puts("PASS: actual native receive/filter detours preserve packets and sequencing; session logs survive a read-only Data file");
}

static int applicationCalls = 0;
static uint32_t applicationFlags = ~0u;
static uint32_t* applicationId = nullptr;
static const void* applicationConnection = nullptr;
static HANDLE applicationEvent = nullptr;
static bool applicationQueueEmpty = false;
static int spawnCalls = 0;
static intptr_t __cdecl CaptureSpawn(int mode, const char* path, const char* const* arguments, const char* const* environment)
{
    ++spawnCalls;
    Check(mode == 0 && !strcmp(path, "WORMS2.EXE") && arguments && !strcmp(arguments[0], "worms2.exe") &&
        arguments[1] == nullptr && environment == nullptr, "native IPX execl forwards mode, path, argument list and environment");
    applicationQueueEmpty = CM::network.controls.empty() && CM::network.transfers.empty(); return 42;
}
static HRESULT __stdcall CaptureApplication(void*, uint32_t flags, uint32_t* id, const void* connection, HANDLE event)
{
    ++applicationCalls; applicationFlags = flags; applicationId = id; applicationConnection = connection; applicationEvent = event;
    applicationQueueEmpty = CM::network.controls.empty() && CM::network.transfers.empty(); return S_OK;
}
static void EngineBarrierTests(BYTE* frontend, const CM::Map& map)
{
    const auto directory = std::filesystem::absolute("Release/Data");
    Check(!std::filesystem::exists(directory), "isolated engine barrier fixture"); std::filesystem::create_directory(directory);
    BYTE* players = frontend + 0x1a7698 + 0x108c;
    std::array<uint32_t, 14> savedPlayers{}; memcpy(savedPlayers.data(), players, sizeof(savedPlayers));
    DWORD oldProtection = 0;
    Check(VirtualProtect(players, sizeof(savedPlayers), PAGE_READWRITE, &oldProtection) != FALSE, "protect private native recipient list");
    memset(players, 0, sizeof(savedPlayers)); const uint32_t peer = 20; memcpy(players, &peer, 4);
    std::array<BYTE, 0x80> object{};
    std::array<void*, 11> vtable{}; vtable[10] = reinterpret_cast<void*>(CaptureApplication);
    void** com = vtable.data(); const void* comObject = &com; memcpy(object.data() + 0x78, &comObject, 4);
    uint32_t id = 0, connection = 17; const HANDLE event = reinterpret_cast<HANDLE>(0x1234);
    auto run = reinterpret_cast<CM::NativeRunApplication>(frontend + 0x144ed);
    const auto setup = [&] {
        CM::ResetNetwork(); CM::network.hosting = true; CM::network.source = 10;
        CM::network.round.revision = 1; CM::network.round.size = static_cast<uint32_t>(map.bytes.size());
        CM::network.round.crc = CM::Checksum(map.bytes); CM::network.roundMap.reset(new CM::Map(map));
        CM::controlSendAttempt = CaptureControlAttempt; mapPackets.clear();
    };
    std::array<BYTE, 20> start{}; const uint32_t type = 14; memcpy(start.data(), &type, 4);
    setup(); applicationCalls = 0;
    CM::SendNetworkPacket(nullptr, 10, 0, true, start.data(), sizeof(start), NativeHandshake);
    Check(!CM::network.controls.empty(), "native start packet waits in the control queue before engine setup");
    Check(run(object.data(), &id, &connection, event) == S_OK && applicationCalls == 1 && applicationQueueEmpty && mapPackets.size() == 1,
        "actual hooked native RunApplication submits start before calling the engine COM entry");
    Check(applicationFlags == 0 && applicationId == &id && applicationConnection == &connection && applicationEvent == event &&
        CM::Load((directory / "land.dat").c_str()).bytes == map.bytes, "engine arguments and frozen terrain survive the native trampoline");
    setup(); CM::SendNetworkPacket(nullptr, 10, 0, true, start.data(), sizeof(start), NativeHandshake);
    CM::controlSendAttempt = BusyControlAttempt; busyAttempts = 0;
    const DWORD began = GetTickCount();
    Check(run(object.data(), &id, &connection, event) == E_ABORT && applicationCalls == 1 && CM::network.failed &&
        CM::network.controls.empty() && !CM::transferTimer && GetTickCount() - began < 2000 && busyAttempts <= 60,
        "permanently busy start returns failure within one second before the native infinite engine wait");
    setup(); SetFileAttributesW((directory / "land.dat").c_str(), FILE_ATTRIBUTE_READONLY);
    Check(run(object.data(), &id, &connection, event) == E_ABORT && applicationCalls == 1,
        "failed final terrain publication blocks actual native engine launch");
    SetFileAttributesW((directory / "land.dat").c_str(), FILE_ATTRIBUTE_NORMAL);
    setup(); CM::network.pendingGo.assign(8, 0);
    Check(run(object.data(), &id, &connection, event) == E_ABORT && applicationCalls == 1,
        "engine cannot launch with an outstanding map or Go barrier");
    CM::ResetNetwork();
    Check(run(object.data(), &id, &connection, event) == S_OK && applicationCalls == 2,
        "generated and local games retain the original native RunApplication behavior");
    const auto savedSpawn = CM::originalSpawn;
    CM::originalSpawn = CaptureSpawn;
    auto execl = reinterpret_cast<intptr_t(__cdecl*)(int, const char*, const char*, ...)>(frontend + 0x97d50);
    setup(); spawnCalls = 0;
    CM::SendNetworkPacket(nullptr, 10, 0, true, start.data(), sizeof(start), NativeHandshake);
    Check(execl(0, "WORMS2.EXE", "worms2.exe", nullptr) == 42 && spawnCalls == 1 && applicationQueueEmpty && mapPackets.size() == 1,
        "actual native IPX execl submits queued start before invoking the spawn entry");
    setup(); CM::SendNetworkPacket(nullptr, 10, 0, true, start.data(), sizeof(start), NativeHandshake);
    CM::controlSendAttempt = BusyControlAttempt;
    Check(execl(0, "WORMS2.EXE", "worms2.exe", nullptr) == -1 && spawnCalls == 1 && CM::network.failed && !CM::transferTimer,
        "a permanently busy start prevents IPX engine spawn too");
    CM::ResetNetwork(); CM::controlSendAttempt = CaptureControlAttempt;
    Check(execl(0, "WORMS2.EXE", "worms2.exe", nullptr) == 42 && spawnCalls == 2,
        "native IPX launch still passes through when no imported round is active");
    CM::originalSpawn = savedSpawn;
    memcpy(players, savedPlayers.data(), sizeof(savedPlayers)); VirtualProtect(players, sizeof(savedPlayers), oldProtection, &oldProtection);
    std::filesystem::remove(directory / "land.dat"); std::filesystem::remove(directory);
    puts("PASS: actual native engine COM entry, start ordering, bounded busy cancellation, publication failure and unchanged native launch");
}

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
    CM::mapSendAttempt = CaptureMapAttempt;
    const uint32_t onlinePlayer = 20, onlineCount = 1;
    memcpy(CM::image + 0x1a7698 + 0x108c, &onlinePlayer, 4);
    memcpy(CM::image + 0x1a7698 + 0x10d4, &onlineCount, 4);
    mapPackets.clear();
    CM::SendNetworkPacket(nullptr, 10, 0, true, go.data(), sizeof(go), CaptureMap);
    Check(mapPackets.empty() && !CM::network.transfers.empty(), "Go returns without synchronously sending terrain");
    DrainMapTransfers();
    Check(mapPackets.size() > 2 && mapPackets.back().size() == 24, "Go transfers complete map before tagged native handshake");
    const auto firstPackets = mapPackets;
    for (size_t i = 0; i + 1 < firstPackets.size(); ++i)
        Check(firstPackets[i].size() <= CM::TransferWireSize + sizeof(CM::CheckedMapChunk) &&
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
    DrainMapTransfers();
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
    DrainMapTransfers();
    const auto preparePacket = mapPackets.back();
    CM::SendNetworkPacket(nullptr, 10, 0, true, start.data(), static_cast<uint32_t>(start.size()), CaptureMap);
    DrainMapTransfers();
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
    DrainMapTransfers();
    Check(CM::network.round.revision != identity.revision && CM::NetworkMap()->bytes == CM::selected->bytes,
        "next round transfers new selection with a fresh identity");
    // Native host snapshot is sent before its map, so a late join knows the sender.
    std::vector<BYTE> snapshot(0x10de); const uint32_t snapshotType = 5; memcpy(snapshot.data(), &snapshotType, 4);
    mapPackets.clear(); CM::SendNetworkPacket(nullptr, 10, 20, false, snapshot.data(), static_cast<uint32_t>(snapshot.size()), CaptureMap);
    Check(mapPackets.size() == 1 && !CM::network.transfers.empty(), "late-join snapshot returns before map transfer");
    DrainMapTransfers();
    Check(mapPackets.front() == snapshot && mapPackets.size() > 2, "late join receives native snapshot followed by selected map");
    CM::selected.reset(); mapPackets.clear(); CM::SelectionChanged();
    DrainMapTransfers();
    Check(mapPackets.size() == 1 && mapPackets[0].size() == sizeof(CM::CheckedMapChunk), "generated-map reset broadcast uses a bounded checked empty transfer");
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
            DrainMapTransfers();
            SW::SendWeaponPacketToAll(nullptr, nullptr, 10, start.data(), sizeof(start));
            DrainMapTransfers();
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

static int nativeGoCalls = 0;
static const CM::Map* expectedGoMap = nullptr;
static void __fastcall NativeGoFixture(void* object, void*)
{
    ++nativeGoCalls;
    HWND window = *reinterpret_cast<HWND*>(static_cast<BYTE*>(object) + 28);
    EnableWindow(window, FALSE); SetTimer(window, 8, 30000, nullptr);
    if (expectedGoMap) Check(CM::network.transfers.empty() && CM::NetworkMap() && CM::NetworkMap()->bytes == expectedGoMap->bytes,
        "native Go disables the UI only after the frozen terrain is confirmed");
    const std::array<uint32_t, 2> go{ 27, 0 };
    CM::SendNetworkPacket(CM::image + 0x176a40, 10, 0, true, go.data(), sizeof(go), CaptureMap);
}
static void HostGoPreflightTests(BYTE* frontend, const CM::Map& map)
{
    const auto directory = std::filesystem::absolute("Release/Data");
    Check(!std::filesystem::exists(directory), "isolated Go preflight fixture"); std::filesystem::create_directory(directory);
    const auto savedHostGo = CM::originalHostGo, savedRoundGo = CM::originalHostRoundGo;
    CM::originalHostGo = CM::originalHostRoundGo = reinterpret_cast<CM::NativeVoid>(NativeGoFixture);
    const uint32_t host = 10, peer = 20, count = 1, none = 0;
    memcpy(frontend + 0x1892ac, &host, 4); memcpy(frontend + 0x1a7698 + 0x108c, &peer, 4);
    memcpy(frontend + 0x1a7698 + 0x10d4, &count, 4);
    CM::mapSendAttempt = CaptureMapAttempt; CM::controlSendAttempt = CaptureControlAttempt;
    for (size_t entry : { 0x336f1u, 0x62214u })
    {
        HWND window = CreateWindowW(L"STATIC", L"Go preflight fixture", WS_POPUP, 0, 0, 100, 100, nullptr, nullptr, nullptr, nullptr);
        SetWindowSubclass(window, HeartbeatProc, 1, 0);
        std::array<BYTE, 32> object{}; memcpy(object.data() + 28, &window, sizeof(window));
        CM::ResetNetwork(); CM::network.hosting = true; CM::network.send = CaptureMap;
        memcpy(frontend + 0x1a7698 + 0x1090, &none, 4);
        memcpy(frontend + 0x1a7698 + 0x1094, &none, 4);
        CM::selected.reset(new CM::Map(map)); expectedGoMap = &map;
        nativeGoCalls = heartbeats = 0; mapPackets.clear(); confirmMap = false;
        auto go = reinterpret_cast<CM::NativeVoid>(frontend + entry);
        go(object.data()); go(object.data());
        Check(CM::network.deferredGo && nativeGoCalls == 0 && IsWindowEnabled(window) && mapPackets.empty(),
            "actual first/next-round Go detours return without disabling the frontend or entering native startup");
        CM::selected.reset(); go(object.data()); CM::selected.reset(new CM::Map(map));
        Check(nativeGoCalls == 0 && CM::network.deferredGo, "a later reset cannot trigger a second native Go while frozen Go is pending");
        PostMessageW(window, WM_APP + 1, 0, 0); MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
        Check(heartbeats == 1 && IsWindowEnabled(window), "frontend stays enabled and dispatches UI messages during Go preflight");
        CM::network.goStarted = GetTickCount() - 30000; CM::PumpTransfers();
        Check(!CM::network.failed && CM::network.deferredGo && nativeGoCalls == 0,
            "a slow map transfer does not spend the native 30-second Go timeout");
        CM::ChangeWater(*CM::selected, L"Purple"); CM::SelectionChanged();
        Check(CM::network.startMap->bytes == map.bytes, "Go preflight retains the selected bytes when later edits occur");
        const uint32_t latePeer = 30, snapshotType = 5;
        memcpy(frontend + 0x1a7698 + 0x1090, &latePeer, 4);
        std::vector<BYTE> snapshot(0x10de); memcpy(snapshot.data(), &snapshotType, 4);
        CM::SendNetworkPacket(frontend + 0x176a40, host, latePeer, false, snapshot.data(), static_cast<uint32_t>(snapshot.size()), CaptureMap);
        Check(CM::network.transfers.size() == 2 && CM::network.transfers.back().map->bytes == map.bytes,
            "a late join during Go preflight receives the frozen map despite later selection edits");
        confirmMap = true; for (auto& flight : CM::network.transfers.front().flights) for (auto& delivery : flight.deliveries) delivery.sentAt = GetTickCount() - delivery.retryDelay;
        DrainMapTransfers();
        Check(nativeGoCalls == 1 && !IsWindowEnabled(window) && CM::NetworkMap()->bytes == map.bytes &&
            CM::PacketType(mapPackets.back().data(), static_cast<uint32_t>(mapPackets.back().size())) == 27,
            "confirmed Go enters native startup exactly once with the frozen terrain and no further bulk transfer");
        // An unchanged next Go confirms its cache reference before disabling UI.
        EnableWindow(window, TRUE); KillTimer(window, 8); CM::selected.reset(new CM::Map(map));
        confirmMap = false; go(object.data());
        Check(IsWindowEnabled(window) && nativeGoCalls == 1 && CM::network.deferredGo && CM::network.transfers.front().reuse,
            "cache-reference confirmation also precedes native Go");
        const uint32_t cachedLatePeer = 40; memcpy(frontend + 0x1a7698 + 0x1094, &cachedLatePeer, 4);
        CM::SendNetworkPacket(frontend + 0x176a40, host, cachedLatePeer, false, snapshot.data(), static_cast<uint32_t>(snapshot.size()), CaptureMap);
        Check(CM::network.transfers.size() == 3 && !CM::network.transfers[1].reuse && CM::network.transfers.back().reuse &&
            CM::SameIdentity(CM::network.transfers.back().identity, CM::network.startIdentity),
            "late join during cached Go receives full terrain followed by the frozen round reference");
        confirmMap = true; DrainMapTransfers();
        Check(nativeGoCalls == 2 && CM::NetworkMap()->bytes == map.bytes, "cached Go preserves frozen terrain and enters startup once");
        EnableWindow(window, TRUE); KillTimer(window, 8); CM::ResetNetwork(); CM::network.hosting = true;
        confirmMap = false; nativeGoCalls = 0; go(object.data());
        CM::network.transfers.front().lastProgress = GetTickCount() - 15000; CM::PumpTransfers();
        Check(CM::network.failed && !CM::network.deferredGo && IsWindowEnabled(window) && nativeGoCalls == 0 && !CM::transferTimer,
            "failed map confirmation leaves the frontend enabled and never enters native timeout/generation");
        const auto rejectedIdentity = CM::network.startIdentity;
        go(object.data()); Check(CM::network.deferredGo && !CM::SameIdentity(rejectedIdentity, CM::network.startIdentity),
            "Go can be retried after failed preflight with a fresh revision");
        CM::PumpTransfers();
        CM::CheckedMapChunk oldReply; oldReply.header.type = CM::MapAckPacketType; oldReply.header.version = 6;
        oldReply.header.identity = rejectedIdentity; oldReply.header.size = CM::ChunkSize;
        oldReply.crc = CM::Checksum(map.bytes.data(), CM::ChunkSize);
        Check(!CM::ReceiveMapAck(peer, &oldReply, sizeof(oldReply)) && !CM::network.transfers.front().confirmedBytes,
            "a delayed reply from the failed Go cannot confirm the restarted round");
        DestroyWindow(window);
        Check(!CM::network.deferredGo && !CM::transferTimer && nativeGoCalls == 0, "closing the lobby cancels deferred native Go before destroying its object");
    }
    expectedGoMap = nullptr; confirmMap = true; CM::ResetNetwork(); CM::selected.reset();
    CM::originalHostGo = savedHostGo; CM::originalHostRoundGo = savedRoundGo;
    memcpy(frontend + 0x1a7698 + 0x1090, &none, 4);
    memcpy(frontend + 0x1a7698 + 0x1094, &none, 4);
    std::filesystem::remove(directory / "land.dat"); std::filesystem::remove(directory);
    puts("PASS: actual first/next-round Go preflight hooks preserve responsive UI, frozen terrain, cached start, timeout recovery and cancellation");
}

static unsigned WireCodecTests(const CM::Map& map)
{
    CM::ResetNetwork();
    CM::MapIdentity identity; identity.revision = 1; identity.size = static_cast<uint32_t>(map.bytes.size()); identity.crc = CM::Checksum(map.bytes);
    for (size_t offset = 0; offset < map.bytes.size(); offset += CM::ChunkSize)
    {
        const uint32_t size = static_cast<uint32_t>((std::min)(size_t(CM::ChunkSize), map.bytes.size() - offset));
        const auto packed = CM::PackChunk(map.bytes.data() + offset, size);
        const auto unpacked = CM::UnpackChunk(packed.data(), static_cast<uint32_t>(packed.size()), size);
        Check(packed.size() <= CM::WireChunkSize && !memcmp(unpacked.data(), map.bytes.data() + offset, size), "network compression preserves every terrain byte");
        CM::CheckedMapChunk checked; checked.header.version = 6; checked.header.identity = identity;
        checked.header.offset = static_cast<uint32_t>(offset); checked.header.size = size;
        checked.crc = CM::Checksum(map.bytes.data() + offset, size);
        std::vector<BYTE> packet(sizeof(checked) + packed.size()); memcpy(packet.data(), &checked, sizeof(checked));
        memcpy(packet.data() + sizeof(checked), packed.data(), packed.size());
        Check(CM::ReceiveMapChunk(packet.data(), static_cast<uint32_t>(packet.size())), "every supplied map chunk passes checked network assembly");
    }
    Check(CM::network.remote && CM::network.remote->bytes == map.bytes && CM::SameIdentity(CM::network.complete, identity),
        "every supplied map reconstructs and validates byte-for-byte through the complete network decoder");
    unsigned tcpPackets = 0;
    for (uint32_t limit : { CM::TcpWireSize, CM::IpxWireSize })
    {
        CM::ResetNetwork();
        CM::MapTransfer transfer; transfer.identity = identity; transfer.map = std::make_shared<CM::Map>(map); transfer.wireLimit = limit;
        unsigned packets = 0;
        while (transfer.offset < identity.size)
        {
            const auto flight = CM::PrepareTransferPacket(transfer, transfer.offset);
            Check(flight.header.size && flight.packet.size() <= sizeof(CM::CheckedMapChunk) + limit,
                "TCP/IP and IPX packets make bounded progress within their transport limits");
            Check(CM::ReceiveMapChunk(flight.packet.data(), static_cast<uint32_t>(flight.packet.size())),
                "every supplied-map packet passes checksums and bounded assembly on both transports");
            transfer.offset += flight.header.size; ++packets;
        }
        Check(CM::network.remote && CM::network.remote->bytes == map.bytes && CM::SameIdentity(CM::network.complete, identity),
            "every supplied map reconstructs byte-for-byte with TCP/IP and IPX packet limits");
        if (limit == CM::TcpWireSize) tcpPackets = packets;
    }
    CM::ResetNetwork(); return tcpPackets;
}
static void WireCodecBoundsTests()
{
    std::vector<BYTE> literal(CM::ChunkSize);
    for (size_t i = 0; i < literal.size(); ++i) literal[i] = static_cast<BYTE>(i);
    const auto packed = CM::PackChunk(literal.data(), CM::ChunkSize);
    Check(packed.size() == CM::WireChunkSize && CM::UnpackChunk(packed.data(), static_cast<uint32_t>(packed.size()), CM::ChunkSize) == literal,
        "worst-case literals fit the native envelope and round-trip unchanged");
    for (const auto& bytes : { std::vector<BYTE>{ 127, 1 }, std::vector<BYTE>{ 128 }, std::vector<BYTE>{ 255, 1 }, std::vector<BYTE>{ 0, 1, 0, 2 } })
    {
        bool rejected = false;
        try { CM::UnpackChunk(bytes.data(), static_cast<uint32_t>(bytes.size()), 1); }
        catch (const std::exception&) { rejected = true; }
        Check(rejected, "truncated, over-expanding and excess compressed chunks are rejected");
    }
    puts("PASS: bounded network compression, all-map byte preservation and malformed compressed packet rejection");
}

static void RejectedAssemblyTests(const CM::Map& map)
{
    CM::ResetNetwork();
    CM::MapIdentity identity; identity.revision = 2; identity.size = static_cast<uint32_t>(map.bytes.size()); identity.crc = CM::Checksum(map.bytes);
    std::vector<std::vector<BYTE>> packets;
    for (uint32_t offset = 0; offset < identity.size; offset += CM::ChunkSize)
    {
        CM::CheckedMapChunk checked; checked.header.version = 6; checked.header.identity = identity; checked.header.offset = offset;
        checked.header.size = (std::min)(CM::ChunkSize, identity.size - offset);
        checked.crc = CM::Checksum(map.bytes.data() + offset, checked.header.size);
        const auto packed = CM::PackChunk(map.bytes.data() + offset, checked.header.size);
        std::vector<BYTE> packet(sizeof(checked) + packed.size()); memcpy(packet.data(), &checked, sizeof(checked));
        memcpy(packet.data() + sizeof(checked), packed.data(), packed.size()); packets.push_back(std::move(packet));
    }
    Check(packets.size() > 2, "multi-chunk failed-assembly fixture");
    for (size_t i = 0; i < packets.size(); ++i)
    {
        auto packet = packets[i];
        if (i == 1)
        {
            // Valid per-chunk CRC but an invalid complete file, reproducing the
            // old terminal-checksum failure independently of the transport.
            CM::CheckedMapChunk checked; memcpy(&checked, packet.data(), sizeof(checked));
            auto bytes = CM::UnpackChunk(packet.data() + sizeof(checked), static_cast<uint32_t>(packet.size() - sizeof(checked)), checked.header.size);
            bytes[0] ^= 1; checked.crc = CM::Checksum(bytes);
            const auto packed = CM::PackChunk(bytes.data(), static_cast<uint32_t>(bytes.size()));
            packet.resize(sizeof(checked) + packed.size()); memcpy(packet.data(), &checked, sizeof(checked));
            memcpy(packet.data() + sizeof(checked), packed.data(), packed.size());
        }
        Check(CM::ReceiveMapChunk(packet.data(), static_cast<uint32_t>(packet.size())) == (i + 1 < packets.size()),
            "complete-file validation still rejects a wrong map despite valid individual chunks");
    }
    Check(CM::network.rejectedAssembly && !CM::network.remote && !CM::network.complete.revision,
        "failed complete assembly never becomes the confirmed preview or terrain");
    Check(!CM::ReceiveMapChunk(packets.back().data(), static_cast<uint32_t>(packets.back().size())) && CM::network.rejectedAssembly,
        "terminal retry cannot falsely confirm a previously rejected map");
    for (const auto& packet : packets) Check(CM::ReceiveMapChunk(packet.data(), static_cast<uint32_t>(packet.size())),
        "a fresh first chunk restarts the same failed assembly and accepts the correct retransmission");
    Check(!CM::network.rejectedAssembly && CM::network.remote && CM::network.remote->bytes == map.bytes && CM::SameIdentity(identity, CM::network.complete),
        "a failed complete map recovers to the exact original bytes instead of rejecting all future Go attempts");
    CM::ResetNetwork();
    puts("PASS: rejected complete-map checksum cannot confirm on a terminal retry and recovers on a fresh transfer");
}

static void HookTests(const char* path, const CM::Map& map)
{
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    Check(file != INVALID_HANDLE_VALUE, "open frontend");
    HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr);
    BYTE* frontend = static_cast<BYTE*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));
    Check(frontend != nullptr, "map frontend privately without running it");
    const size_t sites[] = { 0x42a44, 0x7896a, 0xa349, 0x4468b, 0x46f09, 0x277c4,
        0x3233d, 0x34294, 0x61865, 0x1e32f, 0x358a4, 0x61a81, 0x144ed, 0xa0b70, 0x336f1, 0x62214, 0x105f7, 0x1378d };
    std::array<std::array<BYTE, 16>, 18> originals{};
    for (size_t i = 0; i < originals.size(); ++i) memcpy(originals[i].data(), frontend + sites[i], 16);
    Check(MH_Initialize() == MH_OK, "initialize MinHook");
    for (size_t site : { 0x42a44u, 0x7896au, 0xa349u, 0x4468bu, 0x46f09u, 0x277c4u,
        0x3233du, 0x34294u, 0x61865u, 0x1e32fu, 0x358a4u, 0x61a81u, 0x144edu, 0xa0b70u, 0x336f1u, 0x62214u,
        0x1058bu, 0x12698u, 0x126fau, 0x1e2dcu, 0x1e2c0u, 0x1e371u, 0x3e227u, 0x97d50u, 0x97d61u,
        0x105f7u, 0x1378du, 0x12cc9u, 0x12cb8u })
    {
        DWORD old = 0; VirtualProtect(frontend + site, 1, PAGE_EXECUTE_READWRITE, &old);
        const BYTE saved = frontend[site]; frontend[site] = 0x90;
        Check(!CM::InstallInImage(frontend) && !CM::enabled, "unsupported signature rejected before hooking");
        frontend[site] = saved; VirtualProtect(frontend + site, 1, old, &old);
        for (size_t i = 0; i < originals.size(); ++i) Check(!memcmp(originals[i].data(), frontend + sites[i], 16), "rejection leaves code untouched");
    }
    Check(CM::InstallInImage(frontend), "install eighteen local/network colour-map detours");
    for (size_t i = 0; i < originals.size(); ++i) Check(memcmp(originals[i].data(), frontend + sites[i], 16), "hook enabled");
    DirectSendTests(frontend);
    ReceiveDiagnosticsTests(frontend);
    HostGoPreflightTests(frontend, map);
    EngineBarrierTests(frontend, map);
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
        const bool statusOnly = argc == 4 && strcmp(argv[3], "--status-only") == 0;
        Check(argc == 3 || statusOnly, "supply frontend path and map import directory, optionally --status-only");
        CM::traceEnabled = false; CM::controlSendAttempt = CaptureControlAttempt;
        ChecksumTests();
        INITCOMMONCONTROLSEX controls{ sizeof(controls), ICC_WIN95_CLASSES };
        InitCommonControlsEx(&controls);
        std::vector<BYTE> testImage(0x1b6000); CM::image = testImage.data();
        unsigned count = 0;
        std::unique_ptr<CM::Map> sample;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(argv[2]))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".dat") continue;
            CM::Map map = CM::Load(entry.path().c_str());
            if (statusOnly) { EditorTests(argv[1], map); TransferStatusTests(argv[1], map); return 0; }
            const unsigned packets = WireCodecTests(map);
            if (entry.path().filename() == L"Birthday.dat") Check(CM::Checksum(map.bytes) == 0x3a3314b7,
                "the reported Birthday map has the independently verified complete-file checksum");
            if (entry.path().filename() == L"Birthday.dat")
            {
                Check(packets <= 16, "Birthday still uses fewer receipt round trips than the original 52");
                printf("PASS: Birthday uses %u bounded TCP/IP packets instead of 52\n", packets);
            }
            if (!sample) sample.reset(new CM::Map(map));
            ++count;
        }
        Check(sample != nullptr, "found sample terrain");
        printf("PASS: all %u supplied terrain files load with their palettes and object locations\n", count);
        ParserTests(*sample); WireCodecBoundsTests(); RejectedAssemblyTests(*sample); PublishTests(*sample); EditorTests(argv[1], *sample); TransferStatusTests(argv[1], *sample); NetworkTests(*sample); PacedNetworkTests(*sample); WindowTransferTests(*sample); HandshakeBackpressureTests(*sample); SharedNetworkTests(*sample); HookTests(argv[1], *sample);
        return 0;
    }
    catch (const std::exception& error) { fprintf(stderr, "FAIL: %s\n", error.what()); return 1; }
}
