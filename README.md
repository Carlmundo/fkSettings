# fkSettings for Worms 2
 Extends the frontend interface with an extra Advanced Options button in the Video options tab (runs settings.exe), and launches IPX Address Book (ipxaddress.exe by default) when trying to open the Address Book with IPX mode selected in Network play

Also adds stock editors for **eight secret weapons** to the Weapons screen in
the supported Worms 2 frontend. Stocks persist in an appended `.wep` extension
and populate the existing `game.dat` stock slots for all six teams.
See [implementation notes, build instructions, and verification](docs/secret-weapons.md).

Network hosts can also add saved **computer-controlled teams**, retaining their
AI difficulty. All players need the updated `fkSettings.dll` for these matches.
See [network team implementation and verification](docs/network-teams.md).
