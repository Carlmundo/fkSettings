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
| `The imported map path is too long.` | [ColourMaps.cpp:839](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:839) |
| `Imported map folders cannot use junctions or symbolic links.` | [ColourMaps.cpp:846](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:846) |
| `Invalid relative map path.` | [ColourMaps.cpp:865](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:865) |
| `Invalid map path encoding.` | [ColourMaps.cpp:867](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:867) |
| `Cannot schedule the local map check.` | [ColourMaps.cpp:1068](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1068) |
| `Oversize terrain handshake packet.` | [ColourMaps.cpp:1072](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1072) |
| `Too many pending terrain handshake packets.` | [ColourMaps.cpp:1073](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1073) |
| `The map must be inside Levels\Import.` | [ColourMaps.cpp:1110](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1110) |
| `Map path is too long.` | [ColourMaps.cpp:1120](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1120) |
| `Too many pending map checks.` | [ColourMaps.cpp:1130](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1130) |
| `Error saving Data\land.dat` | [ColourMaps.cpp:1164](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1164) |

## Diagnostic/debug text

Logging is disabled by default. These diagnostics are only emitted if enabled
in the source for troubleshooting; the release DLL creates no Data or TEMP log.

These are written to the network logs or debugger, rather than displayed as frontend captions. Repeated strings appear once. Numeric values, map paths and timestamps are appended at runtime.

| Text | Source |
| --- | --- |
| `joiner host selected` | [ColourMaps.cpp:795](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:795) |
| `transport mode` | [ColourMaps.cpp:796](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:796) |
| `NAT module loaded` | [ColourMaps.cpp:797](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:797) |
| `map wire received` | [ColourMaps.cpp:973](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:973) |
| `native map sequence dropped` | [ColourMaps.cpp:986](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:986) |
| `map Send enter` | [ColourMaps.cpp:1038](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1038) |
| `map Send returned` | [ColourMaps.cpp:1042](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1042) |
| `start Send returned` | [ColourMaps.cpp:1043](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1043) |
| `fkSettings: Local map check failed; network start withheld.` | [ColourMaps.cpp:723](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:723) |
| `control queued` | [ColourMaps.cpp:1087](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1087) |
| `host local map` | [ColourMaps.cpp:1135](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1135) |
| `Go clicked` | [ColourMaps.cpp:1153](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1153) |
| `Go waiting for map` | [ColourMaps.cpp:1178](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1178) |
| `Go preparation failed` | [ColourMaps.cpp:1181](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1181) |
| `local map check failed` | [ColourMaps.cpp:1203](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1203) |
| `map reply accepted` | [ColourMaps.cpp:1206](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1206) |
| `local map confirmed` | [ColourMaps.cpp:1223](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1223) |
| `map confirmation timeout` | [ColourMaps.cpp:1239](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1239) |
| `Go timeout` | [ColourMaps.cpp:1270](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1270) |
| `native Go enter` | [ColourMaps.cpp:1284](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1284) |
| `native Go returned` | [ColourMaps.cpp:1286](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1286) |
| `control timeout` | [ColourMaps.cpp:1301](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1301) |
| `map reply sent` | [ColourMaps.cpp:1310](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1310) |
| `control sent` | [ColourMaps.cpp:1312](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1312) |
| `fkSettings: Local map preview check failed; retrying at Go.` | [ColourMaps.cpp:992](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:992) |
| `local map requested` | [ColourMaps.cpp:1389](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1389) |
| `local source hash actual/expected` | [ColourMaps.cpp:1394](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1394) |
| `local source size actual/expected` | [ColourMaps.cpp:1395](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1395) |
| `local effective hash actual/expected` | [ColourMaps.cpp:1406](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1406) |
| `map handler disabled` | [ColourMaps.cpp:1419](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1419) |
| `map header rejected` | [ColourMaps.cpp:1426](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1426) |
| `map handler received` | [ColourMaps.cpp:1427](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1427) |
| `map sender rejected` | [ColourMaps.cpp:1428](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1428) |
| `local map ready` | [ColourMaps.cpp:1444](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1444) |
| `local map rejected` | [ColourMaps.cpp:1449](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1449) |
| `Go missing map` | [ColourMaps.cpp:1473](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1473) |
| `control received` | [ColourMaps.cpp:1480](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1480) |
| `Go frozen local map` | [ColourMaps.cpp:1510](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1510) |
| `control withheld` | [ColourMaps.cpp:1536](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1536) |
| `IPX start before lobby close` | [ColourMaps.cpp:1546](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1546) |
| `IPX start submission failed` | [ColourMaps.cpp:1549](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1549) |
| `IPX start submitted before lobby close` | [ColourMaps.cpp:1552](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1552) |
| `native send enter` | [ColourMaps.cpp:1557](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1557) |
| `native send returned` | [ColourMaps.cpp:1559](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1559) |
| `ready withheld` | [ColourMaps.cpp:1579](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1579) |
| `ready accepted` | [ColourMaps.cpp:1611](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1611) |
| `host lobby initialized` | [ColourMaps.cpp:1632](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1632) |
| `host setup enter` | [ColourMaps.cpp:1643](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1643) |
| `host setup exit` | [ColourMaps.cpp:1645](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1645) |
| `engine run enter` | [ColourMaps.cpp:1659](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1659) |
| `engine start withheld` | [ColourMaps.cpp:1664](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1664) |
| `engine run returned` | [ColourMaps.cpp:1672](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1672) |
| `engine spawn returned` | [ColourMaps.cpp:1682](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1682) |
| `host terrain prepare enter` | [ColourMaps.cpp:2202](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:2202) |
| `host terrain prepare returned` | [ColourMaps.cpp:2204](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:2204) |
| `terrain generation bypassed` | [ColourMaps.cpp:2217](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:2217) |
| `CRC32 self-test` | [ColourMaps.cpp:2348](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:2348) |
| `frontend path` | [ColourMaps.cpp:2351](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:2351) |
| `DLL path` | [ColourMaps.cpp:2353](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:2353) |
| `Data log path` | [ColourMaps.cpp:2354](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:2354) |
| `TEMP log path` | [ColourMaps.cpp:2355](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:2355) |
| `shared receive hooks` | [ColourMaps.cpp:2357](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:2357) |
| `colour map hooks` | [ColourMaps.cpp:2361](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:2361) |
| `map wire length rejected` | [ColourMaps.cpp:2374](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:2374) |

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
