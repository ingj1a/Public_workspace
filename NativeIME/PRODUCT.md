# NativeIME — Product Requirements

## Product definition

A native Windows Pinyin input method for users who frequently move between Chinese prose, English terminology, and professional desktop applications. The product should feel unobtrusive during typing and provide a polished, focused control center when configuration is needed.

## Jobs to be done

1. Enter accurate Chinese text quickly without correcting avoidable Pinyin mistakes.
2. Type complete phrases and sentences with fewer candidate-selection actions.
3. Preserve English commands, identifiers, and shortcuts inside professional software.
4. Retrieve domain-specific vocabulary without manually switching to a separate dictionary.
5. Tune and inspect the behavior instead of surrendering control to opaque automation.

## Target users

- General Chinese users who want a reliable daily IME.
- Designers and creators using Photoshop, Illustrator, Blender, Figma, and video tools.
- Developers using IDEs, terminals, and code editors.
- Technical writers and users with domain-specific vocabulary.

These segments are product hypotheses, not validated market research.

## Scope and priorities

### P0 — daily-input foundation

- REQ-001: Native Windows TSF integration; no browser or Electron shell.
- REQ-002: Pinyin composition, candidate selection, commit, cancel, backspace, and paging.
- REQ-003: Common full-Pinyin input and a replaceable dictionary provider.
- REQ-004: English passthrough and explicit Chinese/English mode control.
- REQ-005: Keyboard shortcuts and application shortcuts must not be intercepted unexpectedly.
- REQ-006: Engine failure must fail open: unhandled keys continue to the target application.
- REQ-007: Offline typing with no network requirement.
- REQ-008: Install, uninstall, and recover without corrupting Windows input settings.

### P1 — differentiated intelligence

- REQ-101: Spelling correction with conservative ranking; do not silently rewrite committed text.
- REQ-102: Sentence-level candidate ranking and contextual prediction.
- REQ-103: User dictionary, custom phrases, import/export, and frequency learning.
- REQ-104: Professional vocabulary packs with source/license metadata.
- REQ-105: App profiles for language mode and vocabulary weighting.
- REQ-106: Distinguish text-entry focus from shortcut/tool contexts where the host exposes reliable signals.
- REQ-107: User override, temporary mode switch, and per-app disable/lock controls.

### P2 — optional extensions

- REQ-201: Local voice dictation, loaded on demand in a separate process.
- REQ-202: Additional schemas such as double Pinyin.
- REQ-203: Optional prediction model packs and advanced personalization.

## Non-goals for MVP

- Cloud LLM inference on every keystroke.
- Uploading typed content, screenshots, or application documents.
- Global low-level keyboard hooks as a replacement for TSF.
- Claiming pixel-perfect or semantic understanding of every third-party app.
- Bundling dictionaries/models before their provenance and license are audited.
- A large animated candidate window that delays text entry.

## Acceptance examples

- Positive: typing a valid Pinyin sequence produces candidates; selecting a candidate commits exactly once.
- Positive: pressing Ctrl+S in an editor reaches the editor unchanged.
- Positive: when the IME is disabled or inactive, printable keys reach the application normally.
- Negative: a failed engine connection must not freeze or consume the user's keystrokes.
- Negative: automatic app detection must not override an explicit user lock.
- Negative: a correction suggestion must not silently mutate already committed text.

## Success metrics (targets, not measured results)

- P95 key-to-candidate update target: <= 30 ms on a documented reference PC after warm-up.
- Idle CPU target: < 0.1% average over a 60-second idle sample.
- Core resident memory target: <= 100 MB with the baseline dictionary; revisit after profiling.
- Zero lost or duplicated committed text in the P0 regression suite.
- 100% pass rate on the agreed P0 shortcut/pass-through matrix before release.

## Privacy

- No raw keystroke telemetry.
- No network access in the core input path.
- User dictionary and learning data stored locally.
- Secure/password fields and disabled TSF contexts must be respected.
- Logs must redact composition text by default.
