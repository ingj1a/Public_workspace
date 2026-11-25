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
