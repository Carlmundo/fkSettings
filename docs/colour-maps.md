# Colour map importing

The **Select Level** screen now has an **Import map...** button. Choose a
complete Worms 2 `.dat` terrain file. The picker initially opens `Levels/Import`
beside the frontend executable, remembers the last folder for this session, and
can browse external folders and subdirectories. For this collection, browse to
`C:\Users\carln\Documents\GitHub\Worms2-Maps\Levels\Import`.

The imported map replaces the generated thumbnails with a colour preview. Both
previews match the map's aspect ratio, fill their image area and contain no text.
Empty terrain uses the native preview's blue background (RGB 0, 160, 255).
This affects only the display palette; imported terrain colours and file bytes
are unchanged.
Generation controls are disabled while selected, except **Water colour** and
**Level style**. Level style
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
**Use generated map** returns both previews to native landscape rendering and
restores the original style list and selection. Control 1022 retains its native
level list and remains disabled for imports. An imported selection is retained
in memory for subsequent local games, even if the source
file moves or changes. Restarting the frontend clears it.

This implementation supports **local and online games**. The online host chooses
the map, style and water; joining players receive that selection automatically.
Import/reset and style/water editing are unavailable to joining players. All
players must use the updated DLL. Saved-game restoration and remembering imports
across restarts remain outside this change.

## Online play

The host sends the complete validated `.dat` using bounded 32 KB chunks through
the existing targeted/broadcast transport hooks. Native lobby snapshots precede
late-join transfers so clients already know the host's player ID. Host imports,
water/style changes and generated-map resets update remote previews. Each Go
freezes the selected map and sends it again with a new revision, so edits cannot
change the terrain partway through the ready handshake.

Clients accept map packets only from the host, enforce the 8 MB bound, assemble
chunks and verify the complete CRC32 before parsing or publishing terrain.
The native ready, generation and start packets carry the map revision, size and
CRC32. Both client ready replies must confirm that identity; the host rejects
missing, mismatched, non-member and duplicate acknowledgements. Missing chunks,
corrupt files or clients using an older DLL therefore cannot advance the native
start handshake. Such attempts use the frontend's existing timeout/cancellation
behavior; retry Go after every player has the updated DLL and a writable
`Data/land.dat`.

Both the first match and the results screen's next round bypass native terrain
generation for the confirmed import. Host/client launch paths republish the same
frozen bytes immediately before native engine setup. Transfers preserve palette,
collision, objects, Open/Cavern and water settings. Generated maps retain native
network behavior. Closing the lobby/results window clears remote map and role
state. Secret weapon, extended option and CPU-team extensions share the send and
receive hooks; the map trailer is removed before those decoders run.

This has been verified with executable mappings and transport callbacks. A live
match between separate computers remains a required manual check.

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
destination intact. Failed launch publication reports an error and makes the
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
| `0x34294` | First-match host ready replies |
| `0x61865` | Subsequent-round host ready replies |
| `0x1E32F` | Client ready replies; append confirmed map identity |
| `0x358A4` | First-match host engine setup; final publication |
| `0x61A81` | Subsequent-round host engine setup; final publication |

The normal Go preparation returns to `0x7956C`; subsequent local rounds return
to `0x77E04`. Their engine-launch calls return to `0x79591` and `0x77E4B`.
These inspected CALL instructions are validated during installation. Host network
preparation returns to `0x35819` and `0x623E8`; client generation returns to
`0x3E266` and `0x6445C`. These calls and the native player-ID list layout are also
validated before installation.
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
```

Automated checks load all 234 supplied maps, validate malformed-file rejection
and bounded Team17 decompression, verify indexed palette rendering,
byte-identical atomic writes and read-only failure cleanup, exercise local
generation bypass and the launch failure gate, and load actual Select Level and
Game controls resources to test shared palette rendering, aspect ratios,
full-image rendering without text or borders, Open/Cavern and water changes,
case-insensitive water matching, unknown resources, tab switches and restoration
of native dropdown contents, item data and preview geometry on reset. Water edits
exercise growing and shrinking paths, outer-size updates, malformed-field
rejection and byte-preservation of all preceding terrain data.
Fixtures execute the supplied normal Go and next-round CALL instructions at
their original mapped addresses, with MFC dependencies replaced in the private
mapping. They verify local caller recognition, native-generation bypass, final
publication after a staged stale-map overwrite, native network generation and
native generation after reset. Twelve hooks install and restore in a
private, non-running executable mapping. Extended-options tests also verify
simultaneous installation with shared CRT/network, network-team and option hooks.
Run that integration check with
`./Release/ExtendedOptionsTests.exe 'D:/Games/Worms 2/frontend.exe' --hooks-only`.
The full extended-options suite currently fails its existing "remaining
untranslated labels stay blank" assertion; the unchanged HEAD test harness
reproduces the same failure. The colour-map suite, hook integration checks and
network-team suite pass. The existing secret-weapon suite currently fails its
native-translation assertion; its network tests also expect stock 100 although
the production maximum is 99. Those settings/translations were not changed for
online maps.

Network fixtures cover complete/missing/corrupt transfers, both transport length
conventions, late joins, resets, frozen-round selections, read-only destinations,
first-match and next-round ready replies, launch publication, remote preview
selection and window cleanup. They execute the real shared send/lobby/results
detours with map and CPU metadata together, and verify weapon/option decoding
alongside map transfer.

The built DLL is `Release/fkSettings.dll`; it has not been installed into the
game. Actual frontend skin rendering, high-DPI interaction, Windows XP execution
and playing a complete engine match require the checks in `tests/ManualTests.md`.
