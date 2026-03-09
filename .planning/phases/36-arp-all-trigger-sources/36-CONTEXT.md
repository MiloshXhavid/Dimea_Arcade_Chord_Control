---
phase: 36-arp-all-trigger-sources
created: 2026-03-07
status: context-complete
---

# Phase 36 Context: Arp + All Trigger Sources

## Goal

Remove the hardcoded guard that forces all voices to TouchPlate mode when the arp is active.
After the fix, Joystick and Random sources work alongside the arp without collision.

---

## Decisions

### 1. Note collision model — UNIFORM across all sources

**Decision:** Gate-open = live note sounding = arp skips that voice. This rule applies identically
to TouchPlate, Joystick, and RandomFree/RandomHold.

**Behavior per source when arp is on:**

| Source | Gate open when... | Arp action when gate open |
|--------|-------------------|---------------------------|
| TouchPlate | Pad physically held | Skip (live pad note has priority) |
| Joystick | Joystick note currently sounding (settled on pitch) | Skip (direct joystick note has priority) |
| RandomFree/RandomHold | Random note within its gate-length window | Skip (direct random note has priority) |

When the gate closes (pad released / joystick returns to center / random gate expires), the arp
fires that voice at the next scheduled arp step.

### 2. Direct trigger suppression — NONE

Direct notes from Joystick and Random sources fire normally. The arp does NOT suppress them.
There are no double notes because the `isGateOpen` skip at processBlock line 1884 prevents
the arp from firing the same voice while a direct note is already sounding.

### 3. isGateOpen skip direction — unchanged

The existing check `if (trigger_.isGateOpen(voice)) continue;` at the arp step loop is
already correct for all source types. No change needed to this logic.

### 4. TouchPlate + Arp — unchanged

SC3 confirmed: pad behavior with arp on is identical to current behavior.
The arp chokes any sounding arp note on pad press ("instant choke" at line 1523).

### 5. No hanging notes — already handled

When arp is disabled mid-phrase, the existing kill block (lines 1714–1724) sends note-off for
`arpActivePitch_` unconditionally. No additional cleanup needed.

---

## Implementation scope

**Single-site change — PluginProcessor.cpp:**

Remove the force-TouchPlate block (currently lines 1663–1667):

```cpp
// REMOVE THIS BLOCK:
// When arp is on, suppress joystick/random auto-triggers so arp has full
// sequencing control. Pads (TouchPlate) still fire live notes and choke arp.
if (arpOn)
    for (int v = 0; v < 4; ++v)
        tp.sources[v] = TriggerSource::TouchPlate;
```

No other changes required. The existing `isGateOpen` skip and arp kill-on-disable logic
already handle all cases correctly.

---

## Code context

| Location | Role |
|----------|------|
| `PluginProcessor.cpp` lines 1663–1667 | Force-TouchPlate block — **remove** |
| `PluginProcessor.cpp` line 1884 | `if (trigger_.isGateOpen(voice)) continue` — keep unchanged |
| `PluginProcessor.cpp` lines 1714–1724 | Arp kill-on-disable note-off — keep unchanged |
| `Source/TriggerSystem.cpp` lines 209–273 | Joystick gate logic — `gateOpen_` true while note sounding |
| `Source/TriggerSystem.cpp` lines 275–291 | RandomFree gate logic — `gateOpen_` true during gate-length window |

---

## Out of scope

- Changing the arp clock source (it continues to use DAW/looper beat grid)
- Making random events drive individual arp steps (event-clock model — not this phase)
- Any UI changes (no new params, no new controls)
- RandomHold pad-choke behavior (already identical to TouchPlate semantics)
