# Phase 05 Context — LooperEngine Hardening and DAW Sync

**Phase goal:** Replace `std::mutex` with lock-free `AbstractFifo` design, add sync toggle + punch-in recording, validate 1–16 bar loops in Ableton and Reaper.

---

## A. Lock-Free Buffer Design

**Decisions:**
- Replace `std::vector<LooperEvent>` + `std::mutex` with `juce::AbstractFifo` + fixed-size array.
- **Capacity:** 2 048 events (compile-time constant `LOOPER_FIFO_CAPACITY = 2048`).
- **Overflow behaviour:** Recording auto-stops when the configured loop length is reached (not just when the buffer fills). An `std::atomic<bool> recordingCapReached_` flag is set so the UI can show a visual indicator (flash the record button or change its colour).
- **Lock-free scope:** Fully lock-free everywhere — including `hasContent()` and `deleteLoop()`. These use atomics and careful ordering rather than a mutex. The audio thread must never block.
- **Concurrency model:** Sequential — record pass first, then playback. No simultaneous record + playback (no overdub at the buffer level).
- **Mutex enforcement:** Enable `-fsanitize=thread` (ThreadSanitizer) in CMake Debug builds. If TSAN is unavailable on Windows, use `JUCE_ASSERT_MESSAGE_THREAD` / `jassert(!juce::MessageManager::existsAndIsCurrentThread())` guards at the top of `process()` as a secondary check.

---

## B. Recording Behaviour

**Decisions:**
- **Punch-in mode:** When record is activated on a loop that already has content, new events replace old events at the beat positions touched by the new pass. Beat positions not touched in the new pass are untouched.
- **Joystick recording:** Sparse — only record a new joystick event when the X or Y axis moves beyond a configurable threshold (default ≈ 0.02 normalised). This prevents flooding the buffer with redundant static-position events.
- **Live input during playback:** Live gate triggers always pass through to MIDI output even while the looper is playing back. The looper does not block live input.
- **Record-type selection (UI):** Two toggle buttons on the UI:
  - **[REC JOY]** — arms joystick X/Y event recording
  - **[REC GATES]** — arms gate (note-on/off) event recording
  - Both can be active simultaneously. If neither is active, pressing record does nothing.

**Deferred ideas (future phase):**
- Record APVTS parameter changes: transpose, scale preset, per-voice octave, trigger source selection.
- Real-time visual feedback during playback showing current parameter values animating to recorded values.
- These require a new `LooperEvent::Type::Parameter` variant and APVTS write-back mechanism — out of scope for Phase 05.

---

## C. DAW Sync Handshake

**Decisions:**
- **Sync toggle:** A UI button `[SYNC]` switches between two modes:
  - **SYNC OFF (default):** Looper uses its own internal beat clock. Completely independent of DAW transport. Good for standalone jamming.
  - **SYNC ON:** Looper derives beat position from DAW `ppqPosition`. Looper start/stop follows DAW transport.
- **Start alignment (SYNC ON):** When the looper starts while SYNC is ON and the DAW is playing, the looper playhead snaps to the nearest bar boundary in the DAW timeline (floor of `ppqPosition / beatsPerBar * beatsPerBar`). This ensures the loop always aligns to bars 1, 5, 9, etc.
- **Clock handover:** No automatic handover between internal and DAW clock mid-loop. If the user starts the DAW while the looper is running on internal clock, nothing changes until the looper is manually stopped and restarted.
- **DAW MIDI capture:** The plugin always outputs MIDI (ChordJoystick is a MIDI generator). The DAW captures this MIDI only when:
  1. The DAW track's record arm is active, AND
  2. SYNC is ON in the plugin, AND
  3. The plugin is in playback (not just record-armed internally).
  This is standard MIDI passthrough — no special plugin logic required beyond normal output. The sync + record arm combination is what the user uses to control when the DAW "sees" intentional looped output.

---

## D. Validation Plan

**Done criteria for Phase 05 to be considered complete:**
1. `std::mutex` removed from the audio-thread call stack (no mutex in `process()`, `recordJoystick()`, `recordGate()`).
2. Catch2 suite green:
   - Multi-threaded stress test: one write thread + one read thread hammering the FIFO for 5 seconds — no data corruption, no deadlock.
   - Loop-wrap unit tests: `process()` called with mock `ProcessParams` for every bar length 1–16, all time signatures, verifying events fire in the correct beat windows including wrap-around.
3. Reaper: punch-in, sync toggle, 4/4 4-bar loop works end-to-end.
4. Ableton: same 4-bar test passes (Ableton's MIDI routing verified separately).

**Mutex enforcement approach:** ThreadSanitizer (`-fsanitize=thread`) in CMake `Debug` configuration. Add a `CMakeLists.txt` option `ENABLE_TSAN=ON/OFF` (default ON for Debug). On MSVC (Windows), where TSAN is unavailable, fall back to a `JUCE_ASSERT_IS_NOT_AUDIO_THREAD` custom macro at the top of `process()` that fires in Debug builds if the mutex is re-introduced.

---

## Deferred Ideas (Captured, Not Planned)

| Idea | Notes |
|------|-------|
| Parameter automation recording | Record APVTS params (transpose, scale, octave, trigger source) into the looper loop |
| Real-time parameter visual feedback | UI knobs/selectors animate to recorded values during playback |
| Overdub / layering mode | Stack multiple recording passes without wiping previous content |
| Per-voice looper enable | Lock specific voices to live input while others play from loop |
