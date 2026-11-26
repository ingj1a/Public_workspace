# NativeIME

Native Windows Chinese Pinyin input method project. Windows 10/11 x64 is the sole product/release platform; Linux CI is auxiliary portable-core validation only. No Linux, WSL, browser shell or network dependency for ordinary typing.

> Phase 0, pre-alpha. Default TSF build is pass-through; the experimental TSF build connects to a real, separate librime service. Registration and cross-process recovery passed automated tests. Actual active-profile typing remains a release blocker.

## Build and verify

Requires CMake 3.25+, C++17 and the Windows SDK.
```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```
The existing GitHub Actions workflow prepares pinned Windows librime runtime/schema data and builds the independent engine and experimental TSF. InputLab is a seed-only development harness. DesktopLab is an ordinary native EDIT host. TsfDesktopE2E is an opt-in actual TSF keyboard-route verifier, not a simulated IME. NativeIME.exe is the native product shell with General and built-in Diagnostics and Testing pages; no separate diagnostics executable. NativeIME.Session is a private experimental engine lifecycle helper, also usable by CI; startup regression passed. See [diagnostics](tests/DIAGNOSTICS.md). See [desktop acceptance](tests/DESKTOP-ACCEPTANCE.md).

| Capability | Evidence/status |
|---|---|
| Windows build, core, COM registration/removal | PASS in CI |
| Windows real librime, IPC, supervisor recovery and readiness | PASS in CI |
| Experimental TSF composition and popup | Implemented; active-profile desktop verification pending |
| Professional-app context, Chinese/English switching | Base rules only |
| Intelligent correction and professional vocabulary | Fixed-corpus integration passed; general correction and learning pending |
| Sentence preview | Experimental engine commit preview; not post-commit prediction |
| Quiet Motion UI, performance, installer and signing | Pending |

Latest confirmed baseline: [9d4f2a2 Windows run](https://github.com/ingj1a/Public_workspace/actions/runs/45773729655/job/139752258202). This is not a release, and CI does not prove interactive TSF input. No production install or default profile change is authorized.

See [PRODUCT](PRODUCT.md), [ROADMAP](ROADMAP.md), [engineering](.product-intel/ENGINEERING.md), [interaction](.product-intel/INTERACTION.md), [design](DESIGN.md).

## License

Project license not finalized; the vendored librime API header has its own BSD-3-Clause notice. Runtime and data redistribution review remains a release gate.

Alpha lab preparation uses official hash-pinned sources and explicit disposable-VM registration/rollback; see [ALPHA-LAB](tests/ALPHA-LAB.md). New deployment CI pending. No bundled third-party runtime or default profile change.
