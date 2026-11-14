# Offline alliances

The single-player Game controls page automatically enables alliances when its
player roster contains at least two teams. Adding the second team gives both
teams a colour square in addition order: Red (1), then Blue (2). Further teams
use the remaining colours: Green (3), Yellow (4), Purple (5), Teal (6), unless
the current colour choices make another colour less used. The roster's visual
sorting does not affect addition order.

| Colour | RGB |
| --- | --- |
| Red | 255, 128, 128 |
| Blue | 128, 128, 255 |
| Green | 128, 255, 128 |
| Yellow | 255, 255, 128 |
| Purple | 255, 128, 255 |
| Teal | 128, 255, 255 |

Left-click a square to cycle its team's colour forwards, or right-click to cycle
backwards (Blue to Red, Red to Teal). Colour changes use mouse clicks only.
Teams can share colours to form alliances. Newly added teams immediately
receive the least-used colour, with ties resolved in the order above. For
example, Red and Yellow are followed by Blue. Removing a team preserves the
other assignments while at least two teams remain; a re-added team gets a fresh
least-used colour. Removing teams until zero or one remains disables alliances,
clears assignments and hides the squares. Returning to two teams assigns Red
and Blue in addition order again.

The engine extension supplies shared victory and in-game colour behaviour. The
frontend awards a round win to every member of the winning alliance, including
teammates eliminated before the round ended.
Online selection, ownership and synchronisation are described in
[online alliances](online-alliances.md).

## Launch file

Every native `game.dat` write also creates or overwrites `teams.dat` in the same
directory (normally `Data`). Its length is exactly seven bytes:

| Offset | Contents |
| --- | --- |
| 0 | 1 when this is an alliance launch; otherwise 0 |
| 1–6 | Each player's colour/alliance number, 1–6, in `game.dat` team-slot order; 0 for an empty slot |

With zero or one team in the roster, all seven bytes are zero. Quick Play and
other non-alliance writes also publish seven zeros so a previous match cannot
supply stale alliances. Network launches use their host-approved assignments.
The existing `game.dat` and `extended.dat` formats are unchanged.

The frontend orders teams before writing `game.dat`. Assignments are matched
by team name at serialization, so colours remain with their teams, including
subsequent rounds after the setup page has closed. An enabled local launch with
an unrecognized team fails rather than silently assigning an incorrect alliance.

The writer removes the previous sidecar, writes a complete temporary file, closes
it, and replaces `teams.dat` before entering the native game writer. A failed
sidecar write reports failure to the local launch path; a failed native game
write removes the sidecar.

## Turn order

For the first offline round, if at least two teams share a colour and there is
another opposing colour, the frontend shuffles the colours in use and cycles
through them, taking one team from each colour on each pass. A colour is skipped
once all of its teams have been placed. The original team shuffle randomizes
members within each colour before this interleaving. Colour selection does not
favour alliances with more teams.

For three Red, two Blue and one Yellow teams:

| Shuffled colour order | First-round team order |
| --- | --- |
| Blue, Yellow, Red | Blue, Yellow, Red, Blue, Red, Red |
| Red, Blue, Yellow | Red, Blue, Yellow, Red, Blue, Red |

The native swap routine moves the full `0x108`-byte team record and its companion
pointer together. Colour assignments and scoring context are then serialized in
that final order. Later rounds keep this order and use the native starting-index
rotation: `(previous + 1) % teamCount`, at game-object offset `0xAB4` / file offset
`0xA98`. Games with distinct colours and Quick Play retain the original shuffle.
[Online alliances](online-alliances.md) use the same interleaving.

The hook at `0x0042BCFC` runs the native shuffle first, then interleaves the groups
inside the first local launch handler or the validated online first-round call
sites. It validates all names and colours
before consuming extra random values or moving teams. Colour shuffling uses the
frontend's seeded CRT random generator at `0x00496770`; complete teams are swapped
by `0x0042BD78`. An invalid mapping retains the original shuffled order.

## Round results

After a successful alliance `game.dat` write, the frontend keeps the names,
alliance numbers and pre-round scores actually serialized for that match. Subsequent roster edits
cannot change that scoring context. Every non-alliance or Quick Play
launch clears it, as does a failed launch-data write.

The native result reader at `0x0041E9BC` (thunk `0x0040254A`) increments only
one team's live score. Its hook uses the native winner's score change to find
the winning alliance and assigns each member its pre-round score plus one.
Names are matched independently of launch or display order. The fix runs before
either results tab refreshes or the local match tests its victory requirement.
Failed reads, draws, unmatched names or ambiguous score changes retain native
behaviour. Reapplying a score update does not grant another win.

These live scores are at game-object offset `0x485 + team * 0x108`. They supply
the Worms2 tab's Score column, subsequent-round `game.dat` scores, and the local
match-completion check at `0x0042BC8E`. They are separate from the statistics
tables below; both paths must award the same alliance to keep the tabs and
remaining rounds consistent.

The native result-awarding routine at `0x0042DF64` (thunk `0x00402180`) stops at
the first surviving, non-excluded team. The hook runs this routine first, then
sets the same round-win flags for all members of the winning alliance in both
the round and accumulated tables. These are flags, not counters; native
aggregation still updates scores and saved statistics. Repeated awarding does
not add multiple wins for the same round.

The results object uses the following fields, with a `0x644`-byte team stride:

| Offset | Contents |
| --- | --- |
| `0x4` | Number of teams |
| `0x1C` | Result status; native awarding requires 3 |
| `0x60 + team * 0x644` | Accumulated-table round-win flag |
| `0x68C + team * 0x644` | Accumulated-table team name |
| `0x25F8 + team * 0x644` | Round-table win flag |
| `0x2B84 + team * 0x644` | Native exclusion flag |
| `0x2BB8 + team * 0x644` | Native survivor field |
| `0x2C24 + team * 0x644` | Round-table team name |

Both tables are matched independently by name because their sorting can differ
from launch order and each other. All names and mappings are checked before any
extra awards. With no survivors, conflicting surviving alliances, missing or
duplicate names, or a mismatched launch context, the native result is retained.

For `01 04 02 02 02 00 00` with launch order Helmut, Donut, Lenny, Buddy,
a surviving Blue team awards wins to Donut, Lenny and Buddy; Helmut receives
none. The eliminated members of Blue still share their alliance's win.

On the Game Over statistics page, the winner sentence is omitted when more than
one team has reached the required number of victories. The heading, scores and
worm awards are unchanged. A tied lead below the victory limit does not suppress
the sentence, and a sole game winner retains the native text.

The supported frontend's Game Over renderer at `0x0047B6CD` requests string 340
(`%Winner% wins the game.`) after drawing the Game Over heading. String 338
contains the older `%1 wins the game!` template, but has no active caller found
in this executable. Both resource IDs are guarded in the native resource-text
renderer, so the rule also applies if 338 is requested and works across languages.
Winner detection uses the accumulated statistics at `0x0057EB60`: the victory
limit at `0xC` and scores at `0x60 + team * 0x644`.

## Integration

`Alliances.cpp/.h` contains the shared palette, team model, turn ordering, file
writer, frozen scoring context and results hooks used by both modes.
`OfflineAlliances.cpp/.h` contains the local roster controls and local launch
scope. `OnlineAlliances.cpp/.h` contains lobby controls, permissions and network
synchronisation. Local controls register launch-data and shuffle callbacks with
the shared module; online integration continues through its existing callbacks.

Both hook-owning modules validate the supported x86 frontend (PE timestamp
`0x3587BE19`) and their signatures before installing their respective hooks:

| RVA | Module | Purpose |
| --- | --- | --- |
| `0x78F7B` | OfflineAlliances | Local Game controls refresh; attach the roster subclass |
| `0x79343` | OfflineAlliances | First offline game; scope alliance serialization to this local launch |
| `0x77DA2` | OfflineAlliances | Subsequent offline rounds; retain the match's assignments |
| `0x2BCFC` | Alliances | Interleave shuffled alliance colours on the first local or online round |
| `0x2BA3` | Alliances | Shared jump thunk to the game writer; write the seven-byte sidecar |
| `0x2DF64` | Alliances | Award the winning alliance in round results |
| `0x1E9BC` | Alliances | Update live game scores for all winning allies before result display and victory checks |
| `0x7D917` | Alliances | Omit winner strings 338 and 340 when multiple teams reach the victory limit |

The list subclass follows native item insertions and deletions, using each row's
saved-team index to preserve addition order across sorting. State images supply
the colour squares alongside the native human/CPU icons. The image list contains
six colours at indices 0–5; the list's state-image numbers 1–6 select them, while
state-image number 0 hides the square. Square sizes use the roster font. The
roster keeps its native bounds and scrolling behaviour; no extra checkbox is
created. Activation follows the roster count after each insertion or deletion.

With application-controlled high DPI scaling, the classic list view positions
state images relative to its small-image cell, even when the font makes the row
taller. The roster uses a private copy of the worm/CPU sprite list when needed,
with transparent padding that centres each unchanged sprite in a cell at least
as high as the colour square. The square size comes from the roster's already
DPI-scaled font; no second DPI multiplier is applied. This keeps both image
types inside and centred on the text row, including the first row. Font changes
and setup refreshes update the layout. Native sprite indices and widths are
preserved, shared image lists are untouched, and both original lists are
restored before control destruction.

The writer thunk is distinct from NetworkTeams' writer entry hook, and the local
refresh hook is distinct from ColourMaps' page creation hook. This allows the
features to coexist without competing for a MinHook target.

## Verification

```powershell
MSBuild.exe tests/OfflineAlliancesTests.vcxproj /p:Configuration=Release /p:Platform=Win32
./Release/OfflineAlliancesTests.exe 'D:\Games\Worms 2\frontend.exe'
MSBuild.exe tests/StartupTests.vcxproj /p:Configuration=Release /p:Platform=Win32
./Release/StartupTests.exe 'D:\Games\Worms 2\frontend.exe'
```

Automated coverage includes rendered pixels for all six colours, transparent
padding and outlines at 16, 24 and 32 pixels, insertion versus display order,
automatic activation at two teams, deactivation at zero/one team and reactivation,
least-used ties, removal/re-addition, colour wraparound, real Win32 list
interactions, unchanged colours on Space, mouse cycling in both directions, reverse wraparound,
single and double clicks, native bounds and tab order,
cleanup, exact active/inactive bytes, shuffled serialization, missing slots,
online/Quick Play reset, write failures, native signatures, and combined hook
installation. Result tests execute the native award routine via its actual
thunk before and after installation, reproduce the one-versus-three example,
and check independently sorted tables, eliminated allies, a lone opponent's win,
six teams, repeated awards, draws, invalid mappings and snapshot cleanup.
Consecutive-round tests execute the native single-team score-increment block,
statistics accumulation and both match-completion routines. They check two
consecutive alliance wins finish a first-to-two match on round two, alternating
winners require round three, and both tabs retain identical named-team totals
despite different team orders and different surviving allied winners.
Renderer-hook tests cover both winner string IDs, sole winners, tied leads below
the victory limit, two to six winners, invalid statistics and forwarding of
unrelated text with its original arguments.
In-game victory and actual frontend/XP rendering remain manual
checks in `tests/ManualTests.md`.

Native dialog rendering checks also sample the actual roster pixels for all six
assignments, the first team's full colour cycle, and each team added while
alliances are enabled. These cover the mapping between state-image numbers and
image-list indices, including the first team and Teal.
High-DPI checks render the native dialog with 100%, 150% and 200% fonts, including
font changes in both directions. They verify unclipped, centred squares in all
six rows, pixel-preserved native worm/CPU sprites, mouse hit testing, shared-list
isolation and image-list restoration during cleanup.
Turn-order checks cover both documented examples, all six three-colour
permutations, balanced and five-versus-one alliances, Teal, and invalid mappings.
Native execution checks run the original random generator, shuffle, full-record
swaps and starting-index rotation across 96 seeds, preserving every team byte
and companion pointer and checking final `teams.dat` colours. Distinct colours
and non-initial/non-local contexts retain the exact native shuffled bytes.
