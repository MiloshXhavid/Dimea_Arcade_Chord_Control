# ChordJoystick — Development Retrospectives

---

## v1.7 Space Joystick (2026-03-04 → 2026-03-05)

**Theme:** Space-themed visual overhaul + joystick physics + gamepad routing + bug sweep

### What shipped

**Phase 31 — Paint-Only Visual Foundation**
- Deep-space background with milky way particle band (3-layer Gaussian, baked in `resized()` for zero per-frame allocation)
- Density-driven starfield: 150 stars, magnitude-scaled radius + alpha
- Semitone grid on joystick pad: in-scale notes bright, out-of-scale dimmed
- BPM-synced glow ring around the cursor: full cycle = one beat at any tempo; `resetGlowPhase()` prevents drift

**Phase 32 — Spring-Damper Inertia and Angle Indicator**
- Spring-damper cursor physics: inertia while joystick is moving, smooth spring-back on release
- kSpring=0.18, kDamping=0.90 (near-critical) — settles ~150ms, zero overshoot
- Perimeter arc indicator on joystick pad border
- Note-label compass at cardinal positions (N/S/E/W)

**Gamepad SWAP/INV routing (built across Phases 31-32)**
- SWAP: left stick ↔ right stick routing — pitch vs filter CC fully isolated across all 4 dispatch sites (chord params, filter CC lambdas, timerCallback cursor, LFO dispatch block)
- INV: X↔Y axis swap (90° rotation) applied at same 4 sites
- SWAP/INV buttons in GamepadDisplayComponent; L3/R3 labels flip dynamically; cyan/amber lit colors
- 6-bug batch: looper reset syncs random phases, population deterministic shift, left stick deadzone smooth rescaling, CC axis expansion (CC74/71/12/76 on both axes), arp step skip fix, gate length for joystick triggers

**LFO waveform oscilloscope**
- SYNC row split: left = SYNC button, right = oscilloscope
- 48-sample ring buffer per LFO pushed each timerCallback
- Fixed ±1.0 scale — level=0 → flat center, level=1 → full height; blink alpha tracks positive excursion

**Phase 33.1 — Cursor/INV/LFO Fixes (7 bugs)**
- Bug 1: `displayCx_/displayCy_` initialized to pad center in `resized()` — no (0,0) flash on open
- Bug 2: kDamping 0.72 → 0.90 — spring-back has no overshoot
- Bug 3: LFO rate stick offset applied in normalized log space (NormalisableRange skew=0.35, ±0.5) — perceptually symmetric left vs right
- Bug 4: Quantizer X/Y axis labels swap in INV mode (conditional `drawText` in `paint()`)
- Bug 5: Modulation box LEFT X/Y labels, dropdowns, and atten knobs all rewired on INV toggle via `prevInvState_` change-detection guard
- Bug 6: Piano two-pass hit test — `kBlackNotes[]` checked before `kWhiteNotes[]`; no coordinate changes
- Bug 7: Battery icon redesigned as 3 vertical stripe blocks (green/orange/red/gray/cyan/"?")

**Phase 33 — Version Sync**
- GitHub v1.7 released as Latest; installer rebuilt with v1.7 feature text; desktop backup

### What went well

- **Paint-only scope discipline** — Phase 31 explicitly banned audio/MIDI changes; made it safe to bake visuals without any regression risk. The bake-in-`resized()` pattern gave zero per-frame allocation.
- **Spring-damper model simplicity** — Euler integration with two constants (kSpring, kDamping) gave natural feel and predictable tuning. No complex physics library needed.
- **Surgical bug planning** — Phase 33.1 RESEARCH.md pinned exact line numbers before coding; all 7 fixes compiled clean on the first build attempt, total ~35 min elapsed.
- **SWAP/INV isolation** — routing through a single lambda + 4 known dispatch sites kept the feature fully contained; zero changes to MIDI output or audio paths.
- **INV attachment swap pattern** — `prevInvState_` change-detection guard fires once on toggle, not every 30 Hz timer tick; avoids JUCE attachment churn entirely.

### What was hard

- **Codebase reset** — Phases 31-34 were built and installed (2026-03-04) but had bugs. The codebase was reset to the v1.6 tag and all v1.7 work was redone in a single day. Root cause: initial implementation lacked a clean integration checkpoint before building on top of changes.
- **INV attachment rewiring** — JUCE APVTS attachments can't just be relabeled; they must be destroyed and reconstructed with swapped param IDs. The `prevInvState_` guard was necessary to avoid attachment spam. Discovered during 33.1 research, not anticipated at design time.
- **Cursor initial position** — `displayCx_/displayCy_` default to 0.0f in the header; first `paint()` runs before the first `timerCallback`, so the cursor drew at the top-left corner of the pad. Simple one-line fix in `resized()` but required research to find the correct hook.
- **LFO rate perceptual symmetry** — linear offset (`base + stick * 9.995f`) felt heavily asymmetric because the parameter uses a log-skewed NormalisableRange. Required converting to normalized space before applying the offset.

### Key decisions

| Decision | Outcome |
|----------|---------|
| Bake space background in `resized()`, not `paint()` | Zero per-frame allocation; `paint()` is compositing only |
| kDamping=0.90 (near-critical) not exact critical | No overshoot; avoids sluggish feel of exact critical value |
| LFO stick offset in normalized log space (±0.5) | Equal perceptual range in both directions across 3 decades |
| `prevInvState_` guard for APVTS attachment swap | Fires once on state change, not 30x/sec |
| Piano two-pass hit test — iteration order only | No coordinate math changes; simple and correct |
| Battery redesign paint-only, GamepadInput.cpp unchanged | Smaller scope; SDL raw pass-through retained |
| Codebase reset to v1.6 before v1.7 redo | Clean base; no bugs carried forward into release |

### Numbers

- **Phases:** 4 (31, 32, 33.1, 33)
- **Bugs fixed:** 13 (6 in gamepad/routing batch + 7 in Phase 33.1)
- **Files changed:** PluginEditor.h/cpp, PluginProcessor.h/cpp, GamepadInput.cpp
- **Build failures on 33.1 fixes:** 0 (all 7 compiled clean first attempt)
- **Codebase resets:** 1 (2026-03-04)
- **Duration:** 2 days (2026-03-04 → 2026-03-05)

---

## v1.6 Triplets & Fixes (2026-03-02 → 2026-03-03)

**Theme:** Correctness fixes + rhythmic expressiveness + visual looper

### What shipped

- **Defaults fix** — Third/Fifth/Tension octave defaults corrected to musically useful values (4/4/3); scale defaults to Natural Minor on fresh load
- **noteCount_ clamp removal** — `else noteCount_ = 0` removed from all 13 note-off paths; eliminated stuck notes in Single Channel mode when two voices share a pitch
- **Triplet subdivisions** — 1/1T through 1/32T added to both random trigger and quantize selectors; APVTS enum extended; no preset backward compatibility maintained (user accepted this trade-off)
- **Random Free redesign** — three-branch sync matrix: Poisson stochastic (SYNC OFF), internal free-tempo clock grid (SYNC ON + DAW stopped), DAW ppq grid (DAW Sync ON)
- **Looper perimeter bar** — clockwise rectangular bar replaces horizontal strip; ghost ring at idle; LOOPER label excluded from clip at all times; `perimPoint` lambda handles negative-distance wraparound without corner special-casing

### What went well

- **Root cause diagnosis** — `else noteCount_ = 0` was the sole stuck-note root cause; removing it from all 13 paths in one phase was clean and complete
- **Random Free three-branch matrix** — unambiguous mapping of two boolean controls (RND SYNC, DAW Sync) to three behaviors made the implementation straightforward and testable
- **Perimeter bar wraparound** — `fmod` + conditional add for negative distances handled tail wraparound correctly without corner special-casing

### Numbers

- **Phases:** 5 (26–30)
- **Duration:** 1 day (2026-03-02 → 2026-03-03)
- **note-off paths patched:** 13
