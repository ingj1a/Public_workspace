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
The existing GitHub Actions workflow prepares pinned Windows librime runtime/schema data and builds the independent engine and experimental TSF. InputLab is a seed-only development harness. DesktopLab is an ordinary native EDIT host. TsfDesktopE2E is an opt-in actual TSF keyboard-route verifier, not a simulated IME. See [desktop acceptance](tests/DESKTOP-ACCEPTANCE.md).

| Capability | Evidence/status |
|---|---|
| Windows build, core, COM registration/removal | PASS in CI |
| Windows real librime, IPC, supervisor recovery and readiness | PASS in CI |
| Experimental TSF composition and popup | Implemented; active-profile desktop verification pending |
| Professional-app context, Chinese/English switching | Base rules only |
| Intelligent correction, sentence ranking, learning, professional dictionaries | Pending |
| Quiet Motion UI, performance, installer and signing | Pending |

Latest confirmed baseline: [d902751 Windows run](https://github.com/ingj1a/Public_workspace/actions/runs/45747247257/job/139666176684). This is not a release, and CI does not prove interactive TSF input. No production install or default profile change is authorized.

See [PRODUCT](PRODUCT.md), [ROADMAP](ROADMAP.md), [engineering](.product-intel/ENGINEERING.md), [interaction](.product-intel/INTERACTION.md), [design](DESIGN.md).

## License

Project license not finalized; the vendored librime API header has its own BSD-3-Clause notice. Runtime and data redistribution review remains a release gate.
