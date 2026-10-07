#include <cstdio>
#include <stdexcept>
#include "../fkSettings/ExtendedOptions.cpp"
#include "../fkSettings/SecretWeapons.cpp"
#include "../fkSettings/ColourMaps.cpp"
#include "../fkSettings/NetworkTeams.cpp"

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

static void LanguageTests(const char* path)
{
    HMODULE module = LoadLibraryExA(path, nullptr, LOAD_LIBRARY_AS_DATAFILE);
    Check(module != nullptr, "load native weapon string resources without running frontend");
    const HMODULE previousResources = EO::languageResources;
    EO::languageResources = module;
    const std::pair<EO::OptionIndex, UINT> herdIds[] = {
        { EO::OptionIndex::HerdDynamite, 4915 }, { EO::OptionIndex::HerdMine, 4916 },
        { EO::OptionIndex::HerdMingVase, 4917 }, { EO::OptionIndex::HerdSheep, 4930 }
    };
    std::wstring weaponNames[EO::OptionCount];
    EO::SetLanguage("en");
    std::array<std::wstring, EO::OptionCount> english;
    for (const auto& option : EO::Options) english[EO::ToIndex(option.index)] = *option.label;
    const auto englishHerdPrefix = EO::strings.strHerd;
    const auto englishGroupTitle = EO::strings.strExtendedOptions;
    for (const auto& entry : herdIds)
    {
        wchar_t text[256]{};
        Check(LoadStringW(module, entry.second, text, 256) != 0, "requested native weapon string exists");
        const size_t index = EO::ToIndex(entry.first);
        weaponNames[index] = text;
        english[index] = englishHerdPrefix + L": " + text;
    }
    // Hints are editable translations; the fallback checks below must preserve
    // the current wording rather than require the older generated sentence.
    Check(!EO::strings.hintUtilitiesDontEndTurn.empty(), "customized English utilities hint exists");
    EO::values = Pattern(EO::OptionCount);
    const auto before = EO::values;
    EO::SetLanguage("en");
    std::array<std::wstring, EO::OptionCount> englishHints;
    for (const auto& option : EO::Options) englishHints[EO::ToIndex(option.index)] = *option.hint;
    for (const char* language : { "en", "", "unknown", " \r\n", " en\r\n", "\xEF\xBB\xBF" "en\r\n" })
    {
        EO::SetLanguage(language);
        for (const auto& option : EO::Options)
        {
            Check(*option.label == english[EO::ToIndex(option.index)],
                "English and fallback supply every named option label");
            Check(*option.hint == englishHints[EO::ToIndex(option.index)], "English fallback preserves customized hints");
        }
        Check(EO::strings.strExtendedOptions == englishGroupTitle, "English fallback preserves customized group title");
        Check(EO::strings.strHerd == englishHerdPrefix, "English fallback preserves edited herd prefix");
    }
    struct ExpectedTranslation
    {
        const char* code;
        const wchar_t* aquaSheep;
        const wchar_t* herd;
        const wchar_t* title;
        bool hints;
    };
    const ExpectedTranslation expectedTranslations[] = {
        { "cs", L"Vodn\u00ed ovce", L"St\u00e1do zbran\u00ed", L"Roz\u0161\u00ed\u0159en\u00e1 nastaven\u00ed", true },
        { "pt-br", L"Ovelha Aqu\u00e1tica", L"Manada de armas", L"Op\u00e7\u00f5es Avan\u00e7adas", true },
        { "nl", L"Waterschaap", L"Kudde", L"Uitgebreide opties", false },
        { "en", L"Aqua Sheep", L"Herd weapon", englishGroupTitle.c_str(), true },
        { "fr", L"Mouton aquatique", L"Troupeau", L"Options de jeu \u00e9tendues", false },
        { "de", L"Aquaschaf", L"Herde", L"Erweiterte Spieloptionen", false },
        { "is", L"Vatnakind", L"Vopnahj\u00f6r\u00f0", L"Aukastillingar", true },
        { "it", L"Pecora acquatica", L"Mandria", L"Opzioni estese", false },
        { "pl", L"Wodna Owca", L"Stadne bronie", L"Opcje Rozszerzone", true },
        { "pt", L"Ovelha Aqu\u00e1tica", L"Manada de armas", L"Op\u00e7\u00f5es Avan\u00e7adas", true },
        { "ru", L"\u0410\u043a\u0432\u0430-\u043e\u0432\u0446\u0430", L"\u0421\u0442\u0430\u0434\u043e", L"\u0420\u0430\u0441\u0448\u0438\u0440\u0435\u043d\u043d\u044b\u0435 \u043d\u0430\u0441\u0442\u0440\u043e\u0439\u043a\u0438 \u0438\u0433\u0440\u044b", false },
        { "es", L"Oveja acu\u00e1tica", L"Manada", L"Opciones de Juego Extendidas", false },
        { "es-419", L"Oveja acu\u00e1tica", L"Arma de reba\u00f1o", L"Opciones extendidas", false },
        { "sv", L"Vattenf\u00e5r", L"Hjord", L"Ut\u00f6kade Spelinst\u00e4llningar", false },
        { "zh-Hans", L"\u6c34\u4e2d\u7ef5\u7f8a", L"\u7fa4\u53d1\u6b66\u5668", L"\u6269\u5c55\u9009\u9879", true },
    };
    for (const char* language : { "cs", "de", "en", "es", "es-419", "fr", "is", "it", "nl", "pl", "pt", "pt-br",
        "ru", "sv", "zh-Hans", "de\r\n", "\xEF\xBB\xBF" "zh-Hans\r\n" })
    {
        EO::SetLanguage(language);
        const ExpectedTranslation* expected = nullptr;
        const char* code = language;
        if (strcmp(language, "de\r\n") == 0) code = "de";
        if (strcmp(language, "\xEF\xBB\xBF" "zh-Hans\r\n") == 0) code = "zh-Hans";
        for (const auto& entry : expectedTranslations)
            if (strcmp(code, entry.code) == 0)
                expected = &entry;
        Check(expected != nullptr, "every supported language has translation expectations");
        Check(EO::strings.strAquaSheep == expected->aquaSheep && EO::strings.strHerd == expected->herd &&
            EO::strings.strExtendedOptions == expected->title,
            "language selection preserves Unicode and translated group titles");
        for (const auto& option : EO::Options)
        {
            Check(!option.label->empty(), "all option labels are filled in the requested languages");
            Check(option.hint->empty() != expected->hints,
                "translated hints are populated and unmapped hints retain blank placeholders");
        }
        for (const auto& entry : herdIds)
            Check(*EO::FindOption(EO::ToIndex(entry.first))->label == EO::strings.strHerd + L": " + weaponNames[EO::ToIndex(entry.first)],
                "every language composes herd captions with its translated GAME_HERD prefix and native weapon names");
    }
    for (const auto& entry : herdIds)
        Check(EO::FindOption(EO::ToIndex(entry.first))->hint == &EO::strings.hintHerd,
            "all four herd options share the same hint variable");
    Check(EO::values == before, "language changes preserve every stored option value");
    EO::SetLanguage("en");
    EO::languageResources = previousResources;
    FreeLibrary(module);
    Check(*EO::FindOption(EO::ToIndex(EO::OptionIndex::HerdSheep))->label == english[EO::ToIndex(EO::OptionIndex::HerdSheep)],
        "composed labels own their text after resource module unloads");
    EO::SetLanguage("fr");
    Check(EO::strings.strCrateRate == L"Taux de caisses", "preserve customized French crate rate label");
    EO::SetLanguage("pt");
    Check(EO::strings.hintCrateRate ==
        L"Especifica quantas caixas podem aparecer em simult\u00e2neo no in\u00edcio do turno. Com as probabilidades de queda de caixas definidas para 100%, faz aparecer exatamente a quantidade especificada.",
        "preserve current Portuguese crate rate hint and Unicode");
    EO::SetLanguage("en");
    puts("PASS: current translations, Unicode, translated/blank hints, English fallback and herd string IDs");
}

static void SchemeTests()
{
    // Anchor the externally consumed byte map independently of the enum values.
    const EO::OptionIndex storageOrder[] = {
        EO::OptionIndex::GodMode, EO::OptionIndex::HighJump, EO::OptionIndex::SheepHeaven,
        EO::OptionIndex::SuperShopperCrates, EO::OptionIndex::ExtendedFusesHerds,
        EO::OptionIndex::UtilitiesDontEndTurn, EO::OptionIndex::WeaponsDontEndTurn,
        EO::OptionIndex::LossOfControlDoesntEndTurn, EO::OptionIndex::WormSelectAfterMovement,
        EO::OptionIndex::LowGravity, EO::OptionIndex::PersistentRope, EO::OptionIndex::RapidPlay,
        EO::OptionIndex::IndestructibleTerrain, EO::OptionIndex::InvisibleTerrain,
        EO::OptionIndex::FastCrates, EO::OptionIndex::CrateSpy, EO::OptionIndex::CrateLimit,
        EO::OptionIndex::CrateRate, EO::OptionIndex::SuicideBomber, EO::OptionIndex::AquaSheep,
        EO::OptionIndex::InstantMines, EO::OptionIndex::HerdDynamite, EO::OptionIndex::HerdMine,
        EO::OptionIndex::HerdMingVase, EO::OptionIndex::HerdSheep,
        EO::OptionIndex::DisableBackflip, EO::OptionIndex::DisableUnlockedAim,
    };
    static_assert(sizeof(storageOrder) / sizeof(storageOrder[0]) == EO::OptionCount, "Complete byte map");
    for (size_t index = 0; index < EO::OptionCount; ++index)
    {
        Check(EO::ToIndex(storageOrder[index]) == index, "explicit indexes match the updated byte map");
        Check(EO::Options[index].index == storageOrder[index], "current visual order matches the updated byte map");
    }
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
static HWND PointerTarget(HWND parent, POINT point)
{
    // Follow sibling Z order and real WM_NCHITTEST responses, as mouse routing
    // does. Direct BM_CLICK/WM_MOUSEMOVE bypass this and miss covering windows.
    for (HWND child = GetWindow(parent, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
    {
        RECT bounds{};
        if (!(GetWindowLongPtrW(child, GWL_STYLE) & WS_VISIBLE) || !GetWindowRect(child, &bounds) || !PtInRect(&bounds, point)) continue;
        if (SendMessageW(child, WM_NCHITTEST, 0, MAKELPARAM(point.x, point.y)) == HTCLIENT) return child;
    }
    return parent;
}
static void EditorTests(const char* path)
{
    InitCommonControls();
    HMODULE module = LoadLibraryExA(path, nullptr, LOAD_LIBRARY_AS_DATAFILE);
    Check(module != nullptr, "load supplied frontend resources without executing it");
    const HMODULE previousResources = EO::languageResources;
    EO::languageResources = module;
    EO::SetLanguage("en");
    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(154), MAKEINTRESOURCEW(5));
    auto resourceTemplate = static_cast<const DLGTEMPLATE*>(LockResource(LoadResource(module, resource)));
    HWND root = CreateWindowW(L"STATIC", L"Fixture", WS_OVERLAPPEDWINDOW, 0, 0, 700, 500, nullptr, nullptr, nullptr, nullptr);
    HWND container = CreateWindowW(L"STATIC", L"Container", WS_CHILD | WS_VISIBLE, 0, 0, 650, 450, root, nullptr, nullptr, nullptr);
    HWND hintBox = CreateWindowW(L"STATIC", L"Native hint", WS_CHILD | WS_VISIBLE, 0, 450, 600, 40,
        root, reinterpret_cast<HMENU>(1003), nullptr, nullptr);
    HWND unrelated1003 = CreateWindowW(L"BUTTON", L"Unrelated button", WS_CHILD, 0, 0, 10, 10,
        container, reinterpret_cast<HMENU>(1003), nullptr, nullptr);
    Check(hintBox && unrelated1003, "create persistent ancestor hint box and colliding button ID");
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
    const auto configuredGodHint = EO::strings.hintGodMode;
    for (const auto& option : EO::Options)
        for (HWND control : { editor->controls[EO::ToIndex(option.index)] })
            if (control)
            {
                RECT bounds{};
                GetWindowRect(control, &bounds);
                Check(PointerTarget(window, POINT{ bounds.left + 5, bounds.top + 5 }) == control,
                    "real mouse hit testing reaches every option instead of the covering group box");
            }
    RECT godBounds{};
    GetWindowRect(editor->controls[0], &godBounds);
    HWND hoveredGod = PointerTarget(window, POINT{ godBounds.left + 5, godBounds.top + 5 });
    SendMessageW(hoveredGod, WM_SETCURSOR, reinterpret_cast<WPARAM>(hoveredGod), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
    Check(EO::WindowText(hintBox) == configuredGodHint, "mouse routing shows user configured English God Mode hint");
    SendMessageW(hoveredGod, WM_MOUSELEAVE, 0, 0);
    Check(scrollTotal.cx == native.right && scrollTotal.cy > native.bottom, "extend native scroll height without changing width");
    const int expectedOffsets[] = { 0, 11, 22, 33, 64, 75, 86, 97, 108, 119, 130, 141, 152, 163,
        0, 11, 22, 53, 84, 95, 106, 117, 128, 139, 150, 161, 172 };
    const int expectedGaps[] = { 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2,
        0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 2, 2 };
    RECT firstCheckbox{}, secondCheckbox{}, firstSliderLabel{}, secondSliderLabel{};
    GetWindowRect(editor->controls[0], &firstCheckbox);
    GetWindowRect(editor->controls[1], &secondCheckbox);
    GetWindowRect(editor->labels[EO::ToIndex(EO::OptionIndex::CrateLimit)], &firstSliderLabel);
    GetWindowRect(editor->labels[EO::ToIndex(EO::OptionIndex::CrateRate)], &secondSliderLabel);
    const LONG checkboxPitch = secondCheckbox.top - firstCheckbox.top;
    const int halfGap = (checkboxPitch + 1) / 2;
    Check(sameSpacing(secondCheckbox.top - firstCheckbox.top, nextNativeCheckbox.top - nativeCheckbox.top),
        "checkbox pitch matches original controls");
    RECT instantMines{}, herdDynamite{}, rapidPlay{}, terrain{}, suicideBomber{}, aquaSheep{}, herdSheep{}, backflip{};
    GetWindowRect(editor->controls[20], &instantMines);
    GetWindowRect(editor->controls[21], &herdDynamite);
    Check(sameSpacing(herdDynamite.top - instantMines.top, checkboxPitch),
        "Aqua Sheep and Instant Mines precede herd weapons without an extra gap");
    GetWindowRect(editor->controls[EO::ToIndex(EO::OptionIndex::RapidPlay)], &rapidPlay);
    GetWindowRect(editor->controls[EO::ToIndex(EO::OptionIndex::IndestructibleTerrain)], &terrain);
    GetWindowRect(editor->controls[EO::ToIndex(EO::OptionIndex::SuicideBomber)], &suicideBomber);
    GetWindowRect(editor->controls[19], &aquaSheep);
    Check(sameSpacing(aquaSheep.top - suicideBomber.top, checkboxPitch),
        "Suicide Bomber immediately precedes Aqua Sheep");
    GetWindowRect(editor->controls[24], &herdSheep);
    GetWindowRect(editor->controls[25], &backflip);
    Check(sameSpacing(terrain.top - rapidPlay.top, checkboxPitch + halfGap) &&
        sameSpacing(suicideBomber.top - secondSliderLabel.top,
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
    Check((GetWindowLongPtrW(editor->controls[EO::ToIndex(EO::OptionIndex::LowGravity)], GWL_STYLE) & BS_TYPEMASK) == BS_AUTOCHECKBOX &&
        !editor->labels[EO::ToIndex(EO::OptionIndex::LowGravity)] && !editor->readouts[EO::ToIndex(EO::OptionIndex::LowGravity)], "Low Gravity is a checkbox without slider label or readout");
    for (size_t i = 0; i < EO::OptionCount; ++i)
    {
        const bool slider = EO::Options[i].IsSlider();
        const size_t storageIndex = EO::ToIndex(EO::Options[i].index);
        const HWND control = editor->controls[i];
        const HWND labelWindow = slider ? editor->labels[i] : control;
        wchar_t label[100]{};
        GetWindowTextW(labelWindow, label, 100);
        Check(label == *EO::Options[i].label, "every requested option label present");
        RECT bounds{}, group{};
        GetWindowRect(control, &bounds);
        GetWindowRect(editor->group, &group);
        Check(bounds.left > group.left && bounds.right < group.right && bounds.top > group.top && bounds.bottom < group.bottom,
            "all controls contained inside new group");
        Check(GetWindowLongPtrW(control, GWL_STYLE) & WS_TABSTOP, "option is keyboard reachable");
        RECT labelBounds{};
        GetWindowRect(labelWindow, &labelBounds);
        RECT expected{ i < 14 ? 14 : 205, 490 + expectedOffsets[i], 0, 0 };
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
            const UINT zeroString = i == EO::ToIndex(EO::OptionIndex::SuperShopperCrates) ? 141 : 99;
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
            Check(wcscmp(readout, i == EO::ToIndex(EO::OptionIndex::SuperShopperCrates) ? L"No" : L"Default") == 0, "zero readout fallback");
            wchar_t localized[100]{};
            Check(LoadStringW(module, zeroString, localized, 100) != 0 && EO::ValueText(EO::Options[i], module) == localized,
                "zero text loads actual frontend string resource");
            SendMessageW(control, WM_KEYDOWN, VK_END, 0);
            SendMessageW(control, WM_KEYUP, VK_END, 0);
            Check(EO::values[storageIndex] == maximum, "keyboard End on actual trackbar writes maximum immediately");
            GetWindowTextW(editor->readouts[i], readout, 100);
            Check(std::wstring(readout) == (i == EO::ToIndex(EO::OptionIndex::SuperShopperCrates) ? L"Unlimited" : std::to_wstring(maximum)), "maximum readout");
            if (i == EO::ToIndex(EO::OptionIndex::SuperShopperCrates))
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
    for (const auto& option : EO::Options)
    {
        auto& hint = *const_cast<std::wstring*>(option.hint);
        hint = L"Hover hint " + std::to_wstring(EO::ToIndex(option.index)) + L"\n\x0416\x4E2D";
        const size_t index = EO::ToIndex(option.index);
        for (HWND control : { editor->controls[index] })
            if (control)
            {
                SendMessageW(control, WM_MOUSEMOVE, 0, MAKELPARAM(1, 1));
                Check(EO::WindowText(hintBox) == hint, "checkbox and trackbar show option hint");
                SetWindowTextW(hintBox, L"Native cursor hint");
                SendMessageW(control, WM_SETCURSOR, reinterpret_cast<WPARAM>(control), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
                Check(EO::WindowText(hintBox) == hint, "custom hint takes precedence after frontend cursor handling");
                SendMessageW(control, WM_MOUSELEAVE, 0, 0);
                Check(EO::WindowText(hintBox).empty(), "leaving an extended control clears its hint");
            }
        for (HWND label : { editor->labels[index], editor->readouts[index] })
            if (label)
            {
                SetWindowTextW(hintBox, L"Native background hint");
                SendMessageW(label, WM_MOUSEMOVE, 0, MAKELPARAM(1, 1));
                SendMessageW(label, WM_SETCURSOR, reinterpret_cast<WPARAM>(label), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
                Check(EO::WindowText(hintBox) == L"Native background hint",
                    "slider caption and readout do not display option hints");
                DWORD_PTR reference = 0;
                Check(!GetWindowSubclass(label, EO::HintProc, 1, &reference), "static slider text has no hint handler");
            }
    }
    SetWindowTextW(hintBox, L"Native background hint");
    RECT groupBounds{};
    GetWindowRect(editor->group, &groupBounds);
    POINT groupPoint{ groupBounds.left + 5, groupBounds.top + 5 };
    Check(PointerTarget(window, groupPoint) == window, "group background passes hits to page");
    ScreenToClient(window, &groupPoint);
    SendMessageW(window, WM_MOUSEMOVE, 0, MAKELPARAM(groupPoint.x, groupPoint.y));
    Check(EO::WindowText(hintBox) == L"Native background hint", "main group does not replace hint box text");
    DWORD_PTR groupHintReference = 0;
    Check(!GetWindowSubclass(editor->group, EO::HintProc, 1, &groupHintReference), "main group has no hint handler");
    SendMessageW(window, WM_MOUSELEAVE, 0, 0);
    Check(SendMessageW(editor->group, WM_NCHITTEST, 0, 0) == HTTRANSPARENT, "group retains mouse transparency");
    for (HWND control : { editor->labels[EO::ToIndex(EO::OptionIndex::SuperShopperCrates)], editor->readouts[EO::ToIndex(EO::OptionIndex::SuperShopperCrates)] })
    {
        RECT bounds{};
        GetWindowRect(control, &bounds);
        Check(SendMessageW(control, WM_NCHITTEST, 0, MAKELPARAM(bounds.left + 1, bounds.top + 1)) == HTTRANSPARENT,
            "slider title and readout pass mouse hits to the page like ordinary static labels");
    }
    SendMessageW(editor->controls[0], WM_MOUSEMOVE, 0, 0);
    SendMessageW(editor->controls[1], WM_MOUSEMOVE, 0, 0);
    SendMessageW(editor->controls[0], WM_MOUSELEAVE, 0, 0);
    Check(EO::WindowText(hintBox) == EO::strings.hintHighJump, "late leave does not erase next extended hint");
    SetWindowTextW(hintBox, L"Next native hint");
    SendMessageW(editor->controls[1], WM_MOUSELEAVE, 0, 0);
    Check(EO::WindowText(hintBox) == L"Next native hint", "leave does not erase a native control hint");
    Check(EO::WindowText(unrelated1003) == L"Unrelated button", "hint lookup does not write to other controls with ID 1003");
    SendMessageW(editor->controls[0], WM_MOUSEMOVE, 0, 0);
    for (const char* language : { "de", "en" })
    {
        EO::SetLanguage(language);
        Check(EO::WindowText(hintBox) == EO::strings.hintGodMode, "language change refreshes currently hovered customized hint");
        wchar_t text[150]{};
        GetWindowTextW(editor->group, text, 150);
        Check(text == EO::strings.strExtendedOptions, "language updates attached group title");
        for (const auto& option : EO::Options)
        {
            const size_t index = EO::ToIndex(option.index);
            GetWindowTextW(option.IsSlider() ? editor->labels[index] : editor->controls[index], text, 150);
            Check(text == *option.label, "language updates attached checkbox and slider captions");
            Check(option.IsSlider() ? SendMessageW(editor->controls[index], TBM_GETPOS, 0, 0) == beforeLanguageChange[index] :
                SendMessageW(editor->controls[index], BM_GETCHECK, 0, 0) == (beforeLanguageChange[index] ? BST_CHECKED : BST_UNCHECKED),
                "language change preserves control state");
        }
    }
    Check(EO::values == beforeLanguageChange && edits == editsBeforeLanguageChange,
        "language refresh does not edit the scheme");
    SendMessageW(editor->controls[0], WM_MOUSELEAVE, 0, 0);
    puts("PASS: checkbox/trackbar hints in box 1003, passive slider labels/readouts, shared herd hints and hover transitions");
    for (const auto& option : EO::Options)
    {
        const size_t index = EO::ToIndex(option.index);
        const HWND control = editor->controls[index];
        POINT click{ 5, 5 };
        POINT release = click;
        if (option.IsSlider())
        {
            RECT channel{}, thumb{};
            SendMessageW(control, TBM_GETCHANNELRECT, 0, reinterpret_cast<LPARAM>(&channel));
            SendMessageW(control, TBM_GETTHUMBRECT, 0, reinterpret_cast<LPARAM>(&thumb));
            click = POINT{ (thumb.left + thumb.right) / 2, (thumb.top + thumb.bottom) / 2 };
            release = POINT{ channel.left + 2, click.y };
        }
        POINT screen = click;
        ClientToScreen(control, &screen);
        const HWND target = PointerTarget(window, screen);
        Check(target == control, "pointer reaches checkbox and slider at click point");
        const auto beforeClick = EO::values;
        SendMessageW(target, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(click.x, click.y));
        if (option.IsSlider()) SendMessageW(target, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(release.x, release.y));
        SendMessageW(target, WM_LBUTTONUP, 0, MAKELPARAM(release.x, release.y));
        if (!(option.IsSlider() ? EO::values[index] < beforeClick[index] : EO::values[index] == 1 - beforeClick[index]))
            fprintf(stderr, "Mouse click failed at option %zu: before %u after %u, check/position %lld, visible %d, enabled %d\n",
                index, beforeClick[index], EO::values[index], static_cast<long long>(SendMessageW(control,
                    option.IsSlider() ? TBM_GETPOS : BM_GETCHECK, 0, 0)), IsWindowVisible(control), IsWindowEnabled(control));
        Check(option.IsSlider() ? EO::values[index] < beforeClick[index] : EO::values[index] == 1 - beforeClick[index],
            "hit-tested mouse click edits every checkbox and slider");
        for (size_t other = 0; other < EO::OptionCount; ++other)
            if (other != index) Check(EO::values[other] == beforeClick[other], "pointer click edits only selected option");
        EO::values = beforeClick;
        EO::RefreshEditors();
    }
    puts("PASS: mouse hit testing and button input reach all 27 options with group transparent and behind controls");
    const int editsBeforeSave = edits;
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
    Check(edits == editsBeforeSave, "loading scheme does not mark it User defined");
    EO::ResetDefault();
    for (size_t i = 0; i < EO::OptionCount; ++i)
        if (EO::Options[i].IsSlider())
        {
            Check(SendMessageW(editor->controls[i], TBM_GETPOS, 0, 0) == 0, "Default resets numeric slider");
            wchar_t text[100]{};
            GetWindowTextW(editor->readouts[i], text, 100);
            Check(wcscmp(text, i == EO::ToIndex(EO::OptionIndex::SuperShopperCrates) ? L"No" : L"Default") == 0, "Default restores special zero label");
        }
        else Check(SendMessageW(editor->controls[i], BM_GETCHECK, 0, 0) == BST_UNCHECKED, "Default clears visible checkbox");
    EO::strings.hintGodMode = L"Closing hint";
    SendMessageW(editor->controls[0], WM_MOUSEMOVE, 0, 0);
    DestroyWindow(window);
    Check(EO::WindowText(hintBox).empty(), "destroying hovered editor clears its hint without a dangling context");
    EO::SetLanguage("en");
    Check(EO::editors.empty() && !(GetWindowLongPtrW(container, GWL_EXSTYLE) & WS_EX_CONTROLPARENT), "editor cleanup restores ancestor styles");

    // Move every descriptor to a different visual position while preserving
    // its fixed index. Exercise actual control notifications in this layout.
    std::array<EO::Option, EO::OptionCount> moved{};
    std::copy(EO::Options, EO::Options + EO::OptionCount, moved.begin());
    std::rotate(moved.begin(), moved.begin() + 1, moved.begin() + 14);
    std::rotate(moved.begin() + 14, moved.begin() + 15, moved.end());
    int row[2]{};
    for (size_t position = 0; position < moved.size(); ++position)
    {
        auto& option = moved[position];
        option.column = position < 14 ? 0 : 1;
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
    EO::languageResources = previousResources;
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

// Execute the supplied frontend's formatter, including its original CString
// branch and native skin dispatch. Only its CString library dependencies are
// replaced in this private mapping; no frontend initialization is run.
static HMODULE repeatResources;
static int liveNativeStrings;
static void* __fastcall NativeStringCtor(void** self, void*)
{
    *self = new std::string;
    ++liveNativeStrings;
    return self;
}
static void __fastcall NativeStringDtor(void** self, void*)
{
    delete static_cast<std::string*>(*self);
    --liveNativeStrings;
}
static void __cdecl NativeStringFormat(void** self, const char*, int value)
{
    *static_cast<std::string*>(*self) = std::to_string(value);
}
static BOOL __fastcall NativeStringLoad(void** self, void*, UINT id)
{
    char text[256]{};
    const int length = LoadStringA(repeatResources, id, text, sizeof(text));
    *static_cast<std::string*>(*self) = text;
    return length != 0;
}
static const char* __fastcall NativeStringText(void** self, void*)
{
    return static_cast<std::string*>(*self)->c_str();
}
static void* __fastcall NativeStringAppend(void** self, void*, void** other)
{
    *static_cast<std::string*>(*self) += *static_cast<std::string*>(*other);
    return self;
}
struct NativeSkinFixture { void** vtable; HWND window; };
struct NativeControlFixture { std::array<BYTE, 0x40> object{}; NativeSkinFixture skin{}; int id; };
static NativeControlFixture repeatControls[4];
static void* __fastcall SkinGetControl(void*, void*, int id)
{
    for (auto& control : repeatControls) if (control.id == id) return control.object.data();
    return nullptr;
}
static BOOL __fastcall SkinSetText(NativeSkinFixture* self, void*, const char* text)
{
    return SetWindowTextA(self->window, text);
}
static void PatchPrivateCode(BYTE* target, const void* replacement)
{
    DWORD old;
    Check(VirtualProtect(target, 5, PAGE_EXECUTE_READWRITE, &old) != 0, "make private test dependency writable");
    target[0] = 0xe9;
    *reinterpret_cast<int32_t*>(target + 1) = static_cast<int32_t>(static_cast<const BYTE*>(replacement) - target - 5);
    VirtualProtect(target, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), target, 5);
}
static void* repeatReadSite;
__declspec(naked) static void ReturnFromReadSite()
{
    __asm {
        mov esp, ebp
        pop ebp
        ret
    }
}
__declspec(naked) static int CallRepeatReadSite(void*, int*)
{
    __asm {
        push ebp
        mov ebp, esp
        push dword ptr [ebp + 12]
        push 2219
        mov ecx, dword ptr [ebp + 8]
        jmp dword ptr [repeatReadSite]
    }
}
static void RepeatSwingsTests(BYTE* frontend, const char* path)
{
    repeatResources = LoadLibraryExA(path, nullptr, LOAD_LIBRARY_AS_DATAFILE);
    HRSRC resource = FindResourceW(repeatResources, MAKEINTRESOURCEW(154), MAKEINTRESOURCEW(5));
    Check(resource != nullptr, "load Repeat swings dialog resource");
    auto layout = static_cast<const DLGTEMPLATE*>(LockResource(LoadResource(repeatResources, resource)));
    HWND parent = CreateWindowW(L"STATIC", L"Repeat swings fixture", WS_OVERLAPPEDWINDOW,
        0, 0, 700, 500, nullptr, nullptr, nullptr, nullptr);
    HWND dialog = CreateDialogIndirectParamW(repeatResources, layout, parent, DialogProc, 0);
    Check(dialog != nullptr, "create real Repeat swings slider and readouts");
    void* vtable[35]{};
    vtable[0x78 / 4] = reinterpret_cast<void*>(SkinGetControl);
    vtable[0x88 / 4] = reinterpret_cast<void*>(SkinSetText);
    NativeSkinFixture pageSkin{ vtable, dialog };
    std::array<BYTE, 0x40> page{};
    *reinterpret_cast<void**>(page.data() + 0x34) = &pageSkin;
    const int ids[] = { 2211, 2215, 2219, 2218 };
    for (size_t i = 0; i < 4; ++i)
    {
        auto& control = repeatControls[i];
        control.id = ids[i];
        control.skin = { vtable, GetDlgItem(dialog, ids[i]) };
        Check(control.skin.window != nullptr, "native slider/readout exists");
        *reinterpret_cast<HWND*>(control.object.data() + 0x1c) = control.skin.window;
        *reinterpret_cast<void**>(control.object.data() + 0x38) = &control.skin;
    }
    PatchPrivateCode(frontend + 0xc5312, reinterpret_cast<void*>(NativeStringCtor));
    PatchPrivateCode(frontend + 0xc545d, reinterpret_cast<void*>(NativeStringDtor));
    PatchPrivateCode(frontend + 0xb92c4, reinterpret_cast<void*>(NativeStringFormat));
    PatchPrivateCode(frontend + 0xc5b02, reinterpret_cast<void*>(NativeStringLoad));
    PatchPrivateCode(frontend + 0x76c0, reinterpret_cast<void*>(NativeStringText));
    PatchPrivateCode(frontend + 0xc5885, reinterpret_cast<void*>(NativeStringAppend));
    using Formatter = void (__thiscall*)(void*, int, int);
    const auto format = reinterpret_cast<Formatter>(frontend + 0x57f84);
    char random[256]{}, unlimited[256]{};
    Check(LoadStringA(repeatResources, 158, random, sizeof(random)) != 0 &&
        LoadStringA(repeatResources, 4950, unlimited, sizeof(unlimited)) != 0, "load both native sentinel strings");
    format(page.data(), -1, 2210);
    char text[256]{};
    GetWindowTextA(GetDlgItem(dialog, 2211), text, sizeof(text));
    Check(strcmp(text, random) == 0, "native Random sentinel still uses string 158");
    std::array<BYTE, 0xa4> editor{};
    *reinterpret_cast<void**>(editor.data() + 0xa0) = page.data();
    HWND slider = GetDlgItem(dialog, 2218);
    SendMessageW(slider, TBM_SETRANGEMIN, FALSE, -1);
    SendMessageW(slider, TBM_SETRANGEMAX, FALSE, 100);
    const auto continuation = EO::afterReadRepeatSwings;
    Check(continuation == frontend + 0x592bb, "Repeat swings save resumes at the next native option");
    EO::afterReadRepeatSwings = reinterpret_cast<void*>(ReturnFromReadSite);
    repeatReadSite = frontend + 0x592b6;
    for (int pass = 0; pass < 3; ++pass)
        for (int value = -1; value <= 100; ++value)
        {
            SendMessageW(slider, TBM_SETPOS, TRUE, value);
            format(page.data(), value, 2218);
            GetWindowTextA(GetDlgItem(dialog, 2219), text, sizeof(text));
            Check(std::string(text) == (value == -1 ? std::string(unlimited) : std::to_string(value)),
                "real native formatter displays Unlimited only at -1, numbers otherwise");
            int stored = -999;
            Check(CallRepeatReadSite(editor.data(), &stored) == value && stored == value,
                "patched native save call reads slider position, preserving -1 despite its text label");
            format(page.data(), 7, 2214);
            GetWindowTextA(GetDlgItem(dialog, 2215), text, sizeof(text));
            Check(strcmp(text, "7") == 0, "unrelated native readout still displays its numeric value");
            GetWindowTextA(GetDlgItem(dialog, 2211), text, sizeof(text));
            Check(strcmp(text, random) == 0 && liveNativeStrings == 0,
                "other text stays intact and native CString construction/destruction remains balanced");
        }
    EO::afterReadRepeatSwings = continuation;
    DestroyWindow(parent);
    FreeLibrary(repeatResources);
    puts("PASS: actual native formatter/skin path, Random unchanged, Repeat swings -1 through 100 and native save call");
}

struct NativeFindData
{
    uint32_t attributes;
    int32_t created, accessed, modified;
    uint32_t size;
    char name[260];
};
static_assert(offsetof(NativeFindData, size) == 16 && offsetof(NativeFindData, name) == 20,
    "inspected frontend uses the 32-bit CRT find-data layout");
static std::string schemeScanDirectory;
static int schemeScanPasses;
static void CopyFindData(NativeFindData* target, const WIN32_FIND_DATAA& source)
{
    *target = {};
    target->attributes = source.dwFileAttributes;
    target->size = source.nFileSizeLow;
    strcpy_s(target->name, source.cFileName);
}
static intptr_t __cdecl NativeFindFirst(const char* pattern, NativeFindData* result)
{
    ++schemeScanPasses;
    WIN32_FIND_DATAA found{};
    const HANDLE handle = FindFirstFileA((schemeScanDirectory + "/" + pattern).c_str(), &found);
    if (handle != INVALID_HANDLE_VALUE) CopyFindData(result, found);
    return reinterpret_cast<intptr_t>(handle);
}
static int __cdecl NativeFindNext(intptr_t handle, NativeFindData* result)
{
    WIN32_FIND_DATAA found{};
    if (!FindNextFileA(reinterpret_cast<HANDLE>(handle), &found)) return -1;
    CopyFindData(result, found);
    return 0;
}
static int __cdecl NativeFindClose(intptr_t handle) { return FindClose(reinterpret_cast<HANDLE>(handle)) ? 0 : -1; }
static int __cdecl NativeScanChdir(const char*) { return 0; }
static void __cdecl NativeSplitPath(const char* path, char*, char*, char* name, char*)
{
    _splitpath_s(path, nullptr, 0, nullptr, 0, name, 256, nullptr, 0);
}
static int __fastcall NativeAddScheme(void* list, void*, const char* name)
{
    HWND window = *reinterpret_cast<HWND*>(static_cast<BYTE*>(list) + 0x1c);
    return static_cast<int>(SendMessageA(window, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name)));
}
static std::vector<std::string> SchemeNames(HWND list)
{
    std::vector<std::string> names;
    for (int i = 0; i < SendMessageA(list, CB_GETCOUNT, 0, 0); ++i)
    {
        char name[256]{};
        SendMessageA(list, CB_GETLBTEXT, i, reinterpret_cast<LPARAM>(name));
        names.emplace_back(name);
    }
    return names;
}
static void SchemeDiscoveryTests(BYTE* frontend)
{
    // Exercise the actual native scanner and MinHook trampoline, backed by real
    // filesystem enumeration and combo boxes. Stub only runtime dependencies
    // that require the frontend's uninitialized CRT/global working directory.
    const char* fixture = "Release/option-discovery-fixture";
    CreateDirectoryA(fixture, nullptr);
    char absolute[MAX_PATH]{};
    Check(GetFullPathNameA(fixture, MAX_PATH, absolute, nullptr) != 0, "resolve scheme fixture path");
    schemeScanDirectory = absolute;
    const auto writeFixture = [](const char* name, size_t size, const EO::Extension* extension = nullptr)
    {
        auto stream = static_cast<FILE*>(Open((schemeScanDirectory + "/" + name).c_str(), "wb"));
        Check(stream != nullptr, "create scheme discovery fixture");
        std::vector<BYTE> bytes(size);
        if (size >= 7) memcpy(bytes.data(), "OPTIONS", 7);
        if (extension) memcpy(bytes.data() + 135, extension, sizeof(*extension));
        Check(fwrite(bytes.data(), 1, size, stream) == size && fclose(stream) == 0, "write scheme discovery fixture");
    };
    EO::values.fill(0);
    const auto zero = EO::CurrentExtension();
    EO::values = Pattern(EO::OptionCount);
    const auto populated = EO::CurrentExtension();
    writeFixture("legacy.opt", 135);
    writeFixture("all-zero.opt", 166, &zero);
    writeFixture("populated.opt", 166, &populated);
    writeFixture("truncated.opt", 165);
    writeFixture("oversized.opt", 167);
    writeFixture("future.opt", 366, &populated);
    writeFixture("empty.opt", 0);
    writeFixture("unrelated.wep", 135);
    CreateDirectoryA((schemeScanDirectory + "/directory.opt").c_str(), nullptr);
    PatchPrivateCode(frontend + 0x96200, reinterpret_cast<void*>(NativeFindFirst));
    PatchPrivateCode(frontend + 0x96330, reinterpret_cast<void*>(NativeFindNext));
    PatchPrivateCode(frontend + 0x96450, reinterpret_cast<void*>(NativeFindClose));
    PatchPrivateCode(frontend + 0x97070, reinterpret_cast<void*>(NativeScanChdir));
    PatchPrivateCode(frontend + 0x977a0, reinterpret_cast<void*>(NativeSplitPath));
    PatchPrivateCode(frontend + 0xf270, reinterpret_cast<void*>(NativeAddScheme));
    const auto scan = reinterpret_cast<EO::ScanSchemes>(frontend + 0x1e6fe);
    HWND parent = CreateWindowW(L"STATIC", L"Scheme discovery fixture", WS_OVERLAPPEDWINDOW,
        0, 0, 400, 300, nullptr, nullptr, nullptr, nullptr);
    std::array<BYTE, 0x40> scanner{};
    for (int dropdown = 0; dropdown < 2; ++dropdown)
    {
        HWND list = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | CBS_DROPDOWNLIST | CBS_SORT,
            0, 0, 300, 200, parent, nullptr, nullptr, nullptr);
        Check(list != nullptr, "create scheme dropdown");
        std::array<BYTE, 0x40> object{};
        *reinterpret_cast<HWND*>(object.data() + 0x1c) = list;
        for (int restart = 0; restart < 3; ++restart)
        {
            SendMessageA(list, CB_RESETCONTENT, 0, 0);
            schemeScanPasses = 0;
            scan(scanner.data(), object.data(), restart == 1 ? "*.OPT" : "*.opt", 0, 135, "Options");
            Check(schemeScanPasses == 1 && SchemeNames(list) == std::vector<std::string>{
                "all-zero", "empty", "future", "legacy", "oversized", "populated", "truncated" },
                "fresh dropdown scans include every .opt regardless of size, exclude other extensions and directories");
        }
        SendMessageA(list, CB_RESETCONTENT, 0, 0);
        schemeScanPasses = 0;
        scan(scanner.data(), object.data(), "*.wep", 0, 135, "Options");
        Check(schemeScanPasses == 1 && SchemeNames(list) == std::vector<std::string>{ "unrelated" },
            "other file types retain their original discovery rules");
        SendMessageA(list, CB_RESETCONTENT, 0, 0);
        schemeScanPasses = 0;
        scan(scanner.data(), object.data(), "*.opt", 0, 166, "Options");
        Check(schemeScanPasses == 1 && SchemeNames(list) == std::vector<std::string>{
            "all-zero", "empty", "future", "legacy", "oversized", "populated", "truncated" },
            "option discovery does not depend on a hardcoded current or future file size");
        DestroyWindow(list);
    }
    for (const auto& entry : { std::make_pair("all-zero.opt", zero),
        std::make_pair("populated.opt", populated), std::make_pair("future.opt", populated) })
    {
        auto stream = static_cast<FILE*>(Open((schemeScanDirectory + "/" + entry.first).c_str(), "rb"));
        Check(stream != nullptr && fseek(stream, 135, SEEK_SET) == 0, "reopen discovered PLUS scheme");
        EO::values = Pattern(0);
        EO::ReadScheme(frontend + EO::OptionsRva, 128, 1, stream, 1, Read);
        Check(EO::values == entry.second.values, "discovered schemes retain every extended setting on load");
        fclose(stream);
    }
    DestroyWindow(parent);
    for (const char* name : { "legacy.opt", "all-zero.opt", "populated.opt", "truncated.opt", "oversized.opt",
        "future.opt", "empty.opt", "unrelated.wep" })
        DeleteFileA((schemeScanDirectory + "/" + name).c_str());
    RemoveDirectoryA((schemeScanDirectory + "/directory.opt").c_str());
    RemoveDirectoryA(schemeScanDirectory.c_str());
    Check(liveNativeStrings == 0, "native discovery CString lifetime remains balanced");
    puts("PASS: actual native scheme scanner lists all .opt sizes in both fresh dropdowns, including over 200 bytes, and preserves extended values");
}

static HWND defaultDialog;
static void* defaultDialogObject;
static EO::MarkEdited nativeDefaultButton;
static int defaultClicks;
static std::array<BYTE, 0x40> defaultControl{};
static INT_PTR CALLBACK DefaultDialogProc(HWND, UINT message, WPARAM wParam, LPARAM)
{
    if (message == WM_COMMAND && LOWORD(wParam) == 2102 && HIWORD(wParam) == BN_CLICKED)
    {
        ++defaultClicks;
        nativeDefaultButton(defaultDialogObject);
        return TRUE;
    }
    return FALSE;
}
static void* __fastcall DefaultGetControl(void*, void*, int id)
{
    *reinterpret_cast<HWND*>(defaultControl.data() + 0x1c) = GetDlgItem(defaultDialog, id);
    return defaultControl.data();
}
static BOOL __fastcall DefaultIsEnabled(BYTE* object, void*)
{ return IsWindowEnabled(*reinterpret_cast<HWND*>(object + 0x1c)); }
static BOOL __fastcall DefaultEnable(BYTE* object, void*, BOOL enable)
{ return EnableWindow(*reinterpret_cast<HWND*>(object + 0x1c), enable); }
static int __fastcall DefaultFindScheme(BYTE* object, void*, int start, const char* text)
{ return static_cast<int>(SendMessageA(*reinterpret_cast<HWND*>(object + 0x1c), CB_FINDSTRINGEXACT, start, reinterpret_cast<LPARAM>(text))); }
static int __fastcall DefaultSelectScheme(BYTE* object, void*, int index)
{ return static_cast<int>(SendMessageA(*reinterpret_cast<HWND*>(object + 0x1c), CB_SETCURSEL, index, 0)); }
static void __fastcall DefaultRefreshNative(void*, void*) {}
static void* __cdecl DefaultMainWindow() { return defaultControl.data(); }
static void* __cdecl DefaultCopy(void* destination, const void* source, size_t size)
{ return memcpy(destination, source, size); }
static void PrivatePointer(BYTE* target, const void* pointer)
{
    DWORD old;
    Check(VirtualProtect(target, 4, PAGE_EXECUTE_READWRITE, &old) != FALSE, "relocate private Default button operand");
    *reinterpret_cast<const void**>(target) = pointer;
    VirtualProtect(target, 4, old, &old);
}
static void DefaultButtonTests(BYTE* frontend, const char* path)
{
    // Run the actual Default button method, not ResetDefault directly. MFC/CRT
    // dependencies are adapted to real dialog controls in this private image.
    repeatResources = LoadLibraryExA(path, nullptr, LOAD_LIBRARY_AS_DATAFILE);
    const auto layout = [&](int id) {
        HRSRC resource = FindResourceW(repeatResources, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(5));
        Check(resource != nullptr, "load real options dialog resource");
        return static_cast<const DLGTEMPLATE*>(LockResource(LoadResource(repeatResources, resource)));
    };
    HWND root = CreateWindowW(L"STATIC", L"Default button fixture", WS_OVERLAPPEDWINDOW,
        0, 0, 700, 500, nullptr, nullptr, nullptr, nullptr);
    defaultDialog = CreateDialogIndirectParamW(repeatResources, layout(155), root, DefaultDialogProc, 0);
    HWND form = CreateDialogIndirectParamW(repeatResources, layout(154), defaultDialog, DialogProc, 0);
    Check(defaultDialog && form && GetDlgItem(defaultDialog, 2102), "create real Default button and options form");
    std::array<BYTE, 0x200> mainObject{};
    *reinterpret_cast<HWND*>(mainObject.data() + 0x13c + 0x1c) = GetDlgItem(defaultDialog, 2102);
    *reinterpret_cast<HWND*>(mainObject.data() + 0x188 + 0x1c) = GetDlgItem(defaultDialog, 2121);
    defaultDialogObject = mainObject.data();
    nativeDefaultButton = reinterpret_cast<EO::MarkEdited>(frontend + 0x5968f);
    defaultClicks = 0;
    PatchPrivateCode(frontend + 0x96980, reinterpret_cast<void*>(DefaultCopy));
    PatchPrivateCode(frontend + 0x1a28, reinterpret_cast<void*>(DefaultRefreshNative));
    PatchPrivateCode(frontend + 0xc47a6, reinterpret_cast<void*>(DefaultGetControl));
    PatchPrivateCode(frontend + 0xc4bb5, reinterpret_cast<void*>(DefaultIsEnabled));
    PatchPrivateCode(frontend + 0xc4bd0, reinterpret_cast<void*>(DefaultEnable));
    PatchPrivateCode(frontend + 0x3878, reinterpret_cast<void*>(DefaultFindScheme));
    PatchPrivateCode(frontend + 0x36e3, reinterpret_cast<void*>(DefaultSelectScheme));
    PatchPrivateCode(frontend + 0x3cf6, reinterpret_cast<void*>(DefaultMainWindow));
    PrivatePointer(frontend + 0x5969b, frontend + 0x186354);
    PrivatePointer(frontend + 0x596a5, frontend + 0x176a40);
    PrivatePointer(frontend + 0x596b0, frontend + 0x18645f);
    PrivatePointer(frontend + 0x59742, frontend + 0x186354);
    PrivatePointer(frontend + 0x5973d, frontend + 0x1bc550);
    PrivatePointer(frontend + 0x1bc550, reinterpret_cast<void*>(SendMessageA));
    DWORD old;
    Check(VirtualProtect(frontend + 0x186354, 0x110, PAGE_READWRITE, &old) != FALSE, "initialize private native defaults");
    NativeStringCtor(reinterpret_cast<void**>(frontend + 0x186354), nullptr);
    std::array<BYTE, 128> defaults{};
    for (size_t i = 0; i < defaults.size(); ++i) defaults[i] = static_cast<BYTE>(i ^ 0x39);
    memcpy(frontend + 0x186358, defaults.data(), defaults.size());
    char defaultName[256]{};
    Check(LoadStringA(repeatResources, 99, defaultName, sizeof(defaultName)) != 0, "load native Default scheme name");
    HWND combo = GetDlgItem(defaultDialog, 2121);
    SendMessageA(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(defaultName));
    SendMessageA(combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>("Edited scheme"));
    std::array<BYTE, 0xa0> pageObject{};
    *reinterpret_cast<HWND*>(pageObject.data() + 0x1c) = form;
    RECT native{ 0, 0, 386, 468 };
    MapDialogRect(form, &native);
    *reinterpret_cast<SIZE*>(pageObject.data() + 0x44) = { native.right, native.bottom };
    EO::setScrollSizes = reinterpret_cast<EO::SetScrollSizes>(Scroll);
    EO::markEdited = reinterpret_cast<EO::MarkEdited>(Mark);
    EO::SetLanguage("en");
    Check(EO::AttachEditor(pageObject.data()), "attach extended controls for native Default button regression");
    auto editor = static_cast<EO::Editor*>(GetPropW(form, EO::ContextProperty));
    for (int click = 0; click < 2; ++click)
    {
        EO::values = Pattern(EO::OptionCount);
        EO::RefreshEditors();
        memset(frontend + EO::OptionsRva, 0xcd, 128);
        frontend[0x18645f] = 1;
        SendMessageA(combo, CB_SETCURSEL, 1, 0);
        EnableWindow(GetDlgItem(defaultDialog, 2102), TRUE);
        EnableWindow(GetDlgItem(defaultDialog, 2120), TRUE);
        EnableWindow(GetDlgItem(defaultDialog, 2235), TRUE);
        SendMessageW(GetDlgItem(defaultDialog, 2102), BM_CLICK, 0, 0);
        Check(defaultClicks == click + 1 && EO::values == Pattern(EO::OptionCount + 1),
            "actual Default button path clears all extended values");
        Check(memcmp(frontend + EO::OptionsRva, defaults.data(), defaults.size()) == 0 && frontend[0x18645f] == 0,
            "native Default copy and modified flag retain their original behavior");
        Check(SendMessageA(combo, CB_GETCURSEL, 0, 0) == 0 && !IsWindowEnabled(GetDlgItem(defaultDialog, 2102)),
            "native Default button selects Default and disables itself");
        for (const auto& option : EO::Options)
        {
            const size_t i = EO::ToIndex(option.index);
            Check(option.IsSlider() ? SendMessageW(editor->controls[i], TBM_GETPOS, 0, 0) == 0 :
                SendMessageW(editor->controls[i], BM_GETCHECK, 0, 0) == BST_UNCHECKED,
                "native Default button refreshes all visible extended controls to zero");
            if (option.IsSlider()) Check(EO::WindowText(editor->readouts[i]) == EO::ValueText(option),
                "native Default button restores special zero captions");
        }
    }
    DestroyWindow(root);
    NativeStringDtor(reinterpret_cast<void**>(frontend + 0x186354), nullptr);
    VirtualProtect(frontend + 0x186354, 0x110, old, &old);
    FreeLibrary(repeatResources);
    Check(EO::editors.empty() && liveNativeStrings == 0, "Default fixture releases editor and native strings");
    puts("PASS: real Default button notification runs native method and resets native/extended values, controls and scheme selection");
}

static void HookTests(const char* path)
{
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    Check(file != INVALID_HANDLE_VALUE, "open frontend for private hook validation");
    HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr);
    BYTE* frontend = static_cast<BYTE*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));
    Check(frontend != nullptr, "map frontend without running it");
    const std::array<size_t, 17> sites{ 0x57d20, 0x97780, 0x97500, 0x9cdc, 0x9ff8, 0xa5d4,
        0x31cd9, 0x392c0, 0x586a3, 0x5899e, 0x59611, 0x59e3d, 0x580de, 0x580f9, 0x592b6, 0x1e6fe, 0x1e22b };
    // SEC_IMAGE does not run loader relocations; relocate the inspected operands
    // in this private mapping exactly as a relocated executable's loader would.
    for (size_t i = 3; i < 12; ++i)
    {
        DWORD old;
        VirtualProtect(frontend + sites[i], 17, PAGE_EXECUTE_READWRITE, &old);
        *reinterpret_cast<uint32_t*>(frontend + sites[i] + 6) = reinterpret_cast<uint32_t>(frontend + 0x186358);
        *reinterpret_cast<uint32_t*>(frontend + sites[i] + 11) = reinterpret_cast<uint32_t>(frontend + EO::OptionsRva);
        VirtualProtect(frontend + sites[i], 17, old, &old);
    }
    DWORD operandOld;
    VirtualProtect(frontend + 0x592aa, 4, PAGE_EXECUTE_READWRITE, &operandOld);
    *reinterpret_cast<uint32_t*>(frontend + 0x592aa) = reinterpret_cast<uint32_t>(frontend + 0x18640c);
    VirtualProtect(frontend + 0x592aa, 4, operandOld, &operandOld);
    std::array<std::array<BYTE, 17>, 17> before{};
    for (size_t i = 0; i < sites.size(); ++i) memcpy(before[i].data(), frontend + sites[i], before[i].size());
    Check(MH_Initialize() == MH_OK, "initialize MinHook");
    Check(SW::InstallInImage(frontend), "install shared CRT/network hooks");
    for (size_t corrupted : { 11u, 12u, 13u, 14u, 15u, 16u })
    {
        DWORD old;
        VirtualProtect(frontend + sites[corrupted], 17, PAGE_EXECUTE_READWRITE, &old);
        frontend[sites[corrupted]] = 0x90;
        Check(!EO::InstallInImage(frontend) && !EO::enabled, "unsupported Default, Repeat swings or scheme scanner signature rejects hooks");
        frontend[sites[corrupted]] = before[corrupted][0];
        VirtualProtect(frontend + sites[corrupted], 17, old, &old);
        for (size_t i = 0; i < sites.size(); ++i)
            Check(memcmp(before[i].data(), frontend + sites[i], before[i].size()) == 0,
                "signature rejection leaves all extended hook sites untouched");
    }
    Check(EO::InstallInImage(frontend), "install all sixteen extended option hooks on supplied frontend");
    Check(NetworkTeams::InstallInImage(frontend), "install network-team hooks alongside extended options");
    Check(ColourMaps::InstallInImage(frontend), "colour map hooks coexist with shared and extended option hooks");
    for (size_t i = 0; i < sites.size(); ++i)
        Check(memcmp(before[i].data(), frontend + sites[i], before[i].size()) != 0, "extended hook enabled");
    RepeatSwingsTests(frontend, path);
    SchemeDiscoveryTests(frontend);
    DefaultButtonTests(frontend, path);
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
        Check(argc > 1, "supply frontend path for real resource/hook validation");
        if (argc > 2 && strcmp(argv[2], "--hooks-only") == 0)
        {
            HookTests(argv[1]);
            return 0;
        }
        LanguageTests(argv[1]);
        SchemeTests();
        LaunchTests();
        NetworkTests();
        DefaultTests();
        EditorTests(argv[1]);
        HookTests(argv[1]);
        return 0;
    }
    catch (const std::exception& error) { fprintf(stderr, "FAIL: %s\n", error.what()); return 1; }
}
