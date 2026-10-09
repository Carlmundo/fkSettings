# Computer teams in network games

The hosting dialog now includes saved computer teams in its available-team list,
with the native Easy/Medium/Hard icons. Add and remove them using the usual team
controls. They count toward the existing total-team and per-player limits and
retain the difficulty stored in the team editor. Joining players still add human
teams; the host supplies the computer teams.

Selected computer teams in both the hosting dialog (203) and joining dialog
(205) retain their Easy/Medium/Hard sprite from
bitmap 244 in the **Teams** box, including when selected: one computer for skills
1–33, two for 34–66, and three for 67–100. Human team rows keep their worm icon.
The image-list and tree-control APIs used are compatible with Windows XP.
Joining players see these icons while waiting in the lobby, including teams
already selected before they joined and teams subsequently removed or re-added.
Computer teams keep their difficulty in subsequent rounds of matches requiring
more than one victory, on both the host and joining computers.
The end-of-round team results also show the matching one-, two-, or
three-computer icon on both computers.

Install the new `fkSettings.dll` in the Worms 2 game directory on **every player's
computer**, then restart `frontend.exe`. An older DLL or an unmodified client
does not understand the AI settings and produces different game data.

## Implementation

`fkSettings/NetworkTeams.cpp` supports the same x86 frontend as the secret weapon
editor: PE timestamp `0x3587BE19`. Translated resource variants may have different
image sizes. It checks the executable architecture, timestamp, and every
modified instruction before installing. The file on disk is
never patched.

The available-team list initializer takes a filter mode: 0 shows human teams,
1 also shows computer teams. Only the hosting dialog's argument at RVA `0x3264A`
changes from 0 to 1. The joining and local-game dialogs retain their native modes.

In the lobby, each selected team belongs to a network player. Its controller byte
is that player's index, which the native add/remove, team-limit, and ownership
code uses to index player records. Replacing it with an AI controller while the
lobby is open would break those operations.

The game engine already represents computer controllers as negative skill levels
from -1 to -100. The DLL hooks the native game-data writer at RVA `0x26E38` and
changes these bytes only while serializing a network launch. It recognizes the
host and joining launch call sites by return addresses `0x35950` and `0x3B3D5`.
The results dialogs use different launch calls for subsequent rounds, at return
addresses `0x61B13` (host) and `0x6406A` (joining player). These calls also apply
the CPU controller conversion. Previously, only the first-round calls were
recognized, so later rounds wrote the lobby owner's controller instead of AI.
Other writers, including local-game launches, run unchanged. The lobby's owner
bytes are restored after writing, including failed writes and C++ exceptions.

The host identifies its computer teams through the saved-team indices at
game-object offsets `4 + slot * 4` and saved skill fields at image RVA
`0x19D720 + savedIndex * 0x19C`. Remote teams have saved index -1 and are never
classified from the host's local team database. The native team-order swap also
swaps saved indices, preserving this association.

The selected-team tree's native row builder at RVA `0x4B233` always inserts a
worm icon (index 5). After it creates a computer-team row, the DLL sets that
row's normal and selected images to the corresponding difficulty sprite from
bitmap 244, using the native available-team list's thresholds at RVA `0x5CA01`.
The tree gets a private image list that preserves all native bitmap 340 sprites
and appends the three CPU sprites. Its cells widen from 22 to 32 pixels so the
32×16 CPU sprites retain every original pixel. The shared frontend image list
is unchanged, and a window subclass releases the private list when the tree
closes. The row is located using its saved index and team name; tree row indices
can differ from game slots. The joining tree at dialog-object offset `0x3648`
uses the host's lobby metadata instead of the joining player's saved-team
database, including for remote rows with saved index -1. No controller bytes
or tree selection/ownership metadata change. Remote human rows retain their
native icon even when their name or saved index matches a local CPU team.

The end-of-round team list already contains bitmap 244's native 32×16 sprites.
Its native refresh selects CPU images only for negative controller bytes, so
network CPU teams would otherwise get the worm image for their lobby owner.
The DLL hooks results initialization at RVA `0x881D6` and refresh at `0x88336`.
Initialization recognizes the host and joining callers at return addresses
`0x61240` and `0x6351E` and attaches that context to the list control. After each
native refresh, the DLL selects image 1/2/3 using the host's saved skills or the
joining player's received launch metadata. Rows are matched by their native
game-slot item data and team name, preserving victory totals and selection.
The existing image list is reused without copying or widening it. Local-game
results retain native controller-based icons, and the context is removed when
the control closes. No extra network messages are needed.

Team records are at game-object offsets `0x484 + slot * 0x108`. Each record begins
with the controller byte, victories won, then a 17-byte team name. The writer omits
the object's `0x1C` prefix, so controllers are at `game.dat` offsets
`0x468 + slot * 0x108`. The native `0xCD4`-byte file format stays intact.

## Network launch message

Every frontend writes its own game configuration at launch. The DLL therefore
extends start packet type 14 when the host has computer teams. It reuses the
send/receive hooks installed by `FrontendNetwork.cpp`, avoiding competing detours
on the same native functions.

| Packet offset | Bytes | Contents |
| --- | --- | --- |
| 0 | 20 | Unchanged native type and session GUID |
| 20 | 4 | ASCII `FKA1` |
| 24 | 108 | Six records: 17-byte terminated team name, one-byte skill |

Skill 0 means no computer team in that entry. Valid AI skills are 1–100. The
complete packet is 132 bytes; matches containing only humans keep the original
20-byte packet. Targeted sends and broadcasts use the same extension.

The joining lobby accepts settings only from its current host, before the native
start handler runs. It validates the length, signature, difficulty, terminated
names, and duplicate AI names. An absent or invalid host extension clears prior
AI settings. Truncated native start messages are dropped. Reliable transport's
reported length includes an already-skipped sequence DWORD; the shared receiver
subtracts those four bytes before parsing, as it does for weapon schemes.

Clients match AI teams by name when writing their game data, so native team
reordering does not change which team gets each difficulty. Ownership remains
available to the lobby throughout team selection and after launching.

Subsequent rounds send the same extended start message through the existing
broadcast hook. Joining players then use a separate results-dialog dispatcher
at RVA `0x63AC6`, which is also hooked. The results dialog's constructor resolves
the host player ID into offset `0xA8`. The DLL uses that ID to validate and accept
the next round's CPU settings before the native start handler saves `game.dat`.
This dispatcher applies the same transport length correction, extension
validation, stale-state clearing, and truncated-start rejection as the initial
lobby dispatcher. Its original instructions and the new launch callers are
validated before installation, and hook setup rolls back on failure.

## Lobby icon messages

The host also appends the same 112-byte `FKA1` name/skill extension to the native
lobby snapshot (type 5, `0x10DE` bytes) and team-add broadcast (type 10, `0xC8`
bytes) when it has CPU teams. Every native byte, including the snapshot header,
is preserved. Human-only lobbies keep the native packet formats. The joining
player's team-add requests use the separate native send path and stay unchanged.

The shared receiver marks the joining tree before native processing, so its
rows never infer CPU difficulty from the joining player's local saved teams.
After the native snapshot establishes the host at dialog offset `0x163C`, the
DLL validates the extension and refreshes existing rows. This also updates
rows that the native refresh kept rather than recreated. Subsequent team-add
messages replace the cached difficulty data; absent or invalid extensions
clear it and restore any stale CPU images to the native worm icon. The native
team-remove message (type 11, `0x1C` bytes) removes that name from the cache.
Add/remove metadata is accepted only from the current host. Closing the tree
releases its images and clears its cached CPU metadata.

Lobby icon metadata is separate from launch metadata. The start packet remains
the source of AI controllers for `game.dat`. Both transport length conventions
are handled, and truncated snapshots/add/remove packets are dropped before
the native dispatcher reads their fixed payloads.

## Build and verification

The Release DLL uses the project's `v141_xp` compiler, C++14, MFC, and Windows
SDK 7.1A for Windows XP compatibility. Test executables use v143 and SDK 8.1:

```powershell
MSBuild.exe fkSettings/fkSettings.vcxproj /p:Configuration=Release /p:Platform=Win32 /p:OutDir="$PWD/Release/"
MSBuild.exe tests/NetworkTeamsTests.vcxproj /p:Configuration=Release /p:Platform=Win32
MSBuild.exe tests/SecretWeaponsTests.vcxproj /p:Configuration=Release /p:Platform=Win32
./Release/NetworkTeamsTests.exe 'D:\Games\Worms 2\frontend.exe'
./Release/SecretWeaponsTests.exe 'D:\Games\Worms 2\frontend.exe'
```

Verified on the supplied frontend:

- DLL and both test executables build successfully.
- Host and client serialization agrees for humans and AI difficulties 1, 50,
  and 100; only the intended controller bytes change.
- Ownership is restored after success, failure, and a C++ exception.
- A real Windows tree-control fixture verifies both computer icon states at all
  difficulty boundaries, exact bitmap 244 pixels and transparency, unchanged
  shared images, re-added rows, window cleanup, and preserved owner/selection data.
- Joining-dialog fixtures cover initial host establishment, late joins, all CPU
  difficulty boundaries, native tree rebuilds, add/remove/re-add updates,
  conflicting local saved teams, invalid/legacy metadata, and guarded lobby
  packet buffers on both transports.
- A real Windows list-control fixture verifies results-screen CPU icons at
  every difficulty boundary, host and client skill sources, repeated refreshes,
  reordered rows, unchanged native images, human icons, victory totals,
  selection, local results, and cleanup. Supplied native results initialization
  callers also execute the production hook with their real return addresses.
- AI settings survive team reordering and both transport length conventions.
- Non-host messages, stale state, malformed names, duplicate names, invalid
  difficulty, and truncated packets are covered, including guarded receive buffers.
- The game-writer hook and host filter install alongside all ten secret-weapon
  hooks on a non-running `SEC_IMAGE` mapping of the real executable.
- The supplied first-round and subsequent host/client launch instructions
  execute the production detour with their native return addresses. Later-round
  checks cover difficulties 1, 50, and 100 with victories required set to 2,
  preserving human controllers, victories won, and lobby ownership. Other
  writer callers retain native controller handling.
- Repeated results-dialog start packets update joining players' exact CPU
  difficulty before native serialization on both transports, including guarded
  malformed packets and messages from other players.
- Existing secret-weapon scheme, network, stock, editor, and focus tests pass.

A live multiplayer match has not been verified. The transport and serialization
tests use controlled callbacks; the mapped-executable checks validate native
instructions and hook coexistence without launching the game.
