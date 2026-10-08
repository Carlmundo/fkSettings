#include <cstdio>
#include <filesystem>
#include "../fkSettings/ColourMaps.cpp"
#include "../fkSettings/SecretWeapons.cpp"
#include "../fkSettings/ExtendedOptions.cpp"
#include "../fkSettings/NetworkTeams.cpp"

namespace CM = ColourMaps;
static void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
static void Put(std::vector<BYTE>& bytes, size_t offset, uint32_t value) { memcpy(bytes.data() + offset, &value, 4); }
static void LanguageTests()
{
    struct Translation { const char* code; const wchar_t* prefix; };
    const Translation translations[] = {
        { "cs", L"Vyberte" }, { "de", L"W\u00e4hlen" }, { "en", L"Select" },
        { "es", L"Selecciona" }, { "es-419", L"Selecciona" }, { "fr", L"S\u00e9lectionnez" },
        { "is", L"Veldu" }, { "it", L"Seleziona" }, { "nl", L"Selecteer" }, { "pl", L"Wybierz" },
        { "pt", L"Selecione" }, { "pt-br", L"Selecione" },
        { "ru", L"\u0412\u044b\u0431\u0435\u0440\u0438\u0442\u0435" },
        { "sv", L"V\u00e4lj" }, { "zh-Hans", L"\u8bf7\u9009\u62e9" }
    };
    const std::wstring outside = CM::GameDirectory() + L"\\outside.dat";
    for (const auto& translation : translations)
        for (const std::string code : { std::string(translation.code), " \t" + std::string(translation.code) + "\r\n",
            "\xEF\xBB\xBF" + std::string(translation.code) + "\r\n" })
        {
            CM::SetLanguage(code);
            const auto& text = CM::strings.strImportFolder;
            Check(text.find(translation.prefix) == 0 && text.find(L".dat") != std::wstring::npos &&
                text.find(L"Levels\\Import") != std::wstring::npos, "all fifteen languages retain the literal filename extension and folder");
            for (bool relative : { false, true })
            {
                bool rejected = false;
                try { if (relative) CM::ImportedPath(L"..\\outside.dat"); else CM::RelativeImportPath(outside.c_str()); }
                catch (const std::runtime_error& error)
                {
                    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, error.what(), -1, nullptr, 0);
                    Check(count > 1, "localized path exception is valid UTF-8");
                    std::wstring decoded(count, L'\0');
                    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, error.what(), -1, &decoded[0], count);
                    decoded.pop_back();
                    Check(decoded == text, "both path rejection routes preserve the selected language for the Unicode dialog");
                    rejected = true;
                }
                Check(rejected, "invalid map paths remain rejected in every language");
            }
        }
    CM::SetLanguage("pt"); Check(CM::strings.strImportFolder.find(L"ficheiro") != std::wstring::npos, "European Portuguese wording");
    CM::SetLanguage("pt-br"); Check(CM::strings.strImportFolder.find(L"arquivo") != std::wstring::npos, "Brazilian Portuguese wording");
    for (const char* code : { "", "unknown", " \t\r\n", "\xEF\xBB\xBF" "\r\n" })
    {
        CM::SetLanguage(code);
        Check(CM::strings.strImportFolder == L"Select a .dat file inside the game's Levels\\Import folder.", "missing and unknown languages fall back to English");
    }
    CM::SetLanguage("en");
    puts("PASS: localized map-folder errors in all 15 languages, UTF-8/BOM/whitespace and English fallback");
}
static void ChecksumTests()
{
    const BYTE digits[] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
    Check(CM::Checksum(digits, sizeof(digits)) == 0xcbf43926, "CRC32 standard check vector");
    Check(CM::Checksum(nullptr, 0) == 0, "empty CRC32");
    std::vector<BYTE> bytes(256);
    for (size_t i = 0; i < bytes.size(); ++i) bytes[i] = static_cast<BYTE>(i);
    Check(CM::Checksum(bytes) == 0x29058c73, "CRC32 of all byte values matches an independent reference");
    bytes.assign(32768, 0);
    Check(CM::Checksum(bytes) == 0x011ffca6, "CRC32 of a zero-filled block matches an independent reference");
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
    uint32_t spawns = 0; memcpy(&spawns, map.bytes.data() + 20, sizeof(spawns));
    const size_t imageStart = 28 + spawns * 8;
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

static unsigned nativeGenerateCommands = 0;
static INT_PTR CALLBACK DialogProc(HWND, UINT message, WPARAM wparam, LPARAM)
{
    if (message == WM_COMMAND && LOWORD(wparam) == CM::GenerateId && HIWORD(wparam) == BN_CLICKED)
    {
        Check(!CM::selected, "native Generate receives the restored terrain mode");
        ++nativeGenerateCommands;
    }
    return FALSE;
}
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
    const std::array<int, 3> terrainControls{ 1028, 1019, 1025 };
    const std::array<LONG, 3> terrainStyles{ GetWindowLongW(GetDlgItem(window, 1028), GWL_STYLE),
        GetWindowLongW(GetDlgItem(window, 1019), GWL_STYLE), GetWindowLongW(GetDlgItem(window, 1025), GWL_STYLE) };
    const bool generateEnabled = IsWindowEnabled(GetDlgItem(window, CM::GenerateId)) != FALSE;
    LOGFONTW styleLabelFont{};
    Check(GetObjectW(reinterpret_cast<HFONT>(SendDlgItemMessageW(window, 1029, WM_GETFONT, 0, 0)),
        sizeof(styleLabelFont), &styleLabelFont) != 0, "read native style label font");
    // Native page initialization can size these labels independently of the dialog.
    styleLabelFont.lfHeight -= 2;
    HFONT nativeLabelFont = CreateFontIndirectW(&styleLabelFont);
    Check(nativeLabelFont != nullptr, "create independently sized native style label font");
    for (int id : { 1027, 1029 })
        SendDlgItemMessageW(window, id, WM_SETFONT, reinterpret_cast<WPARAM>(nativeLabelFont), FALSE);
    nativeGenerateCommands = 0;
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
    for (HWND page : { window, gameControls })
        Check(!GetDlgItem(page, 51013) && !GetDlgItem(page, 51014), "map status label and progress bar are absent from both tabs");
    Check(!GetDlgItem(window, 51011), "no separate generated-map reset button");
    HWND previewLabel = GetDlgItem(window, CM::PreviewLabelId);
    wchar_t previewText[128]{}; GetWindowTextW(previewLabel, previewText, 128);
    Check(previewText == label(CM::PreviewLabelStringId) && !(GetWindowLongW(previewLabel, GWL_STYLE) & WS_VISIBLE),
        "localized string 726 heading stays hidden for normal terrain");
    LOGFONTW headingFont{};
    Check(GetObjectW(reinterpret_cast<HFONT>(SendMessageW(previewLabel, WM_GETFONT, 0, 0)), sizeof(headingFont), &headingFont) &&
        headingFont.lfWeight == FW_BOLD && headingFont.lfHeight == styleLabelFont.lfHeight &&
        headingFont.lfWidth == styleLabelFont.lfWidth && wcscmp(headingFont.lfFaceName, styleLabelFont.lfFaceName) == 0 &&
        (GetWindowLongW(previewLabel, GWL_STYLE) & SS_TYPEMASK) == SS_CENTER,
        "custom terrain heading is bold and centred with the native Water/Level style label font size");
    wchar_t importText[128]{}; GetWindowTextW(GetDlgItem(window, CM::ImportId), importText, 128);
    Check(importText == label(CM::ImportStringId), "Import uses the frontend's localized string 447");
    const auto importRoot = std::filesystem::path(CM::ImportRoot());
    const auto hiddenImportRoot = importRoot.parent_path() / L"Import-unavailable-fixture";
    Check(!std::filesystem::exists(hiddenImportRoot), "isolated missing-import-folder fixture");
    std::filesystem::rename(importRoot, hiddenImportRoot);
    CM::RefreshAll();
    const bool importHidden = !(GetWindowLongW(GetDlgItem(window, CM::ImportId), GWL_STYLE) & WS_VISIBLE) &&
        !IsWindowEnabled(GetDlgItem(window, CM::ImportId));
    SendMessageW(window, WM_COMMAND, MAKEWPARAM(CM::ImportId, BN_CLICKED), reinterpret_cast<LPARAM>(GetDlgItem(window, CM::ImportId)));
    std::filesystem::rename(hiddenImportRoot, importRoot);
    CM::RefreshAll();
    Check(importHidden && (GetWindowLongW(GetDlgItem(window, CM::ImportId), GWL_STYLE) & WS_VISIBLE) &&
        IsWindowEnabled(GetDlgItem(window, CM::ImportId)), "Import hides for a missing folder and returns when the folder exists");
    HWND cancelButton = GetDlgItem(window, CM::CancelId);
    wchar_t cancelText[128]{}; GetWindowTextW(cancelButton, cancelText, 128);
    Check(cancelText == label(CM::CancelStringId) && !(GetWindowLongW(cancelButton, GWL_STYLE) & WS_VISIBLE),
        "localized string 19 Cancel button stays hidden for normal terrain");
    RECT button{}, picture{}, invert{};
    GetWindowRect(GetDlgItem(window, CM::ImportId), &button);
    GetWindowRect(GetDlgItem(window, 1033), &picture);
    GetWindowRect(GetDlgItem(window, 1057), &invert);
    RECT intersection{};
    Check(button.left >= picture.right && button.top == invert.top && button.bottom == invert.bottom &&
        button.left - picture.right == picture.left - invert.right && !IntersectRect(&intersection, &button, &picture),
        "Import fits to the right of the native preview, inline with Invert terrain and equally spaced");
    CM::selected.reset(new CM::Map(map));
    CM::RefreshAll();
    RECT importedButton{}, importedCancel{}, importedPicture{}, editButton{}, saveAsButton{}, generateButton{};
    GetWindowRect(GetDlgItem(window, CM::ImportId), &importedButton);
    GetWindowRect(cancelButton, &importedCancel);
    GetWindowRect(CM::pages[0]->preview, &importedPicture);
    GetWindowRect(GetDlgItem(window, CM::EditTerrainId), &editButton);
    GetWindowRect(GetDlgItem(window, CM::SaveAsId), &saveAsButton);
    GetWindowRect(GetDlgItem(window, CM::GenerateId), &generateButton);
    Check(importedPicture.right == saveAsButton.right &&
        importedPicture.bottom - generateButton.bottom == CM::Units(window, 0, 6, 0, 0).top &&
        importedPicture.right - importedPicture.left > CM::Units(window, 0, 0, 294, 0).right,
        "enlarged custom preview aligns with Save As on the right and sits below Generate's baseline");
    RECT headingBounds{}; GetWindowRect(previewLabel, &headingBounds);
    Check((GetWindowLongW(previewLabel, GWL_STYLE) & WS_VISIBLE) &&
        importedPicture.top - headingBounds.bottom == CM::Units(window, 0, 0, 0, 2).bottom + CM::Units(window, 0, 6, 0, 0).top &&
        headingBounds.left == importedPicture.left && headingBounds.right == importedPicture.right,
        "custom terrain heading stays centred with a larger gap above the lowered image");
    Check(abs(importedButton.left + importedCancel.right - importedPicture.left - importedPicture.right) <= 1 &&
        importedButton.top > importedPicture.bottom &&
        importedCancel.left - importedButton.right == CM::Units(window, 0, 0, 10, 0).right &&
        importedCancel.top == importedButton.top && importedCancel.bottom == importedButton.bottom &&
        importedButton.top - editButton.top == CM::Units(window, 0, 11, 0, 0).top &&
        importedButton.bottom - importedButton.top == editButton.bottom - editButton.top &&
        (GetWindowLongW(cancelButton, GWL_STYLE) & WS_VISIBLE) && IsWindowEnabled(cancelButton),
        "Import and Cancel are centred beneath the map with increased vertical and horizontal spacing");
    Check(!(GetWindowLongW(GetDlgItem(window, 1018), GWL_STYLE) & WS_VISIBLE), "generated pictures hidden when imported");
    for (int id : terrainControls)
        Check(!(GetWindowLongW(GetDlgItem(window, id), GWL_STYLE) & WS_VISIBLE),
            "Terrain label/dropdown and Current terrain label beside Save As are hidden for an import");
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
    Check(IsWindowEnabled(GetDlgItem(window, CM::GenerateId)), "native Generate stays available for imported maps");
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
    CM::network.joining = true; CM::network.remote.reset(new CM::Map(*CM::selected)); CM::RefreshAll();
    Check(!IsWindowEnabled(GetDlgItem(window, CM::GenerateId)), "joiners cannot replace the host's map using Generate");
    SendMessageW(window, WM_COMMAND, MAKEWPARAM(CM::GenerateId, BN_CLICKED), reinterpret_cast<LPARAM>(GetDlgItem(window, CM::GenerateId)));
    Check(CM::selected && nativeGenerateCommands == 0, "joiner Generate commands leave the host selection intact");
    Check(!IsWindowEnabled(cancelButton), "joiners cannot cancel the host's map");
    SendMessageW(window, WM_COMMAND, MAKEWPARAM(CM::CancelId, BN_CLICKED), reinterpret_cast<LPARAM>(cancelButton));
    Check(CM::selected && nativeGenerateCommands == 0, "joiner Cancel commands leave the host selection intact");
    CM::ResetNetwork(); CM::RefreshAll();
    SendMessageW(window, WM_COMMAND, MAKEWPARAM(CM::CancelId, BN_CLICKED), reinterpret_cast<LPARAM>(cancelButton));
    RECT cancelledImport{}, cancelledGameBounds{};
    GetWindowRect(GetDlgItem(window, CM::ImportId), &cancelledImport);
    GetWindowRect(gamePreview, &cancelledGameBounds);
    Check(!CM::selected && nativeGenerateCommands == 0 && EqualRect(&button, &cancelledImport) &&
        EqualRect(&originalGameBounds, &cancelledGameBounds) &&
        !(GetWindowLongW(cancelButton, GWL_STYLE) & WS_VISIBLE) &&
        !(GetWindowLongW(previewLabel, GWL_STYLE) & WS_VISIBLE),
        "Cancel restores normal views and Import position without generating terrain");
    for (size_t i = 0; i < terrainStyles.size(); ++i)
        Check((GetWindowLongW(GetDlgItem(window, terrainControls[i]), GWL_STYLE) & WS_VISIBLE) == (terrainStyles[i] & WS_VISIBLE),
            "Cancel restores the Terrain controls and Current terrain label beside Save As");
    CM::selected.reset(new CM::Map(map)); CM::RefreshAll();
    SendMessageW(window, WM_COMMAND, MAKEWPARAM(CM::GenerateId, BN_CLICKED), reinterpret_cast<LPARAM>(GetDlgItem(window, CM::GenerateId)));
    Check(!CM::selected && nativeGenerateCommands == 1, "Generate clears the import and forwards its native command");
    Check(!(GetWindowLongW(previewLabel, GWL_STYLE) & WS_VISIBLE), "Generate hides the custom terrain heading");
    Check((GetWindowLongW(GetDlgItem(window, 1018), GWL_STYLE) & WS_VISIBLE) == (initialStyle & WS_VISIBLE), "native visibility restored");
    for (size_t i = 0; i < terrainStyles.size(); ++i)
        Check((GetWindowLongW(GetDlgItem(window, terrainControls[i]), GWL_STYLE) & WS_VISIBLE) == (terrainStyles[i] & WS_VISIBLE),
            "Generate restores the Terrain controls and Current terrain label beside Save As");
    Check((IsWindowEnabled(GetDlgItem(window, CM::GenerateId)) != FALSE) == generateEnabled, "native Generate enabled state restored");
    RECT restoredImport{}; GetWindowRect(GetDlgItem(window, CM::ImportId), &restoredImport);
    Check(EqualRect(&button, &restoredImport) != FALSE, "Generate returns Import to the native preview's right side");
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
    DeleteObject(nativeLabelFont);
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
    if (confirmMap && length >= sizeof(CM::MapReference))
    {
        CM::MapReference reference; memcpy(&reference, packet, sizeof(reference));
        CM::MapReply ack; ack.header = reference.header; ack.header.type = CM::MapAckPacketType; ack.sourceCrc = reference.sourceCrc;
        Check(CM::ReceiveMapAck(target, &ack, sizeof(ack)), "mock peer confirms the installed map identity");
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
    for (size_t i = 0; (!CM::network.checks.empty() || CM::network.deferredGo || !CM::network.controls.empty() || !CM::network.pendingGo.empty()) && i < 8192; ++i) CM::PumpNetworkQueues();
    Check(CM::network.checks.empty() && !CM::network.deferredGo && CM::network.controls.empty() && CM::network.pendingGo.empty() && !CM::networkTimer,
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
    const uint32_t peer = 20; memcpy(CM::image + 0x1a7698 + 0x108c, &peer, 4);
    CM::enabled = true; CM::ResetNetwork(); CM::network.hosting = true; CM::network.source = 10;
    CM::OutgoingMap(&sample); CM::QueueMap(0, true);
    CM::mapSendAttempt = BusyMapAttempt; busyAttempts = 0; CM::PumpNetworkQueues();
    Check(busyAttempts == 1 && !CM::network.checks.empty() && !CM::network.failed, "busy reference send returns after one attempt");
    CM::network.checks.front().lastProgress = GetTickCount() - 15000; CM::PumpNetworkQueues();
    Check(CM::network.failed && CM::network.checks.empty() && !CM::networkTimer, "missing confirmations time out without disabling the frontend");
    CM::ResetNetwork(); CM::mapSendAttempt = CaptureMapAttempt;
    puts("PASS: installed map references use bounded sends and cancellation");
}

static void HandshakeBackpressureTests(const CM::Map& sample)
{
    const uint32_t mode = 2; memcpy(CM::image + 0x188b14, &mode, 4);
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
    while (!CM::network.checks.empty()) CM::PumpNetworkQueues();
    CM::PumpNetworkQueues(); // Move the frozen Go into the control queue.
    Check(CM::QueuedGo() && nativeHandshakeCalls == 0, "Go never calls the native blocking send wrapper");
    const auto identity = CM::network.round;
    busyAttempts = 0; CM::PumpNetworkQueues();
    Check(busyAttempts == 1 && CM::QueuedGo() && !CM::network.failed, "busy Go returns after one attempt and stays queued");
    CM::controlSendAttempt = CaptureControlAttempt; mapPackets.clear(); CM::PumpNetworkQueues();
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
        busyAttempts = 0; CM::PumpNetworkQueues();
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
        busyAttempts = 0; CM::PumpNetworkQueues();
        Check(nativeHandshakeCalls == 0 && busyAttempts == 1 && CM::network.controls.front().readyChannel,
            "client ready packets use their own channel without the native blocking wrapper");
        CM::controlSendAttempt = CaptureControlAttempt; DrainMapTransfers();
        Check(readyPacket.size() == 24 && CM::PacketType(readyPacket.data(), 24) == type,
            "queued client ready preserves its native type and map identity");
    }
    CM::controlSendAttempt = BusyControlAttempt;
    CM::SendReady(nullptr, nullptr, peer, 10, ready.data(), sizeof(ready));
    CM::network.controls.front().lastProgress = GetTickCount() - 15000; CM::PumpNetworkQueues();
    Check(CM::network.failed && CM::network.controls.empty() && !CM::networkTimer && nativeHandshakeCalls == 0,
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
    CM::MapHeader header;
    std::vector<BYTE> maximum(CM::MaximumMapMessage); memcpy(maximum.data(), &header, sizeof(header));
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
            "actual native Send accepts the bounded path reference on both channels and envelopes");
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
    Check(ReadLog(gamePath) == ReadLog(mirrorPath) && ReadLog(gamePath).find("map-network diagnostics 22 protocol 10") != std::string::npos &&
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
    CM::MapHeader header; header.identity.revision = 2;
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
        Check(log.find("map wire received") != std::string::npos && log.find("ver=10") != std::string::npos &&
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
static bool ipxLobbyClosed = false;
static unsigned ipxStartSends = 0, ipxSendsAfterClose = 0;
static unsigned ipxStartAttempts = 0, ipxBusyReplies = 0;
static bool ipxAlwaysBusy = false;
static CM::NativeDirectSend ipxNativeSend = nullptr;
static HRESULT __stdcall CloseIpxLobby(void*) { ipxLobbyClosed = true; return S_OK; }
static HRESULT __stdcall SendIpxStart(void* object, uint32_t source, uint32_t target, uint32_t flags, const void* packet, uint32_t length)
{
    Check(flags == 1 && length >= 4 && CM::PacketType(static_cast<const BYTE*>(packet) + 4, length - 4) == 14,
        "IPX start uses guaranteed Send and the native reliable sequence envelope");
    ++ipxStartSends;
    return CaptureDirectPlay(object, source, target, flags, packet, length);
}
static HRESULT __fastcall IpxDirectSend(void* channel, void*, uint32_t source, uint32_t target, uint32_t flags, const void* packet, uint32_t length)
{
    ++ipxStartAttempts;
    if (ipxLobbyClosed) { ++ipxSendsAfterClose; return static_cast<HRESULT>(0x88770078); }
    // A private SEC_IMAGE mapping has no initialized native CString error
    // formatter. Inject failures at the Send seam; successes still execute
    // the real native COM Send entry and the fake provider's vtable slot 26.
    if (ipxAlwaysBusy || ipxBusyReplies)
    {
        if (ipxBusyReplies) --ipxBusyReplies;
        return static_cast<HRESULT>(0x8877010e);
    }
    return ipxNativeSend(channel, source, target, flags, packet, length);
}
static intptr_t __cdecl CaptureSpawn(int mode, const char* path, const char* const* arguments, const char* const* environment)
{
    ++spawnCalls;
    Check(mode == 0 && !strcmp(path, "WORMS2.EXE") && arguments && !strcmp(arguments[0], "worms2.exe") &&
        arguments[1] == nullptr && environment == nullptr, "native IPX execl forwards mode, path, argument list and environment");
    applicationQueueEmpty = CM::network.controls.empty() && CM::network.checks.empty(); return 42;
}
static HRESULT __stdcall CaptureApplication(void*, uint32_t flags, uint32_t* id, const void* connection, HANDLE event)
{
    ++applicationCalls; applicationFlags = flags; applicationId = id; applicationConnection = connection; applicationEvent = event;
    applicationQueueEmpty = CM::network.controls.empty() && CM::network.checks.empty(); return S_OK;
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
        CM::network.controls.empty() && !CM::networkTimer && GetTickCount() - began < 2000 && busyAttempts <= 60,
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
    Check(execl(0, "WORMS2.EXE", "worms2.exe", nullptr) == -1 && spawnCalls == 1 && CM::network.failed && !CM::networkTimer,
        "a permanently busy start prevents IPX engine spawn too");
    CM::ResetNetwork(); CM::controlSendAttempt = CaptureControlAttempt;
    Check(execl(0, "WORMS2.EXE", "worms2.exe", nullptr) == 42 && spawnCalls == 2,
        "native IPX launch still passes through when no imported round is active");
    CM::originalSpawn = savedSpawn;

    // The real mode-1 launcher calls Close at 0x35b17 before RunApplication
    // at 0x35b30. A live provider at the engine barrier hid this bug before.
    std::vector<BYTE> transport(0x13300);
    std::array<void*, 27> lobbyVtable{};
    lobbyVtable[4] = reinterpret_cast<void*>(CloseIpxLobby);
    lobbyVtable[26] = reinterpret_cast<void*>(SendIpxStart);
    void** lobbyCom = lobbyVtable.data(); const void* lobbyObject = &lobbyCom;
    BYTE* lobby = transport.data() + 0x12d50;
    const uint32_t active = 1;
    memcpy(lobby + 0x39c, &active, 4); memcpy(lobby + 0x3a0, &lobbyObject, 4);
    memcpy(transport.data() + 0x120d4, &active, 4);
    uint32_t savedMode = 0; memcpy(&savedMode, frontend + 0x188b14, 4);
    DWORD modeProtection = 0; VirtualProtect(frontend + 0x188b14, 4, PAGE_READWRITE, &modeProtection);
    memcpy(frontend + 0x188b14, &active, 4);
    using NativeClose = HRESULT (__thiscall*)(void*);
    const auto close = reinterpret_cast<NativeClose>(frontend + 0x1003f);
    for (unsigned recipients : { 1u, 2u })
    {
        setup(); ipxLobbyClosed = ipxAlwaysBusy = false; ipxBusyReplies = 2;
        ipxStartSends = ipxSendsAfterClose = ipxStartAttempts = 0;
        ipxNativeSend = reinterpret_cast<CM::NativeDirectSend>(frontend + 0x1058b);
        CM::directSend = reinterpret_cast<CM::NativeDirectSend>(IpxDirectSend);
        CM::controlSendAttempt = CM::TryPacketSend;
        const uint32_t secondPeer = recipients == 2 ? 30 : 0; memcpy(players + 4, &secondPeer, 4);
        const uint32_t sequence = 17; memcpy(lobby + 0x438, &sequence, 4);
        CM::SendNetworkPacket(transport.data(), 10, 0, true, start.data(), sizeof(start), NativeHandshake);
        Check(ipxStartSends == recipients && ipxStartAttempts == recipients + 2 && CM::network.controls.empty() && !CM::network.failed &&
            *reinterpret_cast<uint32_t*>(lobby + 0x438) == sequence + recipients,
            "IPX start reaches every native provider recipient before the send wrapper returns");
        Check(close(lobby) == S_OK && ipxLobbyClosed, "actual native Close tears down the lobby before the engine call");
        const int calls = applicationCalls;
        Check(run(object.data(), &id, &connection, event) == S_OK && applicationCalls == calls + 1 && !ipxSendsAfterClose,
            "IPX startup survives the native Close-then-RunApplication order without sending after Close");
    }
    setup(); ipxLobbyClosed = false; ipxAlwaysBusy = true; ipxBusyReplies = 0;
    ipxStartSends = ipxStartAttempts = 0;
    CM::controlSendAttempt = CM::TryPacketSend;
    const DWORD ipxBegan = GetTickCount();
    CM::SendNetworkPacket(transport.data(), 10, 0, true, start.data(), sizeof(start), NativeHandshake);
    Check(CM::network.failed && CM::network.controls.empty() && !CM::networkTimer && !ipxStartSends && ipxStartAttempts <= 60 &&
        GetTickCount() - ipxBegan < 2000, "IPX start backpressure cancels within one second before native teardown instead of entering an unbounded retry");
    const int ipxCalls = applicationCalls;
    Check(run(object.data(), &id, &connection, event) == E_ABORT && applicationCalls == ipxCalls,
        "a failed early IPX start submission cannot enter the native engine wait");
    ipxAlwaysBusy = false;
    CM::directSend = ipxNativeSend;
    memcpy(players + 4, &savedPlayers[1], 4);
    memcpy(frontend + 0x188b14, &savedMode, 4); VirtualProtect(frontend + 0x188b14, 4, modeProtection, &modeProtection);
    CM::ResetNetwork(); CM::controlSendAttempt = CaptureControlAttempt;
    memcpy(players, savedPlayers.data(), sizeof(savedPlayers)); VirtualProtect(players, sizeof(savedPlayers), oldProtection, &oldProtection);
    std::filesystem::remove(directory / "land.dat"); std::filesystem::remove(directory);
    puts("PASS: native engine launch, IPX Send-before-Close ordering, multiple recipients, busy retries/cancellation, publication failure and unchanged native launch");
}

static void ReferenceTests(const CM::Map& map)
{
    const auto directory = std::filesystem::path(CM::GameDirectory()) / L"Data";
    Check(!std::filesystem::exists(directory), "isolated reference fixture"); std::filesystem::create_directory(directory);
    const auto identity = CM::OutgoingMap(&map);
    const auto packet = CM::ReferencePacket(&map, identity);
    Check(packet.size() == sizeof(CM::MapReference) + CM::EncodePath(map.relativePath).size() + map.waterPath.size() &&
        packet.size() < 1200 && packet.size() < map.bytes.size(), "exact reference contains path/settings only, never terrain bytes");
    CM::MapReference reference; std::unique_ptr<CM::Map> resolved;
    auto resolve = [&](const std::vector<BYTE>& bytes) { resolved.reset(); return CM::ResolveReference(bytes.data(), static_cast<uint32_t>(bytes.size()), reference, resolved); };
    Check(resolve(packet) == CM::MapResult::Ready && resolved && resolved->bytes == map.bytes &&
        CM::Load((directory / L"land.dat").c_str()).bytes == map.bytes, "installed map hash validates and exact bytes publish before readiness");
    auto edited = map; edited.cavern = !map.cavern; Put(edited.bytes, 16, edited.cavern);
    Check(CM::ChangeWater(edited, L"Purple"), "edit host water");
    const auto editedPacket = CM::ReferencePacket(&edited, CM::OutgoingMap(&edited));
    Check(resolve(editedPacket) == CM::MapResult::Ready && resolved->bytes == edited.bytes &&
        CM::LoadImported(map.relativePath).bytes == map.bytes, "source hash is checked before applying host style/water without changing installed maps");
    const auto localPath = std::filesystem::path(CM::ImportedPath(map.relativePath));
    const auto hidden = std::filesystem::path(localPath.wstring() + L".fixture-backup");
    std::filesystem::rename(localPath, hidden);
    Check(resolve(packet) == CM::MapResult::Missing && !resolved && CM::Load((directory / L"land.dat").c_str()).bytes == edited.bytes,
        "missing exact relative path reports missing and preserves the previous land.dat");
    std::filesystem::rename(hidden, localPath);
    auto wrong = map; wrong.cavern = !wrong.cavern; Put(wrong.bytes, 16, wrong.cavern); Check(CM::Publish(wrong, localPath.wstring()), "stage different local map");
    Check(resolve(packet) == CM::MapResult::Different && !resolved, "same filename with a different hash fails");
    Check(CM::Publish(map, localPath.wstring()), "restore identical installed map");
    SetFileAttributesW((directory / L"land.dat").c_str(), FILE_ATTRIBUTE_READONLY);
    Check(resolve(packet) == CM::MapResult::CannotPublish && !resolved, "read-only land.dat never reports readiness");
    SetFileAttributesW((directory / L"land.dat").c_str(), FILE_ATTRIBUTE_NORMAL);
    auto corrupt = packet; Put(corrupt, offsetof(CM::MapReference, sourceCrc), map.sourceCrc ^ 1);
    Check(resolve(corrupt) == CM::MapResult::Different, "different source CRC fails independently");
    corrupt = packet; Put(corrupt, offsetof(CM::MapReference, sourceSize), map.sourceSize + 1);
    Check(resolve(corrupt) == CM::MapResult::Different, "different source size fails independently");
    corrupt = packet; Put(corrupt, offsetof(CM::MapReference, header) + offsetof(CM::MapHeader, identity) + offsetof(CM::MapIdentity, crc), identity.crc ^ 1);
    Check(resolve(corrupt) == CM::MapResult::Different, "different final CRC fails independently");
    corrupt = packet; Put(corrupt, offsetof(CM::MapReference, cavern), 2); Check(resolve(corrupt) == CM::MapResult::Invalid, "invalid border fails");
    corrupt = packet; Put(corrupt, offsetof(CM::MapReference, pathLength), 0xffffffff); Check(resolve(corrupt) == CM::MapResult::Invalid, "unbounded path length fails");
    corrupt = packet; corrupt.pop_back(); Check(resolve(corrupt) == CM::MapResult::Invalid, "truncated descriptor fails");
    corrupt = packet; corrupt.push_back(0); Check(resolve(corrupt) == CM::MapResult::Invalid, "unexpected map payload fails");
    corrupt = packet; Put(corrupt, offsetof(CM::MapHeader, version), 9); Check(resolve(corrupt) == CM::MapResult::Unsupported, "old file-transfer protocol rejected");
    for (const wchar_t* unsafe : { L"..\\outside.dat", L"C:\\outside.dat", L"\\\\server\\map.dat", L"x/../map.dat", L"x\\..\\map.dat", L"x\\.\\map.dat", L"x\\\\map.dat", L"x \\map.dat", L"x.\\map.dat", L"map.dat:stream", L"map.txt" })
        Check(!CM::ValidRelativePath(unsafe), "absolute/traversing/aliased/non-map paths rejected");
    Check(CM::RelativeImportPath(localPath.c_str()) == map.relativePath, "picker keeps exact relative subfolder and spaces");
    bool rejected = false;
    try { CM::RelativeImportPath((std::filesystem::path(CM::GameDirectory()) / L"map.dat").c_str()); } catch (...) { rejected = true; }
    Check(rejected, "picker refuses files outside Levels/Import");
    rejected = false;
    try { CM::RelativeImportPath((std::filesystem::path(CM::GameDirectory()) / L"Levels/Import-other/map.dat").c_str()); } catch (...) { rejected = true; }
    Check(rejected, "import prefix requires a directory boundary");
    const std::wstring unicode = L"Online Worms\\\u00e9\u65e5\u672c.dat";
    Check(CM::DecodePath(CM::EncodePath(unicode)) == unicode, "Unicode path survives UTF-8 on the wire");
    for (const auto& invalid : { std::string("x\0.dat", 6), std::string("x\xc0\xaf.dat"), std::string("x\xff.dat") })
    { rejected = false; try { CM::DecodePath(invalid); } catch (...) { rejected = true; } Check(rejected, "invalid UTF-8 and embedded NUL fail"); }
    const auto reset = CM::ReferencePacket(nullptr, CM::OutgoingMap(nullptr));
    Check(resolve(reset) == CM::MapResult::Ready && !resolved && reset.size() == sizeof(CM::MapReference), "generated selection uses an empty path reference");
    CM::ResetNetwork(); std::filesystem::remove(directory / L"land.dat"); std::filesystem::remove(directory);
    puts("PASS: local path confinement, Unicode, source/final hashes, host edits, missing files, publication failure and no terrain payload");
}
struct NetworkEvent { uint32_t source, target; bool ready; std::vector<BYTE> packet; };
static std::deque<NetworkEvent> events;
static bool QueueTestMap(void*, uint32_t source, uint32_t target, const void* packet, uint32_t length)
{ events.push_back({ source, target, false, std::vector<BYTE>(static_cast<const BYTE*>(packet), static_cast<const BYTE*>(packet) + length) }); return true; }
static bool QueueTestControl(void*, uint32_t source, uint32_t target, const void* packet, uint32_t length, bool ready)
{ events.push_back({ source, target, ready, std::vector<BYTE>(static_cast<const BYTE*>(packet), static_cast<const BYTE*>(packet) + length) }); return true; }
static void NetworkTests(const CM::Map& sample)
{
    const auto directory = std::filesystem::path(CM::GameDirectory()) / L"Data";
    Check(!std::filesystem::exists(directory), "isolated network fixture"); std::filesystem::create_directory(directory);
    const uint32_t hostId = 10, peer = 20, count = 1;
    memcpy(CM::image + 0x1a7698 + 0x108c, &peer, 4); memcpy(CM::image + 0x1a7698 + 0x10d4, &count, 4);
    CM::enabled = true; CM::mapSendAttempt = QueueTestMap; CM::controlSendAttempt = QueueTestControl;
    for (uint32_t mode : { 0u, 1u })
    {
        memcpy(CM::image + 0x188b14, &mode, 4);
        CM::ResetNetwork(); CM::selected.reset(new CM::Map(sample));
        std::array<uint32_t, 2> go{ 27, 0 }; events.clear();
        CM::SendNetworkPacket(nullptr, hostId, 0, true, go.data(), sizeof(go), CaptureMap);
        Check(CM::network.checks.size() == 1 && !CM::network.pendingGo.empty(), "Go waits for one reference confirmation per player");
        CM::PumpNetworkQueues(); Check(events.size() == 1 && events.front().packet.size() < 1200, "host sends one small descriptor, on either transport");
        CM::NetworkState host = std::move(CM::network), client;
        auto reference = events.front(); events.pop_front();
        CM::network = std::move(client); memcpy(CM::image + 0x1892ac, &peer, 4);
        uint32_t length = static_cast<uint32_t>(reference.packet.size());
        Check(!CM::ReceiveNetworkPacket(30, hostId, reference.packet.data(), length) && !CM::network.joining, "non-host map selections rejected");
        Check(!CM::ReceiveNetworkPacket(hostId, hostId, reference.packet.data(), length) && CM::network.remote &&
            CM::network.remote->bytes == sample.bytes && CM::SameIdentity(CM::network.complete, host.round), "joiner loads local map and consumes custom selection");
        CM::PumpNetworkQueues(); Check(events.size() == 1 && events.front().ready && events.front().packet.size() == sizeof(CM::MapReply),
            "joiner confirms exact published map on its native ready channel");
        client = std::move(CM::network); CM::network = std::move(host); memcpy(CM::image + 0x1892ac, &hostId, 4);
        auto ack = events.front(); events.pop_front();
        Check(!CM::ReceiveMapAck(30, ack.packet.data(), static_cast<uint32_t>(ack.packet.size())), "non-recipient confirmation cannot release Go");
        auto stale = ack.packet; Put(stale, 12, CM::network.round.revision - 1);
        Check(!CM::ReceiveMapAck(peer, stale.data(), static_cast<uint32_t>(stale.size())), "stale revision confirmation rejected");
        Check(CM::ReceiveMapAck(peer, ack.packet.data(), static_cast<uint32_t>(ack.packet.size())), "actual local-map acknowledgement accepted");
        for (unsigned i = 0; i < 4; ++i) CM::PumpNetworkQueues();
        Check(CM::network.checks.empty() && events.size() == 1 && CM::PacketType(events.front().packet.data(), static_cast<uint32_t>(events.front().packet.size())) == 27,
            "matching local map alone releases native Go without any map-data packets");
        const auto identity = CM::network.round; host = std::move(CM::network);
        auto startGo = events.front(); events.pop_front(); CM::network = std::move(client); memcpy(CM::image + 0x1892ac, &peer, 4);
        length = static_cast<uint32_t>(startGo.packet.size());
        Check(CM::ReceiveNetworkPacket(hostId, hostId, startGo.packet.data(), length) && length == 8 && CM::NetworkMap()->bytes == sample.bytes,
            "matching Go strips the trailer and freezes local terrain");
        CM::originalSendReady = reinterpret_cast<CM::NativeSendPlayer>(CaptureReady);
        std::array<uint32_t, 2> ready{ 28, peer }; CM::SendReady(nullptr, nullptr, peer, hostId, ready.data(), sizeof(ready)); CM::PumpNetworkQueues();
        Check(events.size() == 1 && events.front().ready && events.front().packet.size() == 24, "native ready retains map identity and nonblocking channel");
        auto nativeReady = events.front(); events.pop_front(); client = std::move(CM::network); CM::network = std::move(host); memcpy(CM::image + 0x1892ac, &hostId, 4);
        Check(CM::AcceptReady(peer, nativeReady.packet.data(), static_cast<uint32_t>(nativeReady.packet.size())) &&
            !CM::AcceptReady(peer, nativeReady.packet.data(), static_cast<uint32_t>(nativeReady.packet.size())), "native start requires the sent Go and rejects duplicate readiness");
        CM::ResetNetwork(); CM::network.hosting = true; CM::network.source = hostId; CM::OutgoingMap(&sample); CM::QueueMap(0, true); CM::PumpNetworkQueues();
        CM::MapReply negative; negative.header.type = CM::MapAckPacketType; negative.header.identity = CM::network.outgoing;
        negative.sourceCrc = sample.sourceCrc; negative.result = CM::MapResult::Missing;
        Check(CM::ReceiveMapAck(peer, &negative, sizeof(negative)) && CM::network.failed,
            "a missing-map reply immediately blocks the host from starting");
        CM::PumpNetworkQueues(); Check(CM::network.checks.empty() && !CM::networkTimer, "negative confirmation cancels pending starts");
        events.clear(); CM::ResetNetwork();
        CM::network = std::move(client); CM::network.remote.reset(); CM::network.complete = {};
        length = static_cast<uint32_t>(startGo.packet.size());
        Check(!CM::ReceiveNetworkPacket(hostId, hostId, startGo.packet.data(), length), "native Go without a matching validated file is withheld");
        CM::ResetNetwork();
        (void)identity;
    }
    CM::mapSendAttempt = CaptureMapAttempt; CM::controlSendAttempt = CaptureControlAttempt;
    CM::selected.reset(); CM::enabled = false; std::filesystem::remove(directory / L"land.dat"); std::filesystem::remove(directory);
    puts("PASS: TCP/IP and IPX reference/ack/Go/ready flow, sender/revision checks, validation failures and native start gating");
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
                "shared lobby/results receiver consumes path references and forwards native Go/start on both transports");
            Check(NT::receivedTeams[0].skill == 50 && strcmp(NT::receivedTeams[0].name, "Map CPU") == 0,
                "CPU-team metadata survives colour map trailer stripping");
            if (!round)
                Check(SW::secretStocks.front() == 7 && SW::secretStocks.back() == 7 && EO::values[0] == 1,
                    "weapon and option extensions still decode alongside local map selection");
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
    if (expectedGoMap) Check(CM::network.checks.empty() && CM::NetworkMap() && CM::NetworkMap()->bytes == expectedGoMap->bytes,
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
        CM::network.goStarted = GetTickCount() - 30000; CM::PumpNetworkQueues();
        Check(!CM::network.failed && CM::network.deferredGo && nativeGoCalls == 0,
            "a slow map check does not spend the native 30-second Go timeout");
        CM::ChangeWater(*CM::selected, L"Purple"); CM::SelectionChanged();
        Check(CM::network.startMap->bytes == map.bytes, "Go preflight retains the selected bytes when later edits occur");
        const uint32_t latePeer = 30, snapshotType = 5;
        memcpy(frontend + 0x1a7698 + 0x1090, &latePeer, 4);
        std::vector<BYTE> snapshot(0x10de); memcpy(snapshot.data(), &snapshotType, 4);
        CM::SendNetworkPacket(frontend + 0x176a40, host, latePeer, false, snapshot.data(), static_cast<uint32_t>(snapshot.size()), CaptureMap);
        Check(CM::network.checks.size() == 2 && CM::network.checks.back().packet == CM::ReferencePacket(&map, CM::network.startIdentity),
            "a late join during Go preflight receives the frozen map despite later selection edits");
        confirmMap = true; for (auto& delivery : CM::network.checks.front().deliveries) delivery.sentAt = GetTickCount() - 1000;
        DrainMapTransfers();
        Check(nativeGoCalls == 1 && !IsWindowEnabled(window) && CM::NetworkMap()->bytes == map.bytes &&
            CM::PacketType(mapPackets.back().data(), static_cast<uint32_t>(mapPackets.back().size())) == 27,
            "confirmed Go enters native startup exactly once with the frozen terrain and no terrain-data transmission");
        // An unchanged next Go rechecks its installed path before disabling UI.
        EnableWindow(window, TRUE); KillTimer(window, 8); CM::selected.reset(new CM::Map(map));
        confirmMap = false; go(object.data());
        Check(IsWindowEnabled(window) && nativeGoCalls == 1 && CM::network.deferredGo && CM::SameIdentity(CM::network.checks.front().identity, CM::network.startIdentity),
            "local-path confirmation also precedes native Go");
        const uint32_t cachedLatePeer = 40; memcpy(frontend + 0x1a7698 + 0x1094, &cachedLatePeer, 4);
        CM::SendNetworkPacket(frontend + 0x176a40, host, cachedLatePeer, false, snapshot.data(), static_cast<uint32_t>(snapshot.size()), CaptureMap);
        Check(CM::network.checks.size() == 2 && CM::SameIdentity(CM::network.checks.back().identity, CM::network.startIdentity),
            "late join during repeated Go receives the same frozen path reference");
        confirmMap = true; DrainMapTransfers();
        Check(nativeGoCalls == 2 && CM::NetworkMap()->bytes == map.bytes, "repeated Go preserves frozen terrain and enters startup once");
        EnableWindow(window, TRUE); KillTimer(window, 8); CM::ResetNetwork(); CM::network.hosting = true;
        confirmMap = false; nativeGoCalls = 0; go(object.data());
        CM::network.checks.front().lastProgress = GetTickCount() - 15000; CM::PumpNetworkQueues();
        Check(CM::network.failed && !CM::network.deferredGo && IsWindowEnabled(window) && nativeGoCalls == 0 && !CM::networkTimer,
            "failed map confirmation leaves the frontend enabled and never enters native timeout/generation");
        const auto rejectedIdentity = CM::network.startIdentity;
        go(object.data()); Check(CM::network.deferredGo && !CM::SameIdentity(rejectedIdentity, CM::network.startIdentity),
            "Go can be retried after failed preflight with a fresh revision");
        CM::PumpNetworkQueues();
        CM::MapReply oldReply; oldReply.header.type = CM::MapAckPacketType;
        oldReply.header.identity = rejectedIdentity; oldReply.sourceCrc = map.sourceCrc;
        Check(!CM::ReceiveMapAck(peer, &oldReply, sizeof(oldReply)) && !CM::network.checks.front().deliveries.front().confirmed,
            "a delayed reply from the failed Go cannot confirm the restarted round");
        DestroyWindow(window);
        Check(!CM::network.deferredGo && !CM::networkTimer && nativeGoCalls == 0, "closing the lobby cancels deferred native Go before destroying its object");
    }
    expectedGoMap = nullptr; confirmMap = true; CM::ResetNetwork(); CM::selected.reset();
    CM::originalHostGo = savedHostGo; CM::originalHostRoundGo = savedRoundGo;
    memcpy(frontend + 0x1a7698 + 0x1090, &none, 4);
    memcpy(frontend + 0x1a7698 + 0x1094, &none, 4);
    std::filesystem::remove(directory / "land.dat"); std::filesystem::remove(directory);
    puts("PASS: actual first/next-round Go preflight hooks preserve responsive UI, frozen terrain, repeated start, timeout recovery and cancellation");
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
    setvbuf(stdout, nullptr, _IONBF, 0);
    try
    {
        Check(argc == 3, "supply frontend path and map import directory");
        CM::traceEnabled = false; CM::controlSendAttempt = CaptureControlAttempt;
        LanguageTests(); ChecksumTests();
        INITCOMMONCONTROLSEX controls{ sizeof(controls), ICC_WIN95_CLASSES };
        InitCommonControlsEx(&controls);
        std::vector<BYTE> testImage(0x1b6000); CM::image = testImage.data();
        unsigned count = 0;
        std::unique_ptr<CM::Map> sample;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(argv[2]))
        {
            if (!entry.is_regular_file() || entry.path().extension() != ".dat") continue;
            CM::Map map = CM::Load(entry.path().c_str());
            const std::wstring relative = L"Online Worms\\01 Path to Hell.dat";
            map.relativePath = relative;
            CM::MapIdentity id; id.revision = 1; id.size = static_cast<uint32_t>(map.bytes.size()); id.crc = CM::Checksum(map.bytes);
            Check(CM::ReferencePacket(&map, id).size() < 1200, "every map selection has a small fixed metadata footprint");
            if (entry.path().filename() == L"Birthday.dat") Check(CM::Checksum(map.bytes) == 0x3a3314b7,
                "Birthday retains its independently verified complete-file checksum");
            if (!sample) sample.reset(new CM::Map(map));
            ++count;
        }
        Check(sample != nullptr, "found sample terrain");
        printf("PASS: all %u supplied terrain files load with their palettes and object locations\n", count);
        const auto importRoot = std::filesystem::path(CM::ImportRoot());
        Check(!std::filesystem::exists(importRoot), "isolated import fixture");
        std::filesystem::create_directories(importRoot / L"Online Worms");
        Check(CM::Publish(*sample, CM::ImportedPath(sample->relativePath)), "stage installed colour map");
        ParserTests(*sample); ReferenceTests(*sample); PublishTests(*sample); EditorTests(argv[1], *sample); NetworkTests(*sample); PacedNetworkTests(*sample); HandshakeBackpressureTests(*sample); SharedNetworkTests(*sample); HookTests(argv[1], *sample);
        std::filesystem::remove(CM::ImportedPath(sample->relativePath));
        std::filesystem::remove(importRoot / L"Online Worms"); std::filesystem::remove(importRoot); std::filesystem::remove(importRoot.parent_path());
        return 0;
    }
    catch (const std::exception& error) { fprintf(stderr, "FAIL: %s\n", error.what()); return 1; }
}
