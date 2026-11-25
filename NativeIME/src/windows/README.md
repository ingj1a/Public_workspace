# Windows TSF Adapter — implementation contract

This folder will contain the native COM text service. It is intentionally not implemented in Phase 0 yet.

## Required interfaces and lifecycle

- Implement `ITfTextInputProcessorEx` (or `ITfTextInputProcessor` where required) and `ITfKeyEventSink`.
- Register/unregister the text service and language profile through documented TSF APIs.
- Advise key-event sinks only after activation; unadvise every sink on deactivation.
- Handle keystrokes in `OnTestKeyDown` and `OnKeyDown` without processing the same physical key twice.
- Use TSF edit sessions for all composition-range mutations and commits.
- Never call network/model/disk operations synchronously from TSF key callbacks.
- Check secure/disabled/empty contexts and pass keys through when the service does not own the input.
- Keep candidate UI non-activating and anchored to the current caret/layout.

## Required Windows tests

1. Registration and removal on clean Windows 10/11 x64.
2. Activation/deactivation repeated without leaked COM references or stale sinks.
3. Compose, select, commit, backspace, cancel, focus loss and candidate paging.
4. Ctrl/Alt/Win shortcuts pass through; host app receives keys exactly once.
5. Password/secure fields and TSF-disabled contexts do not expose composition.
6. Engine process unavailable: unhandled keys pass through and reconnect is asynchronous.

## References

- Microsoft Windows classic samples: https://github.com/microsoft/Windows-classic-samples/tree/main/Samples/IME
- Rime Weasel TSF implementation: https://github.com/rime/weasel/tree/master/WeaselTSF

Do not copy source files from these projects until their license and required notices have been reviewed and added.
