# Phase 22: LFO Recording - Research

**Researched:** 2026-03-01
**Domain:** JUCE C++17 audio plugin — LFO state machine, ring buffer capture, looper coupling, UI control grayout
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

#### Per-LFO Independence
- LFO X and LFO Y each get their own independent Arm + Clear buttons
- Both LFOs can be armed simultaneously — both capture on the same looper record cycle
- One LFO can be in playback while the other is live or recording

#### Looper Coupling
- Recording is tightly coupled to the looper: capture ONLY starts when the looper enters record mode
- Capture ends exactly when the looper exits record mode (one loop length captured automatically)
- LFO playback syncs to the looper — the captured shape loops in time with the looper
- **Looper Clear** → resets LFO to live mode (recording/playback state wiped)
- **Looper Stop** → LFO keeps its current state (stays in playback if it was playing)

#### Arm Button States
- **Unarmed** — button off/dim
- **Armed (waiting)** — button blinks, matching the looper REC button blink pattern
- **Recording** — button lit steady (capturing in progress)
- **Playback** — button stays lit steady to indicate "playback active"
- Can re-arm an LFO that is already in playback mode — arms for the NEXT looper record cycle and overwrites the current buffer

#### Arm Blocked Condition
- Arm button is blocked (non-interactive) when the LFO Enabled toggle is OFF
- LFO must be enabled before it can be armed

#### UI Grayout During Playback
- When an LFO enters playback mode: Shape, Freq, Phase, and Level controls are visually grayed out and non-interactive
- Distort slider stays fully adjustable and audibly affects playback output
- Clear button is available to exit playback and return to live mode

#### Preset Persistence
- Session-only — the recorded buffer is NOT saved with the preset
- Loading any preset always starts with LFO in live (unarmed) mode
- No buffer serialization needed in getStateInformation / setStateInformation

### Claude's Discretion
- Runtime ring buffer resolution (4096 points with interpolation on playback is a sensible default)
- Exact interpolation method for playback (linear is fine)
- Exact position of Arm and Clear buttons within each LFO panel (ROADMAP says Arm goes next to Sync button)

### Deferred Ideas (OUT OF SCOPE)

None — discussion stayed within phase scope.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| LFOREC-01 | An "Arm" button next to the Sync button lets the user arm LFO recording (unarmed by default) | Arm is a manual onClick button (not APVTS); state machine in LfoEngine |
| LFOREC-02 | When armed and the looper starts recording, pre-distortion LFO output is captured into a dedicated ring buffer for exactly one loop cycle | Edge-detect looper_.isRecording() false→true in processBlock; capture pre-distortion output (after evaluateWaveform * level, before distortion add) |
| LFOREC-03 | After one loop cycle, recording stops automatically and LFO switches to playback — replaying stored values in sync with the looper | Auto-stop on looper isRecording() true→false transition; playback reads ring buffer indexed by looper beat position |
| LFOREC-04 | During LFO playback mode, Shape / Freq / Phase / Level / Sync controls are grayed out; Distort remains adjustable | setEnabled(false) on 5 controls in timerCallback() — existing pattern from filter mod and quantize grayout |
| LFOREC-05 | A "Clear" button returns LFO recording to unarmed/normal mode and re-enables all grayed-out parameters | Manual onClick button; calls lfoX_.clearRecording() / lfoY_.clearRecording() via processor pass-through |
| LFOREC-06 | Distort is applied live on top of stored playback samples — never recorded | In processBlock LFO section: when in playback, read ring buffer value (pre-distortion), then apply distortion from APVTS param as usual |
</phase_requirements>

---

## Summary

Phase 22 adds an Arm/Record/Playback state machine to each of the two existing `LfoEngine` instances (`lfoX_` and `lfoY_` in `PluginProcessor`). The implementation is entirely self-contained within the existing architecture: `LfoEngine` gains a `RecState` enum, a fixed-size ring buffer, and four new public methods (`arm()`, `clearRecording()`, `isArmed()`, `isPlayback()`). `PluginProcessor::processBlock()` drives the state machine by edge-detecting `looper_.isRecording()` transitions and calling the appropriate `LfoEngine` methods. `PluginEditor` adds two `juce::TextButton` widgets per LFO panel (ARM and CLEAR), uses the existing `recBlinkCounter_` blink pattern for the Armed state, and disables 5 controls per panel in `timerCallback()` using the already-established `setEnabled(false)` pattern.

The critical design constraint — Distort applied live to playback, never recorded — maps cleanly to the existing `LfoEngine::process()` structure. Steps 1–7 compute the waveform output; step 8 adds distortion; step 9 scales by level. "Pre-distortion capture" means capturing the result after step 9 (level applied) but before step 8 (distortion added). In playback mode, `process()` returns the stored value and then re-applies distortion from the current APVTS param, leaving all of step 8 in place.

Looper coupling uses the existing atomic `looper_.isRecording()` flag plus a `prevLfoWasLooperRecording_` bool (audio-thread-only, no atomic needed) for edge detection in `processBlock()`. Playback sync positions the ring buffer read head using `looper_.getPlaybackBeat() / looperLengthBeats` normalized to a [0, 1) fraction — the same looper playback beat the UI already reads. Buffer size of 4096 samples provides ~93ms per step at 44100 Hz/1 Hz, which is more than sufficient for any musical LFO shape with linear interpolation.

**Primary recommendation:** Add the `RecState` enum + `float recBuf_[4096]` + state methods to `LfoEngine.h/cpp`; drive state transitions from `processBlock()`; add ARM/CLEAR buttons to `PluginEditor`; use `setEnabled()` in `timerCallback()` for grayout.

---

## Standard Stack

### Core
| Component | Version | Purpose | Why Standard |
|-----------|---------|---------|--------------|
| `LfoEngine` (existing) | Project | DSP class — extends with state machine | Already the canonical LFO DSP object; all LFO state lives here |
| `std::atomic<bool>` | C++17 | Cross-thread state flags (armed, playback) | Established project pattern — `subOctSounding_`, `recording_`, etc. |
| `juce::TextButton` | JUCE 8.0.4 | ARM and CLEAR buttons | All action buttons in the project use TextButton with styleButton() |
| `setEnabled()` | JUCE 8.0.4 | Control grayout in timerCallback | Already used for filter mod, quantize, routing controls |

### Supporting
| Component | Version | Purpose | When to Use |
|-----------|---------|---------|-------------|
| `float recBuf_[4096]` | C++ | Fixed-size ring buffer for captured LFO output | Static array — no allocation, cache-friendly, audio-thread safe |
| `prevLooperRecording_` bool | C++ | Edge-detect looper isRecording() transitions | Audio-thread-only bool — no atomic needed (only processBlock reads/writes) |
| `std::atomic<float>` playback beat | JUCE | Looper playback position for buffer indexing | Already exists: `looper_.getPlaybackBeat()` returns atomic float |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| Fixed `float recBuf_[4096]` | `std::vector<float>` with dynamic size | Dynamic sizing matches loop length exactly, but requires allocation at recording start — violates audio-thread safety. Fixed 4096 is simpler and sufficient. |
| `looper_.getPlaybackBeat()` for sync | Dedicated sample counter in LfoEngine | Looper beat is already the authoritative sync source; replicating it would drift. Use the looper position directly. |
| `setEnabled(false)` for grayout | Custom alpha painting | setEnabled already produces the correct grayed-out appearance in PixelLookAndFeel and blocks mouse interaction. No custom painting needed. |

---

## Architecture Patterns

### Recommended File Changes
```
Source/
├── LfoEngine.h        # Add RecState enum, recBuf_[], arm/clear/state methods
├── LfoEngine.cpp      # Implement capture logic in process(), arm/clear methods
├── PluginProcessor.h  # Add lfoXRecArmed_ / lfoYRecArmed_ state passthrough methods
├── PluginProcessor.cpp # Edge-detect looper recording transitions; drive LfoEngine state
├── PluginEditor.h     # Add lfoXArmBtn_, lfoYArmBtn_, lfoXClearBtn_, lfoYClearBtn_
│                      # Add lfoXArmBlinkCounter_, lfoYArmBlinkCounter_
└── PluginEditor.cpp   # Wire buttons, update timerCallback for blink + grayout
```

### Pattern 1: RecState Enum in LfoEngine

**What:** A 4-state enum tracks Arm/Record/Playback lifecycle. Atomic read from UI, audio-thread written by processBlock dispatch.

**When to use:** Any time transient (non-persistent) state controls audio behavior.

```cpp
// Source: LfoEngine.h — new addition
enum class LfoRecState { Unarmed = 0, Armed, Recording, Playback };

class LfoEngine
{
public:
    // ... existing API unchanged ...
    float process(const ProcessParams& p);
    void  reset();

    // New recording API — called from processBlock (audio thread)
    void arm();                   // transition Unarmed→Armed (or Playback→Armed for re-arm)
    void clearRecording();        // transition any state→Unarmed; clears buffer
    void startCapture(int bufSize); // transition Armed→Recording; sets capture parameters
    void stopCapture();           // transition Recording→Playback

    // State reads — called from message thread (timerCallback) — atomic
    LfoRecState getRecState() const { return recState_.load(std::memory_order_relaxed); }

private:
    // Ring buffer — 4096 pre-distortion LFO values
    static constexpr int kRecBufSize = 4096;
    float  recBuf_[kRecBufSize] = {};

    // Capture progress — audio-thread-only (no atomic needed)
    int    captureWriteIdx_ = 0;  // current write position during recording
    int    captureTotal_    = 0;  // total samples to capture (set at startCapture)
    int    capturedCount_   = 0;  // how many have been written so far

    std::atomic<LfoRecState> recState_ { LfoRecState::Unarmed };
};
```

### Pattern 2: Pre-Distortion Capture in LfoEngine::process()

**What:** During Recording state, capture the waveform output AFTER level scaling but BEFORE distortion noise is added. During Playback state, read from the buffer instead of computing the waveform, then apply distortion normally.

**When to use:** Satisfies LFOREC-06 (Distort applied live, never recorded).

```cpp
// Source: LfoEngine.cpp — modified process() return path
// BEFORE distortion (step 8), AFTER evaluateWaveform * level (steps 7+9):
float output = evaluateWaveform(p.waveform, static_cast<float>(normalizedPhase));
output *= p.level;  // step 9 applied first

// ── Recording capture ──────────────────────────────────────────────────────
const LfoRecState state = recState_.load(std::memory_order_relaxed);
if (state == LfoRecState::Recording)
{
    recBuf_[captureWriteIdx_] = output;  // store pre-distortion, post-level value
    captureWriteIdx_ = (captureWriteIdx_ + 1) % kRecBufSize;
    ++capturedCount_;
    if (capturedCount_ >= captureTotal_)
    {
        // Auto-stop: buffer full for one loop cycle
        // NOTE: actual state transition to Playback is driven by processBlock()
        // detecting looper isRecording() false. Do not self-transition here.
        // (processBlock drives the machine; LfoEngine is the mechanism.)
    }
}

// ── Playback read ──────────────────────────────────────────────────────────
if (state == LfoRecState::Playback)
{
    // playbackPhase is provided by processBlock via ProcessParams (new field)
    // or computed here from looper beat / loop length.
    // Simple approach: caller provides normalizedPlaybackPhase in [0, 1).
    const float fIdx = p.playbackPhase * static_cast<float>(kRecBufSize);
    const int   i0   = static_cast<int>(fIdx) % kRecBufSize;
    const int   i1   = (i0 + 1) % kRecBufSize;
    const float frac = fIdx - static_cast<float>(static_cast<int>(fIdx));
    output = recBuf_[i0] + frac * (recBuf_[i1] - recBuf_[i0]);  // linear interp
}

// ── Distortion — always applied (step 8), including during Playback ──────
if (p.waveform != Waveform::Random && p.distortion > 0.0f)
    output += p.distortion * nextLcg();

return output;  // level already applied before capture; do NOT multiply again
```

**Note on ProcessParams extension:** Add `float playbackPhase = 0.0f;` to `ProcessParams`. When `recState == Playback`, `processBlock()` computes:
```cpp
// In processBlock, LFO section:
xp.playbackPhase = static_cast<float>(
    std::fmod(looper_.getPlaybackBeat() / looperLengthBeats, 1.0));
```

### Pattern 3: Edge-Detection in processBlock()

**What:** Two audio-thread-only bools track the previous looper recording state. A rising edge (false→true) calls `startCapture()`; a falling edge (true→false) calls `stopCapture()`.

**When to use:** This is the established JUCE pattern for cross-thread state detection without additional atomics.

```cpp
// Source: PluginProcessor.cpp — in processBlock(), after looper_.process()
// Audio-thread-only bools: prevLooperRecording_ (already exists as prevLooperWasPlaying_
// for play state; add a parallel one for recording)

const bool looperNowRecording = looper_.isRecording();

if (!prevLooperRecording_ && looperNowRecording)
{
    // Rising edge: looper just started recording
    // Compute buffer size = loopLengthBeats * sampleRate / BPM * 60
    // normalized to kRecBufSize — but we use phase not sample count, so
    // just call startCapture with total blocks or a beat count.
    // Simplest: captureTotal = looperLengthBeats steps normalized to kRecBufSize
    if (lfoX_.getRecState() == LfoRecState::Armed)
        lfoX_.startCapture(kLfoRecBufSize);  // 4096 = full buffer
    if (lfoY_.getRecState() == LfoRecState::Armed)
        lfoY_.startCapture(kLfoRecBufSize);
}
else if (prevLooperRecording_ && !looperNowRecording)
{
    // Falling edge: looper just stopped recording
    if (lfoX_.getRecState() == LfoRecState::Recording)
        lfoX_.stopCapture();
    if (lfoY_.getRecState() == LfoRecState::Recording)
        lfoY_.stopCapture();
}
prevLooperRecording_ = looperNowRecording;

// Looper Clear → reset LFO state (looper_.deleteLoop() sets deleteRequest_ which
// processes as a reset; listen for looperReset flag in loopOut)
if (loopOut.looperReset)
{
    lfoX_.clearRecording();
    lfoY_.clearRecording();
}
```

### Pattern 4: Blink in timerCallback() — Mirror Existing recBlinkCounter_

**What:** Per-LFO blink counters, using the identical `((++counter_) / 5) % 2` pattern at 30Hz.

**When to use:** All blinking buttons in the project use this pattern. Arm button blinks when `Armed`, stays lit when `Recording` or `Playback`.

```cpp
// Source: PluginEditor.cpp — timerCallback()
// Mirrors the existing loopRecBtn_ blink block exactly.

const LfoRecState xState = proc_.getLfoXRecState();
if (xState == LfoRecState::Unarmed)
{
    lfoXArmBlinkCounter_ = 0;
    lfoXArmBtn_.setToggleState(false, juce::dontSendNotification);
}
else if (xState == LfoRecState::Armed)
{
    const bool on = ((++lfoXArmBlinkCounter_) / 5) % 2 == 0;
    lfoXArmBtn_.setToggleState(on, juce::dontSendNotification);
}
else  // Recording or Playback — steady lit
{
    lfoXArmBlinkCounter_ = 0;
    lfoXArmBtn_.setToggleState(true, juce::dontSendNotification);
}
```

### Pattern 5: Control Grayout — setEnabled(false) in timerCallback()

**What:** 5 controls per LFO panel are disabled when `LfoRecState == Playback`. Distort slider and Clear button remain enabled. This uses the exact same `setEnabled()` approach as filter mod controls.

**When to use:** Matches the existing `filterYModeBox_.setEnabled(filterOn)` block in `timerCallback()`.

```cpp
// Source: PluginEditor.cpp — timerCallback()
// LFO X grayout
{
    const bool xPlayback = (proc_.getLfoXRecState() == LfoRecState::Playback);
    lfoXShapeBox_   .setEnabled(!xPlayback);
    lfoXRateSlider_ .setEnabled(!xPlayback);
    lfoXPhaseSlider_.setEnabled(!xPlayback);
    lfoXLevelSlider_.setEnabled(!xPlayback);
    lfoXSyncBtn_    .setEnabled(!xPlayback);
    // lfoXDistSlider_ remains always enabled
    // lfoXClearBtn_ remains always enabled (only meaningful in playback)
    lfoXArmBtn_.setEnabled(
        *proc_.apvts.getRawParameterValue("lfoXEnabled") > 0.5f);  // blocked if LFO off
}
// Mirror for LFO Y
```

### Pattern 6: Arm / Clear Button onClick — Manual (Not APVTS-Backed)

**What:** Arm and Clear drive transient state, not persistent parameters. Use manual `onClick` lambdas calling processor methods. This matches `padHold_`, `loopPlayBtn_`, etc.

```cpp
// Source: PluginEditor.cpp — constructor
lfoXArmBtn_.setButtonText("ARM");
lfoXArmBtn_.setClickingTogglesState(false);  // NOT a toggle — toggleState controlled by timerCallback
styleButton(lfoXArmBtn_);
addAndMakeVisible(lfoXArmBtn_);
lfoXArmBtn_.onClick = [this]()
{
    proc_.armLfoX();  // processor passthrough to lfoX_.arm()
};

lfoXClearBtn_.setButtonText("CLR");
lfoXClearBtn_.setClickingTogglesState(false);
styleButton(lfoXClearBtn_);
addAndMakeVisible(lfoXClearBtn_);
lfoXClearBtn_.onClick = [this]()
{
    proc_.clearLfoXRecording();
};
```

### Pattern 7: LFO Panel Layout — ARM next to SYNC, CLR next to ARM

**What:** In `resized()`, after the SYNC button row, add ARM and CLR in the same column. The LFO panel is 150px wide; split the 22px button row into two 73px buttons with a 4px gap.

```cpp
// Source: PluginEditor.cpp — resized() LFO X panel block (after lfoXSyncBtn_)
col.removeFromTop(4);
{
    auto row = col.removeFromTop(22);
    const int btnW = (row.getWidth() - 4) / 2;
    lfoXArmBtn_ .setBounds(row.removeFromLeft(btnW));
    row.removeFromLeft(4);
    lfoXClearBtn_.setBounds(row);
}
// Update panel bounds to include the new row
lfoXPanelBounds_ = lfoXEnabledBtn_.getBounds()
                      .getUnion(lfoXClearBtn_.getBounds())
                      .withX(lfoXCol.getX())
                      .withWidth(lfoXCol.getWidth())
                      .expanded(0, 10);
```

### Anti-Patterns to Avoid

- **Self-transition in LfoEngine::process():** Do NOT call `recState_.store(Playback)` inside `process()`. Only `processBlock()` drives transitions. `process()` is a pure DSP function — side effects beyond phase state break the audio pipeline.
- **Saving buffer to APVTS:** The recorded buffer is session-only per CONTEXT.md. Do not serialize `recBuf_` in `getStateInformation()`.
- **Re-applying level in playback path:** `recBuf_` stores values that already have level applied. Do not multiply by `p.level` again during playback read — this would double-scale.
- **Using processBlock()-thread `looper_.isRecording()` without edge detection:** Calling `arm()` every block where recording is true would re-arm on every block. Use the edge-detect pattern (prevLooperRecording_ bool).
- **Calling arm() from the UI thread directly into LfoEngine:** Route through processor passthrough methods (`proc_.armLfoX()`) for consistent threading discipline.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Interpolated ring buffer playback | Custom fractional-index class | `recBuf_[i0] + frac * (recBuf_[i1] - recBuf_[i0])` inline | Trivial for 4096-point fixed buffer; no library adds value |
| Cross-thread state signaling | `std::mutex` around LfoEngine state | `std::atomic<LfoRecState>` | Project uses atomics throughout; mutex in audio path is forbidden |
| Looper sync position | Dedicated beat counter in LfoEngine | `looper_.getPlaybackBeat()` already exists | It returns `std::atomic<float>` — already the authoritative sync source |
| Blink timer | Hardware timer or separate Thread | `timerCallback()` at 30Hz — already running | The project already has exactly one UI timer; no additional timers needed |

**Key insight:** This phase adds ~150 lines of well-bounded code. All infrastructure (atomic flags, blink pattern, looper coupling, setEnabled grayout) already exists in the codebase.

---

## Common Pitfalls

### Pitfall 1: Level Double-Application During Playback

**What goes wrong:** `recBuf_` stores values after `output *= p.level` (step 9). If the playback path also multiplies by `p.level`, the output is squared-scaled.

**Why it happens:** The `process()` function always applies level at the end. In playback mode the captured value already has level baked in.

**How to avoid:** In the playback branch, return the interpolated buffer value and apply distortion, but do NOT apply `p.level` again. The stored value is the final pre-distortion output.

**Warning signs:** During playback, LFO output is visibly louder than during recording. Test: record at level=0.5, verify playback peak is the same as recording peak.

### Pitfall 2: Looper Clear vs. Looper Stop Distinction

**What goes wrong:** Clear and Stop are different operations. Stop keeps LFO in playback. Clear (loopOut.looperReset) resets LFO to Unarmed.

**Why it happens:** `looper_.reset()` sets `resetRequest_`; `looper_.deleteLoop()` sets `deleteRequest_`. Both result in `loopOut.looperReset = true` in `LooperEngine::process()`.

**How to avoid:** In `processBlock()`, check `loopOut.looperReset` (not `looper_.isPlaying()` falling edge) to trigger `lfoX_.clearRecording()`. Verify by: stop looper → LFO stays in playback; then delete loop → LFO returns to Unarmed.

**Warning signs:** LFO stays in playback after looper delete, or drops out of playback when looper stops.

### Pitfall 3: Re-Arm During Playback Overwrites Buffer Mid-Read

**What goes wrong:** Re-arming while in Playback is valid per spec. But if the write head starts before the read head finishes one cycle, you get a torn buffer.

**Why it happens:** ARM→Recording starts filling from `captureWriteIdx_ = 0` immediately on the rising edge of looper recording. The read head is still running from the old buffer.

**How to avoid:** On `startCapture()`, keep the old buffer intact (do not zero it). The write head fills from index 0 sequentially. The read head continues reading until `stopCapture()` → Playback, at which point the buffer is fully overwritten. There is a window during Recording where old data is being overwritten — this is intentional and matches the looper's own punch-in behavior.

**Warning signs:** Clicks or pops during the re-arm recording cycle. Acceptable as long as they stop when Playback begins.

### Pitfall 4: ARM Button Enabled When LFO is Off

**What goes wrong:** User clicks ARM on a disabled LFO. LFO arms successfully. On looper record start, capture runs but LFO output is 0.0f (disabled branch in processBlock). Buffer fills with zeros. On Playback, no modulation. Confusing.

**Why it happens:** `arm()` doesn't check the enabled APVTS param — it has no access to APVTS.

**How to avoid:** `lfoXArmBtn_.setEnabled(*proc_.apvts.getRawParameterValue("lfoXEnabled") > 0.5f)` in `timerCallback()`. Button is blocked at the UI level before arm() can be called.

**Warning signs:** ARM button clickable when LFO ON toggle is off.

### Pitfall 5: processBlock Calls lfoX_.arm() from wrong state

**What goes wrong:** `arm()` is called by the UI (message thread via `proc_.armLfoX()`). `startCapture()` and `stopCapture()` are called by `processBlock()` (audio thread). If `arm()` is not atomic-safe, there is a race.

**Why it happens:** `arm()` writes `recState_` (an atomic). `processBlock()` reads `recState_`. Since `recState_` is `std::atomic<LfoRecState>`, this is safe as long as LfoRecState is trivially-copyable (an enum class is).

**How to avoid:** Verify that `std::atomic<LfoRecState>` compiles without error on MSVC (enum class underlying type is int by default — atomic<int> is always lock-free). Use `std::atomic<int>` if MSVC rejects `std::atomic<LfoRecState>` — cast on store/load.

**Warning signs:** MSVC C2338 or C2676 on `std::atomic<LfoRecState>`. Fix: `std::atomic<int>` with casts, matching the project's established workaround for Phase 18-02.

### Pitfall 6: Looper Beat Position Wraps Unexpectedly

**What goes wrong:** `looper_.getPlaybackBeat()` returns a raw beat value (e.g., 0 to 8 for a 2-bar loop in 4/4). Dividing by loop length gives [0, 1) as expected — BUT if the looper is stopped and playbackBeat_ is not reset to 0, the read head stays at its last position.

**Why it happens:** `looper_.getPlaybackBeat()` is an atomic float written by the audio thread. On looper stop, playback freezes but the beat is not zeroed (it's preserved for display).

**How to avoid:** The playback phase computation only matters when `recState_ == Playback`. When the looper is not playing, LFO playback continues from the frozen beat position — which is valid per spec ("Looper Stop → LFO keeps its current state"). The frozen position means a static LFO output, not a crash. Document this expected behavior.

---

## Code Examples

### LfoEngine.h additions (verified against existing file structure)

```cpp
// Source: LfoEngine.h — additions to existing class

enum class LfoRecState { Unarmed = 0, Armed, Recording, Playback };

class LfoEngine
{
public:
    // ... existing unchanged ...
    float process(const ProcessParams& p);
    void  reset();

    // Recording state machine — called from audio thread (startCapture/stopCapture)
    // and from message thread via processor (arm/clearRecording).
    // All state transitions through recState_ atomic are safe.
    void arm();
    void clearRecording();
    void startCapture();   // Armed → Recording; resets write head
    void stopCapture();    // Recording → Playback

    LfoRecState getRecState() const
    {
        return static_cast<LfoRecState>(recState_.load(std::memory_order_relaxed));
    }

private:
    // ... existing unchanged ...

    // Recording ring buffer
    static constexpr int kRecBufSize = 4096;
    float recBuf_[kRecBufSize] = {};
    int   captureWriteIdx_     = 0;  // audio-thread-only
    int   capturedCount_       = 0;  // audio-thread-only

    // Atomic state — written from message thread (arm/clear) and audio thread (start/stop)
    std::atomic<int> recState_ { 0 };  // int cast of LfoRecState; avoids MSVC C2338
};
```

### ProcessParams extension for playback sync

```cpp
// Source: LfoEngine.h — add field to existing ProcessParams struct
struct ProcessParams
{
    // ... existing fields unchanged ...

    // LFO recording playback: normalized looper position [0.0, 1.0)
    // Set by processBlock when recState == Playback.
    // Ignored in all other states.
    float playbackPhase = 0.0f;
};
```

### PluginProcessor — passthrough methods

```cpp
// Source: PluginProcessor.h — public section

void armLfoX()              { lfoX_.arm();            }
void armLfoY()              { lfoY_.arm();            }
void clearLfoXRecording()   { lfoX_.clearRecording(); }
void clearLfoYRecording()   { lfoY_.clearRecording(); }
LfoRecState getLfoXRecState() const { return lfoX_.getRecState(); }
LfoRecState getLfoYRecState() const { return lfoY_.getRecState(); }
```

### PluginProcessor.h — new audio-thread-only bool

```cpp
// Source: PluginProcessor.h — private section (alongside existing prevLooperWasPlaying_)
bool prevLooperRecording_ = false;  // audio-thread-only, no atomic needed
```

### PluginEditor.h additions

```cpp
// Source: PluginEditor.h — private section, alongside existing LFO panel members

// LFO recording buttons (one pair per LFO)
juce::TextButton lfoXArmBtn_,   lfoYArmBtn_;
juce::TextButton lfoXClearBtn_, lfoYClearBtn_;

// Blink counters for ARM button Armed state — mirrors recBlinkCounter_ pattern
int lfoXArmBlinkCounter_ = 0;
int lfoYArmBlinkCounter_ = 0;
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| Per-param APVTS-backed toggles | Transient atomic state machine in DSP class | Phase 22 | No preset save/load burden; matches looper's own non-APVTS design |
| Custom oscilloscope/waveform recorder | Fixed ring buffer + linear interpolation | Phase 22 | Sufficient for LFO shapes; no library dependency |

**Deprecated/outdated:**
- None. This phase introduces new functionality; nothing existing is deprecated.

---

## Open Questions

1. **playbackPhase when looper is not playing**
   - What we know: `looper_.getPlaybackBeat()` freezes when looper stops. `looper_.getLoopLengthBeats()` returns a stable double.
   - What's unclear: Should LFO playback continue running from the frozen beat, or freeze its output too?
   - Recommendation: Per spec, "Looper Stop → LFO keeps its current state." Frozen beat → frozen LFO output during looper stop. This is correct behavior — the LFO doesn't oscillate when the looper is stopped. Verify in DAW smoke test.

2. **captureTotal and buffer fill guarantee**
   - What we know: `startCapture()` sets `captureWriteIdx_ = 0`, `capturedCount_ = 0`. `process()` writes one sample per processBlock call into the buffer. At 44100 Hz with 512-sample blocks, one second = ~86 blocks. At 1 Hz LFO, the loop length would be the relevant period.
   - What's unclear: 4096 writes at 512 samples/block = 4096 writes. The entire buffer fills in `4096` processBlock calls. With a 2-bar loop at 120 BPM in 4/4 = 4 seconds = ~344 blocks. So we get at most 344 samples written per loop cycle — not 4096.
   - Recommendation: `captureTotal_` should be set to `kRecBufSize` (not the number of blocks). Each block writes exactly one sample (the LFO value for that block). For 4096-point playback interpolation, we need 4096 unique values. If the loop is 344 blocks long, we only write 344 values into the 4096-slot buffer. Solution: **write to normalized indices** — compute `writeIdx = (capturedCount_ * kRecBufSize) / totalBlocks` — OR use a simpler approach: write linearly and use only `capturedCount_` for playback, wrapping at `capturedCount_`. The planner should resolve this by choosing: fixed-density write (one per block, use capturedCount as buffer size) over fixed-4096 write (requires knowing totalBlocks upfront).

3. **MSVC compatibility of std::atomic<LfoRecState>**
   - What we know: Phase 18-02 and 19-01 documented MSVC C2923 issues with enum class aliases in templates.
   - What's unclear: Whether `std::atomic<LfoRecState>` directly compiles on MSVC 2026.
   - Recommendation: Use `std::atomic<int>` with explicit casts in LfoEngine (as shown in code examples). This is the safest approach given project history.

---

## Sources

### Primary (HIGH confidence)
- Project source files (LfoEngine.h/cpp, LooperEngine.h, PluginProcessor.h/cpp, PluginEditor.h/cpp) — direct code inspection; all patterns verified from existing implementation
- `.planning/phases/22-lfo-recording/22-CONTEXT.md` — locked decisions and architecture guidance from user discussion
- `.planning/REQUIREMENTS.md` — LFOREC-01 through LFOREC-06 requirements text
- `.planning/STATE.md` — accumulated project decisions, MSVC-specific workarounds

### Secondary (MEDIUM confidence)
- JUCE 8.0.4 TextButton / Component::setEnabled() — established pattern verified in PluginEditor.cpp at lines 2680-2687 (filter mod) and 2854-2857 (quantize)
- C++17 `std::atomic<int>` for enum storage — verified as project practice from Phase 18-02 / 19-01 decisions in STATE.md

### Tertiary (LOW confidence)
- Ring buffer interpolation for LFO shape capture — general DSP practice; not verified against a specific JUCE or DSP library reference (but trivial and well-established)

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all components are existing project code; no new libraries introduced
- Architecture: HIGH — state machine pattern, edge detection, blink timer all verified from existing codebase
- Pitfalls: HIGH — drawn from actual project history (Phase 18-02 MSVC issue, Phase 19-01 atomic patterns, Phase 17-01 looper edge-detection lessons)

**Research date:** 2026-03-01
**Valid until:** 2026-04-01 (stable architecture; JUCE 8.0.4 API is unchanged)
