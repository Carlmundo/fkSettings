# Startup performance

fkSettings installs 35 MinHook detours when the frontend loads: three Windows
API hooks, ten secret-weapon hooks, seventeen extended-option hooks, and five
network-team hooks. Enabling each separately repeated MinHook's thread
enumeration, suspension, and resumption for every detour.

Each group now queues its own hooks with `MH_QueueEnableHook` and activates
them with one `MH_ApplyQueued` call. Startup therefore needs four activation
passes. Signature validation still happens before hook creation, and failed
installations retain their existing cleanup. The Release toolset remains
`v141_xp`.

The shared Default loader hook brings the current feature-hook count to 32;
the activation still uses four passes. The benchmark below preceded that
additional hook and measured 31 feature hooks.

On the development machine on 2026-10-06, the median feature-hook installation
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
fixture does, installs all three feature groups together, and checks activation
and restoration. It reports medians over eight runs after an initial warm-up.
The executable file is never modified.

The Release DLL, combined startup test, and SecretWeapons, ExtendedOptions,
NetworkTeams, and WaterFix regression suites passed against the supplied
frontend. Full launch timing and execution on Windows XP remain manual checks.
