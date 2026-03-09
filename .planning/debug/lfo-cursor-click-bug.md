---
status: resolved
---

# Bug: UI pad click does nothing when LFO is controlling cursor

**Reported:** 2026-03-07
**Symptom:** When LFO is enabled (cursor linked), clicking the JoystickPad UI has no effect.
  Gamepad right stick DOES change the base offset. Mouse clicks do not.
**Gamepad stick state:** resting at center when bug occurs.

---

## Investigation findings

### Key code paths

**Display source (JoystickPad::timerCallback line 1076-1087):**
```cpp
const bool lfoXActive = *proc_.apvts.getRawParameterValue("lfoXEnabled") > 0.5f;
const bool lfoYActive = *proc_.apvts.getRawParameterValue("lfoYEnabled") > 0.5f;
const float dispX = (lfoXActive || lfoYActive)
    ? proc_.modulatedJoyX_.load(...)   // LFO ON: read modulated
    : proc_.joystickX.load(...);       // LFO OFF: read raw
```

**modulatedJoyX_ source (PluginProcessor.cpp line 1116):**
```cpp
modulatedJoyX_.store(chordP.joystickX + (xLinked ? lfoXRampOut_ : 0.0f), ...);
```

**Looper override (PluginProcessor.cpp line 815-816):**
```cpp
if (loopOut.hasJoystickX) joystickX.store(loopOut.joystickX);  // OVERWRITES UI click
if (loopOut.hasJoystickY) joystickY.store(loopOut.joystickY);
```

**PluginEditor timerCallback gamepad mirror (line 5378-5399):**
```cpp
const bool intentional = std::abs(gpX) > kStickBypassThreshold(0.05f) || ...;
if (intentional)
    proc_.joystickX.store(gpX);  // gamepad writes every 30Hz tick if above threshold
else if (gamepadWasLastPitchWriter_)
    proc_.joystickX.store(0.0f);  // one-shot reset when stick returns to center
// else: leave joystickX to mouse
```

### Spring-damper cursor (timerCallback line 1126-1147)
```cpp
if (mouseIsDown_)
    displayCx_ = cx;  // snap — but cx = pixel(modulatedJoyX_), NOT pixel(joystickX)
else
    // spring chases cx = pixel(joystickX + lfoRampOut_)
```

**When LFO is OFF:** cx = pixel(joystickX). Click → joystickX=P → cx=P. Cursor snaps to P.
**When LFO is ON:** cx = pixel(joystickX + lfoRampOut_). Click → joystickX=P → cx=P+lfoOffset.
  Cursor snaps to P+lfoOffset, NOT to the clicked position. User perceives this as "didn't work".

---

## Candidate root causes (in priority order)

### Candidate A: Looper overwriting joystickX (MOST LIKELY if looper is playing)
- If looper is playing a loop with recorded joystick data, processBlock writes
  `joystickX = looper_value` every audio block
- UI click writes joystickX=P on message thread, but NEXT processBlock overwrites it
- Gamepad timerCallback write (30Hz message thread) can temporarily win before processBlock runs
- **Discriminating question:** Is looper playing with recorded joystick content?

### Candidate B: Gamepad drift above kStickBypassThreshold=0.05 (LIKELY if controller connected)
- If getPitchX() > 0.05 after deadzone rescaling, PluginEditor timerCallback writes
  joystickX = driftValue (~0.06) every 30Hz tick
- User click writes joystickX=P, but 33ms later timerCallback overwrites with drift value
- With LFO OFF: cursor shows joystickX=~0.06 (near center), user might not notice 33ms flicker
- With LFO ON: cursor shows ~0.06 + lfoRampOut_ (oscillating near center), user clearly sees
  the click had no lasting effect
- **Discriminating question:** What is the gamepad deadzone setting in the plugin?

### Candidate C: Visual-only confusion from cursor link (possible misperception)
- Click at screen position P → joystickX=P → cursor shows at P+lfoOffset (NOT at P)
- User thinks click didn't work because cursor didn't go to clicked position
- The PITCH does change correctly (chordP.joystickX = P)
- Gamepad "works" because user moves stick until they see/hear the desired result
- **Discriminating question:** Is the JOY cursor link button ON?

---

## Pending user answers
- Q1: Is the looper currently playing a loop that has joystick position recorded?
- Q2: Is the JOY cursor link button in the LFO box ON or OFF?

---

## Likely fix (pending confirmation)

**If Candidate A (looper override):**
- In processBlock lines 815-816, skip looper joystick override when user is actively dragging:
  add a `!mouseIsDraggingJoystick_` atomic guard (message thread sets it in mouseDown/Up).
  OR: make looper joystick data additive (base + looper_delta) instead of absolute override.

**If Candidate B (drift):**
- Increase kStickBypassThreshold from 0.05 to 0.08 or 0.10 in PluginEditor.cpp line 350.
  OR: add hysteresis — only set intentional=true if stick was above threshold for 2+ consecutive ticks.

**If Candidate C (visual confusion):**
- When mouseIsDown_=true, set displayCx_=pixel(joystickX) (NOT pixel(modulatedJoyX_)).
  This makes the cursor snap to the exact clicked position, then spring toward P+lfoOffset on release.
  Fix location: JoystickPad::timerCallback line 1131-1132.
  Change: `displayCx_ = cx` → use a separate cx_raw computed from joystickX (not modulatedJoyX_).
