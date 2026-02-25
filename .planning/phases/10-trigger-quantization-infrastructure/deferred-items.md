# Phase 10 — Deferred Items

## Pre-existing test failures (out of scope for 10-01)

Discovered during 10-01 execution: 5 test cases (TC 4, 5, 6, 10, 11) in
`Tests/LooperEngineTests.cpp` fail with `hasContent() == false`. These failures
existed before any changes from plan 10-01 (verified by stash + rerun). They are
NOT caused by snapToGrid implementation.

Root cause hypothesis: `recordGate()` may have a guard that requires `recGates_`
to be set to true at construction time OR `setRecGates(true)` is not persisting —
needs investigation in a future plan.

**Affected test cases:**
- TC 4 — record gate and play back (line 80)
- TC 5 — reset clears content (line 106)
- TC 6 — deleteLoop clears content (line 127)
- TC 10 — punch-in preserves events at untouched beat positions (line 244)
- TC 11 — loop wrap boundary fires event exactly once per cycle (line 310)

**Status:** Deferred. Do not fix in plan 10-01 scope (out of scope per deviation rule).
