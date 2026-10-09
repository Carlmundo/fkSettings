#include <cstdio>
#include <cstdlib>
#include <vector>
#include <stdexcept>
#include "../fkSettings/SecretWeapons.cpp"
#include "../fkSettings/FrontendNetwork.cpp"
namespace FN = FrontendNetwork;

namespace SW = SecretWeapons;

static void Check(bool value, const char* description)
{
    if (!value) throw std::runtime_error(description);
}

static void LanguageTests()
{
    const std::array<const wchar_t*, 8> english{
        L"Salvation Army", L"MB Bomb", L"Sheep Strike", L"Carpet Bomb",
        L"Cloned Sheep", L"Concrete Donkey", L"Nuclear Bomb", L"Magic Bullet" };
    for (const char* language : { "en", "", "unknown", "\xEF\xBB\xBF" "en\r\n" })
    {
        SW::SetLanguage(language);
        for (size_t i = 0; i < english.size(); ++i)
            Check(*SW::Weapons[i].name == english[i], "English and fallback preserve names");
    }
    const std::array<const wchar_t*, 8> chinese{
        L"\u6551\u4e16\u519b", L"MB\u70b8\u5f39", L"\u7ef5\u7f8a\u7a7a\u88ad", L"\u5730\u6bef\u5f0f\u8f70\u70b8",
        L"\u514b\u9686\u7ef5\u7f8a", L"\u6df7\u51dd\u571f\u5927\u7b28\u9a74", L"\u6838\u5f39", L"\u9b54\u672f\u5b50\u5f39" };
    for (const char* language : { "zh-Hans", "\xEF\xBB\xBF" "zh-Hans\r\n" })
    {
        SW::SetLanguage(language);
        for (size_t i = 0; i < chinese.size(); ++i)
            Check(*SW::Weapons[i].name == chinese[i], "Chinese translations preserve all eight weapon names");
    }
    struct Translation { const char* code; const wchar_t* salvationArmy; const wchar_t* magicBullet; };
    const Translation translations[] = {
        { "cs", L"Arm\u00e1da sp\u00e1sy", L"Magick\u00e1 kulka" },
        { "de", L"Heilsarmee", L"Zauberkugel" },
        { "es", L"Ej\u00e9rcito de Salvaci\u00f3n", L"Bala M\u00e1gica" },
        { "es-419", L"Ej\u00e9rcito Salva", L"Bala m\u00e1gica" },
        { "fr", L"Arm\u00e9e du salut", L"Balle magique" },
        { "is", L"Hj\u00e1lpr\u00e6\u00f0isherinn", L"T\u00f6frak\u00fala" },
        { "it", L"Esercito della salvezza", L"Pallottola magica" },
        { "nl", L"Leger des Twijfels", L"Tover Kogel" },
        { "pl", L"Armia Zbawienia", L"Magiczny pocisk" },
        { "pt", L"Ex\u00e9rcito de Resgate", L"Bala M\u00e1gica" },
        { "pt-br", L"Ex\u00e9rcito da Salva\u00e7\u00e3o", L"Bala M\u00e1gica" },
        { "ru", L"\u0410\u0440\u043c\u0438\u044f \u0441\u043f\u0430\u0441\u0435\u043d\u0438\u044f",
            L"\u0412\u043e\u043b\u0448\u0435\u0431\u043d\u0430\u044f \u043f\u0443\u043b\u044f" },
        { "sv", L"Fr\u00e4lsningsarm\u00e9n", L"Magisk Kula" },
    };
    for (const auto& translation : translations)
    {
        SW::SetLanguage(translation.code);
        Check(SW::strings.strSalvationArmy == translation.salvationArmy &&
            SW::strings.strMagicBullet == translation.magicBullet,
            "native menu translations match current names and preserve Unicode");
        for (const auto& weapon : SW::Weapons)
            Check(!weapon.name->empty(), "all eight translated weapon captions are populated");
    }
    SW::SetLanguage(" \tpt\r\n");
    Check(SW::strings.strSheepStrike == L"Ataque Ovelhas", "whitespace selects Portuguese translations");
    SW::SetLanguage("\xEF\xBB\xBF" "fr\r\n");
    Check(SW::strings.strClonedSheep == L"Mouton clon\u00e9", "BOM selects accented French translation");
    SW::SetLanguage("en");
    puts("PASS: native secret weapon translations, Unicode, Chinese names, English fallback and BOM/whitespace handling");
}

static size_t __cdecl TestRead(void* buffer, size_t size, size_t count, void* stream)
{
    return fread(buffer, size, count, static_cast<FILE*>(stream));
}

static size_t __cdecl TestWrite(const void* buffer, size_t size, size_t count, void* stream)
{
    return fwrite(buffer, size, count, static_cast<FILE*>(stream));
}

static void SchemeTests()
{
    auto payload = SW::image + 0x187648;
    for (size_t i = 0; i < SW::NativeSchemePayloadSize; ++i)
        payload[i] = static_cast<BYTE>(i);
    const std::vector<BYTE> expected(payload, payload + SW::NativeSchemePayloadSize);
    SW::originalRead = TestRead;
    SW::originalWrite = TestWrite;
    for (int stock : { 0, 1, 9, 10, 11, 98, 99 })
    {
        FILE* stream = nullptr;
        Check(fopen_s(&stream, "Release/secret-weapons-test.wep", "w+b") == 0, "open scheme fixture");
        BYTE header[24]{};
        fwrite(header, 1, sizeof(header), stream);
        for (size_t i = 0; i < SW::Weapons.size(); ++i)
            SW::secretStocks[i] = (stock + i) % 100;
        const auto stocks = SW::secretStocks;
        Check(SW::WriteFile(payload, SW::WeaponRecordSize, SW::NativeWeaponCount, stream) == 38, "save payload");
        Check(ftell(stream) == 0x1504, "extension file length");
        fflush(stream);
        fseek(stream, 0x14e0, SEEK_SET);
        std::array<BYTE, 36> trailer{};
        Check(fread(trailer.data(), 1, trailer.size(), stream) == trailer.size(), "read saved trailer");
        Check(memcmp(trailer.data(), "PLUS", 4) == 0, "PLUS signature at native payload end");
        for (size_t i = 0; i < stocks.size(); ++i)
        {
            const size_t offset = 4 + i * 4;
            Check(trailer[offset] == stocks[i] && trailer[offset + 1] == 0 &&
                trailer[offset + 2] == 0 && trailer[offset + 3] == 0,
                "little-endian stocks immediately follow PLUS without a version field");
        }
        fseek(stream, 24, SEEK_SET);
        SW::secretStocks.fill(8);
        Check(SW::ReadFile(payload, SW::WeaponRecordSize, SW::NativeWeaponCount, stream) == 38, "load payload");
        Check(SW::secretStocks == stocks, "round trip all eight independent stocks");
        Check(memcmp(payload, expected.data(), expected.size()) == 0, "native records unchanged");
        fclose(stream);
    }
    for (int malformed : { 0, 1, 2, 3, 4, 5 })
    {
        FILE* stream = nullptr;
        Check(fopen_s(&stream, "Release/secret-weapons-test.wep", "w+b") == 0, "open legacy fixture");
        fwrite(expected.data(), 1, expected.size() - (malformed == 5 ? 1 : 0), stream);
        SW::SchemeExtension extension{ { 'P', 'L', 'U', 'S' }, {} };
        extension.stocks.fill(7);
        if (malformed == 1) extension.magic[0] = 'X';
        if (malformed == 2) extension.stocks.front() = 100;
        if (malformed == 3) extension.stocks.back() = 100;
        if (malformed >= 1 && malformed <= 4)
            fwrite(&extension, 1, malformed == 4 ? 8 : sizeof(extension), stream);
        fflush(stream);
        rewind(stream);
        SW::secretStocks.fill(9);
        SW::ReadFile(payload, SW::WeaponRecordSize, SW::NativeWeaponCount, stream);
        Check(SW::secretStocks == decltype(SW::secretStocks){}, "legacy or invalid extension resets all stocks");
        fclose(stream);
    }
    FILE* stream = nullptr;
    Check(fopen_s(&stream, "Release/secret-weapons-test.wep", "w+b") == 0, "open unrelated fixture");
    SW::WriteFile(expected.data(), 1, expected.size(), stream);
    Check(ftell(stream) == static_cast<long>(expected.size()), "unrelated write has no extension");
    fclose(stream);
}

static std::vector<BYTE> sentPacket;
static void* sentObject = nullptr;
static uint32_t sentSession = 0, sentPlayer = 0;
static void __fastcall CaptureSend(void* object, void*, uint32_t session, uint32_t player,
    const void* packet, uint32_t length)
{
    sentObject = object;
    sentSession = session;
    sentPlayer = player;
    sentPacket.assign(static_cast<const BYTE*>(packet), static_cast<const BYTE*>(packet) + length);
}

static void __fastcall CaptureBroadcast(void* object, void*, uint32_t session, const void* packet, uint32_t length)
{
    CaptureSend(object, nullptr, session, 0xffffffff, packet, length);
}

static int receiveCalls = 0;
static uint32_t receivedLength = 0, receivedSender = 0;
static const void* receivedPointer = nullptr;
static decltype(SW::secretStocks) stocksDuringReceive{};
static void __fastcall CaptureReceive(void*, void*, uint32_t sender, const void* packet, uint32_t length)
{
    ++receiveCalls;
    receivedSender = sender;
    receivedLength = length;
    receivedPointer = packet;
    stocksDuringReceive = SW::secretStocks;
}

static bool cheat = false;
static void __fastcall NativePrepare(void* object, void*, int mode)
{
    if (mode == 0 || mode == -23)
        for (int team = 0; team < 6; ++team)
            memset(static_cast<BYTE*>(object) + 0x497 + team * 0x108, cheat ? 0xff : 0, 49);
}

static void NetworkTests()
{
    FN::originalSendToPlayer = reinterpret_cast<FN::SendToPlayer>(CaptureSend);
    FN::originalSendToAll = reinterpret_cast<FN::SendToAll>(CaptureBroadcast);
    FN::originalReceivePacket = reinterpret_cast<FN::ReceivePacket>(CaptureReceive);
    SW::originalPrepareStocks = reinterpret_cast<SW::PrepareStocks>(NativePrepare);
    std::vector<BYTE> lobby(0x1640);
    constexpr uint32_t host = 1234;
    *reinterpret_cast<uint32_t*>(lobby.data() + 0x163c) = host;
    std::array<BYTE, SW::NativeWeaponPacketSize> native{};
    for (size_t i = 4; i < native.size(); ++i) native[i] = static_cast<BYTE>(i);
    const uint32_t type = SW::WeaponSchemePacketType;
    memcpy(native.data(), &type, sizeof(type));
    for (uint32_t reliable : { 0u, 1u })
    {
        *reinterpret_cast<uint32_t*>(SW::image + 0x188b14) = reliable;
        for (int broadcast : { 0, 1 })
        {
            for (int stock : { 0, 1, 9, 10, 11, 98, 99 })
            {
                for (size_t i = 0; i < SW::Weapons.size(); ++i)
                    SW::secretStocks[i] = (stock + i) % 100;
                const auto hostStocks = SW::secretStocks;
                if (broadcast)
                    FN::SendPacketToAll(lobby.data(), nullptr, host, native.data(), native.size());
                else
                    FN::SendPacketToPlayer(lobby.data(), nullptr, host, 9876, native.data(), native.size());
                Check(sentObject == lobby.data() && sentSession == host &&
                    sentPlayer == (broadcast ? 0xffffffff : 9876), "preserve transport object, session and join target");
                Check(sentPacket.size() == 0x664 && memcmp(sentPacket.data(), native.data(), native.size()) == 0,
                    "append extension without altering 1600-byte normal weapon packet");
                Check(memcmp(sentPacket.data() + 0x640, "PLUS", 4) == 0, "network extension has PLUS and no version");
                SW::secretStocks.fill(5); // A client's unrelated local scheme.
                FN::ReceiveLobbyPacket(lobby.data(), nullptr, host, sentPacket.data(),
                    static_cast<uint32_t>(sentPacket.size()) + reliable * 4);
                Check(SW::secretStocks == hostStocks && stocksDuringReceive == hostStocks,
                    "host stocks replace client scheme before native weapon processing");
                Check(receivedLength == native.size() && receivedSender == host && receivedPointer == sentPacket.data(),
                    "native decoder receives its unchanged original payload");
                std::vector<BYTE> game(0xcf0, 0);
                SW::PrepareWeaponStocks(game.data(), nullptr, 0);
                for (int team = 0; team < 6; ++team)
                    for (size_t i = 0; i < SW::Weapons.size(); ++i)
                        Check(game[SW::GameObjectHeaderSize + SW::Weapons[i].stockOffset + team * SW::TeamStride] ==
                            (hostStocks[i] == 10 ? 0xff : hostStocks[i]), "client game.dat stock matches host for every team");
            }
        }
        for (int malformed : { 0, 1, 2, 3, 4, 5 })
        {
            SW::secretStocks.fill(0); // Default must clear clients too.
            FN::SendPacketToAll(lobby.data(), nullptr, host, native.data(), native.size());
            if (malformed == 1) sentPacket.resize(native.size()); // Unmodified host.
            if (malformed == 2) sentPacket[sentPacket.size() - sizeof(uint32_t)] = 100; // Last DWORD > 99.
            if (malformed == 3) sentPacket[native.size()] = 'X';
            if (malformed == 4) sentPacket.resize(sentPacket.size() - 4);
            if (malformed == 5) sentPacket.push_back(0);
            SW::secretStocks.fill(9);
            FN::ReceiveLobbyPacket(lobby.data(), nullptr, host, sentPacket.data(),
                static_cast<uint32_t>(sentPacket.size()) + reliable * 4);
            Check(SW::secretStocks == decltype(SW::secretStocks){}, "Default, legacy or invalid host extension clears stale stocks");
        }
        SW::secretStocks.fill(7);
        FN::ReceiveLobbyPacket(lobby.data(), nullptr, host + 1, sentPacket.data(),
            static_cast<uint32_t>(sentPacket.size()) + reliable * 4);
        Check(SW::secretStocks.front() == 7 && SW::secretStocks.back() == 7, "non-host packet cannot replace secret scheme");
        // Put a short trailer immediately before an inaccessible page. Reliable
        // receive's reported length includes a sequence DWORD before the pointer.
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        BYTE* guarded = static_cast<BYTE*>(VirtualAlloc(nullptr, info.dwPageSize * 2,
            MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Check(guarded != nullptr, "allocate packet boundary fixture");
        DWORD protect = 0;
        Check(VirtualProtect(guarded + info.dwPageSize, info.dwPageSize, PAGE_NOACCESS, &protect) != FALSE,
            "guard packet boundary");
        for (size_t length : { native.size() - 1, native.size(), size_t(0x660), size_t(0x663) })
        {
            BYTE* packet = guarded + info.dwPageSize - length;
            memcpy(packet, native.data(), (std::min)(length, native.size()));
            if (length > native.size()) memset(packet + native.size(), 0, length - native.size());
            SW::secretStocks.fill(9);
            const int before = receiveCalls;
            FN::ReceiveLobbyPacket(lobby.data(), nullptr, host, packet, static_cast<uint32_t>(length) + reliable * 4);
            Check(receiveCalls == before + (length >= native.size() ? 1 : 0), "drop truncated native packet before decoder");
            Check(SW::secretStocks.front() == (length < native.size() ? 9u : 0u), "bounded trailer reads never cross receive buffer");
        }
        VirtualFree(guarded, 0, MEM_RELEASE);
    }
    *reinterpret_cast<uint32_t*>(SW::image + 0x188b14) = 0;
    const std::array<BYTE, 8> other{ 0x19, 0, 0, 0, 1, 2, 3, 4 };
    SW::secretStocks.fill(8);
    FN::SendPacketToAll(lobby.data(), nullptr, host, other.data(), other.size());
    Check(sentPacket == std::vector<BYTE>(other.begin(), other.end()), "unrelated outgoing packets unchanged");
    FN::ReceiveLobbyPacket(lobby.data(), nullptr, host, other.data(), other.size());
    Check(receivedLength == other.size() && receivedPointer == other.data() && SW::secretStocks.front() == 8,
        "unrelated incoming packets and stock state unchanged");
    puts("PASS: targeted/broadcast scheme transfer, both transport lengths, host authority, Default, malformed packets and all six client team stocks");
}

static void StockTests()
{
    constexpr std::array<size_t, 8> offsets{ 0x49e, 0x49f, 0x4a2, 0x4a3, 0x4a6, 0x4a7, 0x4aa, 0x4ab };
    for (size_t i = 0; i < offsets.size(); ++i)
        Check(SW::Weapons[i].stockOffset == offsets[i], "catalog matches supplied stock offsets");
    SW::originalPrepareStocks = reinterpret_cast<SW::PrepareStocks>(NativePrepare);
    for (int stock : { 0, 1, 9, 10, 11, 98, 99 })
    {
        std::vector<BYTE> object(0xcf0, 0x65);
        for (size_t i = 0; i < SW::Weapons.size(); ++i)
            SW::secretStocks[i] = (stock + i) % 100;
        SW::PrepareWeaponStocks(object.data(), nullptr, 0);
        for (size_t team = 0; team < 6; ++team)
        {
            for (size_t slot = 0x47b; slot < 0x47b + 49; ++slot)
            {
                BYTE expected = 0;
                for (size_t i = 0; i < SW::Weapons.size(); ++i)
                    if (slot == SW::Weapons[i].stockOffset)
                        expected = SW::secretStocks[i] == 10 ? 0xff : static_cast<BYTE>(SW::secretStocks[i]);
                Check(object[0x1c + slot + team * 0x108] == expected, "all secret team offsets and untouched native stock slots");
            }
            Check(object[0x1c + 0x47a + team * 0x108] == 0x65, "stock preparation does not touch preceding field");
        }
        SW::PrepareWeaponStocks(object.data(), nullptr, 1);
        for (size_t team = 0; team < 6; ++team)
            for (size_t i = 0; i < SW::Weapons.size(); ++i)
                Check(object[0x1c + SW::Weapons[i].stockOffset + team * 0x108] ==
                    (SW::secretStocks[i] == 10 ? 0xff : SW::secretStocks[i] * 2), "stock replenishment for every weapon and team");
        cheat = true;
        SW::secretStocks.fill(0);
        SW::PrepareWeaponStocks(object.data(), nullptr, -23);
        for (size_t team = 0; team < 6; ++team)
            for (const auto& weapon : SW::Weapons)
                Check(object[0x1c + weapon.stockOffset + team * 0x108] == 0xff, "all-weapons cheat preserved");
        cheat = false;
    }
}

static int nativeSelectCalls = 0;
static const BYTE* frontendFixture = nullptr;
static int nativeCommitCalls = 0;
static int nativeShowCalls = 0;
static void* expectedNativePage = nullptr;

static void __fastcall NativeCommit(void* page, void*)
{
    Check(page == expectedNativePage, "visibility handler commits the last valid native page");
    ++nativeCommitCalls;
}

static void __fastcall NativeShow(void*, void*, BOOL, UINT)
{
    ++nativeShowCalls;
}

static void ActivationTests(void* object, SW::Editor& editor, void* nativePage)
{
    if (!frontendFixture) return;
    // Run the supplied frontend's OnShowWindow implementation. Redirect only
    // its three callees and absolute dirty-flag address into the test fixture;
    // the handler's branches and native page-array access execute unchanged.
    constexpr size_t codeSize = 0x4c;
    BYTE* code = static_cast<BYTE*>(VirtualAlloc(nullptr, codeSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    Check(code != nullptr, "allocate visibility handler regression fixture");
    memcpy(code, frontendFixture + 0x8a139, codeSize);
    Check(code[0x0f] == 0xa0 && code[0x22] == 0xe8 && code[0x31] == 0xe8 && code[0x41] == 0xe8,
        "supplied frontend visibility handler matches fixture");
    *reinterpret_cast<BYTE**>(code + 0x10) = SW::image + 0x1b4e6c;
    auto redirect = [&](size_t offset, void* target)
    {
        *reinterpret_cast<uint32_t*>(code + offset + 1) =
            static_cast<uint32_t>(reinterpret_cast<uintptr_t>(target) - reinterpret_cast<uintptr_t>(code + offset + 5));
    };
    redirect(0x22, reinterpret_cast<void*>(SW::GetSelection));
    redirect(0x31, reinterpret_cast<void*>(NativeCommit));
    redirect(0x41, reinterpret_cast<void*>(NativeShow));
    DWORD oldProtect = 0;
    Check(VirtualProtect(code, codeSize, PAGE_EXECUTE_READ, &oldProtect) != FALSE, "protect regression code");
    FlushInstructionCache(GetCurrentProcess(), code, codeSize);
    using OnShow = void (__thiscall*)(void*, BOOL, UINT);
    const auto onShow = reinterpret_cast<OnShow>(code);
    expectedNativePage = nativePage;
    nativeCommitCalls = nativeShowCalls = 0;
    Check(SW::image[0x1b4e6c] == 0, "secret edits have no uncommitted native controls");
    for (int i = 0; i < 5; ++i)
    {
        onShow(object, FALSE, SW_OTHERZOOM);
        onShow(object, TRUE, SW_OTHERUNZOOM);
    }
    Check(nativeCommitCalls == 0 && nativeShowCalls == 10, "Alt+Tab preserves normal visibility handling without native commits");
    Check(SW::secretStocks[editor.secretIndex] == 7 && editor.secretSelected, "Alt+Tab preserves secret selection and stock");
    SW::image[0x1b4e6c] = 1;
    SendMessageW(editor.slider, TBM_SETPOS, TRUE, 8);
    SendMessageW(editor.panel, WM_HSCROLL, TB_THUMBPOSITION, reinterpret_cast<LPARAM>(editor.slider));
    Check(SW::image[0x1b4e6c] == 1, "secret edits preserve existing pending native edits");
    onShow(object, FALSE, SW_OTHERZOOM);
    onShow(object, TRUE, SW_OTHERUNZOOM);
    Check(nativeCommitCalls == 1 && nativeShowCalls == 12, "existing native edits still commit on deactivation");
    Check(SW::secretStocks[editor.secretIndex] == 8 && editor.secretSelected, "native commit leaves secret stock and selection intact");
    SW::image[0x1b4e6c] = 0;
    VirtualFree(code, 0, MEM_RELEASE);
    puts("PASS: repeated Alt+Tab visibility transitions execute the supplied native handler without committing a secret weapon as a native page");
}

static void __fastcall NativeSelect(void* object, void*)
{
    const auto window = SW::ObjectWindow(object);
    const int selection = static_cast<int>(SendMessageW(GetDlgItem(window, 2014), LB_GETCURSEL, 0, 0));
    auto editor = SW::GetEditor(window);
    auto safe = editor && SW::IsSecretSelection(selection) ? editor->nativeSelection : selection;
    Check(safe >= 0 && safe < 38, "native handler gets valid page index");
    *reinterpret_cast<int*>(static_cast<BYTE*>(object) + 0x9c) = safe;
    ++nativeSelectCalls;
}

static int __fastcall NativeSelection(void* object, void*)
{
    return static_cast<int>(SendMessageW(SW::ObjectWindow(object), LB_GETCURSEL, 0, 0));
}

static int nameLoadCalls = 0;
static BOOL __fastcall NativeLoadString(void* object, void*, UINT stringId)
{
    Check(object == SW::image + 0x187644 && stringId == 138, "native scheme name uses string 138");
    *static_cast<const char**>(object) = "User defined";
    ++nameLoadCalls;
    return TRUE;
}

static int defaultLoadCalls = 0;
static void __fastcall NativeDefault(void*, void*)
{
    *reinterpret_cast<const char**>(SW::image + 0x187644) = "Default";
    ++defaultLoadCalls;
}

static INT_PTR CALLBACK DialogProc(HWND window, UINT message, WPARAM wParam, LPARAM)
{
    if (message == WM_COMMAND && LOWORD(wParam) == 1266 && HIWORD(wParam) == CBN_SELCHANGE &&
        SendMessageW(GetDlgItem(window, 1266), CB_GETCURSEL, 0, 0) == 0)
        SW::LoadDefault(nullptr, nullptr); // The real dropdown's Default path calls the shared loader.
    if (message == WM_COMMAND && LOWORD(wParam) == 1003 && HIWORD(wParam) == BN_CLICKED)
        SW::LoadDefault(nullptr, nullptr);
    return FALSE;
}

static void EditorTests()
{
    SW::SetLanguage("en");
    // Simulate an edited translation with an accented character.
    SW::strings.strSheepStrike = L"Ataque de Ovelhas \u00e1";
    SW::secretStocks.fill(0);
    SW::loadNativeString = reinterpret_cast<SW::LoadNativeString>(NativeLoadString);
    SW::originalDefault = reinterpret_cast<SW::Select>(NativeDefault);
    INITCOMMONCONTROLSEX controls{ sizeof(controls), ICC_BAR_CLASSES };
    Check(InitCommonControlsEx(&controls) != FALSE, "initialize trackbar controls");
    struct Template { DLGTEMPLATE dialog; WORD menu; WORD windowClass; WORD title; } definition{};
    definition.dialog.style = WS_POPUP | WS_CAPTION;
    definition.dialog.cx = 407;
    definition.dialog.cy = 206;
    HWND root = CreateDialogIndirectParamW(GetModuleHandleW(nullptr), &definition.dialog, nullptr, DialogProc, 0);
    Check(root != nullptr, "create outer dialog for focus regression");
    HWND container = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE,
        0, 0, 600, 360, root, nullptr, nullptr, nullptr);
    definition.dialog.style = WS_CHILD | WS_VISIBLE;
    HWND window = CreateDialogIndirectParamW(GetModuleHandleW(nullptr), &definition.dialog, container, DialogProc, 0);
    Check(window != nullptr, "create hidden editor test dialog");
    HWND list = CreateWindowW(L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | LBS_NOTIFY,
        10, 10, 140, 300, window, reinterpret_cast<HMENU>(2014), nullptr, nullptr);
    CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 170, 10, 360, 260,
        window, reinterpret_cast<HMENU>(1004), nullptr, nullptr);
    HWND defaultButton = CreateWindowW(L"BUTTON", L"Default", WS_CHILD | WS_VISIBLE,
        170, 320, 80, 20, window, reinterpret_cast<HMENU>(1003), nullptr, nullptr);
    EnableWindow(defaultButton, FALSE);
    HWND scheme = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        10, 320, 150, 90, window, reinterpret_cast<HMENU>(1266), nullptr, nullptr);
    SendMessageW(scheme, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Default"));
    SendMessageW(scheme, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Saved scheme"));
    SendMessageW(scheme, CB_SETCURSEL, 1, 0);
    HWND deleteButton = CreateWindowW(L"BUTTON", L"Delete", WS_CHILD | WS_VISIBLE,
        300, 320, 80, 20, window, reinterpret_cast<HMENU>(1267), nullptr, nullptr);
    for (int i = 0; i < 38; ++i) SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Native weapon"));
    SendMessageW(list, LB_SETCURSEL, 0, 0);
    std::array<BYTE, 0x298> object{};
    std::array<BYTE, 0x30> listObject{};
    std::array<BYTE, 0x30> pageObject{};
    auto page = CreateWindowW(L"STATIC", L"Native page", WS_CHILD | WS_VISIBLE,
        170, 10, 360, 260, window, nullptr, nullptr, nullptr);
    const std::array<HWND, 3> nativeStockControls{
        CreateWindowW(L"STATIC", L"Initial stock", WS_CHILD | WS_VISIBLE,
            9, 5, 190, 13, page, reinterpret_cast<HMENU>(5000), nullptr, nullptr),
        CreateWindowW(TRACKBAR_CLASSW, L"", WS_CHILD | WS_VISIBLE | TBS_TOP,
            9, 23, 252, 34, page, reinterpret_cast<HMENU>(5001), nullptr, nullptr),
        CreateWindowW(L"STATIC", L"0", WS_CHILD | WS_VISIBLE,
            270, 27, 78, 13, page, reinterpret_cast<HMENU>(5002), nullptr, nullptr) };
    *reinterpret_cast<HWND*>(object.data() + 0x1c) = window;
    *reinterpret_cast<HWND*>(object.data() + 0x25c + 0x1c) = list;
    *reinterpret_cast<void**>(object.data() + 0xa4) = pageObject.data();
    *reinterpret_cast<HWND*>(pageObject.data() + 0x1c) = page;
    *reinterpret_cast<HWND*>(listObject.data() + 0x1c) = list;
    SW::originalSelect = reinterpret_cast<SW::Select>(NativeSelect);
    SW::originalSelection = reinterpret_cast<SW::Selection>(NativeSelection);
    SW::Attach(object.data());
    auto editor = SW::GetEditor(window);
    if (!editor)
        fprintf(stderr, "attach diagnostics: list=%p frame=%p count=%ld error=%lu styles=%lx/%lx\n",
            GetDlgItem(window, 2014), GetDlgItem(window, 1004),
            static_cast<long>(SendMessageW(list, LB_GETCOUNT, 0, 0)), GetLastError(),
            static_cast<long>(GetWindowLongPtrW(window, GWL_EXSTYLE)),
            static_cast<long>(GetWindowLongPtrW(container, GWL_EXSTYLE)));
    Check(editor != nullptr, "attach secret panel");
    Check(SendMessageW(list, LB_GETCOUNT, 0, 0) == 46, "append eight weapons");
    SW::Attach(object.data());
    Check(SendMessageW(list, LB_GETCOUNT, 0, 0) == 46, "no duplicate attachment");
    Check(GetWindowLongW(editor->slider, GWL_STYLE) & TBS_TOP, "upward thumb with ticks above");
    const std::array<HWND, 3> generatedStockControls{
        GetDlgItem(editor->panel, 5000), editor->slider, editor->value };
    for (size_t i = 0; i < nativeStockControls.size(); ++i)
    {
        RECT nativeBounds{}, generatedBounds{};
        Check(GetWindowRect(nativeStockControls[i], &nativeBounds) &&
            GetWindowRect(generatedStockControls[i], &generatedBounds) && EqualRect(&nativeBounds, &generatedBounds),
            "generated stock label, trackbar and value match native screen positions and sizes");
    }
    Check(SendMessageW(editor->slider, TBM_GETRANGEMIN, 0, 0) == 0 &&
        SendMessageW(editor->slider, TBM_GETRANGEMAX, 0, 0) == 99, "stock slider allows zero through 99");
    SendMessageW(list, LB_SETCURSEL, 38, 0);
    SW::SelectWeapon(object.data(), nullptr);
    for (int stock : { 10, 11, 99 })
    {
        SendMessageW(editor->slider, TBM_SETPOS, TRUE, stock);
        SendMessageW(editor->panel, WM_HSCROLL, TB_THUMBPOSITION, reinterpret_cast<LPARAM>(editor->slider));
        char displayed[128]{};
        GetWindowTextA(editor->value, displayed, sizeof(displayed));
        const char* expected = stock == 10 ? "Unlimited" : stock == 11 ? "11" : "99";
        Check(SW::secretStocks[0] == static_cast<uint32_t>(stock) && strcmp(displayed, expected) == 0,
            "10 remains Unlimited; larger stock values display as numbers");
    }
    for (size_t i = 0; i < SW::Weapons.size(); ++i)
    {
        wchar_t name[128]{};
        SendMessageW(list, LB_GETTEXT, 38 + i, reinterpret_cast<LPARAM>(name));
        Check(*SW::Weapons[i].name == name, "localized wide names appended in requested order");
        SendMessageW(list, LB_SETCURSEL, 38 + i, 0);
        SW::SelectWeapon(object.data(), nullptr);
        Check(editor->secretIndex == i && SW::GetSelection(listObject.data(), nullptr) == 0,
            "each secret maps safely to last native page");
        SendMessageW(editor->slider, TBM_SETPOS, TRUE, i + 1);
        SendMessageW(editor->panel, WM_HSCROLL, TB_THUMBPOSITION, reinterpret_cast<LPARAM>(editor->slider));
        SetFocus(editor->slider);
        SendMessageW(root, WM_ACTIVATE, WA_INACTIVE, 0);
    }
    for (size_t i = 0; i < SW::Weapons.size(); ++i)
    {
        SendMessageW(list, LB_SETCURSEL, 38 + i, 0);
        SW::SelectWeapon(object.data(), nullptr);
        Check(SendMessageW(editor->slider, TBM_GETPOS, 0, 0) == static_cast<LRESULT>(i + 1), "independent stock restored on selection");
    }
    const auto stocksBeforeLanguage = SW::secretStocks;
    const LRESULT selectionBeforeLanguage = SendMessageW(list, LB_GETCURSEL, 0, 0);
    const LRESULT topBeforeLanguage = SendMessageW(list, LB_GETTOPINDEX, 0, 0);
    const int nameLoadsBeforeLanguage = nameLoadCalls;
    SW::SetLanguage("pt\r\n");
    wchar_t refreshedName[128]{};
    SendMessageW(list, LB_GETTEXT, 40, reinterpret_cast<LPARAM>(refreshedName));
    Check(wcscmp(refreshedName, L"Ataque Ovelhas") == 0, "language update refreshes an existing editor with Portuguese names");
    SendMessageW(list, LB_GETTEXT, 0, reinterpret_cast<LPARAM>(refreshedName));
    Check(wcscmp(refreshedName, L"Native weapon") == 0 && SendMessageW(list, LB_GETCOUNT, 0, 0) == 46,
        "language update preserves native weapon names and list size");
    Check(SW::secretStocks == stocksBeforeLanguage && nameLoadCalls == nameLoadsBeforeLanguage &&
        SendMessageW(list, LB_GETCURSEL, 0, 0) == selectionBeforeLanguage &&
        SendMessageW(list, LB_GETTOPINDEX, 0, 0) == topBeforeLanguage && editor->secretSelected && editor->secretIndex == 7 &&
        SendMessageW(editor->slider, TBM_GETPOS, 0, 0) == 8,
        "language update preserves stocks, scheme state, selected weapon and scroll position");
    Check(IsWindowEnabled(defaultButton), "stock changes enable Default");
    Check(strcmp(*reinterpret_cast<const char**>(SW::image + 0x187644), "User defined") == 0 && nameLoadCalls > 0,
        "secret-only edit updates the native stored scheme name");
    char editorSchemeName[128]{};
    GetWindowTextA(scheme, editorSchemeName, sizeof(editorSchemeName));
    Check(SendMessageW(scheme, CB_GETCURSEL, 0, 0) == 1 && strcmp(editorSchemeName, "Saved scheme") == 0 &&
        IsWindowEnabled(deleteButton), "secret edits preserve editor scheme name, selection, and Delete state");
    const int beforeNoOp = nameLoadCalls;
    SendMessageW(editor->panel, WM_HSCROLL, TB_ENDTRACK, reinterpret_cast<LPARAM>(editor->slider));
    Check(nameLoadCalls == beforeNoOp, "unchanged stock does not relabel the scheme");
    SendMessageW(scheme, CB_SETCURSEL, 0, 0);
    SendMessageW(window, WM_COMMAND, MAKEWPARAM(1266, CBN_SELCHANGE), reinterpret_cast<LPARAM>(scheme));
    Check(defaultLoadCalls > 0 && SW::secretStocks == decltype(SW::secretStocks){} &&
        SendMessageW(editor->slider, TBM_GETPOS, 0, 0) == 0,
        "selecting Default calls shared loader and clears all eight stocks");
    SW::secretStocks.fill(5);
    SW::Refresh(*editor);
    SW::LoadDefault(nullptr, nullptr); // Selection on Game controls sends no editor WM_COMMAND.
    Check(SW::secretStocks == decltype(SW::secretStocks){} && SendMessageW(editor->slider, TBM_GETPOS, 0, 0) == 0,
        "Default loaded outside the editor immediately refreshes its stock display");
    FILE* reloadFixture = nullptr;
    Check(fopen_s(&reloadFixture, "Release/secret-weapons-test.wep", "w+b") == 0, "open editor reload fixture");
    for (size_t i = 0; i < SW::Weapons.size(); ++i) SW::secretStocks[i] = static_cast<uint32_t>(i + 1);
    const auto savedStocks = SW::secretStocks;
    auto payload = SW::image + 0x187648;
    Check(SW::WriteFile(payload, SW::WeaponRecordSize, SW::NativeWeaponCount, reloadFixture) == 38,
        "save named scheme stock fixture");
    SW::secretStocks.fill(0);
    SW::Refresh(*editor);
    rewind(reloadFixture);
    const int beforeReload = nameLoadCalls;
    Check(SW::ReadFile(payload, SW::WeaponRecordSize, SW::NativeWeaponCount, reloadFixture) == 38 &&
        SW::secretStocks == savedStocks && SendMessageW(editor->slider, TBM_GETPOS, 0, 0) == 8 &&
        nameLoadCalls == beforeReload, "saved scheme reload refreshes panel without marking it User defined");
    fclose(reloadFixture);
    SW::secretStocks.fill(6);
    SendMessageW(window, WM_COMMAND, MAKEWPARAM(1003, BN_CLICKED), reinterpret_cast<LPARAM>(defaultButton));
    Check(SW::secretStocks == decltype(SW::secretStocks){} && SendMessageW(editor->slider, TBM_GETPOS, 0, 0) == 0,
        "Default clears every secret stock and refreshes selected slider");
    SendMessageW(list, LB_SETCURSEL, 38, 0);
    SW::SelectWeapon(object.data(), nullptr);
    Check(editor->secretSelected, "select Salvation Army");
    Check((GetWindowLongW(page, GWL_STYLE) & WS_VISIBLE) == 0, "hide native page");
    Check((GetWindowLongW(editor->panel, GWL_STYLE) & WS_VISIBLE) != 0, "show secret panel");
    ShowWindow(root, SW_SHOWNOACTIVATE);
    Check((GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_CONTROLPARENT) &&
        (GetWindowLongPtrW(container, GWL_EXSTYLE) & WS_EX_CONTROLPARENT),
        "all ancestors of stock slider participate in outer dialog navigation");
    Check(GetNextDlgTabItem(root, editor->slider, TRUE) == list,
        "Windows reverse traversal from stock slider returns to weapon list");
    Check(GetNextDlgTabItem(root, list, FALSE) == editor->slider,
        "Windows forward traversal can rediscover focused stock slider");
    SetFocus(editor->slider);
    Check(GetFocus() == editor->slider, "stock slider has focus for deactivation regression");
    SendMessageW(root, WM_ACTIVATE, WA_INACTIVE, 0);
    Check(SW::GetSelection(listObject.data(), nullptr) == 0, "save/load safely index native array");
    SendMessageW(editor->slider, TBM_SETPOS, TRUE, 7);
    SendMessageW(editor->panel, WM_HSCROLL, TB_THUMBPOSITION, reinterpret_cast<LPARAM>(editor->slider));
    Check(SW::secretStocks[0] == 7, "slider edits selected stock");
    Check(SW::image[0x1b4e6c] == 0, "stock is committed immediately without pending native edits");
    SetFocus(editor->slider);
    SendMessageW(root, WM_ACTIVATE, WA_INACTIVE, 0);
    ActivationTests(object.data(), *editor, pageObject.data());
    SendMessageW(list, LB_SETCURSEL, 0, 0);
    SW::SelectWeapon(object.data(), nullptr);
    Check(!editor->secretSelected && (GetWindowLongW(page, GWL_STYLE) & WS_VISIBLE), "restore same native page");
    Check(GetFocus() == list, "move focus off secret controls before hiding them");
    SendMessageW(list, LB_SETCURSEL, 38, 0);
    SW::SelectWeapon(object.data(), nullptr);
    SendMessageW(list, LB_SETCURSEL, 1, 0);
    SW::SelectWeapon(object.data(), nullptr);
    Check(editor->nativeSelection == 1 && SW::GetSelection(listObject.data(), nullptr) == 1, "switch to different native weapon");
    Check(nativeSelectCalls > 0, "native selection handler remains active");
    DestroyWindow(window);
    Check(SW::GetEditor(window) == nullptr, "remove editor context on close");
    Check(SW::editorWindows.empty(), "remove editor from scheme refresh registry on close");
    Check(!(GetWindowLongPtrW(container, GWL_EXSTYLE) & WS_EX_CONTROLPARENT),
        "restore original ancestor navigation style on editor destruction");
    DestroyWindow(root);
    puts("PASS: independent stocks, Default from dropdown/outside editor, User defined naming, saved-scheme refresh, and Windows focus traversal");
}

static void FrontendHookTests(const char* path)
{
    // SEC_IMAGE maps the supplied executable without running its entry point or
    // DLL imports. Hook changes are private to this mapping, never written back.
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    Check(file != INVALID_HANDLE_VALUE, "open supplied frontend for hook validation");
    HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr);
    Check(mapping != nullptr, "map supplied frontend image");
    BYTE* frontend = static_cast<BYTE*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));
    Check(frontend != nullptr, "view supplied frontend image");
    frontendFixture = frontend;
    EditorTests();
    frontendFixture = nullptr;
    Check(MH_Initialize() == MH_OK, "initialize MinHook for real image validation");
    std::array<BYTE, 12> before{};
    memcpy(before.data(), frontend + 0x975b0, before.size());
    std::array<BYTE, 9> defaultBefore{};
    memcpy(defaultBefore.data(), frontend + 0x1e698, defaultBefore.size());
    const std::array<size_t, 3> networkSites{ 0x1e29a, 0x1e3c4, 0x39e41 };
    std::array<std::array<BYTE, 10>, 3> networkBefore{};
    for (size_t i = 0; i < networkSites.size(); ++i)
        memcpy(networkBefore[i].data(), frontend + networkSites[i], networkBefore[i].size());
    for (size_t corrupted = 0; corrupted < networkSites.size(); ++corrupted)
    {
        DWORD old;
        Check(VirtualProtect(frontend + networkSites[corrupted], 1, PAGE_EXECUTE_READWRITE, &old) != FALSE, "make network signature fixture writable");
        frontend[networkSites[corrupted]] = 0x90;
        Check(!FN::InstallInImage(frontend), "shared network module rejects unsupported signature before installing hooks");
        frontend[networkSites[corrupted]] = networkBefore[corrupted][0];
        VirtualProtect(frontend + networkSites[corrupted], 1, old, &old);
        for (size_t i = 0; i < networkSites.size(); ++i)
            Check(!memcmp(networkBefore[i].data(), frontend + networkSites[i], networkBefore[i].size()), "network signature rejection leaves every shared hook site unchanged");
    }
    Check((SW::InstallInImage(frontend) && FN::InstallInImage(frontend)), "seven weapon hooks and three shared network hooks install against supplied frontend");
    for (size_t i = 0; i < networkSites.size(); ++i)
        Check(memcmp(networkBefore[i].data(), frontend + networkSites[i], networkBefore[i].size()) != 0,
            "network send/receive detour enabled");
    Check(memcmp(before.data(), frontend + 0x975b0, before.size()) != 0, "native write detour enabled");
    Check(memcmp(defaultBefore.data(), frontend + 0x1e698, defaultBefore.size()) != 0,
        "shared Default loader used by both scheme dropdowns is hooked");
    Check(MH_Uninitialize() == MH_OK, "remove test hooks");
    for (size_t i = 0; i < networkSites.size(); ++i)
        Check(memcmp(networkBefore[i].data(), frontend + networkSites[i], networkBefore[i].size()) == 0,
            "network send/receive native code restored");
    Check(memcmp(before.data(), frontend + 0x975b0, before.size()) == 0, "native code restored after hook test");
    Check(memcmp(defaultBefore.data(), frontend + 0x1e698, defaultBefore.size()) == 0,
        "shared Default loader restored after hook test");
    UnmapViewOfFile(frontend);
    CloseHandle(mapping);
    CloseHandle(file);
    puts("PASS: all ten MinHook detours install and restore on a non-running mapping of the supplied frontend");
}

int main(int argc, char** argv)
{
    try
    {
        std::vector<BYTE> image(0x5b8000);
        FN::image = SW::image = image.data();
        LanguageTests();
        SchemeTests();
        StockTests();
        NetworkTests();
        EditorTests();
        puts("PASS: scheme round trips, legacy/corrupt schemes, team stock offsets, unlimited, replenishment, editor selection and cleanup");
        if (argc > 1) FrontendHookTests(argv[1]);
        return 0;
    }
    catch (const std::exception& error)
    {
        fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
