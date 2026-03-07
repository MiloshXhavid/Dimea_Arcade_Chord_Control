# Phase 35: Arp Subdivision Expansion — Context

**Created:** 2026-03-06
**Status:** Ready for execution (plan exists — see correction note below)

---

## Phase Scope

Expand the arpeggiator subdivision selector from 6 items to 17 items, matching the full
RandomSubdiv ordering already used by the Random Trigger system.

4 change sites (all in RESEARCH.md and 35-01-PLAN.md):
1. `createParameterLayout` — arpSubdiv APVTS param: 6 → 17 items
2. `processBlock` — kSubdivBeats[6] → kSubdivBeats[17], jlimit(0,5) → jlimit(0,16)
3. Gamepad dispatch — stepWrappingParam(0, 5) → stepWrappingParam(0, 16)
4. PluginEditor — arpSubdivBox_ addItem x6 → addItemList x17

---

## Decisions

### A. Default index: 1/4 (index 6) — CORRECTION TO PLAN

**Decision:** Default arpSubdiv = index **6** (= "1/4"), not index 8 (= "1/8") as written in 35-01-PLAN.md.

**Rationale:** Matches the Random Trigger subdivision box default (1/4 is mid-range, not 1/8).
The PLAN.md has `addChoice(..., 8)` and `// default: 1/8` — both must be changed to index 6.

**Impact on plan:**
- CHANGE SITE 1: `addChoice(ParamID::arpSubdiv, "Arp Subdivision", arpSubdivChoices, 6);  // default: 1/4`
- CHANGE SITE 3: comment `// 6: 1/4  (default)` instead of `// 8: 1/8  (default)`
- Checkpoint verify step 4: confirm "1/4" is selected by default, not "1/8"

### B. Preset backward compatibility: no migration guard

**Decision:** Accept silent remap on old presets. No `setStateInformation` guard.

**Rationale:** This is a pre-release. No users have saved presets with the 6-item param in the
wild. The `jlimit(0,16)` crash guard is sufficient. Old normalised floats will remap to different
indices silently — acceptable, no crash, no hang.

**Note:** A migration guard is unreliable anyway — JUCE normalises `index/(N-1)` and old + new
value ranges overlap, making old-vs-new detection ambiguous.

### C. ComboBox width: match Random Trigger subdivision box

**Decision:** Set arpSubdivBox_ width to match the existing randomSubdivBox_ width (which already
displays "1/32T" without clipping). Do not abbreviate labels. If any visual clipping is discovered
post-build, address it then — no pre-emptive resize beyond matching the random box.

---

## Code Context

All 4 change sites are fully specified in 35-RESEARCH.md with exact before/after code.
35-01-PLAN.md contains the complete execute plan.

**Only correction needed vs. the existing plan:** default index 8 → 6 (three occurrences in the plan).

---

## Deferred Ideas

- Preset save UI button (dedicated save/load panel) — raised during discussion, out of scope for phase 35
