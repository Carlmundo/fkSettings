# Secret weapon stock editor

The weapon editor (dialog 200) now has eight secret entries, each with its own
stock slider: 0–100, with 10 displayed as Unlimited. This implementation exposes stock only; it does
not add power, delay, or crate-probability settings for secret weapons.

The implementation lives in `fkSettings/SecretWeapons.cpp`. It uses the
frontend's strings when present, otherwise the following English names. The
entries follow the 38 native weapons in this order:

| Weapon | String ID | Team 1 `game.dat` stock offset |
| --- | --- | --- |
| Salvation Army | 4938 | `0x49E` |
| MB Bomb | 4939 | `0x49F` |
| Sheep Strike | 4940 | `0x4A2` |
| Carpet Bomb | 4941 | `0x4A3` |
| Cloned Sheep | 4942 | `0x4A6` |
| Concrete Donkey | 4943 | `0x4A7` |
| Nuclear Bomb | 4944 | `0x4AA` |
| Magic Bullet | 4945 | `0x4AB` |

It uses the existing string 4950 for Unlimited. Generated trackbars have upward
thumbs and tick marks above the track (`TBS_TOP`), matching the native pages.
The stock label uses string 5000 ("Initial stock" in English). The separate
weapon-name heading is omitted, and the panel and its controls share the
standard button-face background used by the native weapon pages.
The generated label, trackbar, and stock value copy the screen rectangles of
native controls 5000–5002, preserving their placement, size, and font scaling.
Trackbar ticks occur every 10 stocks across the extended range.
The generated trackbar uses native control ID 5001. The editor's existing
cursor handler loads hints from the hovered control ID plus 500, so this
displays string 5501 in the bottom hint area, just like a normal weapon's
Initial stock trackbar. Hint switching remains managed by the frontend's
existing cursor handler.

## Native editor storage

The frontend allocates exactly 38 weapon-page pointers at editor-object offset
`0xA4`. The next member starts at `0x13C`. Increasing the original weapon-list
loop would overwrite that member when selecting the 39th weapon.

Instead, the DLL appends eight list entries after the native editor initializes and
creates a separate Win32 stock panel. It intercepts selection changes and maps
native list-selection queries to the last normal weapon while a secret weapon is
selected. This keeps Save, Load, Default, Apply, and keyboard handlers within
the original 38-page array. Normal weapon pages remain managed by the frontend.

Secret slider edits update that weapon's backing stock immediately and enable
Save As and Default. They load string 138 into the frontend's stored scheme
name, so Game controls displays "User defined" until the scheme is saved or
another scheme is loaded. Weapon editor keeps its selected scheme name and
Delete-button state, matching edits to normal weapons.
The shared native Default loader resets all eight secret stocks, whether invoked
by the Default button or either scheme dropdown. Attached stock panels refresh
immediately after loading Default or a saved scheme, including while hidden.
Edits do not set
the native pending-control-edit flag at `0x1B4E6C`.
That flag makes the frontend's `WM_SHOWWINDOW` handler commit a native weapon
page when the app is hidden, including during Alt+Tab. Secret stock has no
native controls to commit. Existing pending edits to normal weapons retain
their native flag and commit behavior.

The live debugger captured an Alt+Tab hang in the Windows dialog manager:
it repeatedly enumerated controls while trying to return to the focused stock
slider. The custom panel had `WS_EX_CONTROLPARENT`, but the native editor and
its intervening parent did not, so enumeration could not rediscover the slider.
Attachment now marks every child ancestor up to the outer dialog with this
style, and removes only the bits it added when the editor is destroyed. Switching
back to a normal weapon moves focus to the list before hiding the secret panel.

## Scheme extension

Native `.wep` files are 5,344 bytes (`0x14E0`): a 24-byte header and 38 records
of 140 bytes (`0x8C`). The native portion retains its original layout and record
count. Extended files append these 36 bytes, making them 5,380 bytes (`0x1504`):

| File offset | Size | Value |
| --- | --- | --- |
| `0x14E0` | 4 | ASCII `PLUS` |
| `0x14E4` | 32 | Eight little-endian 32-bit stocks, 0–100, in the order above |

10 represents Unlimited in this extension. Loading a legacy scheme, a short
extension, an unrecognized signature, or any invalid stock resets all secret stocks
to zero. There is no version field or migration of earlier prototype extensions.
The hooks use the frontend's own
`fread`/`fwrite` trampolines with its opaque stream pointer, so CRT `FILE`
structures are never passed between the frontend and this DLL's CRT.

The supplied frontend's readers stop after the native records, so they can
read the native part of an extended scheme without this DLL. Saving again
without the extension removes the secret stock settings. Compatibility with
third-party scheme editors has not been tested.

## Game data

The game already has slots for all eight weapons; `game.dat` needs no length
or format extension. Stocks start at file offset `0x47B`. The table above gives
each first-team offset; the other five teams use the same relative slots,
separated by `0x108` bytes.

The frontend's game object has a `0x1C`-byte prefix that is omitted when writing
the `0xCD4`-byte game-data payload. Consequently, the hook changes object offset
`stockOffset + 0x1C` for each first-team weapon (`0x4BE` for Sheep Strike).
It runs after native stock preparation, before the
data is consumed or serialized, rather than modifying a file after it is saved.
The stock byte is 0–100, with 10 replaced by `0xFF` for Unlimited. Native all-weapons cheat and
stock-replenishment behavior are preserved.

## Online weapon schemes

The native lobby sends weapon schemes as packet type `0x18`: a four-byte type
and 38 compressed records of 42 bytes, totaling 1,600 bytes (`0x640`). The DLL
appends the same 36-byte `PLUS` extension used in `.wep` files, making the
packet 1,636 bytes (`0x664`). Its original 1,600 bytes are copied unchanged.
Both broadcasts after edits and targeted transfers to joining players include
all eight stocks, including zeros for Default. Other packet types are unchanged.

The lobby receive dispatcher exposes both sender and length. The DLL accepts
secret stocks from the lobby's current host (object offset `0x163C`) before
native weapon processing, then refreshes attached stock panels. This replaces
the joining player's local stocks so subsequent game-data preparation uses the
host's values. An absent, malformed, or invalid host extension clears all eight
stocks; a truncated native weapon packet is dropped before the native decoder.

The native reliable receiver (`0x12BC2`) skips its sequence DWORD in the returned
pointer but retains it in the reported length. When transport mode at
`0x188B14` is 1, the parser subtracts four bytes before validating the extension.
The raw receiver (`0x12B6C`) returns the payload length directly. Extension reads
are bounded in both modes. Reception runs in the lobby's UI-thread polling loop.

Secret edits already send `WM_USER + 23`, which sets the online lobby's pending
weapon-scheme flag. Returning from Weapon editor therefore follows the normal
scheme broadcast path without setting the unsafe native page-commit flag.
All participants need this DLL build to receive custom secret stocks. There is
no separate secret-stock message or `.wep` format change in this update.

## Supported executable and hook locations

This first implementation targets the supplied 32-bit frontend:

- PE timestamp: `0x3587BE19`
- Image size: `0x5B8000`
- SHA-256 of the inspected file:
  `F0D30BD56696049A8BA26EF03A85C7798DE62A3872AB6A8ACEABC68003279553`

Locations are relative to the executable's loaded image base:

| RVA | Purpose |
| --- | --- |
| `0x89243` | Weapon editor initialization |
| `0x89475` | Weapon selection handler |
| `0x0D760` | Native list-selection helper |
| `0x1E698` | Shared default weapon scheme loader |
| `0xC5B02` | Native MFC string loader (called for scheme name string 138) |
| `0x187644` | Native weapon scheme name (`CString`) |
| `0x97BD0` | Native CRT `fread` |
| `0x975B0` | Native CRT `fwrite` |
| `0x2882F` | Prepare team weapon stocks |
| `0x1E29A` | Targeted lobby packet sender |
| `0x1E3C4` | Broadcast lobby packet sender |
| `0x39E41` | Lobby receive dispatcher (sender, packet, length) |
| `0x187648` | Native weapon records |
| `0x1B4E6C` | Native weapon scheme dirty flag |

Installation checks the PE architecture, timestamp, image size, and signatures
at every hook site. Unsupported builds skip this feature and emit an
`OutputDebugString` diagnostic. Failed hook installation rolls back the hooks
created for this feature.

## Build and verification

Build the solution as **Release / x86** (the project platform is Win32). The
existing project output-directory settings are respected. To keep the build
inside the repository, pass an explicit output-directory override:

```powershell
MSBuild.exe fkSettings/fkSettings.vcxproj /t:Build /p:Configuration=Release /p:Platform=Win32 /p:OutDir="$PWD/Release/"
MSBuild.exe tests/SecretWeaponsTests.vcxproj /t:Build /p:Configuration=Release /p:Platform=Win32
./Release/SecretWeaponsTests.exe 'D:/Games/Worms 2/frontend.exe'
```

Tests exercise the production detours with controlled CRT and native-handler
substitutes. They cover scheme round trips, unchanged native records, legacy
and malformed files, all eight stocks at all six team offsets, unlimited stock, replenishment,
and a Win32 editor fixture with independent selections, stock restoration,
top tick style, Default from the dropdown and outside the editor, native
"User defined" naming, saved-scheme panel refresh, slider, and destruction checks.
The editor fixture includes two nested containers matching the live focus path.
It runs actual Windows forward/reverse tab traversal and dialog deactivation
with the stock slider focused, and checks ancestor-style restoration on close.
When given an executable path, they also execute a relocated copy of its
visibility handler with controlled callees, checking repeated Alt+Tab visibility
transitions after editing a secret weapon and preserving pending normal-weapon
edits. Network fixtures exercise targeted and broadcast transfer, both receive
length conventions, host authority, Default, invalid/truncated extensions, and
the resulting stocks for every weapon on all six client teams. Guard pages
check that short trailers cannot cause reads beyond the receive buffer.
They map the supplied frontend without running it, install all ten
MinHook detours, and verify that uninitializing MinHook restores the original
code. Hook changes stay private to that mapping;
the executable file is never modified. They do not run the actual frontend or
the game engine.

Live verification on 2026-09-30 used the DLL installed in `D:\Games\Worms 2`
with CDB attached. Two Alt+Tab round trips after focused Sheep Strike stock
edits (5 and 7) returned with the selection and stock intact. Switching to Blow
Torch, editing its native stock, and another Alt+Tab round trip also succeeded.
The native weapon test edit was reset to Default afterward. The failing build's dump
and debugger trace showed a busy control-enumeration loop, not an access
violation; these local diagnostic artifacts are in the ignored `Release` folder.

The eight-weapon build was also checked live on 2026-09-30. The list displayed
all eight fallback names in the requested order; the stock trackbar had its
thumb and ticks above the track. Magic Bullet stock 4 survived an Alt+Tab round
trip. Sheep Strike stock 2 and Magic Bullet stock 4 were saved to
`Weapons/fk-secrets-0930.wep`, reset with Default, and both restored on reload.
That check preceded the final trailer change to `PLUS` without a version field.
The final 5,380-byte format is verified by byte-level regression checks, including
the signature and eight little-endian stocks beginning directly at `0x14E4`.
The game engine was not launched
during this check; team offsets were verified by the regression fixture.

The Default/name fixes were checked live on 2026-09-30: Salvation Army stock 2
and Magic Bullet stock 4 both reset to zero when Default was selected in the
Weapons scheme dropdown. A second Magic Bullet edit displayed "User defined"
on Game controls; choosing Default there and returning to Weapons displayed
zero immediately, without selecting another weapon. The shared loader resets
backing stocks and refreshes attached panels even while the editor is hidden.

The scheme-name behavior was also checked live: editing Magic Bullet in the
loaded `fk-secrets-0930` scheme retained that name in Weapon editor, while
Game controls displayed "User defined". Returning to Weapon editor still showed
the original selected scheme name and enabled Delete, matching native edits.

Online host-side verification on 2026-09-30 used the new DLL in
`D:\Games\Worms 2`, with CDB attached. Following Network play, Internet,
Create room (`PLUS test`), and Create a game opened the online weapon editor.
Changing only Magic Bullet stock to 4 and returning to Hosting game emitted
a `0x664`-byte weapon packet with `PLUS` and stocks `[0,0,0,0,0,0,0,4]`.
Setting Sheep Strike to 3 emitted `[0,0,3,0,0,0,0,4]`. Selecting Default and
returning emitted eight zeros. The outgoing buffers were observed at the
native broadcast sender after the DLL extension was appended. The trace is
in the ignored `Release/frontend-network.log`. No second client or game-engine
session was present during this host-side check.

The extended stock range and layout were checked live on 2026-09-30. Switching
between Bazooka and Magic Bullet showed matching stock-label placement and
trackbar width. End on the generated slider displayed 100; stepping from 12
to 11 displayed 11, then stepping to 10 displayed Unlimited. Default restored
zero afterward. Regression checks also cover saving/loading and transferring
stocks of 11, 99, and 100, with 101 rejected as invalid.

The stock hint was checked live on 2026-09-30. Hovering Bazooka's stock trackbar
displayed string 5501, "The stock available at the start of a round."
Returning to Weapon editor with Magic Bullet selected restored the general
page hint; hovering its generated stock trackbar then displayed the same text.
Hover testing left its stock at zero.

For a live check, load the built DLL through the game's existing frontend DLL
loader, open Weapons, select Sheep Strike, and set stock to 3. Save a scheme,
switch to Default, and reload it; the stock should return to 3. Start a local
game and check `Data/game.dat`: byte `0x4A2` should be `03`. Repeat with
Unlimited (`FF`) and with an existing legacy scheme (zero). Also switch back
to the previously selected normal weapon and use Save/Default while Sheep
Strike is selected.

Restoring extension settings from saved `.dat` games remains outside this
prototype's verified scope. Live multiplayer verification is recorded separately
from the controlled packet fixtures above; a complete game with a second updated
client is required to confirm absence of desynchronization.
