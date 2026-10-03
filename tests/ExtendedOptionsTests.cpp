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
    std::wstring utilityNames;
    const UINT utilityIds[] = { 4934, 4923, 4922, 4926 };
    for (size_t i = 0; i < 4; ++i)
    {
        wchar_t text[256]{};
        Check(LoadStringW(module, utilityIds[i], text, 256) != 0, "requested utilities resource string exists");
        if (i) utilityNames += L", ";
        utilityNames += text;
    }
    utilityNames += L".";
    Check(EO::strings.hintUtilitiesDontEndTurn.find(utilityNames) != std::wstring::npos,
        "English utilities hint lists all four native weapon names");
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
        bool extended;
    };
    const ExpectedTranslation expectedTranslations[] = {
        { "pt-br", L"Aqua Carneiro", L"Rebanho/manada", L"Op\u00e7\u00f5es de Jogo Estendidas", true },
        { "nl", L"Waterschaap", L"Kudde", L"Uitgebreide opties", true },
        { "en", L"Aqua Sheep", L"Herd weapon", englishGroupTitle.c_str(), true },
        { "fr", L"Mouton aquatique", L"Troupeau", L"Options de jeu \u00e9tendues", true },
        { "de", L"Aquaschaf", L"Herde", L"Erweiterte Spieloptionen", true },
        { "it", L"Pecora acquatica", L"Mandria", L"Opzioni estese", true },
        { "pt", L"Ovelha Aqu\u00e1tica", L"Manada", L"Op\u00e7\u00f5es de jogo alargadas", true },
        { "ru", L"\u0410\u043a\u0432\u0430-\u043e\u0432\u0446\u0430", L"\u0421\u0442\u0430\u0434\u043e", L"\u0420\u0430\u0441\u0448\u0438\u0440\u0435\u043d\u043d\u044b\u0435 \u043d\u0430\u0441\u0442\u0440\u043e\u0439\u043a\u0438 \u0438\u0433\u0440\u044b", true },
        { "es", L"Oveja acu\u00e1tica", L"Manada", L"Opciones de Juego Extendidas", true },
        { "es-419", L"Oveja acu\u00e1tica", L"Arma de reba\u00f1o", L"Opciones extendidas", true },
        { "sv", L"Vattenf\u00e5r", L"Hjord", L"Ut\u00f6kade Spelinst\u00e4llningar", true },
    };
    for (const char* language : { "cs", "de", "es", "es-419", "fr", "is", "it", "nl", "pl", "pt", "pt-br",
        "ru", "sv", "zh-Hans", "de\r\n", "\xEF\xBB\xBF" "zh-Hans\r\n" })
    {
        EO::SetLanguage(language);
        const ExpectedTranslation* expected = nullptr;
        const bool keepHints = strcmp(language, "pl") == 0 || strcmp(language, "pt") == 0 ||
            strcmp(language, "pt-br") == 0 || strcmp(language, "zh-Hans") == 0 ||
            strcmp(language, "\xEF\xBB\xBF" "zh-Hans\r\n") == 0;
        if (!keepHints)
            for (const auto& option : EO::Options)
                Check(option.hint->empty(), "all hints are empty outside en/pl/pt/pt-br/zh-Hans");
        for (const auto& entry : expectedTranslations)
            if (strcmp(language, entry.code) == 0 || (strcmp(language, "de\r\n") == 0 && strcmp(entry.code, "de") == 0))
                expected = &entry;
        if (expected)
        {
            Check(EO::strings.strAquaSheep == expected->aquaSheep && EO::strings.strHerd == expected->herd &&
                EO::strings.strExtendedOptions == expected->title,
                "language selection preserves Unicode and translated group titles");
            if (keepHints) Check(!EO::strings.hintGodMode.empty() && !EO::strings.hintAquaSheep.empty(), "Portuguese hints are preserved");
            for (const auto* text : { &EO::strings.strExtendedFusesHerds, &EO::strings.strWeaponsDontEndTurn,
                &EO::strings.strLossOfControlDoesntEndTurn, &EO::strings.strPersistentRope,
                &EO::strings.strCrateRate, &EO::strings.strWormSelectAfterMovement, &EO::strings.strCrateLimit })
                Check(text->empty() != expected->extended, "extended labels are translated in the supplied languages");
            if (!keepHints)
                for (const auto& option : EO::Options)
                    Check(!option.label->empty(), "all option labels are filled in the requested languages");
            else for (auto index : { EO::OptionIndex::HighJump, EO::OptionIndex::SheepHeaven, EO::OptionIndex::SuperShopperCrates,
                EO::OptionIndex::UtilitiesDontEndTurn, EO::OptionIndex::RapidPlay, EO::OptionIndex::IndestructibleTerrain,
                EO::OptionIndex::InvisibleTerrain, EO::OptionIndex::FastCrates, EO::OptionIndex::InstantMines,
                EO::OptionIndex::DisableBackflip, EO::OptionIndex::DisableUnlockedAim })
                Check(EO::FindOption(EO::ToIndex(index))->label->empty() && EO::FindOption(EO::ToIndex(index))->hint->empty(),
                    "excluded Portuguese translations retain blank placeholders");
        }
        else
            for (const auto& option : EO::Options)
                if (strcmp(language, "cs") == 0 && option.index == EO::OptionIndex::IndestructibleTerrain)
                    Check(*option.label == L"Nezni\u010diteln\u00fd ter\u00e9n" && option.hint->empty(),
                        "preserve customized Czech terrain translation");
                else Check(option.hint->empty() && (weaponNames[EO::ToIndex(option.index)].empty() ? option.label->empty() :
                    *option.label == L": " + weaponNames[EO::ToIndex(option.index)]), "remaining untranslated labels stay blank");
        for (const auto& entry : herdIds)
            Check(*EO::FindOption(EO::ToIndex(entry.first))->label == EO::strings.strHerd + L": " + weaponNames[EO::ToIndex(entry.first)],
                "every language composes herd captions with its translated GAME_HERD prefix and native weapon names");
        if (!expected) Check(EO::strings.strExtendedOptions.empty(), "untranslated group title remains blank");
        Check(EO::strings.hintHerd.empty(), "unmapped shared herd hint stays blank");
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
    Check(EO::strings.hintCrateRate.find(L"\"Contagem m\u00e1xima de caixas no mapa no in\u00edcio\"") != std::wstring::npos,
        "preserved Portuguese hint decodes escaped quotes into display text");
    EO::SetLanguage("en");
    puts("PASS: imported WA translations, Unicode, hint composition, English fallback, untranslated placeholders and herd string IDs");
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
        for (HWND control : { editor->controls[EO::ToIndex(option.index)], editor->labels[EO::ToIndex(option.index)],
            editor->readouts[EO::ToIndex(option.index)] })
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
        for (HWND control : { editor->controls[index], editor->labels[index], editor->readouts[index] })
            if (control)
            {
                SendMessageW(control, WM_MOUSEMOVE, 0, MAKELPARAM(1, 1));
                Check(EO::WindowText(hintBox) == hint, "checkbox, trackbar, title and readout show option hint");
                SetWindowTextW(hintBox, L"Native cursor hint");
                SendMessageW(control, WM_SETCURSOR, reinterpret_cast<WPARAM>(control), MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
                Check(EO::WindowText(hintBox) == hint, "custom hint takes precedence after frontend cursor handling");
                SendMessageW(control, WM_MOUSELEAVE, 0, 0);
                Check(EO::WindowText(hintBox).empty(), "leaving an extended control clears its hint");
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
        Check(SendMessageW(control, WM_NCHITTEST, 0, MAKELPARAM(bounds.left + 1, bounds.top + 1)) == HTCLIENT,
            "slider title and readout accept mouse hits for hints");
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
    puts("PASS: persistent hint box 1003, all option surfaces, shared herd hints, language refresh and hover transitions");
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
        Check(argc > 1, "supply frontend path for real resource/hook validation");
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
