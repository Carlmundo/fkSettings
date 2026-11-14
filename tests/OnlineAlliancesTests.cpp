#define main OfflineAlliancesTestMain
#include "OfflineAlliancesTests.cpp"
#undef main
#include "../fkSettings/OnlineAlliances.cpp"
#include "../fkSettings/SecretWeapons.cpp"
#include "../fkSettings/NetworkTeams.cpp"
#include "../fkSettings/ExtendedOptions.cpp"
#include "../fkSettings/FrontendNetwork.cpp"
#include "../fkSettings/ColourMaps.cpp"

namespace ON = OnlineAlliances;
namespace FN = FrontendNetwork;
namespace NT = NetworkTeams;
namespace CM = ColourMaps;
namespace EO = ExtendedOptions;
namespace SW = SecretWeapons;

static const char* OnlineNames[] = { "Host human", "Host CPU", "Alice one", "Alice two", "Bob one", "Bob two" };
static constexpr uint32_t HostId = 11, AliceId = 22, BobId = 33;
static std::vector<BYTE> onlineSent, onlineSerialized;
static uint32_t onlineSource = 0, onlineTarget = 0, onlineNativeLength = 0;
static bool onlineBroadcast = false;
static int onlineReceives = 0, onlineSends = 0, onlineWriteResult = 1;

static void OnlineSend(void*, uint32_t source, uint32_t target, bool broadcast, const void* packet, uint32_t length)
{
    onlineSent.assign(static_cast<const BYTE*>(packet), static_cast<const BYTE*>(packet) + length);
    onlineSource = source; onlineTarget = target; onlineBroadcast = broadcast; ++onlineSends;
}
static void __fastcall OnlineSendPlayer(void* object, void*, uint32_t source, uint32_t target, const void* packet, uint32_t length)
{ OnlineSend(object, source, target, false, packet, length); }
static void __fastcall OnlineSendAll(void* object, void*, uint32_t source, const void* packet, uint32_t length)
{ OnlineSend(object, source, 0, true, packet, length); }
static int __fastcall OnlineWriter(void* object, void*, const char*)
{
    const BYTE* data = static_cast<const BYTE*>(object) + 0x1c;
    onlineSerialized.assign(data, data + 0xcd4);
    return onlineWriteResult;
}
static void LocalPlayer(uint32_t id) { memcpy(ON::image + ON::LocalIdRva, &id, 4); }
static BYTE* OnlineGame() { return ON::image + ON::GameRva; }
static void FillOnlineGame()
{
    BYTE* game = OnlineGame();
    memset(game, 0, 0x10d8);
    game[0x483] = 6; game[0xc6d] = 2;
    *reinterpret_cast<unsigned*>(game + 0xc74) = 12345;
    for (int slot = 0; slot < 6; ++slot)
    {
        BYTE* record = game + 0x484 + slot * 0x108;
        record[0] = static_cast<BYTE>(slot / 2);
        record[1] = static_cast<BYTE>(slot % 2);
        strcpy_s(reinterpret_cast<char*>(record + 2), 17, OnlineNames[slot]);
        *reinterpret_cast<int*>(game + 4 + slot * 4) = slot == 1 ? 7 : -1;
    }
    const uint32_t players[] = { HostId, AliceId, BobId };
    for (int index = 0; index < 3; ++index) memcpy(game + 0x108c + index * 4, &players[index], 4);
    *reinterpret_cast<int*>(game + 0x10d4) = 3;
    const int skill = 50;
    memcpy(ON::image + NT::SavedTeamsRva + 7 * NT::SavedTeamStride, &skill, 4);
}
static std::vector<BYTE> NativeOnlinePacket(uint32_t type)
{
    std::vector<BYTE> packet(ON::NativeSize(type));
    memcpy(packet.data(), &type, 4);
    if (type == 5)
    {
        BYTE* data = packet.data() + 8;
        const BYTE* game = OnlineGame();
        memcpy(data, game + 0x1c, 0xcd4);
        memcpy(data + 3284, game + 4, 24);
        memcpy(data + 3308, game + 0x10d4, 4);
        memcpy(data + 3312, game + 0x10c4, 14);
        memcpy(data + 3326, game + 0x108c, 56);
        memcpy(data + 3382, game + 0xcf0, 924);
    }
    return packet;
}
static void __fastcall OnlineReceiveNative(void* object, void*, uint32_t sender, const void* packet, uint32_t length)
{
    ++onlineReceives; onlineNativeLength = length;
    if (ON::PacketType(packet, length) == 5)
    {
        const BYTE* data = static_cast<const BYTE*>(packet) + 8;
        BYTE* game = OnlineGame();
        memcpy(game + 0x1c, data, 0xcd4);
        memcpy(game + 4, data + 3284, 24);
        memcpy(game + 0x10d4, data + 3308, 4);
        memcpy(game + 0x10c4, data + 3312, 14);
        memcpy(game + 0x108c, data + 3326, 56);
        memcpy(game + 0xcf0, data + 3382, 924);
        memcpy(static_cast<BYTE*>(object) + 0x163c, &sender, 4);
    }
}
static void __fastcall OnlineReceiveRoundNative(void*, void*, uint32_t, const void*, uint32_t length)
{ ++onlineReceives; onlineNativeLength = length; }

static int onlineJoinResult = 1;
static int __fastcall OnlineJoinInitFixture(void* object, void*)
{
    Check(ON::Dword(static_cast<BYTE*>(object) + 0x163c) == 0 && !ON::session.host && !ON::session.revision &&
        !ON::session.locked && ON::session.teams.empty(), "new joining lobby clears the previous host and alliance session before native initialization");
    memset(OnlineGame(), 0, 0x10d8);
    return onlineJoinResult;
}
static void __fastcall OnlineJoinReceiveFixture(void* object, void*, uint32_t sender, const void* packet, uint32_t length)
{
    const BYTE* bytes = static_cast<const BYTE*>(packet);
    const uint32_t type = ON::PacketType(packet, length);
    if (type == 6)
    {
        ++onlineReceives;
        const int slot = *reinterpret_cast<int*>(OnlineGame() + 0x10d4);
        memcpy(OnlineGame() + 0x108c + slot * 4, bytes + 4, 4);
        memcpy(OnlineGame() + 0xcf0 + slot * 66, bytes + 9, 17);
        ++*reinterpret_cast<int*>(OnlineGame() + 0x10d4);
    }
    else if (type == 10)
    {
        ++onlineReceives;
        const int slot = OnlineGame()[0x483];
        BYTE* team = OnlineGame() + 0x484 + slot * 0x108;
        const uint32_t owner = ON::Dword(bytes + 4);
        int player = 0;
        while (player < 14 && ON::Dword(OnlineGame() + 0x108c + player * 4) != owner) ++player;
        Check(player < 14, "native team update resolves its previously received player");
        team[0] = static_cast<BYTE>(player); memcpy(team + 2, bytes + 8, 17);
        ++OnlineGame()[0x483];
    }
    else OnlineReceiveNative(object, nullptr, sender, packet, length);
}
static void OnlineJoinSequenceTests(BYTE* image)
{
    Check(MH_Initialize() == MH_OK && FN::InstallInImage(image), "install native joining-lobby lifecycle and packet hooks");
    FN::originalJoinInit = reinterpret_cast<FN::InitLobby>(OnlineJoinInitFixture);
    FN::originalReceivePacket = reinterpret_cast<FN::ReceivePacket>(OnlineJoinReceiveFixture);
    FN::originalSendToPlayer = reinterpret_cast<FN::SendToPlayer>(OnlineSendPlayer);
    FN::originalSendToAll = reinterpret_cast<FN::SendToAll>(OnlineSendAll);
    ON::Callbacks().send = OnlineSend;
    const auto join = reinterpret_cast<FN::InitLobby>(image + 0x3975e);
    const auto receive = reinterpret_cast<FN::ReceivePacket>(image + 0x39e41);
    std::vector<BYTE> hostLobby(0x5000), joinLobby(0x5000);
    for (uint32_t reliable : { 0u, 1u })
        for (uint32_t previousHost : { 0u, BobId, 0xcdcdcdcdu, 0xffffffffu })
            for (int hostTeams : { 0, 1, 2 })
            {
                memcpy(image + 0x188b14, &reliable, 4);
                FillOnlineGame(); LocalPlayer(HostId);
                OnlineGame()[0x483] = static_cast<BYTE>(hostTeams);
                for (int slot = hostTeams; slot < 6; ++slot) memset(OnlineGame() + 0x484 + slot * 0x108, 0, 0x108);
                memset(OnlineGame() + 0x108c, 0, 56); memcpy(OnlineGame() + 0x108c, &HostId, 4);
                memset(OnlineGame() + 0xcf0, 0, 924); strcpy_s(reinterpret_cast<char*>(OnlineGame() + 0xcf0), 17, "Host");
                *reinterpret_cast<int*>(OnlineGame() + 0x10d4) = 1;
                ON::Lobby(hostLobby.data(), true, false, true);
                const auto native = NativeOnlinePacket(5);
                FN::SendPacketToPlayer(image + ON::TransportRva, nullptr, HostId, AliceId, native.data(), static_cast<uint32_t>(native.size()));
                const auto snapshot = onlineSent;
                memcpy(joinLobby.data() + 0x163c, &previousHost, 4);
                // The constructor leaves this native field uninitialized. Reuse
                // the same dialog address and a finished session on every join.
                ON::session.hosting = false; ON::session.host = previousHost; ON::session.revision = 0x10000; ON::session.locked = true;
                LocalPlayer(AliceId);
                Check(join(joinLobby.data()) == 1, "native joining init hook preserves its return value");
                const int before = onlineReceives;
                receive(joinLobby.data(), HostId, snapshot.data(), static_cast<uint32_t>(snapshot.size()) + reliable * 4);
                Check(onlineReceives == before + 1 && ON::session.host == HostId && ON::session.teams.size() == size_t(hostTeams) &&
                    ON::Dword(joinLobby.data() + 0x163c) == HostId && !strcmp(reinterpret_cast<const char*>(OnlineGame() + 0xcf0), "Host"),
                    "initial snapshot establishes the actual host and all existing teams despite reused dialog memory");
                for (int slot = 0; slot < hostTeams; ++slot)
                    Check(!strcmp(reinterpret_cast<const char*>(OnlineGame() + 0x486 + slot * 0x108), OnlineNames[slot]),
                        "host human and CPU teams remain visible after initial synchronization");
                std::array<BYTE, 76> player{};
                const uint32_t playerType = 6; memcpy(player.data(), &playerType, 4); memcpy(player.data() + 4, &BobId, 4);
                strcpy_s(reinterpret_cast<char*>(player.data() + 9), 17, "Third player");
                receive(joinLobby.data(), HostId, player.data(), static_cast<uint32_t>(player.size()) + reliable * 4);
                Check(onlineReceives == before + 2 && ON::Dword(OnlineGame() + 0x1090) == BobId &&
                    !strcmp(reinterpret_cast<const char*>(OnlineGame() + 0xcf0 + 66), "Third player"), "third player's name update reaches native dispatch");
                std::array<BYTE, 0x10d8> clientGame{}; memcpy(clientGame.data(), OnlineGame(), clientGame.size());
                const auto clientSession = ON::session;
                BYTE* added = OnlineGame() + 0x484 + hostTeams * 0x108;
                added[0] = 1; strcpy_s(reinterpret_cast<char*>(added + 2), 17, "Third team"); ++OnlineGame()[0x483];
                ON::Lobby(hostLobby.data(), true, false, true); LocalPlayer(HostId);
                auto team = NativeOnlinePacket(10); memcpy(team.data() + 4, &BobId, 4);
                strcpy_s(reinterpret_cast<char*>(team.data() + 8), 17, "Third team");
                FN::SendPacketToAll(image + ON::TransportRva, nullptr, HostId, team.data(), static_cast<uint32_t>(team.size()));
                const auto update = onlineSent;
                memcpy(OnlineGame(), clientGame.data(), clientGame.size()); ON::session = clientSession; LocalPlayer(AliceId);
                receive(joinLobby.data(), HostId, update.data(), static_cast<uint32_t>(update.size()) + reliable * 4);
                Check(onlineReceives == before + 3 && OnlineGame()[0x483] == hostTeams + 1 &&
                    !strcmp(reinterpret_cast<const char*>(OnlineGame() + 0x486 + hostTeams * 0x108), "Third team") &&
                    ON::session.teams.size() == size_t(hostTeams + 1), "third player's team update follows their name instead of being discarded");
            }
    onlineJoinResult = 0;
    const uint32_t stale = 0xcdcdcdcd;
    memcpy(joinLobby.data() + 0x163c, &stale, 4);
    Check(join(joinLobby.data()) == 0, "joining-lobby initialization failure preserves the native return value");
    onlineJoinResult = 1;
    Check(MH_Uninitialize() == MH_OK, "restore native joining lifecycle and packet hooks");
    FN::originalReceivePacket = reinterpret_cast<FN::ReceivePacket>(OnlineReceiveNative);
    puts("PASS: initial snapshots and third-player teams survive stale host IDs, empty/human/CPU host rosters and repeated joins on both transports");
}

static void OnlineModelTests()
{
    FillOnlineGame(); LocalPlayer(HostId);
    ON::session = ON::Session{}; ON::session.hosting = true; ON::session.host = HostId;
    Check(ON::SyncHost() && ON::session.teams.size() == 6, "host discovers all local and remote teams");
    for (int slot = 0; slot < 6; ++slot)
        Check(ON::session.teams[slot].colour == slot + 1 && ON::session.teams[slot].owner == uint32_t((slot / 2 + 1) * 11),
            "initial colours follow addition order and ownership uses actual player IDs");
    Check(!ON::SyncHost(), "unchanged roster does not publish another revision");
    const uint32_t revision = ON::session.revision;
    Check(!ON::Change(AliceId, OnlineNames[1], 1) && ON::session.revision == revision,
        "a player cannot recolour a host-owned CPU team");
    Check(ON::Change(AliceId, OnlineNames[2], -1) && ON::session.teams[2].colour == 2,
        "a remote player can reverse their own team's colour");
    Check(ON::Change(HostId, OnlineNames[4], 1) && ON::session.teams[4].colour == 6,
        "host can recolour another player's team");
    ON::session.teams[0].colour = 1;
    Check(ON::Change(HostId, OnlineNames[0], -1) && ON::session.teams[0].colour == 6, "online Red wraps backwards to Teal");
    Check(ON::Change(HostId, OnlineNames[0], 1) && ON::session.teams[0].colour == 1, "online Teal wraps forwards to Red");
    ON::session.locked = true;
    Check(!ON::Change(AliceId, OnlineNames[2], 1), "launch snapshot cannot be changed by late requests");
    ON::session.locked = false;
    BYTE* game = OnlineGame();
    for (int slot = 1; slot < 6; ++slot) memset(game + 0x484 + slot * 0x108, 0, 0x108);
    game[0x483] = 1;
    Check(ON::SyncHost() && ON::session.teams.size() == 1 && ON::session.teams[0].colour == 0,
        "one remaining online team disables colours");
    BYTE* added = game + 0x484 + 0x108;
    added[0] = 1; strcpy_s(reinterpret_cast<char*>(added + 2), 17, OnlineNames[2]); game[0x483] = 2;
    Check(ON::SyncHost() && ON::session.teams[0].colour == 1 && ON::session.teams[1].colour == 2,
        "adding another online team resets Red and Blue");
    ON::session.teams[1].colour = 4;
    added += 0x108; added[0] = 2; strcpy_s(reinterpret_cast<char*>(added + 2), 17, OnlineNames[4]); game[0x483] = 3;
    Check(ON::SyncHost() && ON::session.teams[2].colour == 2, "online Red and Yellow choose unused Blue for a new team");
}

static void OnlinePacketTests()
{
    FillOnlineGame(); LocalPlayer(HostId);
    std::vector<BYTE> hostLobby(0x5000), joinLobby(0x5000), results(0x200);
    ON::Lobby(hostLobby.data(), true, false, true);
    for (int index = 0; index < 6; ++index) ON::session.teams[index].colour = static_cast<BYTE>(index / 2 + 1);
    const auto hostSession = ON::session;
    const auto native = NativeOnlinePacket(5);
    for (uint32_t reliable : { 0u, 1u })
    {
        memcpy(ON::image + 0x188b14, &reliable, 4);
        ON::session = hostSession; LocalPlayer(HostId);
        FN::SendPacketToPlayer(ON::image + ON::TransportRva, nullptr, HostId, AliceId, native.data(), static_cast<uint32_t>(native.size()));
        const auto snapshot = onlineSent;
        Check(snapshot.size() == native.size() + 112 + sizeof(ON::State) &&
            !memcmp(snapshot.data(), native.data(), native.size()) && !memcmp(snapshot.data() + native.size(), "FKA1", 4),
            "snapshot composes CPU and alliance extensions without changing native bytes");
        Check(onlineSource == HostId && onlineTarget == AliceId && !onlineBroadcast, "late join settings use targeted delivery");
        ON::session = ON::Session{}; LocalPlayer(AliceId); memset(joinLobby.data(), 0, joinLobby.size());
        memset(OnlineGame(), 0, 0x10d8);
        const int before = onlineReceives;
        FN::ReceiveLobbyPacket(joinLobby.data(), nullptr, HostId, snapshot.data(), static_cast<uint32_t>(snapshot.size()) + reliable * 4);
        Check(onlineReceives == before + 1 && onlineNativeLength == native.size() + 112 + reliable * 4,
            "both transports strip only alliance metadata before native and CPU decoders");
        Check(ON::session.host == HostId && ON::session.teams.size() == 6 && ON::session.teams[3].owner == AliceId &&
            ON::session.teams[3].colour == 2 && NT::lobbyTeams[1].skill == 50,
            "late join establishes the native host and receives all colours, owners and CPU skills");
        const auto clientSession = ON::session;
        const auto clientRevision = ON::session.revision;
        ON::Click(OnlineNames[2], -1);
        Check(onlineSent.size() == sizeof(ON::ChangePacket) && onlineSource == AliceId && onlineTarget == HostId && !onlineBroadcast &&
            ON::session.revision == clientRevision && ON::session.teams[2].colour == 2,
            "client sends a direction request and waits for authoritative host confirmation");
        const auto request = onlineSent;
        const int sends = onlineSends;
        ON::Click(OnlineNames[0], 1);
        Check(onlineSends == sends, "client UI cannot send changes for another player's team");
        ON::session = hostSession; LocalPlayer(HostId);
        CM::HostReceive(hostLobby.data(), nullptr, AliceId, request.data(), static_cast<uint32_t>(request.size()) + reliable * 4);
        Check(ON::session.teams[2].colour == 1 && onlineBroadcast && onlineSent.size() == sizeof(ON::StatePacket),
            "host authenticates the owner request and broadcasts a complete updated state");
        const auto update = onlineSent;
        const auto updatedHost = ON::session;
        ON::session = clientSession; LocalPlayer(AliceId);
        const int receives = onlineReceives;
        FN::ReceiveLobbyPacket(joinLobby.data(), nullptr, HostId, update.data(), static_cast<uint32_t>(update.size()) + reliable * 4);
        Check(onlineReceives == receives && ON::session.teams[2].colour == 1 && ON::session.revision > clientRevision,
            "custom state is consumed and changes the client only after a host broadcast");
        auto stale = update;
        auto state = reinterpret_cast<ON::StatePacket*>(stale.data()); state->state.revision = clientRevision;
        FN::ReceiveLobbyPacket(joinLobby.data(), nullptr, HostId, stale.data(), static_cast<uint32_t>(stale.size()) + reliable * 4);
        Check(ON::session.teams[2].colour == 1, "stale updates cannot undo a newer colour choice");
        state->state.revision += 10; state->state.teams[2].colour = 6;
        FN::ReceiveLobbyPacket(joinLobby.data(), nullptr, BobId, stale.data(), static_cast<uint32_t>(stale.size()) + reliable * 4);
        Check(ON::session.teams[2].colour == 1, "non-host packets cannot publish alliance settings");
        ON::session = updatedHost; LocalPlayer(HostId);
        const auto unchanged = ON::CurrentState();
        ON::ChangePacket bad{}; memcpy(&bad, request.data(), sizeof(bad)); strcpy_s(bad.name, OnlineNames[0]);
        CM::HostReceive(hostLobby.data(), nullptr, AliceId, &bad, sizeof(bad) + reliable * 4);
        const auto afterForged = ON::CurrentState();
        Check(!memcmp(&unchanged, &afterForged, sizeof(unchanged)),
            "host rejects a forged request for a different owner's team");
        const auto start = NativeOnlinePacket(14);
        FN::SendPacketToAll(ON::image + ON::TransportRva, nullptr, HostId, start.data(), static_cast<uint32_t>(start.size()));
        const auto launch = onlineSent;
        Check(launch.size() == 20 + 112 + sizeof(ON::State), "start message carries CPU and alliance data together");
        ON::session = clientSession; LocalPlayer(AliceId);
        memcpy(results.data() + 0xa8, &HostId, 4);
        ON::Lobby(results.data(), false, true, false);
        NT::ReceiveNextRoundPacket(results.data(), nullptr, HostId, launch.data(), static_cast<uint32_t>(launch.size()) + reliable * 4);
        Check(ON::Valid(ON::session.round) && ON::session.round.teams[2].colour == 1 && NT::receivedTeams[1].skill == 50 &&
            onlineNativeLength == 132 + reliable * 4, "next-round dispatcher accepts updated colours and CPU metadata before native launch");
        const int good = onlineReceives;
        NT::ReceiveNextRoundPacket(results.data(), nullptr, BobId, launch.data(), static_cast<uint32_t>(launch.size()) + reliable * 4);
        NT::ReceiveNextRoundPacket(results.data(), nullptr, HostId, start.data(), static_cast<uint32_t>(start.size()) + reliable * 4);
        Check(onlineReceives == good, "non-host and outdated starts cannot reach the engine launch handler");
    }
}

static void NativeOnlineCall(BYTE* image, size_t rva, const char* filename)
{
    BYTE* code = image + rva;
    std::array<BYTE, 16> previous{}; memcpy(previous.data(), code, previous.size());
    Check(code[0] == 0x68 && code[5] == 0xb9 && code[10] == 0xe8, "native online writer caller structure");
    auto patched = previous;
    BYTE* game = image + ON::GameRva;
    memcpy(patched.data() + 1, &filename, 4); memcpy(patched.data() + 6, &game, 4); patched[15] = 0xc3;
    Patch(code, patched.data(), patched.size());
    const int result = reinterpret_cast<int (__cdecl*)()>(code)();
    Patch(code, previous.data(), previous.size());
    Check(result == onlineWriteResult, "native online caller preserves writer result");
    Check(AC::WriterCaller() == nullptr, "shared writer restores the forwarded native caller");
}
static void NativeOnlineShuffle(BYTE* image, size_t rva)
{
    BYTE previous[6]; memcpy(previous, image + rva, sizeof(previous));
    const BYTE ret = 0xc3; Patch(image + rva + 5, &ret, 1);
    reinterpret_cast<OA::NativeVoid>(image + rva)(OnlineGame());
    Patch(image + rva, previous, sizeof(previous));
}
static void OnlineScoreSeriesTests(BYTE* image, void* lobby, const char* gamePath)
{
    FillOnlineGame(); LocalPlayer(HostId);
    OnlineGame()[0xc6d] = 2;
    for (int slot = 0; slot < 6; ++slot) OnlineGame()[0x485 + slot * 0x108] = 0;
    ON::Lobby(lobby, true, false, true);
    for (int team = 0; team < 6; ++team) ON::session.teams[team].colour = static_cast<BYTE>(team / 2 + 1);
    ResultsFixture accumulated;
    accumulated.bytes.fill(0); accumulated.Field(4) = 6; accumulated.Field(0xc) = 2;
    for (int row = 0; row < 6; ++row) accumulated.SetName(0x68c + row * 0x644, OnlineNames[5 - row]);
    std::array<int, 6> expected{};
    using Merge = void (__thiscall*)(void*, const void*);
    using Winner = int (__thiscall*)(void*);
    const auto merge = reinterpret_cast<Merge>(image + 0x2e220);
    const auto gameWinner = reinterpret_cast<Winner>(image + 0x2bc8e);
    const auto statsWinner = reinterpret_cast<Winner>(image + 0x2df18);
    const int survivors[] = { 0, 2, 4, 1 };
    for (int round = 0; round < 4; ++round)
    {
        if (!round) NativeOnlineShuffle(image, 0x35932);
        else reinterpret_cast<OA::NativeVoid>(image + 0x1843)(OnlineGame());
        NativeOnlineCall(image, round ? 0x61b04 : 0x35941, gamePath);
        const int survivor = survivors[round], alliance = survivor / 2;
        for (int slot = 0; slot < 6; ++slot)
            if (!strcmp(reinterpret_cast<const char*>(OnlineGame() + 0x486 + slot * 0x108), OnlineNames[survivor]))
                ++OnlineGame()[0x485 + slot * 0x108];
        Check(AC::UpdateMatchScores(OnlineGame(), 3, AC::launchedTeams), "repair each round from its online launch snapshot");
        ++expected[alliance * 2]; ++expected[alliance * 2 + 1];
        ResultsFixture result;
        result.bytes.fill(0); result.Field(4) = 6; result.Field(0xc) = 2; result.Field(0x1c) = 3;
        for (int row = 0; row < 6; ++row)
        {
            const int team = (row + round) % 6;
            result.SetName(0x2c24 + row * 0x644, OnlineNames[team]);
            result.SetName(0x68c + row * 0x644, OnlineNames[(11 - row - round) % 6]);
            if (team == survivor) result.Field(0x2bb8 + row * 0x644) = 100;
        }
        reinterpret_cast<OA::NativeVoid>(image + 0x2180)(result.bytes.data());
        merge(accumulated.bytes.data(), result.bytes.data());
        for (int row = 0; row < 6; ++row)
        {
            const char* name = reinterpret_cast<const char*>(OnlineGame() + 0x486 + row * 0x108);
            const auto team = std::find_if(std::begin(OnlineNames), std::end(OnlineNames), [&](const char* entry) { return !strcmp(entry, name); });
            Check(team != std::end(OnlineNames) && OnlineGame()[0x485 + row * 0x108] == expected[team - std::begin(OnlineNames)],
                "six-team Worms2 scores retain all three alliances across four online rounds");
            Check(accumulated.Field(0x60 + row * 0x644) == expected[5 - row],
                "native accumulated Game statistics agree by name after every online round");
        }
        Check((gameWinner(OnlineGame()) >= 0) == (round == 3) &&
            (statsWinner(accumulated.bytes.data()) >= 0) == (round == 3),
            "both native online match-completion checks finish only at two victories in round four");
    }
    puts("PASS: three online alliances retain matching live/statistics scores over four rounds and finish together");
}
static void OnlineLaunchTests(BYTE* image)
{
    Check(MH_Initialize() == MH_OK && AC::InstallInImage(image) && OA::InstallInImage(image) && NT::InstallInImage(image),
        "install shared sidecar writer, turn order, scoring and CPU hooks together");
    NativeTurnOrderTests(image); // Also relocate the private mapping's CRT memcpy tables.
    Check(MH_CreateHook(image + 0x9d680, reinterpret_cast<void*>(TurnThreadData), nullptr) == MH_OK &&
        MH_EnableHook(image + 0x9d680) == MH_OK, "supply private CRT state for network shuffle checks");
    NT::originalWriteGame = reinterpret_cast<NT::WriteGame>(OnlineWriter);
    char temporary[MAX_PATH]{}, directory[MAX_PATH]{};
    Check(GetTempPathA(MAX_PATH, temporary) && GetTempFileNameA(temporary, "fko", 0, directory), "reserve online launch fixture");
    DeleteFileA(directory); Check(CreateDirectoryA(directory, nullptr) != FALSE, "create online launch directory");
    const std::string gamePath = std::string(directory) + "\\game.dat", sidecar = AC::SidecarPath(gamePath.c_str());
    std::vector<BYTE> hostLobby(0x5000), clientLobby(0x5000);
    for (unsigned seed : { 1u, 42u, 0xabcdefu })
    {
        FillOnlineGame(); LocalPlayer(HostId);
        for (int slot = 0; slot < 6; ++slot) OnlineGame()[0x485 + slot * 0x108] = 0;
        memcpy(OnlineGame() + 0xc74, &seed, 4);
        ON::Lobby(hostLobby.data(), true, false, true);
        for (int index = 0; index < 6; ++index) ON::session.teams[index].colour = static_cast<BYTE>(index / 2 + 1);
        std::array<BYTE, 0x10d8> unshuffled{}; memcpy(unshuffled.data(), OnlineGame(), unshuffled.size());
        NativeOnlineShuffle(image, 0x35932);
        NativeOnlineCall(image, 0x35941, gamePath.c_str());
        const auto hostBytes = onlineSerialized;
        const auto hostColours = ReadFileBytes(sidecar);
        Check(ON::CanRunEngine() && ON::session.locked && hostColours[0] == 1 && AC::launchedTeams.size() == 6,
            "host publishes a complete alliance sidecar and freezes online result context");
        std::array<bool, 4> firstColours{};
        for (int slot = 0; slot < 3; ++slot)
        {
            firstColours[hostColours[slot + 1]] = true;
            Check(hostColours[slot + 1] == hostColours[slot + 4], "host interleaves each alliance once per cycle");
        }
        Check(firstColours[1] && firstColours[2] && firstColours[3], "all three alliances take a turn before a colour repeats");
        for (int slot = 0; slot < 6; ++slot)
        {
            const char* name = reinterpret_cast<const char*>(hostBytes.data() + 0x46a + slot * 0x108);
            if (!strcmp(name, OnlineNames[1])) Check(static_cast<signed char>(hostBytes[0x468 + slot * 0x108]) == -50,
                "CPU skill survives the sidecar thunk and native writer-entry hook chain");
            const auto source = std::find_if(std::begin(OnlineNames), std::end(OnlineNames), [&](const char* entry) { return !strcmp(entry, name); });
            Check(source != std::end(OnlineNames) && OnlineGame()[0x484 + slot * 0x108] ==
                unshuffled[0x484 + (source - std::begin(OnlineNames)) * 0x108],
                "network lobby ownership is restored after serialization");
        }
        const auto start = NativeOnlinePacket(14);
        FN::SendPacketToAll(image + ON::TransportRva, nullptr, HostId, start.data(), static_cast<uint32_t>(start.size()));
        const auto startPacket = onlineSent;
        const auto hostSession = ON::session;
        std::array<BYTE, 0x10d8> shuffled{}; memcpy(shuffled.data(), OnlineGame(), shuffled.size());
        ON::session = ON::Session{}; LocalPlayer(AliceId);
        memcpy(OnlineGame(), unshuffled.data(), unshuffled.size());
        memcpy(clientLobby.data() + 0x163c, &HostId, 4);
        ON::Lobby(clientLobby.data(), false, false, true);
        const uint32_t reliable = 0; memcpy(image + 0x188b14, &reliable, 4);
        FN::ReceiveLobbyPacket(clientLobby.data(), nullptr, HostId, startPacket.data(), static_cast<uint32_t>(startPacket.size()));
        Check(ON::Valid(ON::session.round), "client freezes host launch metadata before its native shuffle");
        NativeOnlineShuffle(image, 0x3b3b7);
        NativeOnlineCall(image, 0x3b3c6, gamePath.c_str());
        Check(onlineSerialized == hostBytes && ReadFileBytes(sidecar) == hostColours && ON::CanRunEngine(),
            "host and client write identical first-round game and alliance bytes for the same seed");
        const auto clientSession = ON::session;
        for (bool host : { true, false })
        {
            ON::session = host ? hostSession : clientSession; LocalPlayer(host ? HostId : AliceId);
            memcpy(OnlineGame(), shuffled.data(), shuffled.size());
            reinterpret_cast<OA::NativeVoid>(image + 0x1843)(OnlineGame());
            Check(OnlineGame()[0xab4] == 1, "later online round advances the native starting index");
            NativeOnlineCall(image, host ? 0x61b04 : 0x6405b, gamePath.c_str());
            Check(ReadFileBytes(sidecar) == hostColours && onlineSerialized[0xa98] == 1,
                "host and client later rounds retain colours and the established order");
            auto changed = hostBytes; changed[0xa98] = 1;
            Check(onlineSerialized == changed, "subsequent online serialization changes only the native starter in this fixture");
        }
        // Verify six-team scoring from the actual network launch snapshot.
        int winner = 0;
        while (strcmp(reinterpret_cast<const char*>(OnlineGame() + 0x486 + winner * 0x108), OnlineNames[2])) ++winner;
        ++OnlineGame()[0x485 + winner * 0x108];
        Check(AC::UpdateMatchScores(OnlineGame(), 3, AC::launchedTeams), "repair online live scores for a winning alliance");
        ResultsFixture stats; stats.bytes.fill(0); stats.Field(4) = 6; stats.Field(0x1c) = 3;
        for (int row = 0; row < 6; ++row)
        {
            stats.SetName(0x2c24 + row * 0x644, OnlineNames[row]);
            stats.SetName(0x68c + row * 0x644, OnlineNames[5 - row]);
            if (row == 2) stats.Field(0x2bb8 + row * 0x644) = 100;
        }
        reinterpret_cast<OA::NativeVoid>(image + 0x2180)(stats.bytes.data());
        stats.CheckWins({ OnlineNames[2], OnlineNames[3] });
        for (int row = 0; row < 6; ++row)
        {
            const char* name = reinterpret_cast<const char*>(OnlineGame() + 0x486 + row * 0x108);
            Check(OnlineGame()[0x485 + row * 0x108] == ((!strcmp(name, OnlineNames[2]) || !strcmp(name, OnlineNames[3])) ? 1 : 0),
                "Worms2 and Game statistics award both online allies and no opponents");
        }
    }
    OnlineScoreSeriesTests(image, hostLobby.data(), gamePath.c_str());
    onlineWriteResult = 0;
    NativeOnlineCall(image, 0x6405b, gamePath.c_str());
    Check(!ON::CanRunEngine() && AC::launchedTeams.empty() && GetFileAttributesA(sidecar.c_str()) == INVALID_FILE_ATTRIBUTES,
        "failed online writer blocks engine launch and removes stale alliance data");
    onlineWriteResult = 1;
    OA::localLaunch = false;
    Check(AC::WriteGame(OnlineGame(), nullptr, gamePath.c_str()) == 1 && ReadFileBytes(sidecar) == std::array<BYTE, 7>{} &&
        ON::CanRunEngine() && AC::launchedTeams.empty(), "a non-network launch clears online sidecars and scoring context");
    DeleteFileA(sidecar.c_str()); Check(RemoveDirectoryA(directory) != FALSE, "clean online launch files");
    Check(NT::SetHostListMode(0), "restore native host list filter after tests");
    Check(MH_Uninitialize() == MH_OK, "restore online native test hooks");
}

static POINT OnlineSquare(HWND window, HTREEITEM item)
{
    RECT bounds{};
    Check(TreeView_GetItemRect(window, item, &bounds, FALSE), "find native online team row");
    int first = -1, last = -1;
    for (int x = 0; x < bounds.right; ++x)
    {
        TVHITTESTINFO hit{}; hit.pt = { x, (bounds.top + bounds.bottom) / 2 };
        TreeView_HitTest(window, &hit);
        if (hit.hItem == item && (hit.flags & TVHT_ONITEMSTATEICON))
        { if (first < 0) first = x; last = x; }
    }
    Check(first >= 0, "hit testing identifies the alliance square beside the team icon");
    return { (first + last) / 2, (bounds.top + bounds.bottom) / 2 };
}
static void OnlineMouse(HWND window, const POINT& point, bool reverse)
{
    const LPARAM position = MAKELPARAM(point.x, point.y);
    SendMessageW(window, reverse ? WM_RBUTTONDOWN : WM_LBUTTONDOWN, reverse ? MK_RBUTTON : MK_LBUTTON, position);
    SendMessageW(window, reverse ? WM_RBUTTONUP : WM_LBUTTONUP, 0, position);
}
struct OnlineTreeRestore { HIMAGELIST native; int height; bool restored = false; };
static LRESULT CALLBACK ObserveOnlineTree(HWND window, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR, DWORD_PTR reference)
{
    if (message == WM_DESTROY)
    {
        auto& check = *reinterpret_cast<OnlineTreeRestore*>(reference);
        check.restored = TreeView_GetImageList(window, TVSIL_STATE) == check.native && TreeView_GetItemHeight(window) == check.height;
        if (!check.restored) fprintf(stderr, "Tree cleanup images=%p expected=%p height=%d expected=%d\n",
            TreeView_GetImageList(window, TVSIL_STATE), check.native, TreeView_GetItemHeight(window), check.height);
    }
    return DefSubclassProc(window, message, wparam, lparam);
}
static void OnlineTreeTests(const char* path)
{
    INITCOMMONCONTROLSEX controls{ sizeof(controls), ICC_TREEVIEW_CLASSES };
    Check(InitCommonControlsEx(&controls), "initialize online roster tree controls");
    HMODULE resources = LoadLibraryExA(path, nullptr, LOAD_LIBRARY_AS_DATAFILE);
    HBITMAP bitmap = static_cast<HBITMAP>(LoadImageW(resources, MAKEINTRESOURCEW(244), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
    HIMAGELIST sprites = ImageList_Create(32, 16, ILC_COLOR32 | ILC_MASK, 5, 0);
    Check(bitmap && sprites && ImageList_AddMasked(sprites, bitmap, RGB(255, 0, 255)) == 0, "load native online human/CPU sprites");
    DeleteObject(bitmap);
    for (bool hosting : { true, false })
    {
        FillOnlineGame(); LocalPlayer(HostId);
        ON::session = ON::Session{}; ON::session.hosting = true; ON::session.host = HostId; ON::SyncHost();
        const auto state = ON::CurrentState();
        LocalPlayer(hosting ? HostId : AliceId);
        std::vector<BYTE> lobby(0x5000);
        HWND parent = CreateWindowExW(0, L"STATIC", L"Online alliance fixture", WS_OVERLAPPED,
            0, 0, 600, 500, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        HWND window = CreateWindowExW(0, WC_TREEVIEWW, L"", WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS,
            0, 0, 480, 420, parent, nullptr, GetModuleHandleW(nullptr), nullptr);
        Check(window != nullptr, "create host and client native roster trees");
        TreeView_SetBkColor(window, RGB(255, 255, 255)); TreeView_SetImageList(window, sprites, TVSIL_NORMAL);
        BYTE* tree = lobby.data() + (hosting ? ON::HostTreeOffset : ON::JoinTreeOffset);
        memcpy(tree + 0x1c, &window, sizeof(window));
        memcpy(lobby.data() + 0x163c, &HostId, 4);
        std::array<HTREEITEM, 3> players{};
        for (int player = 0; player < 3; ++player)
        {
            TVINSERTSTRUCTA row{}; row.hParent = TVI_ROOT; row.hInsertAfter = TVI_LAST;
            row.item.mask = TVIF_TEXT; row.item.pszText = const_cast<char*>(player == 0 ? "Host" : player == 1 ? "Alice" : "Bob");
            players[player] = reinterpret_cast<HTREEITEM>(SendMessageA(window, TVM_INSERTITEMA, 0, reinterpret_cast<LPARAM>(&row)));
        }
        for (int slot = 0; slot < 6; ++slot)
        {
            TVINSERTSTRUCTA row{}; row.hParent = players[slot / 2]; row.hInsertAfter = TVI_LAST;
            row.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
            row.item.pszText = const_cast<char*>(OnlineNames[slot]); row.item.lParam = 0x11000000 + slot;
            row.item.iImage = row.item.iSelectedImage = slot == 1 ? 2 : 0;
            *reinterpret_cast<HTREEITEM*>(tree + 0x90 + slot * 4) = reinterpret_cast<HTREEITEM>(
                SendMessageA(window, TVM_INSERTITEMA, 0, reinterpret_cast<LPARAM>(&row)));
            strcpy_s(reinterpret_cast<char*>(tree + 0x40e + slot * 0x108), 17, OnlineNames[slot]);
        }
        for (HTREEITEM player : players) TreeView_Expand(window, player, TVE_EXPAND);
        OnlineTreeRestore restore{ TreeView_GetImageList(window, TVSIL_STATE), TreeView_GetItemHeight(window) };
        Check(SetWindowSubclass(window, ObserveOnlineTree, 4, reinterpret_cast<DWORD_PTR>(&restore)), "observe online tree cleanup");
        ON::Lobby(lobby.data(), hosting, false, true);
        if (!hosting) Check(ON::Adopt(state, HostId), "client displays the authoritative host colour state");
        HFONT previousFont = nullptr;
        for (int dpi : { 96, 144, 192 })
        {
            HFONT font = CreateFontW(-MulDiv(13, dpi, 96), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH, L"Tahoma");
            SendMessageW(window, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
            if (previousFont) DeleteObject(previousFont); previousFont = font;
            auto images = static_cast<ON::TreeImages*>(GetPropW(window, ON::TreeProperty));
            Check(images && ImageList_GetImageCount(images->images) == 7 && TreeView_GetItemHeight(window) >= images->size + 2 &&
                TreeView_GetImageList(window, TVSIL_NORMAL) == sprites, "DPI-scaled squares coexist with the unchanged native sprite list");
            HDC screen = GetDC(nullptr), dc = CreateCompatibleDC(screen);
            HBITMAP rendered = CreateCompatibleBitmap(screen, 480, 420); ReleaseDC(nullptr, screen);
            HGDIOBJ old = SelectObject(dc, rendered);
            RECT surface{ 0, 0, 480, 420 }; FillRect(dc, &surface, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
            SendMessageW(window, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(dc), PRF_CLIENT | PRF_ERASEBKGND);
            for (int slot = 0; slot < 6; ++slot)
            {
                TVITEMA item{}; item.mask = TVIF_STATE | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
                item.stateMask = TVIS_STATEIMAGEMASK;
                item.hItem = *reinterpret_cast<HTREEITEM*>(tree + 0x90 + slot * 4);
                Check(SendMessageA(window, TVM_GETITEMA, 0, reinterpret_cast<LPARAM>(&item)), "read online team image state");
                item.state &= TVIS_STATEIMAGEMASK;
                const POINT point = OnlineSquare(window, item.hItem);
                if (item.state != static_cast<UINT>(INDEXTOSTATEIMAGEMASK(slot + 1)) || GetPixel(dc, point.x, point.y) != ExpectedColours[slot])
                    fprintf(stderr, "Online tree host=%d dpi=%d slot=%d state=%x point=%ld,%ld pixel=%06lx expected=%06lx\n",
                        hosting, dpi, slot, item.state, point.x, point.y, GetPixel(dc, point.x, point.y), ExpectedColours[slot]);
                Check(item.state == static_cast<UINT>(INDEXTOSTATEIMAGEMASK(slot + 1)) && GetPixel(dc, point.x, point.y) == ExpectedColours[slot],
                    "all six online square colours render beside their own team at every DPI");
                Check(item.iImage == (slot == 1 ? 2 : 0) && item.iSelectedImage == item.iImage && item.lParam == 0x11000000 + slot,
                    "alliance squares preserve native human/CPU images and team item data");
            }
            SelectObject(dc, old); DeleteObject(rendered); DeleteDC(dc);
            const HTREEITEM ownTeam = *reinterpret_cast<HTREEITEM*>(tree + 0x90 + 2 * 4);
            const HTREEITEM otherTeam = *reinterpret_cast<HTREEITEM*>(tree + 0x90);
            const POINT own = OnlineSquare(window, ownTeam), other = OnlineSquare(window, otherTeam);
            const int sends = onlineSends;
            OnlineMouse(window, own, false);
            if (hosting)
            {
                Check(ON::session.teams[2].colour == 4 && onlineBroadcast, "host left click changes any team's colour and broadcasts it");
                OnlineMouse(window, own, true);
                Check(ON::session.teams[2].colour == 3, "host right click reverses the selected colour");
            }
            else
            {
                Check(onlineSends == sends + 1 && onlineTarget == HostId && ON::session.teams[2].colour == 3,
                    "client click on its own team sends a request without changing authoritative state");
                OnlineMouse(window, other, false); OnlineMouse(window, other, true);
                Check(onlineSends == sends + 1, "client cannot change other teams with either mouse button");
            }
            TreeView_SelectItem(window, ownTeam);
            SendMessageW(window, WM_KEYDOWN, VK_SPACE, 0); SendMessageW(window, WM_CHAR, L' ', 0); SendMessageW(window, WM_KEYUP, VK_SPACE, 0);
            TVITEMA item{}; item.mask = TVIF_STATE; item.stateMask = TVIS_STATEIMAGEMASK; item.hItem = ownTeam;
            SendMessageA(window, TVM_GETITEMA, 0, reinterpret_cast<LPARAM>(&item));
            Check((item.state & TVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK(3) && ON::session.teams[2].colour == 3,
                "online colours remain mouse-only");
        }
        DestroyWindow(parent); DeleteObject(previousFont);
        Check(restore.restored && ON::session.tree == nullptr, "online tree cleanup restores image list and item height");
    }
    ImageList_Destroy(sprites); FreeLibrary(resources);
}

static void OnlineGuardTests()
{
    FillOnlineGame(); LocalPlayer(HostId);
    std::vector<BYTE> hostLobby(0x5000), joinLobby(0x5000);
    ON::Lobby(hostLobby.data(), true, false, true);
    const auto start = NativeOnlinePacket(14);
    FN::SendPacketToAll(ON::image + ON::TransportRva, nullptr, HostId, start.data(), static_cast<uint32_t>(start.size()));
    const auto valid = onlineSent;
    ON::session = ON::Session{}; LocalPlayer(AliceId); memcpy(joinLobby.data() + 0x163c, &HostId, 4);
    ON::Lobby(joinLobby.data(), false, false, true);
    SYSTEM_INFO info{}; GetSystemInfo(&info);
    BYTE* memory = static_cast<BYTE*>(VirtualAlloc(nullptr, 2 * info.dwPageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    DWORD previous; Check(memory && VirtualProtect(memory + info.dwPageSize, info.dwPageSize, PAGE_NOACCESS, &previous), "guard alliance receive payload boundaries");
    for (uint32_t reliable : { 0u, 1u })
    {
        memcpy(ON::image + 0x188b14, &reliable, 4);
        for (size_t length : { size_t(0), size_t(1), size_t(3), size_t(4), size_t(19), size_t(20), size_t(131), size_t(132), valid.size() - 1 })
        {
            BYTE* packet = memory + info.dwPageSize - length;
            if (length) memcpy(packet, valid.data(), length);
            const int before = onlineReceives;
            FN::ReceiveLobbyPacket(joinLobby.data(), nullptr, HostId, packet, static_cast<uint32_t>(length) + reliable * 4);
            Check(onlineReceives == before, "truncated latest-version starts never reach the native decoder or read the guard page");
        }
        for (int fault = 0; fault < 9; ++fault)
        {
            auto packet = valid;
            ON::State state; memcpy(&state, packet.data() + packet.size() - sizeof(state), sizeof(state));
            switch (fault)
            {
            case 0: state.magic[0] = 'X'; break;
            case 1: state.version = 2; break;
            case 2: state.count = 7; break;
            case 3: state.teams[0].colour = 7; break;
            case 4: state.teams[0].owner = 0; break;
            case 5: memset(state.teams[0].name, 'X', 17); break;
            case 6: strcpy_s(state.teams[1].name, state.teams[0].name); break;
            case 7: state.revision = 0; break;
            case 8: state.footer[4] = 1; break;
            }
            memcpy(packet.data() + packet.size() - sizeof(state), &state, sizeof(state));
            const int before = onlineReceives;
            FN::ReceiveLobbyPacket(joinLobby.data(), nullptr, HostId, packet.data(), static_cast<uint32_t>(packet.size()) + reliable * 4);
            Check(onlineReceives == before, "invalid versions, mappings and ownership cannot start a network match");
        }
    }
    VirtualFree(memory, 0, MEM_RELEASE);
}

static std::vector<BYTE> onlineMapPacket;
static bool OnlineMapControl(void*, uint32_t, uint32_t, const void* packet, uint32_t length, bool)
{
    onlineMapPacket.assign(static_cast<const BYTE*>(packet), static_cast<const BYTE*>(packet) + length);
    return true;
}
static void OnlineMapTests(const char* frontend)
{
    wchar_t samplePath[MAX_PATH]{};
    Check(MultiByteToWideChar(CP_ACP, 0, frontend, -1, samplePath, MAX_PATH) > 0, "resolve supplied game's map directory");
    wchar_t* filename = wcsrchr(samplePath, L'\\'); Check(filename != nullptr, "frontend path has its game directory");
    wcscpy_s(filename + 1, MAX_PATH - (filename + 1 - samplePath), L"Levels\\Import\\Online Worms\\01 Path to Hell.dat");
    const auto map = CM::Load(samplePath);
    const std::wstring dataDirectory = CM::GameDirectory() + L"\\Data", land = dataDirectory + L"\\land.dat";
    Check(GetFileAttributesW(dataDirectory.c_str()) == INVALID_FILE_ATTRIBUTES && CreateDirectoryW(dataDirectory.c_str(), nullptr),
        "isolate combined map/alliance publication in the test executable directory");
    const auto sendControl = CM::controlSendAttempt;
    CM::controlSendAttempt = OnlineMapControl;
    CM::enabled = true;
    for (uint32_t reliable : { 0u, 1u })
    {
        FillOnlineGame(); LocalPlayer(HostId);
        std::vector<BYTE> hostLobby(0x5000), joinLobby(0x5000);
        ON::Lobby(hostLobby.data(), true, false, true);
        const auto state = ON::CurrentState();
        CM::ResetNetwork(); CM::network.hosting = true;
        CM::network.goRecipients = { AliceId, BobId };
        CM::network.roundMap.reset(new CM::Map(map));
        CM::network.round.revision = 9; CM::network.round.size = static_cast<uint32_t>(map.bytes.size());
        CM::network.round.crc = CM::Checksum(map.bytes);
        const auto identity = CM::network.round;
        memcpy(ON::image + 0x188b14, &reliable, 4);
        const auto native = NativeOnlinePacket(14);
        FN::SendPacketToAll(ON::image + ON::TransportRva, nullptr, HostId, native.data(), static_cast<uint32_t>(native.size()));
        if (!reliable)
        {
            Check(!CM::network.controls.empty(), "raw transport queues the combined native start");
            onlineMapPacket = CM::network.controls.front().packet;
        }
        const auto packet = onlineMapPacket;
        Check(packet.size() == 20 + 112 + sizeof(ON::State) + sizeof(CM::MapIdentity),
            "colour-map trailer follows the composed native CPU and alliance metadata");
        CM::MapIdentity receivedIdentity;
        Check(CM::ReadTrailer(packet.data(), static_cast<uint32_t>(packet.size()), receivedIdentity) && CM::SameIdentity(identity, receivedIdentity),
            "outer map identity remains readable");
        Check(!CM::ReadTrailer(packet.data(), static_cast<uint32_t>(packet.size() - sizeof(CM::MapIdentity)), receivedIdentity),
            "alliance metadata is never mistaken for a map trailer");
        CM::ResetNetwork(); CM::network.host = HostId; CM::network.joining = true;
        CM::network.roundMap.reset(new CM::Map(map)); CM::network.round = identity;
        ON::session = ON::Session{}; LocalPlayer(AliceId); memcpy(joinLobby.data() + 0x163c, &HostId, 4);
        ON::Lobby(joinLobby.data(), false, false, true);
        const int before = onlineReceives;
        FN::ReceiveLobbyPacket(joinLobby.data(), nullptr, HostId, packet.data(), static_cast<uint32_t>(packet.size()) + reliable * 4);
        Check(onlineReceives == before + 1 && onlineNativeLength == 132 + reliable * 4 &&
            ON::Valid(ON::session.round) && ON::session.round.teams[5].colour == state.teams[5].colour && NT::receivedTeams[1].skill == 50,
            "both transports independently decode map, alliance and CPU data before native startup");
        Check(CM::Load(land.c_str()).bytes == map.bytes, "combined start publishes exactly the selected colour map");
    }
    CM::ResetNetwork(); CM::enabled = false; CM::controlSendAttempt = sendControl;
    Check(DeleteFileW(land.c_str()) && RemoveDirectoryW(dataDirectory.c_str()), "clean combined map publication");
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 2, "Usage: OnlineAlliancesTests.exe frontend.exe");
        Check(OfflineAlliancesTestMain(argc, argv) == 0, "offline alliances remain unchanged");
        HANDLE file = CreateFileA(argv[1], GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
        Check(file != INVALID_HANDLE_VALUE, "open frontend for online alliance checks");
        HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY | SEC_IMAGE, 0, 0, nullptr);
        BYTE* image = static_cast<BYTE*>(MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0));
        Check(image && ON::InstallInImage(image), "validate online alliance native integration sites");
        FN::image = NT::image = CM::image = image;
        NT::enabled = true; CM::enabled = EO::enabled = false;
        FN::originalSendToPlayer = reinterpret_cast<FN::SendToPlayer>(OnlineSendPlayer);
        FN::originalSendToAll = reinterpret_cast<FN::SendToAll>(OnlineSendAll);
        FN::originalReceivePacket = reinterpret_cast<FN::ReceivePacket>(OnlineReceiveNative);
        NT::originalReceiveRoundPacket = reinterpret_cast<NT::ReceiveRoundPacket>(OnlineReceiveRoundNative);
        CM::originalHostReceive = reinterpret_cast<CM::NativeReceive>(OnlineReceiveNative);
        ON::Callbacks().send = OnlineSend;
        OnlineModelTests(); OnlinePacketTests(); OnlineJoinSequenceTests(image); OnlineLaunchTests(image); OnlineTreeTests(argv[1]); OnlineGuardTests(); OnlineMapTests(argv[1]);
        ON::Callbacks() = {}; ON::session = ON::Session{};
        UnmapViewOfFile(image); CloseHandle(mapping); CloseHandle(file);
        puts("PASS: online ownership, colour cycling, least-used assignment, late joins, composed CPU/alliance packets, both transports and subsequent-round dispatch");
        return 0;
    }
    catch (const std::exception& error) { fprintf(stderr, "FAIL: %s\n", error.what()); return 1; }
}
