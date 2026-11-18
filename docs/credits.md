# Credits text selection

The native credits initializer at RVA `0x71BA` creates Rich Edit control 1000
and loads `Data\About.rtf`. The dialog manager can select its entire contents
when assigning keyboard focus after `OnInitDialog`.

`Credits.cpp` hooks the initializer, calls the original, then subclasses only
that credits control before dialog initialization finishes. It clears the
initial selection and removes `DLGC_HASSETSEL` from `WM_GETDLGCODE` responses.
The dialog manager consequently preserves the selection when assigning focus.
Manual selections, copying, read-only behavior and navigation remain available.
The subclass removes itself on destruction, so reopening credits creates a
fresh instance. Other controls retain their normal behavior.

Installation checks the supported frontend timestamp and credits code
signatures before creating the hook. The executable on disk is unchanged.
The Release build continues to use `v141_xp` and XP-compatible APIs.

## Verification

```powershell
MSBuild.exe tests/CreditsTests.vcxproj /p:Configuration=Release /p:Platform=Win32
./Release/CreditsTests.exe 'D:/Games/Worms 2/frontend.exe'
MSBuild.exe tests/StartupTests.vcxproj /p:Configuration=Release /p:Platform=Win32
./Release/StartupTests.exe 'D:/Games/Worms 2/frontend.exe'
```

The focused test uses real dialogs with legacy and newer Rich Edit controls.
It checks initial focus, repeated focus changes, intentional selections,
read-only behavior, unrelated edit fields and reopening. A private mapping
of the frontend checks signature rejection, hook installation and cleanup.
The combined startup test installs the credits hook with all other features.

On 2026-11-18 both tests and the Win32 Release DLL build passed on modern
Windows. Live game behavior and execution on Windows XP remain manual checks
in `tests/ManualTests.md`.
