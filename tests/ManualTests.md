Test all of the following with a modern OS and Windows XP.

- [ ] Frontend return after Alt+Tab: Start Quick Play and a normal local match, Alt+Tab to the frontend while the engine is running, return to the engine, then finish/exit the match. Confirm the frontend comes back and accepts input. Repeat for a second round, TCP/IP and IPX; also check normal completion without switching apps and launch failure.

- [ ] 48-worm setup — local: Add six teams, select eight worms each, remove and re-add teams, and confirm eight remains selectable. Also try smaller counts. A seventh team and a ninth worm must remain unavailable.
- [ ] 48-worm setup — launch: With the engine extension installed, start six teams of eight. Confirm all 48 names and health values load from `Data\game.dat` (still `0xCD4` bytes), and repeat for the next round.
- [ ] 48-worm setup — online: Repeat on TCP/IP and IPX with the updated DLL on every computer, using human and CPU teams. Check late joins, remove/re-add teams, and subsequent rounds; all computers should write the same full roster.

- [ ] Main Menu: Confirm the Exit button is correctly positioned, fully visible, and clickable at high DPI.
- [ ] Video Options: Confirm the Advanced Options button appears when settings.exe is present, scales correctly, and launches it when clicked.
- [ ] Music Options: Confirm track names and list rows remain readable, correctly spaced, and selectable at high DPI.
- [ ] Team Editor: Confirm the Play button previews random samples from the selected soundbank. Click it several times and repeat with different soundbanks.
- [ ] Hint text: Confirm hints are readable and do not overlap or get clipped at 200% scaling.
- [ ] Terrain editor — water preview: With `W2PATH` set to `.` in `HKEY_CURRENT_USER\Software\Team17SoftwareLTD\Worms2`, without the old standalone `fkWaterFix.dll`. Open the Terrain editor and check the colours are all visible.
- [ ] Weapon Editor: Confirm Tab and Shift+Tab retain their entry point and order on every native weapon page. Switch weapons repeatedly, return to earlier pages, and close/reopen the editor. Check sliders, checkboxes and readout labels at 100%, 150% and 200% scaling, including focus and unchanged control bounds. Repeat navigation through the secret-weapon stock editor.
- [ ] Secret weapons — schemes: Change several secret weapon stocks, save the scheme, load another scheme, then reload the saved scheme. Confirm every value is restored.
- [ ] Secret weapons — unsaved changes: Change a secret weapon’s stock without saving. Confirm “User defined” appears in the Game Options tab.
- [ ] Secret weapons — offline: Start a local game and confirm teams receive the configured secret weapons and stocks.
- [ ] Repeat swings: Set slider to -1 and confirm the value shows as "Unlimited".
- [ ] Extended options - At 100, Super Shopper Crates should show string 4950 ("Unlimited")
- [ ] Use Tab/Shift+Tab through the new checkboxes and Alt+Tab with one focused; confirm normal focus and frontend responsiveness.
- [ ] Check different combinations, save an `.opt`, then click the Default button. Confirm all extended checkboxes clear, all extended sliders reset to 0. Check "User defined" naming and Save As/Default state.
- [ ] Select vanilla schemes from both scheme dropdowns, including while the options editor is hidden; confirm all 27 values reset and special zero labels return.
- [ ] Secret weapons — online: Change secret stocks after another player has joined, then test a player joining after the changes. Start a multiplayer game and confirm all players receive the host’s configured secret weapon stocks, with no desynchronisation.
- [ ] CPU teams — icons: Confirm the host and joining players see the correct difficulty icons, including when a team is selected.
- [ ] CPU teams - Lobby: Add, remove, and re-add CPU teams before and after another player joins. Confirm icons and team lists update on both computers.
- [ ] CPU teams — IPX: Complete a game containing both human and CPU teams.
- [ ] CPU teams — Internet: Complete a game containing both human and CPU teams.
- [ ] End Game Statistics: Confirm text, columns, and buttons are correctly spaced at high DPI, with no clipping or overlap.
- [ ] IPX provider compatibility: Check that a 3 player game can initialise with a colour map.
- [ ] XP IPX joiner — engine launch: With XP SP3 `dplayx.dll` 5.3.2600.5512, join a modern host and start a generated-map game, then an imported-map game. Confirm the frontend survives launch and returns after the engine exits. Repeat for a subsequent round, disconnect/rejoin, small rosters and 48 worms. Check XP hosting and TCP/IP joining too. If a crash remains, record the module offset, exception code and faulting read address/registers (the reported `0x28F0C` alone does not establish that the provider table was null).
- [ ] Colour maps: Import, select a .dat in a subfolder and confirm it loads. Try another folder outisde the permitted area.
- [ ] Colour-map language: Set language.txt to a non english code then use Import to choose a .dat outside Levels\Import.
- [ ] Colour maps: Online - Load map, change water colour and Open to Cavern (or vice versa).
- [ ] Colour maps online: Install identical maps and this DLL on every PC. Select Levels\Import\Online Worms\01 Path to Hell.dat; confirm each player resolves the same relative path under its own game folder, displays the map, writes matching Data\land.dat. Complete first games and subsequent rounds on TCP/IP and IPX with an XP joiner.
- [ ] Colour maps online local files: Test errors for files not found and file mismatches
