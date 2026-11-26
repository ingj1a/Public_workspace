# Windows TSF Adapter — Phase 0 probe

The current DLL is a **registration/activation probe only**. It registers a native TSF text service and subscribes to keyboard events, but deliberately returns `eaten = FALSE` for every key. It does not compose Pinyin, display candidates, or insert text yet.

## Build (Windows, Visual Studio 2022)

```powershell
cmake -S NativeIME -B NativeIME/build -A x64
cmake --build NativeIME/build --config Release
```

## Register (elevated PowerShell)

Registering a text service modifies Windows COM/TSF registration. Only do this on a disposable test account or VM during Phase 0.

```powershell
regsvr32 .\NativeIME\build\Release\NativeIME.TSF.dll
```

Unregister:

```powershell
regsvr32 /u .\NativeIME\build\Release\NativeIME.TSF.dll
```

After registration, inspect Windows Settings → Time & language → Language & region → Chinese (Simplified) → Language options → Keyboards. Verify activation and confirm ordinary key input still reaches the focused application.

Do not use on a production machine until registration, rollback, secure-field, and host-shortcut tests pass. This probe has not yet been compiled with MSVC or tested in Windows.

## Experimental inline composition (not for normal installation)

Configure with `-DNATIVEIME_EXPERIMENTAL_TSF=ON` to compile the experimental code path. The default DLL is still a pass-through probe. The experiment uses synchronous TSF edit sessions, limits input to standard non-password Win32 Edit controls, shows raw Pinyin inline and commits the top seed candidate on Space or Enter. Backspace and Escape edit or cancel the preedit. No candidate popup, candidate selection, application awareness, or production dictionary yet. No system registration is performed by CI. Do not deploy this build to a daily-use machine. Test focus changes, external composition termination, existing selection, readonly/password controls, Caps Lock, shortcuts, lock failure and repeated activation on a disposable Windows environment before enabling it by default.
