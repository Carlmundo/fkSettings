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

## Validation and internal exception messages

These 31 unique messages are hardcoded in English. The folder-selection error is translated separately below. File-selection and terrain-validation exceptions can appear in the Import error dialog. Network queue/encoding exceptions are handled internally; they do not all become visible popups.

| Message | Source |
| --- | --- |
| `Truncated terrain file.` | [ColourMaps.cpp:80](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:80) |
| `Invalid terrain resource path.` | [ColourMaps.cpp:91](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:91) |
| `Missing terrain image.` | [ColourMaps.cpp:101](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:101) |
| `Expected an 8-bit foreground and 1-bit terrain masks.` | [ColourMaps.cpp:112](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:112) |
| `Invalid image palette.` | [ColourMaps.cpp:117](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:117) |
| `The foreground has no palette.` | [ColourMaps.cpp:127](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:127) |
| `This frontend supports standard 1920 x 696 terrain files.` | [ColourMaps.cpp:130](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:130) |
| `Image exceeds its dimensions.` | [ColourMaps.cpp:141](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:141) |
| `Invalid compressed terrain image.` | [ColourMaps.cpp:151](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:151) |
| `Incomplete compressed terrain image.` | [ColourMaps.cpp:154](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:154) |
| `Incorrect terrain image size.` | [ColourMaps.cpp:166](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:166) |
| `Foreground index exceeds its palette.` | [ColourMaps.cpp:170](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:170) |
| `Terrain file is too large.` | [ColourMaps.cpp:177](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:177) |
| `Not a complete Worms 2 LND terrain file.` | [ColourMaps.cpp:182](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:182) |
| `Invalid cavern flag.` | [ColourMaps.cpp:186](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:186) |
| `Expected 18 to 32 object locations.` | [ColourMaps.cpp:189](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:189) |
| `Object location is outside the map.` | [ColourMaps.cpp:194](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:194) |
| `Unexpected trailing terrain data.` | [ColourMaps.cpp:201](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:201) |
| `Cannot open the selected terrain file.` | [ColourMaps.cpp:244](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:244) |
| `Cannot read a complete terrain file (maximum 8 MB).` | [ColourMaps.cpp:255](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:255) |
| `The imported map path is too long.` | [ColourMaps.cpp:501](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:501) |
| `Imported map folders cannot use junctions or symbolic links.` | [ColourMaps.cpp:508](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:508) |
| `Invalid relative map path.` | [ColourMaps.cpp:527](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:527) |
| `Invalid map path encoding.` | [ColourMaps.cpp:529](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:529) |
| `Cannot schedule the local map check.` | [ColourMaps.cpp:729](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:729) |
| `Oversize terrain handshake packet.` | [ColourMaps.cpp:733](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:733) |
| `Too many pending terrain handshake packets.` | [ColourMaps.cpp:734](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:734) |
| `The map must be inside Levels\Import.` | [ColourMaps.cpp:770](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:770) |
| `Map path is too long.` | [ColourMaps.cpp:780](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:780) |
| `Too many pending map checks.` | [ColourMaps.cpp:790](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:790) |
| `Error saving Data\land.dat` | [ColourMaps.cpp:1664](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1664) |

## Diagnostic/debug text

These are written to the network logs or debugger, rather than displayed as frontend captions. Repeated strings appear once. Numeric values, map paths and timestamps are appended at runtime.

| Text | Source |
| --- | --- |
| `joiner host selected` | [ColourMaps.cpp:457](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:457) |
| `transport mode` | [ColourMaps.cpp:458](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:458) |
| `NAT module loaded` | [ColourMaps.cpp:459](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:459) |
| `map wire received` | [ColourMaps.cpp:635](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:635) |
| `native map sequence dropped` | [ColourMaps.cpp:648](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:648) |
| `map Send enter` | [ColourMaps.cpp:699](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:699) |
| `map Send returned` | [ColourMaps.cpp:703](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:703) |
| `start Send returned` | [ColourMaps.cpp:704](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:704) |
| `fkSettings: Local map check failed; network start withheld.` | [ColourMaps.cpp:723](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:723) |
| `control queued` | [ColourMaps.cpp:748](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:748) |
| `host local map` | [ColourMaps.cpp:795](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:795) |
| `Go clicked` | [ColourMaps.cpp:812](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:812) |
| `Go waiting for map` | [ColourMaps.cpp:837](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:837) |
| `Go preparation failed` | [ColourMaps.cpp:840](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:840) |
| `local map check failed` | [ColourMaps.cpp:860](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:860) |
| `map reply accepted` | [ColourMaps.cpp:863](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:863) |
| `local map confirmed` | [ColourMaps.cpp:873](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:873) |
| `map confirmation timeout` | [ColourMaps.cpp:889](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:889) |
| `Go timeout` | [ColourMaps.cpp:916](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:916) |
| `native Go enter` | [ColourMaps.cpp:930](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:930) |
| `native Go returned` | [ColourMaps.cpp:932](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:932) |
| `control timeout` | [ColourMaps.cpp:947](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:947) |
| `map reply sent` | [ColourMaps.cpp:956](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:956) |
| `control sent` | [ColourMaps.cpp:958](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:958) |
| `fkSettings: Local map preview check failed; retrying at Go.` | [ColourMaps.cpp:992](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:992) |
| `local map requested` | [ColourMaps.cpp:1033](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1033) |
| `local source hash actual/expected` | [ColourMaps.cpp:1038](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1038) |
| `local source size actual/expected` | [ColourMaps.cpp:1039](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1039) |
| `local effective hash actual/expected` | [ColourMaps.cpp:1050](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1050) |
| `map handler disabled` | [ColourMaps.cpp:1062](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1062) |
| `map header rejected` | [ColourMaps.cpp:1069](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1069) |
| `map handler received` | [ColourMaps.cpp:1070](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1070) |
| `map sender rejected` | [ColourMaps.cpp:1071](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1071) |
| `local map ready` | [ColourMaps.cpp:1084](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1084) |
| `local map rejected` | [ColourMaps.cpp:1089](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1089) |
| `Go missing map` | [ColourMaps.cpp:1111](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1111) |
| `control received` | [ColourMaps.cpp:1118](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1118) |
| `Go frozen local map` | [ColourMaps.cpp:1147](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1147) |
| `control withheld` | [ColourMaps.cpp:1172](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1172) |
| `IPX start before lobby close` | [ColourMaps.cpp:1182](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1182) |
| `IPX start submission failed` | [ColourMaps.cpp:1185](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1185) |
| `IPX start submitted before lobby close` | [ColourMaps.cpp:1188](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1188) |
| `native send enter` | [ColourMaps.cpp:1193](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1193) |
| `native send returned` | [ColourMaps.cpp:1195](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1195) |
| `ready withheld` | [ColourMaps.cpp:1214](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1214) |
| `ready accepted` | [ColourMaps.cpp:1246](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1246) |
| `host lobby initialized` | [ColourMaps.cpp:1267](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1267) |
| `host setup enter` | [ColourMaps.cpp:1279](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1279) |
| `host setup exit` | [ColourMaps.cpp:1281](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1281) |
| `engine run enter` | [ColourMaps.cpp:1295](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1295) |
| `engine start withheld` | [ColourMaps.cpp:1300](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1300) |
| `engine run returned` | [ColourMaps.cpp:1308](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1308) |
| `engine spawn returned` | [ColourMaps.cpp:1318](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1318) |
| `host terrain prepare enter` | [ColourMaps.cpp:1810](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1810) |
| `host terrain prepare returned` | [ColourMaps.cpp:1812](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1812) |
| `terrain generation bypassed` | [ColourMaps.cpp:1825](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1825) |
| `CRC32 self-test` | [ColourMaps.cpp:1957](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1957) |
| `frontend path` | [ColourMaps.cpp:1960](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1960) |
| `DLL path` | [ColourMaps.cpp:1962](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1962) |
| `Data log path` | [ColourMaps.cpp:1963](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1963) |
| `TEMP log path` | [ColourMaps.cpp:1964](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1964) |
| `shared receive hooks` | [ColourMaps.cpp:1966](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1966) |
| `colour map hooks` | [ColourMaps.cpp:1970](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1970) |
| `map wire length rejected` | [ColourMaps.cpp:1983](C:/Users/carln/Documents/GitHub/fkSettings/fkSettings/ColourMaps.cpp:1983) |

Additional diagnostic templates:

- `map-network diagnostics 22 protocol 10 build %s %s` (compiler build date and time).
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
