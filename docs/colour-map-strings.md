# Colour Maps string inventory

English frontend resource values were checked in `D:\Games\Worms 2\frontend.exe`. The inventory covers captions, error text and diagnostics used by the feature in `ColourMaps.cpp`. It excludes C++ include names, Windows class names, binary hook signatures, path separators and generic formatting placeholders.

## Frontend string IDs used directly

| String ID | English value | Use |
| --- | --- | --- |
| 19 | `Cancel` | Return to normal terrain; button shown for colour maps. |
| 357 | `Terrain` | Import filter: Worms 2 Terrain (*.dat). |
| 447 | `Import` | Import button, file-picker title and import/error dialog titles; button hidden when Levels\Import is missing. |
| 501 | `Open` | Native Level style option retained for colour maps. |
| 502 | `Cavern` | Native Level style option retained for colour maps. |
| 510 | `< Random >` | Recognized and removed from style/water lists while a colour map is selected; restored on reset. |
| 726 | `Current terrain:` | Bold heading above the colour preview; also the native label beside Save As, hidden during imports. |

The fallback captions are `Import`, `Terrain`, `Current terrain:` and `Cancel` if their resource strings cannot be loaded. These are not additional string IDs.

## Existing native controls used by the feature

These captions remain supplied by the original frontend. Colour Maps changes control visibility, enabled state or uses their positions for layout; it does not load these ten captions itself.

| String ID | English value | Control ID |
| --- | --- | --- |
| 720 | `Terrain code...` | 1030 |
| 721 | `Generate` | 1031 |
| 722 | `Edit terrain` | 1032 |
| 723 | `Preview terrain` | 1034 |
| 724 | `Save As...` | 1023 |
| 725 | `Delete` | 1024 |
| 727 | `Terrain:` | 1028 |
| 728 | `Water style:` | 1027 |
| 729 | `Level style:` | 1029 |
| 739 | `Invert terrain` | 1057 |

Water names come from the native water list and the imported file's final water path. Colour Maps does not hardcode an additional list of colour captions or string IDs.

## Other visible text without resource IDs

- File filter: `Worms 2 ` + string 357 + ` (*.dat)`. The filter pattern is `*.dat`.
- Failed import/local/network publication: `Error saving Data\land.dat`

## Translated folder-selection message

`strImportFolder` is selected by `ColourMaps::SetLanguage` from the existing `language.txt` value. The same 15 codes as the other features are supported, with BOM/whitespace trimming and English fallback. It is used by both invalid-relative-path and picker-outside-folder errors. These translations are in [ColourMapsStrings.h](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMapsStrings.h).

| Language code | Message |
| --- | --- |
| `cs` | Vyberte soubor .dat ve složce Levels\Import v adresáři hry. |
| `de` | Wählen Sie eine .dat-Datei im Ordner Levels\Import des Spiels aus. |
| `en` | Select a .dat file inside the game's Levels\Import folder. |
| `es` | Selecciona un archivo .dat dentro de la carpeta Levels\Import del juego. |
| `es-419` | Selecciona un archivo .dat dentro de la carpeta Levels\Import del juego. |
| `fr` | Sélectionnez un fichier .dat dans le dossier Levels\Import du jeu. |
| `is` | Veldu .dat-skrá í möppunni Levels\Import í leikjamöppunni. |
| `it` | Seleziona un file .dat nella cartella Levels\Import del gioco. |
| `nl` | Selecteer een .dat-bestand in de map Levels\Import van het spel. |
| `pl` | Wybierz plik .dat w folderze Levels\Import w katalogu gry. |
| `pt` | Selecione um ficheiro .dat na pasta Levels\Import do jogo. |
| `pt-br` | Selecione um arquivo .dat na pasta Levels\Import do jogo. |
| `ru` | Выберите файл .dat в папке Levels\Import в каталоге игры. |
| `sv` | Välj en .dat-fil i spelets Levels\Import-mapp. |
| `zh-Hans` | 请选择游戏 Levels\Import 文件夹中的 .dat 文件。 |

## Translated network alerts

These eleven additional strings use the same `language.txt` selection, 15 language
codes, BOM/whitespace handling and English fallback as `strImportFolder`. All
translations are in ColourMapsStrings.h. Error titles reuse frontend string 447.
The host popup uses one heading and one player-name row with a short reason in
parentheses. IDs appear only as a fallback when a name is unavailable. Host
warnings omit the map path and only appear for Start Game checks. Each joiner
sees the detailed reason and required path starting at Levels\Import, without a
drive or game-folder prefix. Warnings use Windows' native error message box on
an independent UI thread; later failures replace the dialog on that same worker,
and changed reasons update their existing player row. Windows supplies the
standard OK caption from the OS language; no additional frontend resource ID is
used for dismissing errors.

| Variable | English message |
| --- | --- |
| `strHostMapError` | The following players are unable to use your map: |
| `strLocalMapError` | You cannot use the host's map. |
| `strPlayer` | Player |
| `strMissingMap` | The map file is missing. |
| `strDifferentMap` | The map does not match the host's version. |
| `strInvalidMap` | The map file or path is invalid. |
| `strCannotSaveMap` | Error saving Data\land.dat |
| `strFileNotFound` | File not found |
| `strFileMismatch` | File mismatch |
| `strInvalidFile` | Invalid file |
| `strSaveFailed` | Save failed |

## Validation and internal exception messages

These 31 unique messages are hardcoded in English. The folder-selection and network alerts are translated separately above. File-selection and terrain-validation exceptions can appear in the Import error dialog. Network queue/encoding exceptions are handled internally; they do not all become visible popups.

| Message | Source |
| --- | --- |
| `Truncated terrain file.` | [ColourMaps.cpp:248](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:248) |
| `Invalid terrain resource path.` | [ColourMaps.cpp:259](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:259) |
| `Missing terrain image.` | [ColourMaps.cpp:269](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:269) |
| `Expected an 8-bit foreground and 1-bit terrain masks.` | [ColourMaps.cpp:280](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:280) |
| `Invalid image palette.` | [ColourMaps.cpp:285](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:285) |
| `The foreground has no palette.` | [ColourMaps.cpp:295](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:295) |
| `This frontend supports standard 1920 x 696 terrain files.` | [ColourMaps.cpp:298](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:298) |
| `Image exceeds its dimensions.` | [ColourMaps.cpp:309](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:309) |
| `Invalid compressed terrain image.` | [ColourMaps.cpp:319](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:319) |
| `Incomplete compressed terrain image.` | [ColourMaps.cpp:322](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:322) |
| `Incorrect terrain image size.` | [ColourMaps.cpp:334](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:334) |
| `Foreground index exceeds its palette.` | [ColourMaps.cpp:338](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:338) |
| `Terrain file is too large.` | [ColourMaps.cpp:345](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:345) |
| `Not a complete Worms 2 LND terrain file.` | [ColourMaps.cpp:350](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:350) |
| `Invalid cavern flag.` | [ColourMaps.cpp:354](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:354) |
| `Expected 18 to 32 object locations.` | [ColourMaps.cpp:357](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:357) |
| `Object location is outside the map.` | [ColourMaps.cpp:362](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:362) |
| `Unexpected trailing terrain data.` | [ColourMaps.cpp:369](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:369) |
| `Cannot open the selected terrain file.` | [ColourMaps.cpp:412](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:412) |
| `Cannot read a complete terrain file (maximum 8 MB).` | [ColourMaps.cpp:423](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:423) |
| `The imported map path is too long.` | [ColourMaps.cpp:834](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:834) |
| `Imported map folders cannot use junctions or symbolic links.` | [ColourMaps.cpp:841](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:841) |
| `Invalid relative map path.` | [ColourMaps.cpp:860](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:860) |
| `Invalid map path encoding.` | [ColourMaps.cpp:862](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:862) |
| `Cannot schedule the local map check.` | [ColourMaps.cpp:1063](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1063) |
| `Oversize terrain handshake packet.` | [ColourMaps.cpp:1067](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1067) |
| `Too many pending terrain handshake packets.` | [ColourMaps.cpp:1068](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1068) |
| `The map must be inside Levels\Import.` | [ColourMaps.cpp:1105](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1105) |
| `Map path is too long.` | [ColourMaps.cpp:1115](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1115) |
| `Too many pending map checks.` | [ColourMaps.cpp:1125](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1125) |
| `Error saving Data\land.dat` | [ColourMaps.cpp:1159](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1159) |

## Diagnostic/debug text

Logging is disabled by default. These diagnostics are only emitted if enabled
in the source for troubleshooting; the release DLL creates no Data or TEMP log.

These are written to the network logs or debugger, rather than displayed as frontend captions. Repeated strings appear once. Numeric values, map paths and timestamps are appended at runtime.

| Text | Source |
| --- | --- |
| `joiner host selected` | [ColourMaps.cpp:790](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:790) |
| `transport mode` | [ColourMaps.cpp:791](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:791) |
| `NAT module loaded` | [ColourMaps.cpp:792](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:792) |
| `map wire received` | [ColourMaps.cpp:968](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:968) |
| `native map sequence dropped` | [ColourMaps.cpp:981](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:981) |
| `map Send enter` | [ColourMaps.cpp:1033](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1033) |
| `map Send returned` | [ColourMaps.cpp:1037](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1037) |
| `start Send returned` | [ColourMaps.cpp:1038](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1038) |
| `fkSettings: Local map check failed; network start withheld.` | [ColourMaps.cpp:723](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:723) |
| `control queued` | [ColourMaps.cpp:1082](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1082) |
| `host local map` | [ColourMaps.cpp:1130](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1130) |
| `Go clicked` | [ColourMaps.cpp:1148](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1148) |
| `Go waiting for map` | [ColourMaps.cpp:1173](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1173) |
| `Go preparation failed` | [ColourMaps.cpp:1176](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1176) |
| `local map check failed` | [ColourMaps.cpp:1198](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1198) |
| `map reply accepted` | [ColourMaps.cpp:1201](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1201) |
| `local map confirmed` | [ColourMaps.cpp:1218](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1218) |
| `map confirmation timeout` | [ColourMaps.cpp:1234](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1234) |
| `Go timeout` | [ColourMaps.cpp:1265](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1265) |
| `native Go enter` | [ColourMaps.cpp:1279](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1279) |
| `native Go returned` | [ColourMaps.cpp:1281](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1281) |
| `control timeout` | [ColourMaps.cpp:1296](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1296) |
| `map reply sent` | [ColourMaps.cpp:1305](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1305) |
| `control sent` | [ColourMaps.cpp:1307](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1307) |
| `fkSettings: Local map preview check failed; retrying at Go.` | [ColourMaps.cpp:992](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:992) |
| `local map requested` | [ColourMaps.cpp:1384](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1384) |
| `local source hash actual/expected` | [ColourMaps.cpp:1389](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1389) |
| `local source size actual/expected` | [ColourMaps.cpp:1390](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1390) |
| `local effective hash actual/expected` | [ColourMaps.cpp:1401](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1401) |
| `map handler disabled` | [ColourMaps.cpp:1414](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1414) |
| `map header rejected` | [ColourMaps.cpp:1421](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1421) |
| `map handler received` | [ColourMaps.cpp:1422](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1422) |
| `map sender rejected` | [ColourMaps.cpp:1423](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1423) |
| `local map ready` | [ColourMaps.cpp:1439](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1439) |
| `local map rejected` | [ColourMaps.cpp:1444](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1444) |
| `Go missing map` | [ColourMaps.cpp:1468](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1468) |
| `control received` | [ColourMaps.cpp:1475](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1475) |
| `Go frozen local map` | [ColourMaps.cpp:1505](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1505) |
| `control withheld` | [ColourMaps.cpp:1531](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1531) |
| `IPX start before lobby close` | [ColourMaps.cpp:1541](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1541) |
| `IPX start submission failed` | [ColourMaps.cpp:1544](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1544) |
| `IPX start submitted before lobby close` | [ColourMaps.cpp:1547](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1547) |
| `native send enter` | [ColourMaps.cpp:1552](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1552) |
| `native send returned` | [ColourMaps.cpp:1554](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1554) |
| `ready withheld` | [ColourMaps.cpp:1574](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1574) |
| `ready accepted` | [ColourMaps.cpp:1606](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1606) |
| `host lobby initialized` | [ColourMaps.cpp:1627](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1627) |
| `host setup enter` | [ColourMaps.cpp:1638](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1638) |
| `host setup exit` | [ColourMaps.cpp:1640](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1640) |
| `engine run enter` | [ColourMaps.cpp:1654](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1654) |
| `engine start withheld` | [ColourMaps.cpp:1659](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1659) |
| `engine run returned` | [ColourMaps.cpp:1667](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1667) |
| `engine spawn returned` | [ColourMaps.cpp:1677](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1677) |
| `host terrain prepare enter` | [ColourMaps.cpp:2169](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:2169) |
| `host terrain prepare returned` | [ColourMaps.cpp:2171](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:2171) |
| `terrain generation bypassed` | [ColourMaps.cpp:2184](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:2184) |
| `CRC32 self-test` | [ColourMaps.cpp:2315](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:2315) |
| `frontend path` | [ColourMaps.cpp:2318](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:2318) |
| `DLL path` | [ColourMaps.cpp:2320](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:2320) |
| `Data log path` | [ColourMaps.cpp:2321](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:2321) |
| `TEMP log path` | [ColourMaps.cpp:2322](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:2322) |
| `shared receive hooks` | [ColourMaps.cpp:2324](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:2324) |
| `colour map hooks` | [ColourMaps.cpp:2328](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:2328) |
| `map wire length rejected` | [ColourMaps.cpp:2341](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:2341) |

Additional diagnostic templates:

- `map-network diagnostics 26 protocol 10 build %s %s` (compiler build date and time).
- `pid=%lu Data log append failed error=%lu; using TEMP log`.
- Log entries include a UTC timestamp followed by `pid=`, `tick=` and the message.
- Numeric trace arguments use two eight-digit hexadecimal values.
- Map trace fields use `peer=`, `other=`, `type=`, `ver=`, `rev=`, `wire=`, `seq=` and `crc=`.

## Fixed paths and identifiers

- `Levels\Import` — game-local installed map folder.
- `Data\land.dat` — active terrain output.
- `Data\Water\` — prefix when changing the map's water colour.
- `.dat` — required imported map extension.
- `fkSettings-map-network.log` — log name in the game's Data directory and TEMP directory.
- `fkWorm2NAT.dll` — module name checked for diagnostics.
- `worms2.exe` — game engine filename checked at launch.
- `fkm` — prefix for the temporary file used during atomic publication.

Compiler-only assertion descriptions are `network layout` and `CRC32 polynomial`.
