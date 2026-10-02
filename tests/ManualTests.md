Test all of the following with a modern OS and Windows XP.

- [ ] Main Menu: Confirm the Exit button is correctly positioned, fully visible, and clickable at high DPI.
- [ ] Hint text: Confirm hints are readable and do not overlap or get clipped at 200% scaling.
- [ ] Video Options: Confirm the Advanced Options button appears when settings.exe is present, scales correctly, and launches it when clicked.
- [ ] Music Options: Confirm track names and list rows remain readable, correctly spaced, and selectable at high DPI.
- [ ] Team Editor: Confirm the Play button previews random samples from the selected soundbank. Click it several times and repeat with different soundbanks.
- [ ] Weapon Editor: Confirm Tab and Shift+Tab move through controls in a logical order, including secret weapon controls.
- [ ] Secret weapons — schemes: Change several secret weapon stocks, save the scheme, load another scheme, then reload the saved scheme. Confirm every value is restored.
- [ ] Secret weapons — unsaved changes: Change a secret weapon’s stock without saving. Confirm “User defined” appears in the Game Options tab.
- [ ] Secret weapons — offline: Start a local game and confirm teams receive the configured secret weapons and stocks.
- [ ] Secret weapons — online: Change secret stocks after another player has joined, then test a player joining after the changes. Confirm both receive the current values.
- [ ] Secret weapons — online: Start a multiplayer game and confirm all players receive the host’s configured secret weapon stocks, with no desynchronisation.
- [ ] CPU teams — icons: Confirm the host and joining players see the correct difficulty icons, including when a team is selected.
- [ ] CPU teams - Lobby: Add, remove, and re-add CPU teams before and after another player joins. Confirm icons and team lists update on both computers.
- [ ] CPU teams — IPX: Complete a game containing both human and CPU teams.
- [ ] CPU teams — Internet: Complete a game containing both human and CPU teams.
- [ ] End Game Statistics: Confirm text, columns, and buttons are correctly spaced at high DPI, with no clipping or overlap.

# Extended Options (Dialog 154)

- [ ] Scroll below the native groups and confirm "Extended Options" displays all 27 settings in two balanced columns inside a single group box, without inner group boxes or a "Herd weapons" label. Confirm Persistent Rope replaces Fast Crates before Rapid Play; a half-row blank gap follows Rapid Play, then Indestructible Terrain and Invisible Terrain. In the right column, confirm Fast Crates and Crate Spy precede Crate Limit and Crate Rate; Aqua Sheep and Instant Mines precede Herd weapon: Dynamite and the remaining herd weapons. Confirm all blank gaps are half a checkbox row high and "Disable Unlocked Aim" is last. Checkboxes should match the original controls' height and spacing; trackbars should be close beneath their labels, with matching borders and readout alignment. Confirm complete labels at normal and high DPI.
- [ ] Check the three slider ranges: Super Shopper Crates/Crate Limit/Crate Rate 0–100. At zero, verify frontend string 141 for Super Shopper Crates and string 99 for the other two. At 100, Super Shopper Crates should show string 4950 ("Unlimited"); intermediate values should display numbers. Confirm "Low Gravity" is a checkbox that stores 0 when unchecked and 1 when checked. Confirm "Super Shopper" is absent.
- [ ] Use Tab/Shift+Tab through the new checkboxes and Alt+Tab with one focused; confirm normal focus and frontend responsiveness.
- [ ] Check different combinations, save an `.opt`, load Default and reload the saved scheme. Confirm independent values, "User defined" naming, Save As/Default state, and native option values.
- [ ] Select Default and another native scheme from both scheme dropdowns, including while the options editor is hidden; confirm all 27 values reset and special zero labels return.
- [ ] Save an `.opt` and confirm `PLUS` at 135–138, including with every option unset. Launch a local game with any option enabled and inspect `Data/extended.dat`: exactly 31 bytes, `PLUS` at 0–3 and the documented boolean/numeric values at 4–30. Launch again with Default/all options unset and confirm all 31 bytes are zero, including the first four.
- [ ] Confirm native `Data/game.dat` remains 3,284 bytes. Use a compatible engine extension to verify each feature's actual behavior.
- [ ] Host a game with two updated frontend/engine clients; edit and load schemes, test a late join, and confirm identical options and sidecars on both clients before starting a complete match.
