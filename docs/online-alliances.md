# Online alliances

With at least two teams, a colour square appears beside each team in the online
Players tree. Left-click cycles forwards; right-click cycles backwards. Space
does not change colours. The host can edit every team, and each joining player
can edit their own teams. Human/CPU icons and native selection/removal behaviour
remain intact. Sizes follow the already DPI-scaled font.

The host assigns Red, Blue, Green, Yellow, Purple and Teal in addition order.
Later additions receive the least-used colour, with ties resolved in that order.
Removing a team preserves the others while at least two remain. Zero/one team
clears assignments; returning to two resets Red and Blue. Names identify teams
independently of tree sorting and shuffled game slots.

Clients send direction requests rather than changing their display optimistically.
The host checks the sender against the native player-ID/ownership table and
broadcasts the complete state after an accepted change. Settings freeze at
launch. Late joins, team additions/removals and each subsequent start receive the
current assignments. All participants must use the latest frontend DLL and the
engine's alliance extension. Missing or invalid alliance start data is rejected.

## Turn order, files and results

The first round uses the same shuffled-colour interleaving as
[offline play](offline-alliances.md#turn-order). Native team randomization and
the name-based colour map give every frontend the same order. Later rounds keep
that order and advance the starting index by one.

Every frontend writes a seven-byte `Data\teams.dat` in final `game.dat` slot
order. CPU skills still serialize as negative controllers and native network
ownership is restored afterwards. The shared sidecar hook forwards the native call site
to the CPU writer hook so both extensions identify host/client launches correctly.
Failed or mismatched launch data blocks engine startup and clears the scoring
snapshot. Ordinary non-network launches clear stale online sidecars.

Network launches populate the existing frozen name/colour/score context. The
Worms2 and Game statistics tabs therefore award every winning ally, including
defeated members, while native aggregation and saved statistics remain in use.
The single-winner sentence stays hidden for multiple game winners.

## Protocol

The shared send pipeline appends alliance metadata after any CPU extension and
before ColourMaps' outer trailer. Receivers strip them in reverse order. This
works with both raw and reliable length conventions. The map control queue is
bounded at 512 bytes, accommodating the 312-byte combined start packet.

Native types 5 (snapshot), 10 (team added), 11 (team removed) and 14 (start) carry
this 164-byte state trailer:

| Offset | Bytes | Contents |
| --- | --- | --- |
| 0 | 4 | ASCII `FKAL` |
| 4 | 4 | Protocol version 1 |
| 8 | 4 | Nonzero revision |
| 12 | 4 | Team count, 0–6 |
| 16 | 132 | Six packed records: 17-byte terminated name, colour byte, owner player-ID DWORD |
| 148 | 16 | ASCII `ALND` followed by twelve zeros |

Unused records are zero; active names are unique and at most 16 bytes, owner IDs
are nonzero, and colours are 1–6 (zero with fewer than two teams). The fixed
footer prevents team names being mistaken for a map trailer. Older revisions
cannot undo newer choices.

Custom type `0x464B4131` is a DWORD type followed by this state. Type `0x464B4132`
is a 26-byte request: type DWORD, version DWORD, 17-byte name and signed direction
byte (+1 or -1). Custom packets never reach native dispatch. Clients cannot
publish settings; the host rejects non-owner requests and changes after launch.

`Alliances.cpp/.h` supplies the common palette, turn ordering, sidecar writer and
results hooks. `OnlineAlliances.cpp` validates native first-round shuffle and first/subsequent
host/client writer callers, then registers callbacks with existing lifecycle,
tree, packet, writer, shuffle and engine hooks. No competing detours are added.
Tree state-image index 0 is reserved; indices 1–6 hold the palette. Original state
images and item height are restored before destruction.

The shared network module hooks joining-lobby initialization at RVA `0x3975E`.
The native constructor and initializer leave the host-ID DWORD at dialog offset
`0x163C` uninitialized; only the first type-5 snapshot sets it. Clear that field
and reset the alliance session before native initialization starts. Otherwise a
stale host ID can reject the initial snapshot and later team packets, while
type-6 player-name updates still appear. This also handles leaving and rejoining
with the same dialog address. Once the snapshot establishes the host, the normal
host-only checks remain in force.

## Verification

```powershell
MSBuild.exe tests/OnlineAlliancesTests.vcxproj /p:Configuration=Release /p:Platform=Win32
./Release/OnlineAlliancesTests.exe 'D:\Games\Worms 2\frontend.exe'
```

Checks cover ownership, least-used colours, host/client mouse input, every colour
at 100%/150%/200%, CPU sprites, cleanup, late joins, revisions, composed CPU/map
packets, both transports, guarded truncated payloads and invalid versions/mappings.
Joining-init and receive hooks are executed with zero, old and poisoned host IDs,
empty/human/CPU host rosters, a third player's name/team updates, repeated dialog
reuse and initialization failure.
Actual native host/client call sites execute production shuffle and writer hooks,
checking identical launch bytes, CPU controllers, later rounds, allied scoring,
failures and non-network resets. A four-round, three-alliance match checks native
score aggregation and both match-completion decisions after every round. The
offline suite also runs. Live TCP/IP/IPX
checks are listed in `tests/ManualTests.md`.
