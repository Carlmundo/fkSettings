# Integrated water colour preview fix

`fkSettings.dll` now includes the behaviour from `fkWaterFix` without requiring
a second DLL. It fixes the Terrain editor's water colour preview when `W2PATH`
under `HKEY_CURRENT_USER\Software\Team17SoftwareLTD\Worms2` is `.`.

The original fix captures the frontend's working directory when the DLL loads
and restores it after loading the colour text file. `WaterFix.cpp` keeps that
behaviour and the original two executable signatures, using
`GetCurrentDirectoryA` and `SetCurrentDirectoryA` instead of `std::filesystem`.
Both APIs support Windows XP. It does not change the registry.

The integration uses fkSettings' existing executable-section pattern scanner.
The scanner accepts an optional module so tests can inspect a private frontend
mapping; existing callers still scan the running executable. Both water
signatures and the intercepted jump's destination are checked before any code
is changed. The fix replaces exactly the original five-byte jump, preserves
all general-purpose registers and flags, flushes the instruction cache, and
stays installed for the frontend's process lifetime. Unsupported executables
are skipped and reported through `OutputDebugStringA`.

## Build and installation

fkSettings' existing project configurations, MFC linkage, runtime dependencies
and Release `v141_xp` toolset are unchanged. Build **Release / Win32** for Worms 2
and Windows XP. Debug retains its existing modern toolset.

From a Visual Studio developer PowerShell prompt, build into this repository
without overwriting the installed game DLL:

```powershell
MSBuild.exe fkSettings/fkSettings.vcxproj /t:Build /p:Configuration=Release /p:Platform=Win32 /p:OutDir="$PWD/Release/"
MSBuild.exe tests/WaterFixTests.vcxproj /t:Build /p:Configuration=Release /p:Platform=Win32
./Release/WaterFixTests.exe 'D:/Games/Worms 2/frontend.exe'
```

Install `Release/fkSettings.dll` using the game's existing frontend DLL loader,
and remove or disable the separate `fkWaterFix.dll`. Keep the runtime setup that
already works with fkSettings on XP. If the old water fix is loaded first, the
merged fix's original signature will no longer match and it will be skipped.

## Verification

The water regression test maps the supplied frontend without executing its
entry point or imports. Changes affect only that private mapping. It installs
the production hook, executes the jump with three simulated terrain-directory
changes, and verifies the captured directory, all seven general-purpose
registers and CPU flags. It also checks repeated installation, persistent
installation, and rejection when either signature changes before installation.
The test builds with `v141_xp`; its runtime is static and uses UCRT 10.0.10240.0
so the test itself can also be run on XP.

The merged Release DLL and all four regression suites passed on the development
machine against `D:/Games/Worms 2/frontend.exe`. The DLL retains PE subsystem
version 5.01. Its direct imports include the two XP directory APIs and no
`std::filesystem` implementation. Actual terrain previews and execution on an
XP machine remain manual checks in `tests/ManualTests.md`.
