#include <cstdio>
#include <stdexcept>
#include <vector>
#include "../fkSettings/NetworkTeams.cpp"
#include "../fkSettings/SecretWeapons.cpp"

namespace NT = NetworkTeams;
namespace SW = SecretWeapons;
static void Check(bool value, const char* message)
{
    if (!value) throw std::runtime_error(message);
}
static std::vector<BYTE> sent;
static std::vector<BYTE> serialized;
static int writeResult = 1, received = 0;
static bool throwOnWrite = false;
static void __fastcall Send(void*, void*, uint32_t, uint32_t, const void* packet, uint32_t length)
{
    sent.assign(static_cast<const BYTE*>(packet), static_cast<const BYTE*>(packet) + length);
}
static void __fastcall Broadcast(void* object, void*, uint32_t session, const void* packet, uint32_t length)
{
    Send(object, nullptr, session, 0, packet, length);
}
static int __fastcall Write(void* object, void*, const char*)
{
    BYTE* payload = static_cast<BYTE*>(object) + 0x1c;
    serialized.assign(payload, payload + 0xcd4);
    if (throwOnWrite) throw std::runtime_error("write exception fixture");
    return writeResult;
}
static void __fastcall Receive(void*, void*, uint32_t, const void*, uint32_t)
{
    ++received;
}
static void SetTeam(BYTE* game, size_t slot, const char* name, int savedIndex, BYTE owner)
{
    memcpy(game + 4 + slot * 4, &savedIndex, sizeof(savedIndex));
    BYTE* team = game + NT::TeamOffset + slot * NT::TeamStride;
    team[0] = owner;
    strcpy_s(reinterpret_cast<char*>(team + 2), 17, name);
}
static void SetSkill(size_t index, int skill)
{
    memcpy(NT::image + NT::SavedTeamsRva + index * NT::SavedTeamStride, &skill, sizeof(skill));
}
static void GameAndPacketTests()
{
    std::vector<BYTE> image(0x5b8000);
    NT::image = SW::image = image.data();
    NT::enabled = true;
    NT::originalWriteGame = reinterpret_cast<NT::WriteGame>(Write);
    SW::originalSendToAll = reinterpret_cast<SW::SendToAll>(Broadcast);
    SW::originalSendToPlayer = reinterpret_cast<SW::SendToPlayer>(Send);
    SW::originalReceivePacket = reinterpret_cast<SW::ReceivePacket>(Receive);
    BYTE* game = image.data() + NT::GameRva;
    SetTeam(game, 0, "Human", 0, 0);
    SetSkill(0, 0);
    SetTeam(game, 1, "Easy", 1, 0);
    SetSkill(1, 1);
    SetTeam(game, 2, "Medium", 2, 0);
    SetSkill(2, 50);
    SetTeam(game, 3, "Hard", 3, 0);
    SetSkill(3, 100);
    SetTeam(game, 4, "Remote", -1, 1);
    SetTeam(game, 5, "Other Human", 4, 2);
    SetSkill(4, 0);
    const std::vector<BYTE> lobby(game, game + 0xcf0);
    Check(NT::WriteNetworkGame(game, "fixture.dat", true) == 1, "host write result");
    const auto hostData = serialized;
    const std::array<int, 6> controllers{ 0, -1, -50, -100, 1, 2 };
    for (size_t slot = 0; slot < controllers.size(); ++slot)
        Check(static_cast<signed char>(serialized[0x468 + slot * NT::TeamStride]) == controllers[slot],
            "host serializes exact AI difficulty and retains human controllers");
    auto expected = lobby;
    for (size_t slot = 0; slot < controllers.size(); ++slot)
        expected[NT::TeamOffset + slot * NT::TeamStride] = static_cast<BYTE>(controllers[slot]);
    Check(memcmp(serialized.data(), expected.data() + 0x1c, serialized.size()) == 0,
        "only controller bytes change in native game.dat payload");
    Check(memcmp(game, lobby.data(), lobby.size()) == 0, "host lobby ownership restored");
    writeResult = 0;
    Check(NT::WriteNetworkGame(game, "fixture.dat", true) == 0 && memcmp(game, lobby.data(), lobby.size()) == 0,
        "failed write retains native return value and restores lobby ownership");
    throwOnWrite = true;
    try { NT::WriteNetworkGame(game, "fixture.dat", true); } catch (const std::runtime_error&) {}
    Check(memcmp(game, lobby.data(), lobby.size()) == 0, "exception restores lobby ownership");
    throwOnWrite = false;
    writeResult = 1;

    std::array<BYTE, 20> start{ 14 };
    for (size_t i = 4; i < start.size(); ++i) start[i] = static_cast<BYTE>(i);
    SW::SendWeaponPacketToAll(nullptr, nullptr, 9, start.data(), start.size());
    Check(sent.size() == NT::StartPacket{}.size() && memcmp(sent.data(), start.data(), start.size()) == 0,
        "start packet extension preserves native type and GUID");
    const auto broadcast = sent;
    SW::SendWeaponPacketToPlayer(nullptr, nullptr, 9, 10, start.data(), start.size());
    Check(sent == broadcast, "targeted and broadcast start messages agree");
    std::vector<BYTE> client(lobby);
    for (size_t slot = 0; slot < 6; ++slot) SetTeam(client.data(), slot,
        reinterpret_cast<const char*>(game + NT::TeamOffset + slot * NT::TeamStride + 2), -1, lobby[NT::TeamOffset + slot * NT::TeamStride]);
    std::vector<BYTE> joinLobby(0x1640);
    const uint32_t host = 123;
    memcpy(joinLobby.data() + 0x163c, &host, sizeof(host));
    for (int reliable : { 0, 1 })
    {
        memcpy(image.data() + 0x188b14, &reliable, sizeof(reliable));
        NT::receivedTeams = {};
        SW::ReceiveWeaponPacket(joinLobby.data(), nullptr, host, broadcast.data(), static_cast<uint32_t>(broadcast.size()) + reliable * 4);
        Check(NT::WriteNetworkGame(client.data(), "fixture.dat", false) == 1 && serialized == hostData,
            "both transports produce identical host/client game configurations");
        auto swapped = client;
        std::swap_ranges(swapped.begin() + NT::TeamOffset, swapped.begin() + NT::TeamOffset + NT::TeamStride,
            swapped.begin() + NT::TeamOffset + 3 * NT::TeamStride);
        NT::WriteNetworkGame(swapped.data(), "fixture.dat", false);
        Check(static_cast<signed char>(serialized[0x468]) == -100 && serialized[0x468 + 3 * NT::TeamStride] == 0,
            "AI identification survives team reorder before client serialization");
        NT::ReceiveStartPacket(999, host, start.data(), start.size());
        Check(NT::receivedTeams[1].skill == 1, "non-host packet cannot change AI state");
        SW::ReceiveWeaponPacket(joinLobby.data(), nullptr, host, start.data(), start.size() + reliable * 4);
        NT::WriteNetworkGame(client.data(), "fixture.dat", false);
        Check(serialized[0x468 + NT::TeamStride] == 0, "native host start clears stale AI state");

        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        BYTE* guard = static_cast<BYTE*>(VirtualAlloc(nullptr, info.dwPageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Check(guard != nullptr, "allocate receive boundary fixture");
        DWORD protect;
        Check(VirtualProtect(guard + info.dwPageSize, info.dwPageSize, PAGE_NOACCESS, &protect) != FALSE, "protect receive boundary");
        for (size_t length : { size_t(4), size_t(19), size_t(20), size_t(131), size_t(132) })
        {
            BYTE* packet = guard + info.dwPageSize - length;
            memcpy(packet, broadcast.data(), length);
            if (length == 132) packet[20] = 'X';
            NT::receivedTeams[1].skill = 99;
            const int before = received;
            SW::ReceiveWeaponPacket(joinLobby.data(), nullptr, host, packet, static_cast<uint32_t>(length) + reliable * 4);
            Check(received == before + (length >= 20 ? 1 : 0), "truncated native start dropped before native processing");
            Check(NT::receivedTeams[1].skill == 0, "short or invalid extension clears AI state without overread");
        }
        VirtualFree(guard, 0, MEM_RELEASE);
    }
    NT::Extension extension;
    memcpy(&extension, broadcast.data() + 20, sizeof(extension));
    Check(NT::ValidExtension(extension), "valid AI extension");
    extension.teams[1].skill = 101;
    Check(!NT::ValidExtension(extension), "invalid AI difficulty rejected");
    extension.teams[1].skill = 1;
    memset(extension.teams[1].name, 'x', 17);
    Check(!NT::ValidExtension(extension), "unterminated AI name rejected");
    memcpy(&extension, broadcast.data() + 20, sizeof(extension));
    extension.teams[2] = extension.teams[1];
    Check(!NT::ValidExtension(extension), "ambiguous duplicate AI names rejected");
    for (size_t index : { size_t(1), size_t(2), size_t(3) }) SetSkill(index, 0);
    SW::SendWeaponPacketToAll(nullptr, nullptr, 9, start.data(), start.size());
    Check(sent == std::vector<BYTE>(start.begin(), start.end()), "human-only game keeps native start packet");
    SetSkill(1, 101);
    SetSkill(2, -1);
    Check(NT::HostComputerTeams(game)[1].skill == 0 && NT::HostComputerTeams(game)[2].skill == 0,
        "invalid saved skill is ignored");
    NT::enabled = false;
    puts("PASS: host/client AI controllers, exact difficulty, ownership restoration, reordered teams, both transports, host authority and malformed packets");
}

static void LaunchCallSiteTests(BYTE* frontend)
{
    BYTE* game = frontend + NT::GameRva;
    DWORD protect;
    Check(VirtualProtect(game, 0xcf0, PAGE_READWRITE, &protect) != FALSE, "make mapped game fixture writable");
    memset(game, 0, 0xcf0);
    Check(VirtualProtect(frontend + NT::SavedTeamsRva, 0x400, PAGE_READWRITE, &protect) != FALSE,
        "make mapped saved-team fixture writable");
    SetTeam(game, 0, "Native Caller", 1, 2);
    SetSkill(1, 67);
    NT::originalWriteGame = reinterpret_cast<NT::WriteGame>(Write);
    const auto launch = [&](size_t rva)
    {
        // Execute the supplied frontend's push/mov/call at its real RVA, so
        // the production detour sees the exact native return address. Relocate
        // just the object pointer, filename and callee into this private mapping.
        BYTE* code = frontend + rva;
        std::array<BYTE, 16> before;
        memcpy(before.data(), code, before.size());
        Check(code[0] == 0x68 && code[5] == 0xb9 && code[10] == 0xe8, "native launch call structure");
        DWORD previous;
        Check(VirtualProtect(code, before.size(), PAGE_EXECUTE_READWRITE, &previous) != FALSE,
            "prepare native launch caller fixture");
        const char* filename = "fixture.dat";
        memcpy(code + 1, &filename, sizeof(filename));
        memcpy(code + 6, &game, sizeof(game));
        const auto displacement = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(NT::SaveGame) -
            reinterpret_cast<uintptr_t>(code + 15));
        memcpy(code + 11, &displacement, sizeof(displacement));
        code[15] = 0xc3;
        FlushInstructionCache(GetCurrentProcess(), code, before.size());
        Check(reinterpret_cast<int (__cdecl*)()>(code)() == 1, "native caller preserves writer return value");
        memcpy(code, before.data(), before.size());
        VirtualProtect(code, before.size(), previous, &protect);
        FlushInstructionCache(GetCurrentProcess(), code, before.size());
    };
    launch(0x35941);
    Check(static_cast<signed char>(serialized[0x468]) == -67 && game[NT::TeamOffset] == 2,
        "real host launch return address converts AI and restores owner");
    NT::receivedTeams = {};
    strcpy_s(NT::receivedTeams[0].name, "Native Caller");
    NT::receivedTeams[0].skill = 67;
    launch(0x3b3c6);
    Check(static_cast<signed char>(serialized[0x468]) == -67 && game[NT::TeamOffset] == 2,
        "real client launch return address converts AI and restores owner");
    NT::SaveGame(game, nullptr, "fixture.dat");
    Check(serialized[0x468] == 2, "non-network writer caller retains native controller");
    puts("PASS: supplied host/client launch instructions execute the production detour with their native return addresses");
}

static void HookTests(const char* path)
{
    HANDLE file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    Check(file != INVALID_HANDLE_VALUE, "open frontend");
    HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr);
    Check(mapping != nullptr, "map frontend without executing it");
    BYTE* frontend = static_cast<BYTE*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));
    Check(frontend != nullptr, "view frontend");
    Check(MH_Initialize() == MH_OK, "initialize MinHook");
    std::array<BYTE, 16> before;
    memcpy(before.data(), frontend + NT::WriteGameRva, before.size());
    Check(SW::InstallInImage(frontend), "shared packet hooks install");
    Check(NT::InstallInImage(frontend), "AI hook and host-only filter install alongside secret weapons");
    Check(frontend[0x3264b] == 1 && frontend[0x399fc] == 0, "only hosting dialog shows computer teams");
    Check(memcmp(before.data(), frontend + NT::WriteGameRva, before.size()) != 0, "game writer hook enabled");
    LaunchCallSiteTests(frontend);
    Check(NT::SetHostListMode(0), "restore test list mode");
    Check(MH_Uninitialize() == MH_OK, "remove mapped image hooks");
    Check(memcmp(before.data(), frontend + NT::WriteGameRva, before.size()) == 0, "game writer restored");
    Check(NT::SetHostListMode(1), "alter signature fixture");
    Check(!NT::InstallInImage(frontend), "modified frontend signature rejected before installation");
    Check(memcmp(before.data(), frontend + NT::WriteGameRva, before.size()) == 0, "rejection leaves writer unchanged");
    NT::SetHostListMode(0);
    NT::enabled = false;
    UnmapViewOfFile(frontend);
    CloseHandle(mapping);
    CloseHandle(file);
    puts("PASS: supported frontend hook installation, host-only team filter, coexistence with secret weapons and signature rejection");
}
int main(int argc, char** argv)
{
    try
    {
        GameAndPacketTests();
        if (argc > 1) HookTests(argv[1]);
        return 0;
    }
    catch (const std::exception& error)
    {
        fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
