# NativeIME Alpha 0.1 lab candidate — not a production installer

Use an isolated Windows 10/11 x64 VM with a snapshot. The first-party files are unsigned; obtain them from the project's authenticated GitHub Actions artifact and verify provenance. Hashes in the manifest provide consistency, not code signing. No third-party redistributables are included. Preparation downloads official pinned upstream archives once, verifies SHA256, and retains upstream notices. Ordinary typing is offline. Project/third-party redistribution review is still required for a release.

1. Extract the whole artifact to a fresh directory in the VM. Open **64-bit PowerShell without administrator rights** in that directory. Run:
   .\Prepare-Alpha.ps1 -IsolatedVm
2. Review the prepared runtime and source notices. In **elevated 64-bit PowerShell**, from the same directory:
   .\Install-Alpha.ps1 -Action Install -IsolatedVm
   The script refuses any existing NativeIME profile/folder and registers only from Program Files. Do not set NativeIME as default or use on a daily-use computer.
3. Open the installed NativeIME.exe in Program Files\NativeIME-AlphaLab **as the ordinary user**. Start Engine. Enable/select NativeIME manually in Windows language settings if available; Simplified Chinese input language is required. Try the built-in native edit box. If blocked, use Diagnostics and Testing > Self-check and export the ZIP. The isolated desktop verifier tests the real TSF route, not physical key delivery.
4. Start capture, test normal text / corrections / cancel / focus / shortcuts, stop capture, export ZIP. Inspect before sharing. Do not submit personal input or screenshots containing private text.
5. Switch to another input method, close NativeIME (stop engine), then in elevated PowerShell from the original extracted directory:
   .\Install-Alpha.ps1 -Action Remove -IsolatedVm
   This unregisters only the owned Alpha DLL. It intentionally retains the protected files and user data. After sign-out/restart, manually remove the exact protected NativeIME-AlphaLab folder if desired. Restore VM snapshot if Windows does not return to expected state. No registry-wide deletion or automatic default profile change.

If a script is blocked by execution policy, do not change machine-wide security policy. Review script contents and use a trusted, organization-approved way to run it. No need to disable antivirus, SmartScreen or other security protection.

Current gates: first-party build and core/IPC tests, upstream prepare engine check, disposable registration/rollback CI; actual interactive TSF, visual and performance acceptance remain separate.

NativeIME.exe now has a built-in Input Settings / Deployment page. In the original extracted trusted package it can start the existing preparation/install/remove scripts (explicit VM confirmation and UAC where needed); it is not yet a self-contained native installer. No policy bypass. The protected installed copy does not contain deployment scripts: use the original extracted package for rollback. The input shortcut and optional Home/End candidate navigation are saved per user, effective at next TSF activation. Home/End remain host keys by default. No user typing is captured.
