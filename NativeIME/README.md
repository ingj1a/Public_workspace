# NativeIME

**NativeIME** is an open-source, native Windows Chinese Pinyin input method project focused on reliable typing, context-aware language switching, professional vocabulary, and a restrained desktop UI.

> Status: Phase 0 — repository and core-engine foundation. This is not yet an installable IME. The TSF text service, composition lifecycle, candidate UI, and Windows installation flow are still pending.

## Product principles

- Native Windows Text Services Framework (TSF); no Electron/WebView shell.
- Offline-first core typing; no network dependency for ordinary input.
- Predictable keyboard behavior: never swallow shortcuts unless the IME explicitly owns the active composition.
- Small idle footprint; optional heavyweight features load only when requested.
- Transparent, user-editable app rules and vocabulary.
- Privacy by default: no raw keystroke telemetry.

## Current scope

The first milestone establishes a portable Pinyin core contract and test harness. Windows TSF integration follows only after the core behavior is testable.

See:
- [Product requirements](PRODUCT.md)
- [Engineering decisions](.product-intel/ENGINEERING.md)
- [Interaction contract](.product-intel/INTERACTION.md)
- [Visual direction](DESIGN.md)
- [Roadmap](ROADMAP.md)

## Build the core

Requires CMake 3.25+ and a C++17 compiler.

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

The portable core tests can also run on Linux/macOS. The Windows TSF adapter will require Windows 10/11 and the Windows SDK.

## Project status

| Capability | Status |
|---|---|
| Product/architecture contracts | In progress |
| Portable Pinyin core contract | In progress |
| TSF text service | Not started |
| Composition and candidate UI | Not started |
| Context-aware app rules | Not started |
| Professional dictionaries | Not started |
| Installer/updater | Not started |
| Windows compatibility/performance tests | Not run |

## License

The project license is not finalized. No third-party code or dictionary is bundled in this initial scaffold. A license will be selected after the dependency and data-license audit.
