# Returning to the frontend after a match

The supported frontend saves its bounds and minimizes at RVA `0x25B80`,
then restores at `0x25BA6`. Its local ShellExecute engine-exit event wait,
IPX CRT process wait and four DirectPlay engine-exit event waits use `WaitForSingleObject(INFINITE)` on
the UI thread. Attempting to activate the frontend while it waits can
leave Windows activation messages unanswered. In a local Quick Play
reproduction, activation timed out during the match and the frontend
remained minimized afterward; manually activating it recovered the menu.
This reproduction did not establish a crash.

`FrontendFocus.cpp` tracks the native minimize/restore interval and changes
only these six infinite waits on that launch thread. It uses
`MsgWaitForMultipleObjects` with `QS_SENDMESSAGE` and `PeekMessage(PM_NOREMOVE)`
to service synchronous messages. It leaves posted commands, timers, input
and quit messages queued for the normal MFC loop, avoiding a second game
launch or other posted UI actions inside the native launch stack. Finite
waits, other callers and other threads keep the original wait function.
The wait still returns the native handle result; auto-reset events are
not consumed by a preliminary wait.

The native restore routine still restores the saved bounds. After a paired
game launch, the hook also restores a still-iconic window and requests
foreground activation. Windows retains control over foreground permissions.

Installation validates the supported executable timestamp, lifecycle
signatures and all six wait-call operands before creating hooks. It queues
the two lifecycle hooks and Windows wait detour as one batch and removes its
hooks on installation failure. The frontend executable on disk is unchanged.
All APIs are available on Windows XP; the Release toolset remains `v141_xp`.

## Verification

```powershell
MSBuild.exe tests/FrontendFocusTests.vcxproj /p:Configuration=Release /p:Platform=Win32
./Release/FrontendFocusTests.exe 'D:/Games/Worms 2/frontend.exe'
```

Run the message-queue fixture on the Windows desktop outside a sandbox that
filters thread messages. A simulated engine sends a synchronous message
before signaling exit. The test checks replies on all six wait paths over
two rounds, manual-reset and auto-reset exit events, queued commands and
`WM_QUIT` preservation, unrelated/finite waits, failure results, native
restore callbacks, signature rejection and MinHook cleanup against a private
mapping of the real frontend. Each actual native wait instruction is also
executed through the installed API detour to verify caller-address gating.
`StartupTests` also installs the focus hooks
alongside all feature groups.

On 2026-10-12 the XP-toolset DLL build, focused regression and combined
startup installation test passed. A live Quick Play check on modern Windows
also passed: frontend activation during play returned immediately, returning
to the engine still worked, and closing the engine brought the frontend
back into view with working controls. Network matches, natural match
completion and execution on Windows XP remain manual checks.

Manual checks: start a match, Alt+Tab to the frontend, return to the engine,
and end the match. Confirm that the frontend returns and accepts input;
repeat for subsequent rounds, Quick Play, normal local games, TCP/IP and IPX,
on both modern Windows and Windows XP. Also test normal completion without
Alt+Tab and a launch failure.
