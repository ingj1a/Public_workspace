# Roadmap

## Phase 0 — foundation (current)

- [x] Product scope and P0/P1/P2 requirements.
- [x] Architecture and interaction contracts.
- [x] Initial visual direction.
- [ ] Portable Pinyin core implementation and tests.
- [ ] Windows TSF adapter implementation.
- [ ] Windows build, registration and real text insertion.
- [ ] Baseline performance measurement.

Exit gate: a real Windows test machine can register the TSF service, type Pinyin into supported controls, commit/cancel text correctly, and preserve host shortcuts.

## Phase 1 — daily-use MVP

- [ ] Full Pinyin dictionary and candidate ranking.
- [ ] Candidate panel with caret anchoring.
- [ ] Chinese/English mode and shortcuts.
- [ ] Installer/uninstaller and configuration backup.
- [ ] App compatibility matrix.

## Phase 2 — intelligence

- [ ] Conservative typo correction.
- [ ] Sentence-level ranking and prediction.
- [ ] User dictionary and learning.
- [ ] Professional vocabulary packs with license/provenance.
- [ ] Per-app rules and focus-aware switching.

## Phase 3 — polish and release

- [ ] Quiet Motion candidate UI and settings manager.
- [ ] Dark/high-contrast/reduced-motion/high-DPI support.
- [ ] CPU/memory/latency profiling.
- [ ] Long-run stability, upgrade/uninstall and security review.
- [ ] Signed release artifacts and reproducible source build.
