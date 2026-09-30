# Computer teams in network games

The hosting dialog now includes saved computer teams in its available-team list,
with the native Easy/Medium/Hard icons. Add and remove them using the usual team
controls. They count toward the existing total-team and per-player limits and
retain the difficulty stored in the team editor. Joining players still add human
teams; the host supplies the computer teams.

Install the new `fkSettings.dll` in the Worms 2 game directory on **every player's
computer**, then restart `frontend.exe`. An older DLL or an unmodified client
does not understand the AI settings and produces different game data.

## Implementation

`fkSettings/NetworkTeams.cpp` supports the same x86 frontend as the secret weapon
editor: PE timestamp `0x3587BE19`, image size `0x5B8000`. It checks the executable
identity and every modified instruction before installing. The file on disk is
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
Other writers, including local-game launches, run unchanged. The lobby's owner
bytes are restored after writing, including failed writes and C++ exceptions.

The host identifies its computer teams through the saved-team indices at
game-object offsets `4 + slot * 4` and saved skill fields at image RVA
`0x19D720 + savedIndex * 0x19C`. Remote teams have saved index -1 and are never
classified from the host's local team database. The native team-order swap also
swaps saved indices, preserving this association.

Team records are at game-object offsets `0x484 + slot * 0x108`. Each record begins
with the controller byte, worm count, then a 17-byte team name. The writer omits
the object's `0x1C` prefix, so controllers are at `game.dat` offsets
`0x468 + slot * 0x108`. The native `0xCD4`-byte file format stays intact.

## Network launch message

Every frontend writes its own game configuration at launch. The DLL therefore
extends start packet type 14 when the host has computer teams. It reuses the
send/receive hooks installed by `SecretWeapons.cpp`, avoiding competing detours
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

## Build and verification

Build with Visual Studio 2022, the v143 Win32 compiler, MFC, and Windows SDK 8.1:

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
- AI settings survive team reordering and both transport length conventions.
- Non-host messages, stale state, malformed names, duplicate names, invalid
  difficulty, and truncated packets are covered, including guarded receive buffers.
- The game-writer hook and host filter install alongside all ten secret-weapon
  hooks on a non-running `SEC_IMAGE` mapping of the real executable.
- The supplied host and client launch instructions execute the production
  detour with their native return addresses; other writer callers retain native
  controller handling.
- Existing secret-weapon scheme, network, stock, editor, and focus tests pass.

A live multiplayer match has not been verified. The transport and serialization
tests use controlled callbacks; the mapped-executable checks validate native
instructions and hook coexistence without launching the game.
