# fkSettings for Worms 2
 Extends the frontend interface with an extra Advanced Options button in the Video options tab (runs settings.exe), and launches IPX Address Book (ipxaddress.exe by default) when trying to open the Address Book with IPX mode selected in Network play

Includes **fkWaterFix**: fixes the Terrain editor's water colour preview when
the registry's `W2PATH` is `.`. Use the combined `fkSettings.dll` in place of the
separate `fkWaterFix.dll`. The existing Windows XP Release build configuration
is preserved. See [integration and verification](docs/water-fix.md).

Also adds stock editors for **eight secret weapons** to the Weapons screen in
the supported Worms 2 frontend. Stocks persist in an appended `.wep` extension
and populate the existing `game.dat` stock slots for all six teams.
See [implementation notes, build instructions, and verification](docs/secret-weapons.md).

Network hosts can also add saved **computer-controlled teams**, retaining their
AI difficulty. All players need the updated `fkSettings.dll` for these matches.
See [network team implementation and verification](docs/network-teams.md).

Game options (dialog 154) now includes **Extended Options** with 24 checkboxes
and three numeric trackbars. Values persist in an appended `.opt` extension, transfer with the
host's online options, and are written to `Data/extended.dat` for an engine
extension to consume. See [file offsets and verification](docs/extended-options.md).

The Select Level screen includes **Import map...** for premade indexed-colour
`.dat` terrains in local and online games, with a colour preview and **Use generated map**
reset. Maps retain their palettes, collision masks and spawn points without
locking `land.dat`. Online hosts transfer the map to other players, including
late joins and subsequent rounds. All players need the updated DLL.
See [usage and verification](docs/colour-maps.md).

Startup hooks are activated in batches to avoid repeated thread suspension.
See [startup timing and verification](docs/startup-performance.md).
