# NativeIME — Visual Direction

## Chosen direction: Quiet Motion

A quiet, precision-oriented Windows utility. The GIF reference informs the use of floating surfaces, restrained motion, soft depth, and concise status capsules; its healthcare imagery/cards are not copied.

### Candidate UI

- A compact, horizontally oriented native floating panel anchored to the caret.
- Candidate text is the primary visual object; index and annotation are secondary.
- Neutral surface with high-contrast text. No large hero area, decorative chart, or persistent status dashboard.
- Update only when composition/candidate state changes. No animation on every key repeat.
- Use DirectWrite for text and a native rendering path appropriate to Windows; measure rendering cost before adding blur or acrylic.

### Control center

- Native Windows desktop application; use WPF for settings only if it remains separate from the TSF input path.
- Left navigation: Overview, Input, App Rules, Dictionaries, Shortcuts, Appearance, Privacy, Diagnostics.
- Main content uses clear sections and compact setting rows. Cards are reserved for distinct tasks, not every setting.
- Animated state changes are short, causal, and optional. Respect Windows animation/reduced-motion preferences.

### Tokens (initial proposal)

| Token | Value |
|---|---|
| Window background | #F4F5F6 |
| Surface | #FFFFFF |
| Primary text | #20242A |
| Secondary text | #737B86 |
| Accent | #4778D5 |
| Border | #E6E9ED |
| Radius | 12–16 px for settings; candidate panel tuned for compactness |

### Typography

- System UI font for controls.
- Candidate Chinese glyphs use a user-configurable CJK font fallback.
- Candidate text 16–18 px at 100% scale as a starting point; index and annotation smaller.
- Preserve word groups and Latin identifiers; never split technical tokens arbitrarily.

## Alternative considered: Studio Console

A denser, developer-tool visual language with a dark canvas, monospaced metadata, and explicit diagnostic readouts. It is useful for an advanced diagnostics screen but is too technical as the default experience for a daily Chinese IME.

## Anti-reference

- No always-on animated gradients or glows.
- No excessive glass blur in the per-keystroke path.
- No oversized cards in the candidate window.
- No motion that delays candidate updates or text commit.
- No hover-only controls; keyboard access is mandatory.
- No fake “AI thinking” indicator for deterministic local lookup.

## Verification required

No actual UI has been rendered yet. Visual quality is NOT_RATED until real Windows screenshots exist for: normal composition, dense candidate list, English mode, app-rule notification, high-DPI scaling, dark mode, and reduced-motion setting.
