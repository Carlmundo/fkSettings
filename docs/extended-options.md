# Extended game options

Dialog 154 has a new **Extended Options** group below the native groups. Its
27 settings (24 checkboxes and three trackbars) are arranged in two columns,
with trackbars beneath their titles, using the native form's font, dialog units
and existing scrolling. Controls flow down the left column, then the right,
with blank gaps half a checkbox row high, rounded to the nearest pixel.
Persistent Rope precedes Rapid Play in the left column, followed by a blank
gap and the two terrain options. The right column starts with Fast Crates,
Crate Spy, Crate Limit and Crate Rate. Aqua Sheep and Instant Mines precede
the four herd weapon checkboxes. Checkboxes
match the native 10-dialog-unit height and 11-dialog-unit pitch. Slider labels
and readouts are 8 dialog units high, with each trackbar starting 10 dialog
units below its label and successive slider rows 31 dialog units apart. Only the
outer Extended Options group box is present. Trackbars copy the native
trackbars' border styles.
"Disable Unlocked Aim" is the last control. The scroll height extends to fit
the controls and half-row gaps; the viewport and native option layout retain
their dimensions. All interactive controls are keyboard reachable through the
surrounding native containers.

Checking an option writes 1 to its backing byte immediately; unchecking it
writes 0. Trackbars write their numeric byte values immediately and display
numbers between their special endpoints. Super Shopper Crates displays string
141 ("No" in English) at zero and string 4950 ("Unlimited" in English) at 100.
Low Gravity is a checkbox with a 0/1 byte, like the other boolean options.
Crate Limit and Crate Rate display string 99
("Default" in English) at zero. English fallbacks apply when a resource is
unavailable. The native option-edit handler updates the game-controls scheme name
to "User defined", enables Save As and Default, and notifies the frontend of
the change. The editor retains its selected scheme as the Save As source, just
as with native edits. Loading a scheme refreshes attached controls, including
hidden ones. Every native Default copy resets all 27 values to zero, restoring
the trackbars' special zero labels.

This DLL stores and transports settings. The user's separate engine extension
must implement their effects and determine whether its feature requirements
are met. No engine patches are added here.

## File contract

Native `.opt` files contain a seven-byte header and a 128-byte option payload:
135 bytes (`0x87`) in total. Extended schemes append ASCII `PLUS` and 27
unsigned one-byte values in fixed index order, totaling 166 bytes (`0xA6`). There
is no padding, version field, bit packing or 32-bit conversion. Schemes retain
the `PLUS` signature even when every option is zero.

Each option has a permanent zero-based index defined by `OptionIndex` in
[`ExtendedOptions.h`](../fkSettings/ExtendedOptions.h). These indexes preserve
the existing byte offsets: `.opt` offset = **139 + index** and `extended.dat`
offset = **4 + index**. UI controls and stored values use this index as well.
Moving or renaming a control must retain its index. To move it, reorder its
descriptor in `Options` and adjust its row/column/blank gaps; never renumber
the enum. A compile-time check rejects missing, duplicate or out-of-range
indexes. The table is in storage order, independent of future visual ordering.

Before the native game starts, the DLL writes a 31-byte extension block
to **`Data/extended.dat`**, beside `Data/game.dat`. When any option is nonzero,
ASCII `PLUS` occupies sidecar offsets 0–3. When every option is zero, these
four bytes are `00 00 00 00` too, making the entire sidecar zero-filled.
A consumer should treat the zero header as disabled settings; active settings
require exactly 31 bytes, the `PLUS` signature and values within each option's
range. Missing or invalid data should disable every feature. Earlier
extended-option builds are not migrated.

| Index | Option | Range | `.opt` byte offset | `extended.dat` byte offset |
| --- | --- | --- | --- | --- |
| 0 | God Mode | 0–1 | `0x8B` (139) | `0x04` (4) |
| 1 | High Jump | 0–1 | `0x8C` (140) | `0x05` (5) |
| 2 | Change Kamikaze to Suicide Bomber | 0–1 | `0x8D` (141) | `0x06` (6) |
| 3 | Sheep Heaven | 0–1 | `0x8E` (142) | `0x07` (7) |
| 4 | Super Shopper Crates | 0–100 | `0x8F` (143) | `0x08` (8) |
| 5 | Extended Fuses/Herds | 0–1 | `0x90` (144) | `0x09` (9) |
| 6 | Utilities don't end turn | 0–1 | `0x91` (145) | `0x0A` (10) |
| 7 | Weapons don't end turn | 0–1 | `0x92` (146) | `0x0B` (11) |
| 8 | Loss of control doesn't end turn | 0–1 | `0x93` (147) | `0x0C` (12) |
| 9 | Worm select after movement | 0–1 | `0x94` (148) | `0x0D` (13) |
| 10 | Low Gravity | 0–1 | `0x95` (149) | `0x0E` (14) |
| 11 | Persistent Rope | 0–1 | `0x96` (150) | `0x0F` (15) |
| 12 | Rapid Play | 0–1 | `0x97` (151) | `0x10` (16) |
| 13 | Indestructible Terrain | 0–1 | `0x98` (152) | `0x11` (17) |
| 14 | Invisible Terrain | 0–1 | `0x99` (153) | `0x12` (18) |
| 15 | Fast Crates | 0–1 | `0x9A` (154) | `0x13` (19) |
| 16 | Crate Spy | 0–1 | `0x9B` (155) | `0x14` (20) |
| 17 | Crate Limit | 0–100 | `0x9C` (156) | `0x15` (21) |
| 18 | Crate Rate | 0–100 | `0x9D` (157) | `0x16` (22) |
| 19 | Aqua Sheep | 0–1 | `0x9E` (158) | `0x17` (23) |
| 20 | Instant Mines | 0–1 | `0x9F` (159) | `0x18` (24) |
| 21 | Herd weapon: Dynamite | 0–1 | `0xA0` (160) | `0x19` (25) |
| 22 | Herd weapon: Mine | 0–1 | `0xA1` (161) | `0x1A` (26) |
| 23 | Herd weapon: Ming Vase | 0–1 | `0xA2` (162) | `0x1B` (27) |
| 24 | Herd weapon: Sheep | 0–1 | `0xA3` (163) | `0x1C` (28) |
| 25 | Disable Backflip | 0–1 | `0xA4` (164) | `0x1D` (29) |
| 26 | Disable Unlocked Aim | 0–1 | `0xA5` (165) | `0x1E` (30) |

Spacing does not occupy bytes. The four herd-weapon
entries are independent checkboxes. "Herd weapon: Sheep" is one checkbox and one byte.

Legacy schemes, truncated native payloads or extensions, unknown signatures,
and any value outside its permitted range reset all 27
settings. Scheme hooks use
the frontend's own CRT trampolines and opaque stream pointer, avoiding CRT
`FILE` structure sharing. Native readers stop after the original payload;
saving through an unmodified frontend removes the appended settings.
Compatibility with third-party scheme editors is unverified.

## Why use extended.dat?

Secret weapons already have reserved stock slots in the native `game.dat`
structure. No verified unused bytes have been identified for these extended
settings. Overwriting arbitrary bytes inside it can corrupt native fields.

The inspected engine reads exactly 3,284 bytes (`0xCD4`) and validates the
embedded length as 3,284. That particular reader ignores trailing data, but
this does not establish append compatibility across saved-game writers,
frontend loaders or other consumers. The separate file avoids extending the
native game structure or assigning unverified offsets. Every native game-data
byte and its embedded length remain unchanged.

The native CRT open hook recognizes a writable file whose basename is
`game.dat`, case-insensitively, and removes its previous `extended.dat`. After
a successful native 3,284-byte write, the shared write hook creates a complete
temporary sidecar, closes it, and publishes it with `MoveFileEx`. This happens
synchronously before the native write returns and the launch proceeds.
Each match rewrites all settings, including an all-zero block for Default or
legacy schemes. Close removes stream tracking; ordinary saved `.dat` files
do not change launch settings.

Sidecar failure returns a failed native write and emits an `OutputDebugString`
diagnostic. Native local launch checks this failure; the inspected online
callers do not consistently check the native writer's return value. The engine
extension must handle missing/invalid sidecars as disabled settings. Launches
without this updated frontend DLL cannot refresh the sidecar; engine consumers
should only enable this integration when that frontend component is present.
Restoring extended settings from saved `.dat` games is outside this change.

## Online options

Native options use packet type `0x19`: a four-byte type and 128 native option
bytes, totaling 132 bytes (`0x84`). The shared targeted/broadcast hooks append
the 31-byte `PLUS` block, producing 163 bytes (`0xA3`), while preserving the
original packet bytes. Default uses `PLUS` followed by 27 zero option bytes;
the four-byte zero header is specific to an unset launch sidecar.

Reception accepts extension values only from the current host, before the
native decoder runs. Legacy or malformed host extensions clear the values;
truncated native packets are dropped before the native unbounded decoder.
The decoder receives the original native packet length. Both raw and reliable
transport lengths follow the existing secret-weapon normalization, including
the reliable receiver's skipped sequence DWORD.

Every participant needs the updated frontend DLL and a compatible engine
extension for these options to take effect consistently. There is no peer
capability handshake. A real match between updated clients remains necessary
to verify engine behavior and absence of desynchronization.

## Supported frontend and hooks

The feature targets the same inspected 32-bit frontend as secret weapons:
PE timestamp `0x3587BE19`, image size `0x5B8000`. Installation checks the PE
identity and all hook signatures before modifying code, and rolls back hooks
if installation fails. It depends on the shared CRT/network hooks installed
by `SecretWeapons`; it installs independently of the network computer-team
feature.

| RVA | Purpose |
| --- | --- |
| `0x57D20` | Native Dialog 154 form creation; append controls afterward |
| `0x57E6F` | Native option-edit handler (called, not hooked) |
| `0xCF5B5` | Native scroll sizing (called, not hooked) |
| `0x97780` / `0x97500` | Native CRT open/close hooks for launch stream tracking |
| `0x1863D8` | Native 128-byte option payload |
| `0x186354` | Native option scheme name (`CString`) |
| `0x9CDC`, `0x9FF8`, `0xA5D4`, `0x31CD9`, `0x392C0`, `0x586A3`, `0x5899E`, `0x59611`, `0x59E3D` | Native default option copies |

The nine copy detours preserve registers and flags, clear extended settings,
then resume the native instructions through their MinHook trampolines. Copy
operand signatures are checked against the relocated executable image.

## Translations

`dllmain.cpp` passes the existing `lang` value from `language.txt` to
`ExtendedOptions::SetLanguage` when assigning labels. The switch in
`fkSettings/ExtendedOptionsStrings.h` defines variables named after each
permanent option index, such as `strGodMode`, `strHighJump` and
`strSuicideBomber`, plus `strExtendedOptions` for the group title.
The descriptors reference these variables instead of literal captions.

The `en`/default case contains the English labels. Each other supported
language (`cs`, `de`, `es`, `es-419`, `fr`, `is`, `it`, `nl`, `pl`, `pt`,
`pt-br`, `ru`, `sv`, `zh-Hans`) has its own case with empty `L""` assignments
ready for translation. These remain blank until filled in. Missing or
unrecognized language codes use English; surrounding whitespace and a UTF-8
BOM are accepted. Use wide string literals (`L"..."`) for translated text.
Language changes also refresh captions on any attached editor without
changing option values, indexes or file/packet bytes.

## Build and verification

Build **Release / Win32**, with the output directory explicitly set inside the
repository instead of the project's default game installation directory:

```powershell
MSBuild.exe fkSettings/fkSettings.vcxproj /t:Build /p:Configuration=Release /p:Platform=Win32 /p:OutDir="$PWD/Release/"
MSBuild.exe tests/ExtendedOptionsTests.vcxproj /t:Build /p:Configuration=Release /p:Platform=Win32
./Release/ExtendedOptionsTests.exe 'D:/Games/Worms 2/frontend.exe'
```

Regression tests check every language case, English fallback, blank translation
placeholders and caption updates on an attached dialog, then exercise the
production shared hooks, all 27 independent
settings, every numeric slider value, all-maximum/all-zero settings, byte
offsets, unchanged native data, every short trailer length, invalid signatures
and out-of-range values, and save/sidecar failures.
An additional Dialog 154 fixture moves every descriptor to a different visual
position, clicks all checkboxes and uses each slider, then verifies original
byte positions in saved `.opt` files, launch sidecars and lobby packets.
It also checks scheme/network refresh and tab traversal in the moved layout.
Launch fixtures check `PLUS` for each independently enabled option, four zero
header bytes for all-unset values, sidecar rewrites, stream cleanup and unchanged 3,284-byte
native output. Network fixtures cover both send paths, both transports, host
authority, legacy/invalid extensions, native decoder lengths and guarded receive
buffers. All nine Default wrappers run with register/flag preservation checks.

A Win32 fixture loads the actual Dialog 154 resource without running the
frontend, checks labels/text fit, a single outer group box, two-column order,
the Low Gravity checkbox, trackbars beneath titles with native
border styles, checkbox height/pitch, label-to-trackbar distance and readout
alignment measured against the original dialog controls, and half-row blank gaps,
restored checkbox/slider states, keyboard slider edits,
localized endpoint labels, scheme reload/Default refresh, nested forward
and reverse Windows tab traversal,
scroll sizing and destruction. Private executable mappings validate coexistence
with the shared hooks, signature rejection and installation/removal of all
twelve new detours. Tests do not modify the supplied executable or installed DLL.

Live frontend navigation, high-DPI layouts, game-engine feature activation and
a complete multiplayer match are not yet verified. Manual checks are listed
in `tests/ManualTests.md`.
