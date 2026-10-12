# 48-worm game setup

Local and network setup allows up to six teams with one to eight worms each,
for a maximum of 48 worms. Adding or removing teams no longer reduces the
maximum worms per team to share the old 18-worm allowance. Smaller rosters
remain selectable. This DLL changes the frontend only; `wkEnchancedOptions`
provides the in-game logic separately. Install the updated frontend DLL on
every computer in a network match.

## Frontend changes

`GameSetup.cpp` validates the supported x86 frontend (PE timestamp
`0x3587BE19`) and all three instruction signatures before applying changes.
It changes the total allowance from 18 to 48 in these native calculations:

| RVA | Purpose |
| --- | --- |
| `0x27678` | Game configuration maximum, used when adding teams and setting worm counts |
| `0x78DEC` | Setup control maximum after adding a team |
| `0x7924D` | Setup control maximum after removing a team |

Each calculation remains `min(8, total allowance / selected teams)`. The
existing six-team slot limits and eight-worm caps remain in place. No packet
layout changes or extra network messages are needed. Installation is independent
of the network and options hooks, so local setup also works if those are unavailable.
All patch sites must become writable before any operand changes; signature or
protection failure leaves the limits unchanged. Repeated installation is harmless.

## Launch data

The native `game.dat` already accommodates all 48 worms in its `0xCD4` bytes:

| File offset | Contents |
| --- | --- |
| `0x467` | Selected team count, maximum 6 |
| `0x468 + team * 0x108` | Team record, including controller, name, soundbank and weapon stocks |
| Team `+0x66 + worm * 20` | Worm record: two-byte health and 18-byte name |
| Team `+0x106` | Worm count, maximum 8 |

Eight worm records end exactly at team offset `0x106`, immediately before the
count byte. The sixth team's last worm also fits inside the file. The frontend
writer already saves the entire payload, so both first games and subsequent
rounds keep the native serialization path. Network AI handling continues to
change only controller bytes during serialization and restores lobby ownership.

The requested overflow condition does not occur: there is enough room in
`game.dat`, so `Data\teams.dat` is not created and no new engine-side file reader
is required.

## Verification

```powershell
MSBuild.exe tests/GameSetupTests.vcxproj /p:Configuration=Release /p:Platform=Win32
./Release/GameSetupTests.exe 'D:\Games\Worms 2\frontend.exe'
MSBuild.exe tests/NetworkTeamsTests.vcxproj /p:Configuration=Release /p:Platform=Win32
./Release/NetworkTeamsTests.exe 'D:\Games\Worms 2\frontend.exe'
MSBuild.exe tests/StartupTests.vcxproj /p:Configuration=Release /p:Platform=Win32
./Release/StartupTests.exe 'D:\Games\Worms 2\frontend.exe'
```

The setup test maps the actual executable without running its entry point. It
executes the native game maximum/setter and both setup control recalculations
for one through six selected teams, checks smaller counts and the eight-worm
cap, rejects corrupted signatures at every patch site, and checks repeated
installation. It executes the native writer's size/pointer/fwrite block against
a temporary file and verifies that all 48 distinct worm names and health values,
team counts and controllers survive in exactly `0xCD4` bytes. MFC initialization
and cleanup are outside this isolated writer test.

The network fixture uses six teams of eight with human and CPU controllers,
checking identical host/joiner payloads on both transport length conventions,
reordering and subsequent-round handling. Startup tests check coexistence with
the existing hooks. A live 48-worm match still requires manual verification with
the engine extension; these tests do not exercise the game's in-game logic.
