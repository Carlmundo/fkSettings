# Colour map importing

The **Select Level** screen has an **Import** button using the frontend's
localized string 447. The same caption is used for the file picker and import
error dialogs, through the shared `ImportStringId` constant. The button is hidden
unless the game folder contains a `Levels\Import` directory. With normal maps it sits to the right of the central
preview, inline with **Invert terrain** on the left; with a colour map, **Import**
and **Cancel** (string 19) are centred side by side beneath the image, with a
10-dialog-unit gap between the buttons and their row 11 dialog units below
**Edit terrain** (1032) to leave more space beneath the preview. The file
filter uses localized string 357 for **Terrain**. Install complete Worms 2 `.dat` terrain files in `Levels\Import` inside each player's game folder.
Selection is restricted to that folder and its subdirectories. The picker opens
there, remembers the last permitted folder for the session, and refuses files
outside the tree. Absolute network paths, parent traversal and junctions or
symbolic links inside the import tree are rejected.

The folder-selection error is the owned wide string `strImportFolder` in
`fkSettings/ColourMapsStrings.h`. `dllmain.cpp` passes the existing `language.txt`
value to `ColourMaps::SetLanguage` alongside Secret Weapons and Extended Options.
Translations cover the same 15 codes: `cs`, `de`, `en`, `es`, `es-419`, `fr`, `is`,
`it`, `nl`, `pl`, `pt`, `pt-br`, `ru`, `sv` and `zh-Hans`. Missing or unknown codes
use English; a UTF-8 BOM and surrounding whitespace are stripped before matching.
Both relative-path and picker-folder rejection use this message. It is encoded
as UTF-8 for exceptions and decoded to Unicode for the dialog, preserving accents,
Cyrillic and Chinese on Windows XP as well as newer systems.

The imported map replaces the generated thumbnails with a colour preview. Both
previews match the map's aspect ratio, fill their image area and contain no text.
The custom preview in Terrain selection extends to the right edge of **Save As...**
(1023), with its bottom 6 dialog units below the bottom of **Generate** (1031).
Terrain selection shows a centred, bold heading using localized string 726,
with the same font size and face as the native Water style and Level style labels
(**Current terrain:** in English) above the custom image. Generate or Cancel hides this
heading when normal maps return. An 8-dialog-unit gap separates the heading from
the image; the image and Import/Cancel row are lowered together to make this space.
Empty terrain uses the native preview's blue background (RGB 0, 160, 255).
This affects only the display palette; imported terrain colours and file bytes
are unchanged.
The **Terrain:** label (1028), its dropdown (1019), and the native **Current terrain:**
label beside **Save As...** (1025) are hidden while a colour map is selected and
restored on reset. Generation controls are disabled, except **Generate**,
**Water colour** and **Level style**. Level style
uses control **1021** and keeps its existing **Open** (string 501) and **Cavern**
(string 502) options. **< Random >** (string 510) is removed while importing and
restored on reset. The dropdown initially reflects the imported file. Switching
style changes the selected map's border flag without generating new terrain.
**Water colour** uses control **1020**, matches the final water resource path
in the imported file and lets you choose another native water colour. Matching
ignores case and accepts both path separators and relative prefixes. The native
owner-drawn swatches and their item data are preserved. If an imported water
resource is unavailable in the native list, no swatch is selected; its original
path remains intact until you choose a colour. Random entries are unavailable
during imports. Reset restores the water list, selection and enabled state.
The **Game controls** tab draws the same imported map in its terrain preview,
including after switching tabs or choosing another import.
**Generate** (1031) clears the imported selection, returns both previews to
native landscape rendering and restores the original controls, style list and
selection before running the normal Generate command. **Cancel** restores the
normal view and controls without running Generate. It is hidden for normal maps.
Control 1022 retains its native level list and remains disabled
for imports. An imported selection is retained
in memory for subsequent local games, even if the source
file moves or changes. Restarting the frontend clears it.

This implementation supports **local and online games**. The online host chooses
the map, style and water; joining players receive that selection automatically.
Import/Cancel/Generate and style/water editing are unavailable to joining players. All
players must use the updated DLL. Saved-game restoration and remembering imports
across restarts remain outside this change.

## Online play

Every player must already have the same map installed and use this DLL version.
No terrain files or image data are transferred. The host sends a small selection
message containing the exact path relative to `Levels\Import`, the original
file size and CRC32, the selected Open/Cavern flag and water path, and the resulting
map's size/CRC32/revision.

For example, selecting `Levels\Import\Online Worms\01 Path to Hell.dat`
sends `Online Worms\01 Path to Hell.dat`. Each joiner looks under its own game
folder's `Levels\Import`, preserving folder names and spaces. `Online Worms`
and `OnlineWorms` are different folder names. Windows filename case rules apply.
Paths use UTF-8 over the network and wide Windows file APIs, including on XP.

The joining frontend checks the local source file's size and CRC32, applies the
host's style/water choices in memory, checks the resulting size/CRC32, and writes
`Data\land.dat` atomically. Only then does it send a 32-byte success reply. The
installed source file remains untouched. Thus identical unedited installed maps
still work when the host changes the dropdowns. A failed check returns a specific
missing-file, hash-mismatch, invalid-map/path, or publication error. A read-only
`land.dat` is never unlocked automatically.

Protocol 10 uses a 44-byte reference header followed only by the UTF-8 relative
path and water resource path. The maximum reference is 1,079 bytes; ordinary
filenames need around 100 bytes. The XP fixture selects Birthday with an 83-byte
message. TCP/IP and IPX use the same small messages. Chunking, compression, packet
windows, size adaptation, map assembly and legacy file-transfer decoding have
been removed. There is no fallback that sends a missing map. The remaining
network queues contain only map references, confirmations and native startup
controls. Their bounded retries and cancellation still keep the frontend
responsive and preserve the IPX launch ordering.

Host selection, style and water changes check connected players immediately.
Native late-join snapshots go first so the joining frontend knows the host's
player ID; a path reference then loads the selection. Rapid edits discard
obsolete queued checks. Every Go and next-round Go rechecks the installed files
with a fresh revision, including retrying the same map after a failure. Delayed
replies from a previous attempt cannot release the current start. The map and
settings chosen when Go is clicked remain frozen while checking players.

The native Go handler disables the UI and starts a 30-second startup timer. It is
deferred until every player has confirmed its local map. The frontend remains
responsive while checking. The map status label and progress bar are omitted
from both tabs. Go during checking queues one start and proceeds automatically
after confirmation. Missing/different files hold back the game and record the
reason in the network log. Correct the local installation or unlock `land.dat`,
then retry Go. Reset/disconnect restores the normal terrain controls.

Reference and reply sends are paced by a 20 ms UI timer and call the validated
native DirectPlay Send entry once, preserving its guaranteed flag and reliable
sequence envelope. Busy sends remain queued; they do not enter the frontend's
blocking ACK polling loop. Unanswered references retry after one second and
cancel after 15 seconds without a confirmation. Client replies use the native
ready channel and local player ID. Native Go, prepare and ready controls
retain their nonblocking queues and map identity trailers. The IPX launcher
(native mode 1) closes its lobby DirectPlay session before `RunApplication`.
Its final start packet (14) is therefore submitted inside the send hook, while
that connection is still open, with a bounded one-second retry budget. Deferring
it to the engine hook made every Send fail against the closed session and
cancelled startup, returning to the menus. TCP/IP retains its existing queued
start and engine gate. Both engine launch paths publish the frozen map and
withhold startup after a failed submission or while barriers remain queued.
CPU teams, secret weapons and extended options retain their shared hook routing.

Diagnostics append to `Data\fkSettings-map-network.log` and mirror to
`%TEMP%\fkSettings-map-network.log`. The marker is
`map-network diagnostics 22 protocol 10`. Logs include the executable/DLL paths,
CRC32 first-use self-test (`cbf43926`), requested relative path, local validation
result, confirmations, native control/start phases and transport mode. CRC32's
table is initialized at compile time to avoid the XP dynamically-loaded DLL TLS
initialization issue. Older DLLs cannot confirm protocol 10; update all players.

## File handling

Selection validates the entire file before changing the active map. It publishes
the original bytes to `Data/land.dat`, then republishes them before each local
launch in place of running `landgen`. Normal Game controls Go, subsequent local
rounds and quick-game preparation all use the selected bytes. A final publication
immediately before the local engine launcher runs prevents an intervening native
write from restoring the previous generated map. Online preparation and launch
use the confirmed host map. Missions keep native terrain behavior.
Writes close a temporary file in the
destination directory before replacing `land.dat` atomically. Palette,
collision masks, background, terrain resource path, unknown field and object
locations are preserved byte for byte. The cavern flag is preserved unless
changed using Level style. Changing Water colour replaces only the final
byte-length-prefixed water path with `Data\Water\<colour>` and updates the outer
LND length. The original water path is preserved when selecting the same colour.
Source files are never edited. No spawn regeneration occurs.

The importer never changes file attributes. If CTerrain has locked `land.dat`,
unlock it there first. Failed selection leaves the previous selection and
destination intact. Failed publication reports `Error saving Data\land.dat`.
Failed launch publication makes the
native local engine launcher return failure before writing game data or starting
the engine. Publication failures clean up temporary files.

Supported files have the `LND\x1A` signature, accurate outer size, standard
1920 × 696 dimensions, an 8-bit palettized foreground, three 1-bit masks
(1920 × 696, 1920 × 696 and 240 × 87), and 18–32 object locations. Unused
locations may be `(-1, -1)`. Files are bounded to 8 MB. Embedded `IMG\x1A`
blocks receive bounded palette, size and compressed-copy validation. Embedded
sizes may be absolute stream end positions, as saved by Syroot, or individual
chunk lengths. Incomplete and unsupported files are rejected before use.

## Frontend integration

The preview/local hooks install independently; online packet routing shares the
secret weapon and network-team dispatchers. It targets the inspected Win32
frontend with PE timestamp `0x3587BE19`,
checks signatures before patching and rolls back partial hook installations.
It uses XP-compatible Windows APIs and existing MinHook. No executable on disk
or engine code is patched.

| RVA | Purpose |
| --- | --- |
| `0x42A44` | Select Level initialization; attach after native layout and preserve its return value |
| `0x7896A` | Game controls initialization; subclass its terrain preview control 1262 |
| `0xA349` | Quick-game terrain preparation; scope the import to local generation |
| `0x4468B` | Shared terrain preparation; classify normal Go/next-round callers as local |
| `0x46F09` | Terrain generator; publish imported bytes during local preparation |
| `0x277C4` | Engine launcher; republish for local game callers and prevent launch after failed publication |
| `0x3233D` | Host lobby initialization and lifecycle |
| `0x336F1` | First-match Go; confirm frozen terrain before native UI disable/start timer |
| `0x62214` | Next-round Go; apply the same map preflight |
| `0x34294` | First-match host ready replies |
| `0x61865` | Subsequent-round host ready replies |
| `0x1E32F` | Client ready replies; append confirmed map identity |
| `0x358A4` | First-match host engine setup; final publication |
| `0x61A81` | Subsequent-round host engine setup; final publication |
| `0x144ED` | DirectPlay engine launcher; submit pending start controls or return failure |
| `0xA0B70` | CRT spawn entry used by IPX; apply the same gate for worms2.exe |
| `0x105F7` | Native DirectPlay receive; log custom packets before native filtering |
| `0x1378D` | Native sequence filter; log custom discards while preserving its result |

The normal Go preparation returns to `0x7956C`; subsequent local rounds return
to `0x77E04`. Their engine-launch calls return to `0x79591` and `0x77E4B`.
These inspected CALL instructions are validated during installation. Host network
preparation returns to `0x35819` and `0x623E8`; client generation returns to
`0x3E266` and `0x6445C`. These calls and the native player-ID list layout are also
validated before installation.
The DirectPlay Send entry, both native channel offsets, reliable envelope and
sequence counter, and the IPX execl-to-spawn call are validated too.
Control 1020 is owner-drawn without `CBS_HASSTRINGS`. Its item data indexes the
native CStringList at RVA `0x1B5120`; names are resolved through the inspected
list layout rather than treating item data as text. Installation also validates
the native FindIndex signature at `0xBC9AF` before accessing that layout.

## Build and verification

Build Release/Win32, overriding the default installation output:

```powershell
MSBuild.exe fkSettings/fkSettings.vcxproj /t:Build /p:Configuration=Release /p:Platform=Win32 /p:OutDir="$PWD/Release/"
MSBuild.exe tests/ColourMapsTests.vcxproj /t:Build /p:Configuration=Release /p:Platform=Win32
./Release/ColourMapsTests.exe 'D:/Games/Worms 2/frontend.exe' 'C:/Users/carln/Documents/GitHub/Worms2-Maps/Levels/Import'
MSBuild.exe tests/ColourMapsXPTests.vcxproj /t:Build /p:Configuration=Release /p:Platform=Win32
./Release/ColourMapsXPTests.exe 'C:/Users/carln/Documents/GitHub/Worms2-Maps/Levels/Import/Worms Armageddon/Birthday.dat'
```

The main fixture loads all 232 supplied maps and checks their bounded metadata
messages. It exercises local source/final hashes, missing and changed files,
read-only destinations, path confinement, UTF-8, malformed/old descriptors,
Open/Cavern and water edits, and generated-map resets. TCP/IP/IPX fixtures run
reference/reply/Go/ready flows and check sender IDs, revisions and native launch
gates. Shared dispatcher tests cover CPU teams, weapons and options. Native
dialog fixtures check both previews, dropdowns and the absence of map status
labels and progress bars.
Actual frontend instructions are mapped privately to verify all 18 hook sites,
Go preflight, subsequent rounds, both engine launch barriers and code restoration.
The IPX regression uses the actual native Send, Close and RunApplication
entries, closing the provider before engine startup as the launcher does. It
covers one/multiple recipients, transient busy sends without advancing the
sequence, bounded permanent failure and no sends after the lobby closes.

The focused XP test uses the production `v141_xp` compiler and C++14 settings. It
sets the CRT TLS epoch to zero before the first CRC32 call, checks known vectors,
round-trips Unicode filenames, and validates/publishes Birthday from a local
installed file using an 83-byte reference. These fixtures run on the development
PC; live multiplayer play on XP, TCP/IP and IPX still requires the manual checks
in `tests/ManualTests.md`.
