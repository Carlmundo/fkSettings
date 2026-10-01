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
static int rowBuilds = 0;
static BYTE ownerDuringRowBuild = 0;
static void __fastcall BuildTreeRow(void* object, void*, const BYTE* team, int savedIndex)
{
    BYTE* tree = static_cast<BYTE*>(object);
    HWND window = *reinterpret_cast<HWND*>(tree + 0x1c);
    // A free tree row need not have the same index as the team's game slot.
    const size_t slot = 4;
    ownerDuringRowBuild = team[0];
    ++rowBuilds;
    HTREEITEM previous = *reinterpret_cast<HTREEITEM*>(tree + 0x90 + slot * 4);
    if (previous) SendMessageA(window, TVM_DELETEITEM, 0, reinterpret_cast<LPARAM>(previous));
    TVINSERTSTRUCTA insert{};
    insert.hParent = TVI_ROOT;
    insert.hInsertAfter = TVI_LAST;
    insert.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
    insert.item.pszText = const_cast<char*>(reinterpret_cast<const char*>(team + 2));
    insert.item.iImage = insert.item.iSelectedImage = 5;
    insert.item.lParam = 0x11000004;
    *reinterpret_cast<HTREEITEM*>(tree + 0x90 + slot * 4) = reinterpret_cast<HTREEITEM>(
        SendMessageA(window, TVM_INSERTITEMA, 0, reinterpret_cast<LPARAM>(&insert)));
    memcpy(tree + 0x40e + slot * NT::TeamStride, team + 2, 17);
    memcpy(tree + 0x40 + slot * 4, &savedIndex, sizeof(savedIndex));
}
static void __fastcall ReceiveLobby(void* object, void*, uint32_t sender, const void* packet, uint32_t)
{
    ++received;
    uint32_t type;
    memcpy(&type, packet, sizeof(type));
    BYTE* tree = static_cast<BYTE*>(object) + 0x3648;
    const auto refreshRow = [&](const BYTE* team, int savedIndex)
    {
        // The real native refresh retains existing rows with the same name.
        const char* previous = reinterpret_cast<const char*>(tree + 0x40e + 4 * NT::TeamStride);
        if (strcmp(previous, reinterpret_cast<const char*>(team + 2)) != 0)
            NT::AddComputerTeamRow(tree, nullptr, team, savedIndex);
    };
    if (type == 5)
    {
        // The native snapshot establishes the host before rebuilding the tree.
        memcpy(static_cast<BYTE*>(object) + 0x163c, &sender, sizeof(sender));
        refreshRow(static_cast<const BYTE*>(packet) + 8 + 0x468, 1);
    }
    else if (type == 10)
    {
        std::array<BYTE, NT::TeamStride> team{};
        team[0] = 2;
        memcpy(team.data() + 2, static_cast<const BYTE*>(packet) + 8, 17);
        refreshRow(team.data(), -1);
    }
    else if (type == 11)
    {
        HTREEITEM item = *reinterpret_cast<HTREEITEM*>(tree + 0x90 + 4 * 4);
        SendMessageA(*reinterpret_cast<HWND*>(tree + 0x1c), TVM_DELETEITEM, 0, reinterpret_cast<LPARAM>(item));
        memset(tree + 0x40e + 4 * NT::TeamStride, 0, 17);
        *reinterpret_cast<HTREEITEM*>(tree + 0x90 + 4 * 4) = nullptr;
    }
}
static void CheckComputerSprite(HIMAGELIST images, int icon, HBITMAP bitmap, int level)
{
    HDC screen = GetDC(nullptr), source = CreateCompatibleDC(screen), destination = CreateCompatibleDC(screen);
    HBITMAP rendered = CreateCompatibleBitmap(screen, 32, 18);
    ReleaseDC(nullptr, screen);
    HGDIOBJ oldSource = SelectObject(source, bitmap), oldDestination = SelectObject(destination, rendered);
    RECT area{ 0, 0, 32, 18 };
    HBRUSH background = CreateSolidBrush(RGB(255, 0, 255));
    FillRect(destination, &area, background);
    DeleteObject(background);
    Check(ImageList_Draw(images, icon, destination, 0, 0, ILD_TRANSPARENT) != FALSE, "draw CPU sprite");
    bool identical = true;
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 32; ++x)
            identical = identical && GetPixel(source, (level + 1) * 32 + x, y) == GetPixel(destination, x, y + 1);
    SelectObject(source, oldSource);
    SelectObject(destination, oldDestination);
    DeleteObject(rendered);
    DeleteDC(source);
    DeleteDC(destination);
    Check(identical, "CPU sprite retains every bitmap 244 pixel, including transparency");
}
static void TeamIconTests(const char* path)
{
    INITCOMMONCONTROLSEX controls{ sizeof(controls), ICC_TREEVIEW_CLASSES };
    Check(InitCommonControlsEx(&controls) != FALSE, "initialize native tree control");
    HWND window = CreateWindowExA(0, WC_TREEVIEWA, "Team icon fixture", WS_POPUP,
        0, 0, 200, 150, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    Check(window != nullptr, "create hidden Teams tree fixture");
    HMODULE resources = LoadLibraryExA(path, nullptr, LOAD_LIBRARY_AS_DATAFILE);
    Check(resources != nullptr, "load frontend bitmap resources");
    HBITMAP nativeBitmap = static_cast<HBITMAP>(LoadImageA(resources, MAKEINTRESOURCEA(340), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
    HBITMAP computers = static_cast<HBITMAP>(LoadImageA(resources, MAKEINTRESOURCEA(244), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
    Check(nativeBitmap && computers, "load native tree and CPU sprites");
    HIMAGELIST native = ImageList_Create(22, 18, ILC_COLOR32 | ILC_MASK, 26, 0);
    Check(native && ImageList_AddMasked(native, nativeBitmap, RGB(128, 0, 128)) == 0, "create native Teams images");
    TreeView_SetImageList(window, native, TVSIL_NORMAL);
    NT::TeamImages* images = NT::PrepareTeamImages(window, computers);
    Check(images && images == NT::GetTeamImages(window), "prepare and retain per-tree CPU images");
    int width, height;
    Check(ImageList_GetIconSize(images->expanded, &width, &height) && width == 32 && height == 18,
        "CPU sprites fit without cropping or scaling");
    Check(ImageList_GetImageCount(native) == 26 && ImageList_GetImageCount(images->expanded) == 29 && images->firstComputer == 26,
        "shared native images preserved and exactly three CPU images appended");
    for (int level = 0; level < 3; ++level) CheckComputerSprite(images->expanded, 26 + level, computers, level);
    std::vector<BYTE> image(0x5b8000), tree(0xa40), team(NT::TeamStride);
    NT::image = image.data();
    memcpy(tree.data() + 0x1c, &window, sizeof(window));
    strcpy_s(reinterpret_cast<char*>(team.data() + 2), 17, "Computer");
    team[0] = 2;
    NT::originalAddTreeTeam = reinterpret_cast<NT::AddTreeTeam>(BuildTreeRow);
    const auto checkIcon = [&](int savedIndex, int skill, int expected)
    {
        SetSkill(1, skill);
        const int before = rowBuilds;
        NT::AddComputerTeamRow(tree.data(), nullptr, team.data(), savedIndex);
        TVITEMA item{};
        item.mask = TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
        item.hItem = *reinterpret_cast<HTREEITEM*>(tree.data() + 0x90 + 4 * 4);
        Check(SendMessageA(window, TVM_GETITEMA, 0, reinterpret_cast<LPARAM>(&item)) != FALSE, "read native row images");
        Check(item.iImage == expected && item.iSelectedImage == expected, "normal and selected icons match team controller");
        Check(rowBuilds == before + 1 && ownerDuringRowBuild == 2 && team[0] == 2 && item.lParam == 0x11000004,
            "native row creation, lobby ownership and selection metadata preserved");
    };
    checkIcon(1, 1, 26);
    checkIcon(1, 33, 26);
    checkIcon(1, 34, 27);
    checkIcon(1, 66, 27);
    checkIcon(1, 67, 28);
    checkIcon(1, 100, 28);
    checkIcon(1, 0, 5);
    checkIcon(-1, 100, 5); // Same name as a local AI team is still a remote human.
    checkIcon(100, 100, 5);
    checkIcon(1, 101, 5);
    checkIcon(1, -1, 5);
    Check(NT::GetTeamImages(window) == images && ImageList_GetImageCount(images->expanded) == 29,
        "re-added teams reuse their image list without accumulating sprites");

    // Use the joining dialog's actual tree offset, and deliver lobby packets
    // through the shared receiver rather than feeding the icon hook directly.
    std::vector<BYTE> join(0x3648 + 0xa40);
    BYTE* joinTree = join.data() + 0x3648;
    memcpy(joinTree + 0x1c, &window, sizeof(window));
    BYTE* game = image.data() + NT::GameRva;
    SetTeam(game, 0, "Computer", 1, 2);
    NT::enabled = true;
    SW::image = image.data();
    SW::originalSendToPlayer = reinterpret_cast<SW::SendToPlayer>(Send);
    SW::originalSendToAll = reinterpret_cast<SW::SendToAll>(Broadcast);
    SW::originalReceivePacket = reinterpret_cast<SW::ReceivePacket>(ReceiveLobby);
    std::array<BYTE, NT::NativeSnapshotPacketSize> snapshot{ 5 };
    const uint32_t snapshotSize = snapshot.size();
    memcpy(snapshot.data() + 4, &snapshotSize, sizeof(snapshotSize));
    memcpy(snapshot.data() + 8, game + 0x1c, 0xcd4);
    std::array<BYTE, NT::NativeAddTeamPacketSize> add{ 10 };
    memcpy(add.data() + 8, team.data() + 2, 17);
    std::array<BYTE, 0x1c> remove{ 11 };
    memcpy(remove.data() + 8, team.data() + 2, 17);
    constexpr uint32_t host = 123;
    const auto readIcon = [&]()
    {
        TVITEMA item{};
        item.mask = TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM;
        item.hItem = *reinterpret_cast<HTREEITEM*>(joinTree + 0x90 + 4 * 4);
        Check(SendMessageA(window, TVM_GETITEMA, 0, reinterpret_cast<LPARAM>(&item)) != FALSE, "read joining row");
        Check(item.iImage == item.iSelectedImage && item.lParam == 0x11000004 && ownerDuringRowBuild == 2,
            "joining icon preserves normal/selected states, owner and selection data");
        return item.iImage;
    };
    const auto deliver = [&](const std::vector<BYTE>& packet, int reliable)
    {
        SW::ReceiveWeaponPacket(join.data(), nullptr, host, packet.data(), static_cast<uint32_t>(packet.size()) + reliable * 4);
    };
    for (int reliable : { 0, 1 })
    {
        memcpy(image.data() + 0x188b14, &reliable, sizeof(reliable));
        memset(join.data() + 0x163c, 0, sizeof(host));
        SetSkill(1, 67);
        SW::SendWeaponPacketToPlayer(nullptr, nullptr, 9, 10, snapshot.data(), snapshot.size());
        const auto lateJoin = sent;
        Check(lateJoin.size() == snapshot.size() + sizeof(NT::Extension) &&
            memcmp(lateJoin.data(), snapshot.data(), snapshot.size()) == 0, "snapshot extension preserves every native byte");
        SetSkill(1, 0); // The joining player's local database disagrees with the host.
        deliver(lateJoin, reliable);
        Check(readIcon() == 28, "late join displays hard CPU from initial snapshot before game start");
        Check(NT::lobbyTeams[0].skill == 67, "initial snapshot establishes host before CPU metadata is accepted");
        NT::AddComputerTeamRow(joinTree, nullptr, team.data(), -1);
        Check(readIcon() == 28, "native join tree rebuild retains remote CPU icon from cached host metadata");
        for (int skill : { 1, 33, 34, 66, 67, 100 })
        {
            SetSkill(1, skill);
            SW::SendWeaponPacketToAll(nullptr, nullptr, 9, add.data(), add.size());
            const auto update = sent;
            Check(update.size() == add.size() + sizeof(NT::Extension) &&
                memcmp(update.data(), add.data(), add.size()) == 0, "team update preserves native payload");
            SetSkill(1, 100 - skill);
            const int buildsBeforeUpdate = rowBuilds;
            deliver(update, reliable);
            Check(readIcon() == 26 + (skill > 66 ? 2 : skill > 33 ? 1 : 0), "joining CPU difficulty boundaries match bitmap 244");
            Check(rowBuilds == buildsBeforeUpdate, "CPU update refreshes an existing native row without rebuilding it");
            SW::ReceiveWeaponPacket(join.data(), nullptr, host + 1, remove.data(), remove.size() + reliable * 4);
            Check(NT::lobbyTeams[0].skill == skill, "non-host removal cannot withdraw CPU metadata");
            deliver(update, reliable); // Rebuild a row removed by the fake native callback.
            deliver(std::vector<BYTE>(remove.begin(), remove.end()), reliable);
            Check(NT::lobbyTeams[0].skill == 0, "team removal clears cached difficulty");
            deliver(update, reliable);
            Check(readIcon() == 26 + (skill > 66 ? 2 : skill > 33 ? 1 : 0), "re-added remote CPU keeps its icon");
            auto invalid = update;
            invalid[add.size()] = 'X';
            deliver(invalid, reliable);
            Check(readIcon() == 5 && NT::lobbyTeams[0].skill == 0, "invalid lobby trailer clears stale CPU icon");
            deliver(update, reliable);
            deliver(std::vector<BYTE>(add.begin(), add.end()), reliable);
            Check(readIcon() == 5, "same-name human update clears CPU icon despite local CPU team");
        }
        deliver(lateJoin, reliable);
        deliver(std::vector<BYTE>(snapshot.begin(), snapshot.end()), reliable);
        Check(readIcon() == 5 && NT::lobbyTeams[0].skill == 0, "legacy or new human-only lobby snapshot clears previous CPU metadata");
        SetSkill(1, 0);
        SW::SendWeaponPacketToAll(nullptr, nullptr, 9, add.data(), add.size());
        Check(sent == std::vector<BYTE>(add.begin(), add.end()), "human-only team packets retain native format");
        SW::SendWeaponPacketToPlayer(nullptr, nullptr, 9, 10, snapshot.data(), snapshot.size());
        Check(sent == std::vector<BYTE>(snapshot.begin(), snapshot.end()), "human-only snapshots retain native format");
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        BYTE* guard = static_cast<BYTE*>(VirtualAlloc(nullptr, info.dwPageSize * 3, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        DWORD protect;
        Check(guard && VirtualProtect(guard + info.dwPageSize * 2, info.dwPageSize, PAGE_NOACCESS, &protect), "guard lobby packet boundary");
        for (const auto& valid : { lateJoin, std::vector<BYTE>(add.begin(), add.end()), std::vector<BYTE>(remove.begin(), remove.end()) })
        {
            const uint32_t nativeSize = NT::LobbyPacketSize(valid[0]);
            for (size_t size : { size_t(4), size_t(nativeSize - 1), size_t(nativeSize), size_t(valid.size() - 1), valid.size() })
            {
                BYTE* packet = guard + info.dwPageSize * 2 - size;
                memcpy(packet, valid.data(), size);
                const int before = received;
                SW::ReceiveWeaponPacket(join.data(), nullptr, host, packet, static_cast<uint32_t>(size) + reliable * 4);
                Check(received == before + (size >= nativeSize ? 1 : 0), "truncated lobby payload dropped before native handler without overread");
            }
        }
        VirtualFree(guard, 0, MEM_RELEASE);
    }
    SetSkill(1, 100);
    SW::SendWeaponPacketToPlayer(nullptr, nullptr, 9, 10, snapshot.data(), snapshot.size());
    deliver(sent, 1);
    DestroyWindow(window);
    Check(NT::GetTeamImages(window) == nullptr && ImageList_GetImageCount(native) == 26 && !NT::joiningTree && NT::lobbyTeams[0].skill == 0,
        "closing the tree removes its subclass and preserves the shared native list");
    NT::enabled = false;
    ImageList_Destroy(native);
    DeleteObject(computers);
    DeleteObject(nativeBitmap);
    FreeLibrary(resources);
    puts("PASS: Teams tree retains bitmap 244's three exact CPU sprites at native difficulty thresholds, preserves humans/owners and cleans up its private images");
    puts("PASS: joining dialog CPU icons, late-join snapshots, live add/remove/re-add updates, stale metadata, independent local database and guarded packets on both transports");
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
    std::array<BYTE, 9> treeBefore;
    memcpy(treeBefore.data(), frontend + NT::AddTreeTeamRva, treeBefore.size());
    Check(SW::InstallInImage(frontend), "shared packet hooks install");
    Check(NT::InstallInImage(frontend), "AI hook and host-only filter install alongside secret weapons");
    Check(frontend[0x3264b] == 1 && frontend[0x399fc] == 0, "only hosting dialog shows computer teams");
    Check(memcmp(before.data(), frontend + NT::WriteGameRva, before.size()) != 0, "game writer hook enabled");
    Check(memcmp(treeBefore.data(), frontend + NT::AddTreeTeamRva, treeBefore.size()) != 0, "Teams row hook enabled");
    LaunchCallSiteTests(frontend);
    Check(NT::SetHostListMode(0), "restore test list mode");
    Check(MH_Uninitialize() == MH_OK, "remove mapped image hooks");
    Check(memcmp(before.data(), frontend + NT::WriteGameRva, before.size()) == 0, "game writer restored");
    Check(memcmp(treeBefore.data(), frontend + NT::AddTreeTeamRva, treeBefore.size()) == 0, "Teams row builder restored");
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
        if (argc > 1)
        {
            TeamIconTests(argv[1]);
            HookTests(argv[1]);
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
