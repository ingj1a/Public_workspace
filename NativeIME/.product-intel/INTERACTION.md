# Interaction Contract

## FLOW-001 — Pinyin composition

Idle → composing → candidates visible → candidate selected → committed → idle.

- Letter keys update the composition only while the IME is enabled and the active context accepts text.
- Space/Enter commits the selected candidate according to configured behavior.
- 1–9 selects a visible candidate only while the candidate list owns the composition.
- Up/Down moves selection; PageUp/PageDown changes pages.
- Backspace removes one composition unit; Escape cancels composition.
- Unhandled shortcuts and keys pass through to the host application.
- Focus loss/deactivation terminates or safely finalizes composition according to TSF lifecycle rules; never duplicate text.

## FLOW-002 — language mode

Chinese → English → temporary English → Chinese.

- User hotkey has priority over app-profile suggestion.
- Per-app policy can be Auto, Chinese, English, or Disabled.
- Explicit user lock persists until the user changes it or the configured lock scope ends.
- Unknown context defaults to the user's last explicit mode; no aggressive auto-switch.

## FLOW-003 — app context

Foreground app detected → policy lookup → context confidence evaluation → apply suggestion or keep current mode.

- Process identity alone is insufficient to claim text-field awareness.
- Use TSF context/input scope when exposed; add per-app rules and manual overrides.
- Unsupported/custom controls fall back to conservative mode.
- Never infer that every keystroke in a graphics app is a command or that every text box is ordinary prose.

## State matrix

| State | Feedback | Recovery |
|---|---|---|
| Disabled/secure context | no candidate UI; pass through | resume only when TSF context permits |
| Idle | no floating panel | begin composition |
| Composing | compact preedit + candidates | backspace/cancel |
| Engine unavailable | no false candidate state | pass through; reconnect asynchronously |
| App rule uncertain | retain explicit user mode | manual switch/lock |
| Settings invalid | show validation error | keep last known-good config |
| Restart during composition | cancel transient composition safely | return to idle |

## Accessibility and motion

- Full keyboard navigation; no hover-only functionality.
- High contrast, DPI scaling, screen-reader labels for settings.
- Candidate panel must not steal keyboard focus.
- Reduced motion disables nonessential transitions.
- Animation never gates key processing or commit.
