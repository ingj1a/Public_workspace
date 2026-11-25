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
