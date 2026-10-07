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

The host compresses 256-byte raw blocks with bounded literal/repeated-byte
runs, grouping blocks into at most 16 KB of compressed payload for TCP/IP or
4 KB for IPX. Protocol 9 uses smaller blocks so retries can shrink below the
old 32 KB raw-block size, even for incompressible maps. The 36-byte header and
optional four-byte native sequence are additional to the payload cap. Native
receive-buffer capacity alone is insufficient: the host logs showed missing
replies for large packets on both transports, including packets over 65,535
bytes. Birthday uses nine TCP/IP packets or 35 IPX packets at the initial caps;
every supplied map reconstructs byte-for-byte with both limits.
Each group's compression and checksum are cached for retries and other recipients.
The reconstructed file, palette, collision and water data remain byte-identical. A
20 ms UI timer sends bursts of at most four packets, ending the burst after
five milliseconds or immediately on transport backpressure. Up to 16 groups
and 128 KB of packet data can remain in flight; recipients confirm each group
independently without holding up subsequent packets. The first group is
confirmed by every recipient before filling the window, so each receiver has
started its assembly before out-of-order groups can arrive. Compression work
also yields between groups after five milliseconds. Each send calls the
validated native DirectPlay Send entry once, preserving the guaranteed flag and
the reliable channel's sequence envelope without entering the frontend's
per-packet ACK polling loop. Busy sends stay queued for a later tick. Each
recipient confirms the exact revision, file checksum, offset, chunk length and
decoded-group CRC32 on the native client ready channel, using 36-byte headers.
Corrupt chunks are rejected before modifying the assembly or queuing a receipt,
so the host retries the affected chunk instead of discovering damage only at
the end of the file. Send success alone never advances the transfer.
Custom packets are copied out of the native shared receive buffer before
logging, UI refresh or decoding, so those operations use stable bytes.
Unconfirmed groups retry after one second with a fresh native sequence. A
second missed reply halves the payload cap, down to 512 bytes, and rebuilds the
outstanding window from the prefix confirmed by every recipient. A single lost
packet/reply can therefore recover without shrinking the remaining transfer.
Persistent synchronous Send failures also reduce the cap every two seconds.
Resizing keeps the same revision and reconfirms smaller groups with every recipient. The receiver
verifies overlapping bytes and counts each raw block once, allowing recovery
even when a larger group arrived but its reply was lost. Delayed replies for a
different group size cannot advance the replacement. After eight groups are
confirmed, a reduced cap doubles toward its initial transport limit; isolated
loss no longer leaves the entire map stuck with tiny packets. At the minimum
cap, retries back off up to eight seconds; a missing confirmation still has
a 15-second timeout. Receipts are also sent in bounded bursts. This
recovers lost replies and packets overtaken by native lobby traffic without
repeatedly submitting a packet the provider cannot deliver. The last
chunk's receipt is held if it arrives before other chunks, then sent when the
remaining holes are filled and complete CRC32/file validation succeeds. Native
lobby snapshots precede
late-join transfers so clients already know the host's player ID. Host imports,
water/style changes and generated-map resets update remote previews. Each Go
freezes the selected map. The first-match and next-round Go handlers defer native
startup until all transfers and the round's cache reference are confirmed. Native
Go disables the main frontend and starts its 30-second timeout; neither action
occurs during bulk transfer. A failed transfer leaves the frontend enabled and
Go can be retried with a fresh revision, preventing delayed replies from the
failed attempt from advancing the new round. A rejected complete assembly
accepts a fresh first chunk and rebuilds its bitmap/bytes; terminal retries alone
cannot confirm it. Closing the lobby discards the deferred native call.
Both Terrain selection and Game controls display transfer percentages outside
the image, with a progress bar and a persistent **Map ready** status after
validation. Game controls places its status beside native Go. An early Go click
displays **Game queued**/**Queued** while waiting, then starts automatically
after confirmation. A failed transfer displays **Retry Go**. Joining players
see receive progress before the complete preview arrives. Generated/local maps
hide these controls; page destruction releases them with the native children.
Status captions paint an opaque background and the current text into a buffer
before displaying it. This prevents the native transparent static-control skin
from retaining earlier percentages underneath newer captions.
If every recipient has confirmed the current map, a 32-byte cache reference gives
it a fresh round revision without retransmitting the terrain. Clients accept
that reference only when their complete cached map has the same size and CRC32.
Edits cannot change the terrain partway through a queued ready handshake.

Go, generate and start controls (27, 29 and 14) and both client ready replies
(28 and 30) use that same bounded queue. Client replies retain their separate
native ready channel. A host accepts readiness only from a recipient whose Go
has actually been submitted, even if other recipients are still queued.

This replaces the original synchronous bulk-send loop that caused the frontend
to stall after imports and again at Go: the reliable native wrapper waits up to
200 ms for each recipient's ACK and retries seven times for each chunk. Its
outer wrappers retry the transport's Busy result indefinitely, which also
exposed imported-map Go/start to a permanent freeze after pacing the map alone.
Imported handshakes no longer call those wrappers. Rapid
map/style/water changes now discard obsolete queued transfers. Closing a lobby,
the native start timeout and DLL shutdown cancel timer callbacks before transport
objects disappear. A transfer cancels after 15 seconds without a confirmed chunk;
Bulk transfer does not spend the native startup timeout. Once native Go starts,
its queued control has a 25-second deadline within the native 30-second timeout.

Clients accept map packets only from the host, enforce the 8 MB bound, assemble
chunks, check each decoded chunk, and verify the complete CRC32 before parsing
or publishing terrain. The complete-file check remains mandatory even if all
individual chunk checks pass.
The native ready, generation and start packets carry the map revision, size and
CRC32. Both client ready replies must confirm that identity; the host rejects
missing, mismatched, non-member and duplicate acknowledgements. Missing chunks,
corrupt files or clients using an older DLL therefore cannot advance the native
start handshake. Such attempts use the frontend's existing timeout/cancellation
behavior; retry Go after every player has the updated DLL and a writable
`Data/land.dat`.

The host/joiner logs from the previous DLL showed submitted map bytes but no
complete map at the joiner, followed by a rejected cache reference and Go.
The installed fkWorm2NAT DLL's inspected hooks change server login, connection
dialogs, resource lookup and socket keepalive; they do not directly replace the
terrain send/receive hooks. A transport interaction remains possible and needs
a live comparison to establish. The confirmation protocol does not assume
that submitting a packet proves delivery through either connection method.

Both the first match and the results screen's next round bypass native terrain
generation for the confirmed import. Host/client launch paths republish the same
frozen bytes immediately before native engine setup. Both the DirectPlay and IPX
engine launchers submit pending control packets before entering native engine
launch, allowing at most one second of retries with repaint dispatch. They
return failure if a send or final publication fails, or if terrain/Go is still
pending. This keeps a failed final send out of the native infinite engine-event
wait. Transfers preserve palette,
collision, objects, Open/Cavern and water settings. Generated maps retain native
network behavior. Closing the lobby/results window clears remote map and role
state. Secret weapon, extended option and CPU-team extensions share the send and
receive hooks; the map trailer is removed before those decoders run.

This has been verified with executable mappings and transport callbacks. A live
match between separate computers remains a required manual check.

Diagnostics are appended to `Data/fkSettings-map-network.log` beside the game's
`land.dat` and mirrored to `%TEMP%\fkSettings-map-network.log` on each computer.
The TEMP copy records Data-file write errors and remains useful when the Data
copy is locked, unwritable or redirected by Windows. Each frontend launch
records `map-network diagnostics 13 protocol 9`, its build date/time, executable
and DLL paths, and installation status for the shared and colour-map hooks.
Lines include UTC time, process ID and a millisecond tick count.
The startup `CRC32 self-test cbf43926 cbf43926` records actual and expected
checksums of `123456789`. The CRC32 table is initialized at compile time: a
function-local runtime initializer depended on the CRT's thread-local epoch,
which could be zero in a DLL loaded on XP and leave the table uninitialized.
That produced `ffffffff` for nonempty chunks and prevented confirmation.

Each custom packet records its sender/recipient, version, map revision, offset,
raw/wire lengths, native sequence and CRC32. Separate checksum entries compare
the transmitted/received chunk checksum; checksum rejection records actual and
expected values. Wire reception is recorded before
the game's reliable sequence filter; custom packets discarded there have a
separate entry. Handler receipt, sender/parse rejection, chunk acceptance and
reply queue/send/acceptance are logged independently. Native Send entry/return
records its HRESULT and elapsed time, with repeated busy diagnostics limited.
Go clicks are recorded even when no import or host role is active. The existing
startup phases cover transfer completion/timeouts, controls, readiness, terrain
preparation, native Go and engine launch/return. These diagnostics distinguish
missing delivery from a discarded packet or a missing reply without changing
the native receive/filter result. Logging failures never prevent play.

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

The focused XP test uses the DLL's `v141_xp` compiler and C++14 settings. It
sets the CRT thread-local epoch to zero before the first checksum call, checks
independent CRC32 vectors, and optionally checks the unmodified Birthday map
(`3a3314b7`) and its first raw block (`832248dd`), then reconstructs the grouped
transfer byte-for-byte in three packets. The old guarded initializer
reproduced the joiner's `ffffffff` result under this test; the compile-time
table passes. This simulates the initialization failure on the development
computer; a live XP match remains a manual check.

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
native generation after reset. Eighteen hooks install and restore in a
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
selection and window cleanup. Additional regressions verify real Windows timer
delivery alongside UI messages, one-attempt backpressure, multiple recipients,
late joins during a transfer, coalesced edits, immutable pending Go, cache-reference
validation, stale ready rejection and cancellation. Confirmation regressions
exchange actual map/receipt packets between independent host and joiner states,
drop chunks and replies, reject wrong offsets and senders, exercise both host
dispatchers and transport length conventions, and withhold Go until the entire
validated map is confirmed. A missing reply times out even when Send succeeds;
native generation controls stay withheld after cancellation. A private executable mapping
also executes both Go detours against a UI-disable callback, proving that slow
bulk transfer leaves the window enabled, retains frozen bytes, does not spend
the native Go timer, starts exactly once after confirmation, and cancels on
failure/window destruction. Every supplied map reconstructs byte-for-byte
through the checked network decoder, including the reported Birthday map and
its independently verified CRC32. Corrupted, well-formed compressed chunks get
no receipt and recover on retransmission. Full-file checksum rejection remains
enforced even with valid per-chunk checks, and the same failed assembly can
restart from its first chunk. Retried Go revisions reject stale receipts.
Protocol 6 and protocol 9 reconstruct all supplied maps; protocol 9 is checked
with both the TCP/IP and IPX payload limits. Window regressions use a simulated
clock and delayed, reordered delivery to two independent receivers, through
both native host dispatchers and transport length conventions. They cover lost
data/replies, valid compressed corruption, corrupt/non-recipient/duplicate
receipts, successful Send calls that silently lose oversized packets, shrinking
and regrowing caps, window/burst bounds, and a dead peer's bounded timeout.
The terminal receipt is explicitly withheld until full validation, including
when the last packet arrives before missing data; valid per-group checksums
cannot confirm a wrong complete-file CRC. An active transfer does not spend
the queued-Go startup deadline. The XP-toolset regression also
reconstructs Birthday at 16 KB, 4 KB and 512-byte payload caps. Native-dialog tests verify
confirmed-byte percentages, queued starts, receiving/ready/failure captions,
text fitting, placement outside the image/Go/team list, and disconnect cleanup.
Repaint tests use a transparent parent brush and compare several successive
captions with a fresh render of the final caption on both native dialogs.
Run the focused UI checks with the colour-map test command above followed by
`--status-only`.
Worst-case literals and malformed compressed runs are bounded.
A private executable mapping
executes the actual native DirectPlay Send entry against a COM fixture for both
transport envelopes and validates sequence continuity. Native Receive and
sequence-filter detours also execute against a COM fixture, preserving payloads,
lengths, HRESULTs, duplicate rejection and sequence advancement. Logging tests
verify the session marker, both log copies and recovery from a read-only Data
log. They execute the real shared send/lobby/results
detours with map and CPU metadata together, and verify weapon/option decoding
alongside map transfer.
Busy-send regressions cover every imported handshake type and per-recipient
readiness. Fixtures execute the hooked native RunApplication entry against a
COM implementation and the IPX execl call against a spawn callback, verifying
start ordering, one-second busy cancellation, preserved arguments, failed
publication and unchanged launch behavior without an imported round.

The built DLL is `Release/fkSettings.dll`; it has not been installed into the
game. Actual frontend skin rendering, high-DPI interaction, Windows XP execution
and playing a complete engine match require the checks in `tests/ManualTests.md`.
