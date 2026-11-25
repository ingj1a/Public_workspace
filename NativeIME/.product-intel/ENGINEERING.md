# Engineering Contract

## Target platform

Windows 10/11 x64 first. Windows ARM64 and older Windows versions are not in the initial acceptance matrix.

## Architecture

1. **TSF adapter (native C++ DLL):** registration, activation/deactivation, key-event sink, text composition, candidate-window anchoring, secure/disabled-context handling.
2. **Input engine process/library:** Pinyin parsing, dictionary lookup, correction, candidate ranking, user lexicon, and bounded caching.
3. **Local IPC:** versioned, bounded messages; explicit timeout and disconnect behavior. The TSF callback must not block on expensive model work.
4. **Candidate UI:** native Windows rendering, separated from core engine lifecycle.
5. **Settings manager:** separate native desktop process; configuration changes are validated and atomically persisted.
6. **App-context service:** foreground process identity plus TSF context/input-scope signals when available. No global low-level keyboard hook in the MVP.

## Technology decisions

- TSF is mandatory for native text-service integration.
- C++17 for the core and TSF adapter.
- Rime/librime is the preferred engine candidate, but integration is not yet approved. Before adopting it, pin a release/commit, inspect ABI/build requirements, dictionary provenance, transitive licenses, runtime footprint, and candidate update latency.
- The initial portable core is intentionally dependency-free to establish interfaces and tests before an engine adapter is selected.
- WPF may be used for the settings manager, never as a replacement for the TSF service.
- CMake 3.25+ for reproducible builds.

## Reliability rules

- Fail open: on engine timeout/disconnect, do not eat keys not owned by an active composition.
- No blocking disk/network/model operations in TSF key callbacks.
- Composition updates and commits must be serialized through TSF edit sessions.
- Every COM sink and event subscription must be released on Deactivate.
- No background polling loop; subscribe to relevant focus/context events.
- Configuration writes are atomic and versioned.
- Diagnostics redact composition text by default.

## Phase 0 implementation boundary

Portable core contract, deterministic tests, Windows build workflow, and a TSF adapter design contract. No claim of a registered or working IME until a Windows build, registration, and real text insertion test are recorded.

## Traceability

- REQ-001/002/004/005/006 → TSF adapter + input engine → TSF registration, composition, key-routing and fail-open tests.
- REQ-003/101/102 → Pinyin engine → deterministic parsing/correction/ranking tests.
- REQ-103/104 → dictionary subsystem → import, provenance, lookup and rollback tests.
- REQ-105/106/107 → app-context subsystem → app/focus/override matrix.
- REQ-201 → isolated voice process → on-demand memory and offline tests.
