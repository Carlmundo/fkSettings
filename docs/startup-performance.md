# Startup performance

Enabling MinHook detours separately repeats thread enumeration, suspension,
and resumption for every detour. The Windows API hooks and the Secret Weapons,
Frontend Network, Extended Options and Network Teams groups already queue their
hooks with `MH_QueueEnableHook` and activate each group with one `MH_ApplyQueued`.

Colour Maps now does the same for its 23 hooks, replacing 23 individual
activation passes with one. Signature validation still happens before hook
creation. Hook creation, queueing or activation failure retains the existing
disable/remove cleanup, and networking is enabled only after successful
activation. The Release toolset remains `v141_xp`.

The startup benchmark now includes Colour Maps installation after the other
feature groups. It checks that all 23 Colour Maps sites are patched and their
original bytes restored by MinHook cleanup, and reports their installation time
separately as well as including it in the total hook-installation time.

On the development machine on 2026-10-09, the median Colour Maps installation
time fell from 17.892 ms to 1.100 ms. Total feature-hook installation fell from
21.508 ms to 4.862 ms. Both measurements used the extended benchmark with eight
measured iterations after one warm-up, against the same frontend. These are
private image-mapping measurements, not total game launch times.

For historical comparison, on 2026-10-06 the median feature-hook installation
time fell from 3,562.126 ms to 340.671 ms. Pattern scanning and water-fix
installation took 2.250 ms before and 2.152 ms after; the scanner was unchanged.
These measurements cover a private mapping of the supplied frontend and the
31 feature hooks, excluding the three Windows API hooks, DLL dependency
loading, and the frontend's own startup. They are not total launch times.

## Verification

From a Visual Studio developer PowerShell prompt:

```powershell
MSBuild.exe tests/StartupTests.vcxproj /t:Build /p:Configuration=Release /p:Platform=Win32
./Release/StartupTests.exe 'D:/Games/Worms 2/frontend.exe'
```

The test uses a fresh non-running image mapping per iteration, adjusts the
absolute operands inspected by Extended Options as the existing regression
fixture does, installs Secret Weapons, Frontend Network, Extended Options,
Network Teams and Colour Maps together, and checks activation and restoration.
It reports medians over eight runs after an initial warm-up.
The executable file is never modified.

The Release DLL build, extended startup test and XP-toolset Colour Maps fixture
passed for the batching change. The full Colour Maps suite stops at a
pre-existing localized folder-string assertion. An isolated runner using the
same test source with only `LanguageTests()` omitted passed all remaining
Colour Maps regressions, including actual hook installation, signature rejection,
restoration and TCP/IP/IPX launch ordering. Full launch timing and execution on
Windows XP remain manual checks.
