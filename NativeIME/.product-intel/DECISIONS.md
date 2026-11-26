# Decision Log

## DEC-001 — Native Windows TSF, not a wrapper

- Status: Accepted
- Reason: Input methods must participate in Windows text composition and keyboard routing. A web shell cannot replace TSF.
- Consequence: Windows SDK and native C++ integration are required.

## DEC-002 — Fail-open input path

- Status: Accepted
- Reason: An IME failure must not make the host application unusable.
- Consequence: TSF callbacks stay short; disconnected/unhandled keys are passed to the application.

## DEC-003 — Engine adapter before engine lock-in

- Status: Accepted
- Reason: Rime is a strong candidate, but its exact build/runtime and dictionary licensing must be audited.
- Consequence: The initial Pinyin core interface is dependency-free; Rime integration remains a gated decision.

## DEC-004 — Quiet Motion visual direction

- Status: Accepted
- Reason: The reference animation's restrained floating surfaces and stateful motion fit a low-distraction utility, but the input surface must prioritize speed.
- Consequence: Candidate UI stays compact; richer motion is reserved for settings and non-critical transitions.

## DEC-005 — GPL-3.0 as a possible license, not yet selected

- Status: Open
- Reason: Weasel is GPL-3.0, librime is BSD-3-Clause, and dictionary datasets may carry separate terms.
- Consequence: Do not copy or redistribute upstream source/data until the project license and notices are settled.

## DEC-006 — Isolated native input lab before system-wide key interception

- Status: Accepted
- Reason: Validate bounded session state and candidate actions without modifying Windows registration or capturing keys outside the lab.
- Consequence: The existing TSF probe remains pass-through; the lab is a native Win32 EXE, not an installer. COM smoke tests exercise exports and direct object lifetime, not interactive activation. Unknown Pinyin is preserved as raw text on commit; switching language cancels an uncommitted preview. The five-entry fixture is not a real dictionary. GUI review and full TSF edit sessions remain release blockers.

## DEC-007 — Beta transition: close the actual lifecycle and desktop evidence gap
- Mode: HARDEN / FOCUSED, PDI 2.0.0-beta.1 (manual protocols; scripts NOT_CHECKED). Existing repository has prior formal PDI history, not a clean A/B comparison.
- Baseline: aca9e2b, Windows registration and prior integration passed. No live desktop activation evidence.
- Alternatives: direct per-host spawning (rejected for duplicate process and input callback risk), explicit per-session supervisor with bounded restart (selected), Windows service (rejected for privilege and installation expansion).
- Falsifier: verified child crash does not produce a new engine returning real candidates, or shutdown leaves a child alive. Test uses only descendants of its own Job object.
- Desktop test artifact contains first-party binaries only; no automatic registration/default change or third-party distribution. No visual rating until actual screenshot. Windows release only; Linux auxiliary validation preserved.

## DEC-008 — Active-profile TSF verification without global key injection
- PDI Beta HARDEN/FOCUSED, scripts SCRIPT_NOT_RUN. Baseline d902751 verified in Windows CI; keep default pass-through and independent engine.
- Compared ordinary EDIT insertion (already verified, insufficient) with actual process-scoped profile activation + TSF keystroke manager (selected) and global SendInput automation (rejected to avoid sending keys to another application).
- Falsifier: missing real TSF context, no accepted first key, absent actual candidate, text/readback mismatch, duplicate commits or stale popup; no result may be inferred from compilation. Actual physical keyboard, custom editors, screenshots and resource metrics remain separate.

## DEC-009 — One user-facing product, built-in diagnostics
User explicitly rejected a standalone diagnostics executable. NativeIME.exe owns General and Diagnostics pages in one process, retains capture across navigation, and manages an explicitly started internal engine session. Remove standalone diagnostics target and staged artifact. Preserve out-of-process Rime/supervisor and lab-only verifier for fault containment. No forced registration/default, no release. Main UI smoke is not visual or desktop TSF acceptance.

## DEC-010 — Diagnostics and input features progress together
Continue diagnostics as an embedded page in NativeIME.exe while advancing the TSF product. Experimental manual language toggle is Ctrl+Shift+F12, restricted to writable supported edit controls and idle composition. Diagnostics report includes bounded process CPU/working-set sampling and ZIP32 bundle; CPU scope is the NativeIME UI process only. No user input content captured. Both changes require Windows CI and real desktop acceptance before enabling by default.

## DEC-011 — Parallel product vocabulary and owned-job diagnostics
Small original exact-spelling professional vocabulary is added after real Rime lookup; preserve the first two existing candidates and never auto-commit or infer app domain. No external dictionary licensing expansion. For resources use only the main app's own engine Job, not arbitrary process scans; peak commit is explicitly distinct from current working set. Both are experimental and require real Windows acceptance; no default profile change.

## DEC-012 — Bounded typo suggestions and fixed quality diagnostics
Choose a reviewed whitelist rather than exhaustive edit-distance search to bound extra lookup cost and false matches before desktop validation. At most one additional exact engine lookup; correction is visibly tagged and never displaces existing first two candidates or raw default. Fixed synthetic cases run through real Rime/IPC and embedded diagnostics; no user text or real-user hit-rate claim. Broader language modeling, correction coverage and latency remain unverified.
