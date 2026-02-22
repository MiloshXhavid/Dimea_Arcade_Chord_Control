# Phase 05: LooperEngine Hardening and DAW Sync - Research

**Researched:** 2026-02-22
**Domain:** JUCE lock-free audio buffers (AbstractFifo), DAW sync via ppqPosition, Catch2 concurrency testing on Windows
**Confidence:** HIGH (AbstractFifo API), MEDIUM (ppqPosition patterns), HIGH (TSAN/Windows constraints)

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**A. Lock-Free Buffer Design**
- Replace `std::vector<LooperEvent>` + `std::mutex` with `juce::AbstractFifo` + fixed-size array.
- Capacity: 2 048 events (compile-time constant `LOOPER_FIFO_CAPACITY = 2048`).
- Overflow behaviour: Recording auto-stops when the configured loop length is reached (not just when the buffer fills). An `std::atomic<bool> recordingCapReached_` flag is set so the UI can show a visual indicator.
- Lock-free scope: Fully lock-free everywhere — including `hasContent()` and `deleteLoop()`. These use atomics and careful ordering rather than a mutex. The audio thread must never block.
- Concurrency model: Sequential — record pass first, then playback. No simultaneous record + playback (no overdub at the buffer level).
- Mutex enforcement: Enable `-fsanitize=thread` in CMake Debug builds. If TSAN is unavailable on Windows, use `JUCE_ASSERT_IS_NOT_AUDIO_THREAD` custom macro at the top of `process()` as a secondary check.

**B. Recording Behaviour**
- Punch-in mode: new events replace old events at the beat positions touched by the new pass. Beat positions not touched are untouched.
- Joystick recording: sparse — only record when axis moves beyond threshold (~0.02 normalised).
- Live input during playback: always passes through to MIDI output.
- Record-type selection (UI): [REC JOY] and [REC GATES] toggle buttons; both can be active simultaneously.

**C. DAW Sync Handshake**
- Sync toggle: [SYNC] button — SYNC OFF = internal clock, SYNC ON = DAW ppqPosition.
- Start alignment (SYNC ON): snap to nearest bar boundary (floor of `ppqPosition / beatsPerBar * beatsPerBar`).
- Clock handover: no automatic handover — manual stop/restart required.
- DAW MIDI capture: standard MIDI passthrough, no special plugin logic required.

**D. Validation Plan**
- `std::mutex` removed from audio-thread call stack entirely.
- Catch2 suite green: multi-threaded FIFO stress test (5 sec, one writer + one reader), loop-wrap unit tests (1–16 bars, all time sigs).
- Reaper: punch-in, sync toggle, 4/4 4-bar loop end-to-end.
- Ableton: same 4-bar test.
- ThreadSanitizer in Debug config; MSVC fallback = custom macro.

### Claude's Discretion
- Internal clock implementation (any approach that is lock-free and accurate)
- Exact Catch2 test structure for multi-thread stress (pattern selection)
- ppqPosition anchor calculation detail (modular arithmetic approach)

### Deferred Ideas (OUT OF SCOPE)
- Record APVTS parameter changes (transpose, scale preset, per-voice octave, trigger source selection)
- Real-time visual feedback during playback showing parameter values animating to recorded values
- Overdub / layering mode
- Per-voice looper enable
</user_constraints>

---

## Summary

The current LooperEngine uses `std::mutex` in `process()`, `recordJoystick()`, `recordGate()`, `hasContent()`, `reset()`, and `deleteLoop()`. This is a real-time audio violation — any of these locks can block the audio thread indefinitely if the UI thread holds the mutex. The fix replaces `std::vector` + mutex with `juce::AbstractFifo` managing a fixed-size `std::array<LooperEvent, 2048>`. AbstractFifo is JUCE's built-in SPSC (single-producer, single-consumer) lock-free FIFO; it uses atomic indices internally and is safe for one writer thread + one reader thread with no synchronization primitives.

The punch-in design requires a two-phase approach: during a punch-in pass, the audio thread both writes new events to the buffer AND must mark old events at the same beat positions as superseded. Since the sequential model (no simultaneous record+play) is locked, the safest implementation is: on punch-in arm, the audio thread drains all existing events from the FIFO, keeps events outside the punch zone, and reinserts them alongside the new recording. This is safe because punch-in only activates when playback stops (no concurrent reader), making it a single-threaded operation on the buffer at that moment.

For DAW sync, `ppqPosition` from `AudioPlayHead::getPosition()` (JUCE 8 API) returns `Optional<double>` and is the only reliable beat source during DAW playback. A critical finding is that ppqPosition can be negative (Cubase pre-roll), discontinuous at loop wrap, and unreliable at transport start. The anchor pattern — storing `loopStartPpq_` at the moment recording begins — is the standard defensive approach, combined with a guard that ignores the first block if ppqPosition is negative. ThreadSanitizer (`-fsanitize=thread`) is **not available on Windows with MSVC**; the fallback must be a custom assertion macro checked in Debug builds.

**Primary recommendation:** Use `juce::AbstractFifo` + `std::array<LooperEvent, 2048>` with the ScopedWrite/ScopedRead RAII API. Implement destructive operations (reset, delete) via `std::atomic<bool>` request flags polled at the start of `process()`. Never call `AbstractFifo::reset()` from a non-audio thread while the audio thread may be reading or writing.

---

## Standard Stack

### Core

| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| juce::AbstractFifo | JUCE 8.0.4 (already in project) | Lock-free SPSC FIFO index manager | JUCE's canonical inter-thread buffer; no external deps; atomic internally |
| std::array<LooperEvent, 2048> | C++17 stdlib | Fixed-size event storage (no heap alloc) | Zero allocation on audio thread; cache-friendly contiguous memory |
| std::atomic<bool> | C++17 stdlib | Flags: playing, recording, deleteRequest, resetRequest, recJoy, recGates, capReached | Lock-free on all modern targets; guaranteed by C++17 spec |
| std::atomic<double> | C++17 stdlib | playbackBeat_ for UI read, loopStartPpq_ anchor | Note: double atomic is NOT lock-free on all platforms — see pitfalls |
| juce::AudioPlayHead::PositionInfo | JUCE 8.0.4 | DAW ppqPosition, bpm, isPlaying, timeSignature | Modern JUCE 8 API replacing deprecated CurrentPositionInfo |

### Supporting

| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| Catch2 | v3.8.1 (already in project) | Multi-threaded FIFO stress test + loop-wrap unit tests | All unit tests for LooperEngine |
| std::atomic<int> | C++17 | writeCount_ / readCount_ atomic counters for test verification | Only in test code to validate FIFO correctness |
| std::thread | C++17 | Writer thread in Catch2 stress test | Stress test only — never in production audio code |

### Alternatives Considered

| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| juce::AbstractFifo | moodycamel::ReaderWriterQueue | Higher performance, header-only, no JUCE dep — but adds external dependency when AbstractFifo already exists in project |
| std::atomic<double> | std::atomic<float> for playbackBeat_ | float is always lock-free; double is not guaranteed on 32-bit; prefer float if sub-beat precision is acceptable for UI display |
| Custom punch-in logic | Separate "incoming" and "stored" buffers with double-buffer swap | Simpler atomicity — but doubles memory usage and requires an extra copy; overkill for sequential record/play model |

**No additional installation needed** — AbstractFifo is in `juce_core` (already linked). Catch2 already in project via FetchContent.

---

## Architecture Patterns

### Recommended LooperEngine Data Layout

```
class LooperEngine
{
    // ── Immutable after construction ──────────────────────────────
    static constexpr int LOOPER_FIFO_CAPACITY = 2048;
    std::array<LooperEvent, LOOPER_FIFO_CAPACITY> eventBuf_;
    juce::AbstractFifo fifo_ { LOOPER_FIFO_CAPACITY };

    // ── Atomic state flags (audio + UI threads) ───────────────────
    std::atomic<bool> playing_         { false };
    std::atomic<bool> recording_       { false };
    std::atomic<bool> recJoy_          { false };  // [REC JOY] armed
    std::atomic<bool> recGates_        { false };  // [REC GATES] armed
    std::atomic<bool> syncToDaw_       { false };  // [SYNC] toggle
    std::atomic<bool> capReached_      { false };  // UI indicator

    // ── Destructive op requests (UI sets, audio thread executes) ──
    std::atomic<bool> deleteRequest_   { false };
    std::atomic<bool> resetRequest_    { false };

    // ── Internal clock (audio thread only — no atomic needed) ─────
    double internalBeat_               { 0.0 };
    double loopStartPpq_               { -1.0 };  // anchor, audio-thread only

    // ── UI read-out (atomic double — see pitfall note) ────────────
    std::atomic<double> playbackBeat_  { 0.0 };

    // ── Config (atomics for UI thread writes) ─────────────────────
    std::atomic<int> subdiv_           { (int)LooperSubdiv::FourFour };
    std::atomic<int> loopBars_         { 2 };
    std::atomic<float> joystickThresh_ { 0.02f };
};
```

### Pattern 1: AbstractFifo Write (Audio Thread — Recording)

**What:** Writer calls `fifo_.write(N)` to get a scoped write object, then uses `forEach` to fill slots. The scoped object's destructor calls `finishedWrite()` automatically.

**When to use:** `recordJoystick()` and `recordGate()` — called from processBlock on the audio thread.

```cpp
// Source: JUCE docs.juce.com/master/classAbstractFifo.html
void LooperEngine::recordGate(double beatPos, int voice, bool on)
{
    if (!recording_.load(std::memory_order_relaxed)) return;
    if (!recGates_.load(std::memory_order_relaxed)) return;
    const double loopLen = getLoopLengthBeats();
    if (loopLen <= 0.0) return;

    // Check cap BEFORE writing
    if (fifo_.getFreeSpace() < 1)
    {
        capReached_.store(true, std::memory_order_relaxed);
        return;
    }

    const double pos = std::fmod(beatPos, loopLen);
    LooperEvent ev { pos, LooperEvent::Type::Gate, voice, on ? 1.0f : 0.0f };

    auto scope = fifo_.write(1);
    scope.forEach([&](int index)
    {
        eventBuf_[index] = ev;
    });
    // scope destructor calls finishedWrite(1)
}
```

### Pattern 2: AbstractFifo Read (Audio Thread — Playback)

**What:** The reader scans ALL ready events looking for those in the current beat window. Since SPSC (audio = only reader in playback mode), no concurrent writer exists during playback.

**Important:** AbstractFifo is a queue — you cannot random-access it. In playback-only mode, you must read all `getNumReady()` events, check each one for the beat window, and then "re-queue" them if they need to fire again next loop cycle. The standard looper pattern with a fixed-slot buffer is actually NOT to use AbstractFifo as a queue at all, but as a fixed-size-bounded circular event store where you snapshot the full contents each process call.

**The correct pattern for a looper** (scan all events, fire those in window):

```cpp
// Pattern: Snapshot-scan. Audio thread is the only thread touching the
// buffer in playback mode (sequential model — no concurrent recording).
// We read all events from the fifo, check each, and write them back.
// This is safe because: recording_=false during playback => no writer.

LooperEngine::BlockOutput LooperEngine::process(const ProcessParams& p)
{
    BlockOutput out{};

    // Service destructive requests first
    if (resetRequest_.exchange(false, std::memory_order_acq_rel))
    {
        fifo_.reset();  // safe: audio thread is sole user when reset is requested
        internalBeat_  = 0.0;
        loopStartPpq_  = -1.0;
        playing_.store(false, std::memory_order_relaxed);
        recording_.store(false, std::memory_order_relaxed);
        playbackBeat_.store(0.0, std::memory_order_relaxed);
        return out;
    }
    if (deleteRequest_.exchange(false, std::memory_order_acq_rel))
    {
        fifo_.reset();  // wipes indices — fixed array stays allocated
        return out;
    }

    if (!playing_.load(std::memory_order_relaxed)) return out;
    // ... beat window calculation, then scan fifo ...
}
```

### Pattern 3: Destructive Ops via Atomic Request Flags

**What:** UI thread sets `deleteRequest_` or `resetRequest_` atomically. Audio thread polls these at the top of `process()` and executes the destructive op in its own thread context, where it is the sole user of the FIFO.

**Why:** `AbstractFifo::reset()` is documented as NOT thread-safe if another thread is concurrently reading/writing. By executing it from the audio thread (which is the only FIFO user), we avoid the race entirely.

```cpp
// UI thread (e.g., button callback):
void LooperEngine::deleteLoop()
{
    deleteRequest_.store(true, std::memory_order_release);
    // Audio thread will drain at next process() call
}

void LooperEngine::reset()
{
    resetRequest_.store(true, std::memory_order_release);
}
```

### Pattern 4: ppqPosition Anchor for DAW Sync

**What:** Record the absolute ppqPosition at loop-start. All subsequent beat calculations are relative to this anchor. This prevents drift from floating-point accumulation.

**When to use:** When SYNC ON and looper transitions from stopped to playing.

```cpp
// Source: JUCE forum PPQ discussion + official PositionInfo docs
// Called from process() when syncToDaw_=true and looper just started

void LooperEngine::anchorToBar(double ppqPosition, double beatsPerBar)
{
    // Snap to nearest bar boundary (floor)
    const double barIndex = std::floor(ppqPosition / beatsPerBar);
    loopStartPpq_ = barIndex * beatsPerBar;
}

// In process(), DAW-sync beat calculation:
double currentBeat = 0.0;
if (syncToDaw_.load() && p.isDawPlaying && p.ppqPosition >= 0.0)
{
    if (loopStartPpq_ < 0.0)
        anchorToBar(p.ppqPosition, beatsPerBar());  // first block

    const double elapsed = p.ppqPosition - loopStartPpq_;
    currentBeat = std::fmod(elapsed < 0.0 ? 0.0 : elapsed, loopLen);
}
else
{
    currentBeat = std::fmod(internalBeat_, loopLen);
}
```

### Pattern 5: Sparse Joystick Recording with Threshold

**What:** Only record a new joystick event when the axis delta exceeds threshold since the last recorded value.

```cpp
// Audio thread — called with raw joystick values
void LooperEngine::maybeRecordJoystick(double beatPos, float x, float y)
{
    const float thresh = joystickThresh_.load(std::memory_order_relaxed);
    const bool xMoved = std::abs(x - lastRecordedX_) > thresh;
    const bool yMoved = std::abs(y - lastRecordedY_) > thresh;
    if (!xMoved && !yMoved) return;

    if (xMoved) { recordAxis(beatPos, LooperEvent::Type::JoystickX, x); lastRecordedX_ = x; }
    if (yMoved) { recordAxis(beatPos, LooperEvent::Type::JoystickY, y); lastRecordedY_ = y; }
}
// lastRecordedX_ and lastRecordedY_ are audio-thread-local (plain float, no atomic needed)
```

### Pattern 6: hasContent() Without Mutex

**What:** `hasContent()` reads `fifo_.getNumReady()` which is implemented with atomic reads internally.

```cpp
// Source: AbstractFifo thread safety analysis (JUCE forum, docs.juce.com)
bool LooperEngine::hasContent() const
{
    return fifo_.getNumReady() > 0;
}
// getNumReady() reads validEnd and validStart atomics — safe from any thread.
// No mutex required.
```

### Pattern 7: Catch2 Multi-Thread Stress Test (Correct Pattern)

**What:** Catch2 assertions are NOT thread-safe from secondary threads. The correct pattern is: spawn threads, collect pass/fail into shared atomics or flags, then assert from main thread after join.

```cpp
// Source: Catch2 known limitations (github.com/catchorg/Catch2)
TEST_CASE("LooperEngine FIFO stress — 5 second writer/reader", "[looper][threadsafe]")
{
    LooperEngine eng;
    std::atomic<int>  writtenCount { 0 };
    std::atomic<int>  readCount    { 0 };
    std::atomic<bool> errorDetected { false };

    auto writerFn = [&]() {
        for (int i = 0; i < 10000; ++i) {
            eng.recordGate(i * 0.01, i % 4, (i % 2) == 0);
            writtenCount.fetch_add(1, std::memory_order_relaxed);
        }
    };

    auto readerFn = [&]() {
        LooperEngine::ProcessParams p { 44100, 120.0, -1.0, 512, false };
        for (int i = 0; i < 200; ++i) {
            auto out = eng.process(p);
            readCount.fetch_add(1, std::memory_order_relaxed);
            (void)out;
        }
    };

    std::thread writer(writerFn);
    std::thread reader(readerFn);
    writer.join();
    reader.join();

    // Assert only from main thread after join
    CHECK_FALSE(errorDetected.load());
    CHECK(readCount.load() == 200);
    // No deadlock = success (test would hang otherwise)
}
```

### Anti-Patterns to Avoid

- **Calling AbstractFifo::reset() from UI thread while audio thread is active:** `reset()` is documented as not thread-safe. Always use the atomic-flag request pattern (Pattern 3).
- **Using std::atomic<double> assuming lock-free:** On 32-bit targets `std::atomic<double>` may use a lock. Check `playbackBeat_.is_lock_free()` in debug. Consider `std::atomic<float>` instead if sub-beat UI resolution is acceptable.
- **Using CHECK/REQUIRE from secondary threads in Catch2:** Catch2 assertion state is not thread-safe. All assertions must be in the main test thread after joining worker threads.
- **Assuming ppqPosition is non-negative at transport start:** Cubase and some other DAWs report negative ppqPosition during pre-roll. Guard with `p.ppqPosition >= 0.0` before using as anchor.
- **Simultaneous record and process during punch-in:** The sequential model locks this out. Punch-in must only activate when recording is stopped (playing_=false at that moment). Attempting to punch-in while playing violates SPSC guarantee.
- **Calling events_.push_back() on audio thread:** The old implementation allocated heap memory on the audio thread. This is eliminated by using fixed `std::array` — never resize the array.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| SPSC FIFO index management | Custom atomic head/tail with manual memory ordering | `juce::AbstractFifo` | AbstractFifo handles the two-block wrap-around case, memory ordering, and edge cases; rolling this incorrectly creates subtle ABA problems |
| Thread-safe beat clock for UI display | Mutex-protected double | `std::atomic<double>` or `std::atomic<float>` for playbackBeat_ | Already in APVTS-style pattern; atomic stores are wait-free on modern x86 |
| Data race detection on Windows | Custom instrumentation | Debug-mode `jassert(!juce::MessageManager::existsAndIsCurrentThread())` at top of process() | TSAN not available on MSVC/Windows — this assertion catches the most common wrong-thread call pattern during development |
| ppqPosition → beat-within-loop | Accumulator-only approach | Anchor-based: `elapsed = ppq - loopStartPpq_` then `fmod(elapsed, loopLen)` | Accumulator drifts over long sessions; anchor-relative calculation stays bounded to `[0, loopLen)` |

**Key insight:** The hardest correctness problem in this phase is not the FIFO itself but the destructive operations (reset, delete) and punch-in. Both are correctly solved by serialising them through the audio thread via atomic flags, not by adding a mutex back.

---

## Common Pitfalls

### Pitfall 1: AbstractFifo::reset() Called From Wrong Thread

**What goes wrong:** `reset()` clears the internal indices non-atomically. If the audio thread calls `write()` concurrently, you get corrupted state — the FIFO appears to have space that isn't there, or valid events get overwritten.

**Why it happens:** UI button handlers (stop, delete) call reset() directly.

**How to avoid:** Set `deleteRequest_.store(true)` from UI thread. Audio thread polls at top of `process()` and calls `fifo_.reset()` only when it is the sole active user (playing_=false at that point).

**Warning signs:** Random FIFO state corruption under stress test — read count doesn't match write count.

### Pitfall 2: std::atomic<double> Not Lock-Free on 32-bit / Some MSVC Targets

**What goes wrong:** `playbackBeat_` uses `std::atomic<double>`. On 32-bit x86 (or some MSVC targets), `sizeof(double) > sizeof(void*)` means the atomic is implemented with a lock — violating the lock-free requirement.

**Why it happens:** The C++ standard guarantees `is_lock_free()` only for `std::atomic<int>` and types ≤ pointer size.

**How to avoid:** Use `std::atomic<float>` for `playbackBeat_` (single-beat precision is sufficient for UI progress display). Alternatively, check `playbackBeat_.is_lock_free()` in a Debug jassert at startup.

**Warning signs:** `is_lock_free()` returns false at runtime.

### Pitfall 3: ppqPosition Negative or Discontinuous at Transport Start

**What goes wrong:** Anchoring `loopStartPpq_` on the first process block gives a negative value (Cubase pre-roll) or stale zero (host hasn't updated yet). All subsequent elapsed calculations produce garbage.

**Why it happens:** DAWs may call processBlock from a "pre-roll" position before the visible playhead. Cubase explicitly sends negative ppqPosition as a feature. Some DAWs return 0.0 on the very first block regardless of transport position.

**How to avoid:** Guard: `if (p.ppqPosition < 0.0) return out;` before any anchor assignment. Only set `loopStartPpq_` when `ppqPosition >= 0.0` AND `isDawPlaying == true`.

**Warning signs:** Looper appears to start one or two bars early in Cubase; events fire at wrong positions in first loop cycle.

### Pitfall 4: Catch2 Assertions from Writer Thread Crash the Test Runner

**What goes wrong:** Using `CHECK(fifo.getFreeSpace() > 0)` inside the writer lambda causes a data race on Catch2's internal assertion state — process crash or silent incorrect results.

**Why it happens:** Catch2 keeps test execution serial; its assertion macro state is not thread-safe.

**How to avoid:** Collect results in `std::atomic<bool> errorDetected`. Only call `CHECK` / `REQUIRE` from the main test thread after `writer.join()` and `reader.join()`.

**Warning signs:** Test runner crashes with access violation; or test passes even when FIFO is corrupted.

### Pitfall 5: Punch-In Race — Recording While Playing

**What goes wrong:** If `recording_` is set to true while `playing_` is also true (allowing a concurrent reader + writer on the FIFO), AbstractFifo's SPSC guarantee is violated because both the record path (writer) and playback scan path (reader) are active simultaneously.

**Why it happens:** Attempting an overdub-like behavior or calling `record()` mid-playback.

**How to avoid:** The locked decision is "sequential: record first, then playback" — enforce this: `record()` is only callable when `!playing_.load()`. Assert this in the UI button handler and in `process()`.

**Warning signs:** Events appear duplicated or at wrong positions after punch-in.

### Pitfall 6: ThreadSanitizer Unavailable on MSVC/Windows

**What goes wrong:** CMake option `ENABLE_TSAN=ON` passes `-fsanitize=thread` to MSVC, which produces a compile error (`/fsanitize:thread is not supported`). Developer assumes TSAN is working but it is silently ignored or fails.

**Why it happens:** TSan is only supported on Linux (Clang/GCC). MSVC and clang-cl on Windows do not support it. Intel Inspector (the Windows alternative) reached end-of-life in late 2024.

**How to avoid:**
1. In CMakeLists.txt, gate TSAN behind a `NOT MSVC` check.
2. Implement the fallback macro: a Debug-mode `jassert` that checks the calling thread is NOT the message thread (the main/UI thread calling into audio-thread-only functions is the most common mistake, not the reverse).
3. The stress test with `std::thread` under Valgrind on WSL2 is an option if deeper validation is needed (medium complexity).

**Warning signs:** `target_compile_options` for `-fsanitize=thread` causes build error on MSVC.

---

## Code Examples

### Example 1: Complete AbstractFifo Write Pattern

```cpp
// Source: docs.juce.com/master/classAbstractFifo.html (ScopedReadWrite API)
// Safe to call from audio thread (writer). Returns false if buffer full.
bool LooperEngine::writeSingleEvent(const LooperEvent& ev)
{
    if (fifo_.getFreeSpace() < 1)
        return false;

    auto scope = fifo_.write(1);  // ScopedWrite RAII
    scope.forEach([&](int idx)
    {
        eventBuf_[idx] = ev;
    });
    // scope destructor calls finishedWrite(1)
    return true;
}
```

### Example 2: Full Playback Scan (SPSC — audio thread only reader in playback)

```cpp
// Source: AbstractFifo JUCE forum + JUCE docs
// During playback (recording_=false), audio thread is sole FIFO user.
// We must snapshot-scan: read all, check window, write back unmatched.
// Simplified here — full implementation accounts for punch-in clear.

LooperEngine::BlockOutput LooperEngine::scanEvents(
    double beatStart, double beatEnd, double loopLen)
{
    BlockOutput out{};
    const bool wraps = beatEnd < beatStart;
    const int numReady = fifo_.getNumReady();

    // Temp holding for events that don't fire this block
    // (stack allocation — fixed max size, no heap)
    LooperEvent tempBuf[LOOPER_FIFO_CAPACITY];
    int tempCount = 0;

    {
        auto scope = fifo_.read(numReady);
        scope.forEach([&](int idx)
        {
            const auto& ev = eventBuf_[idx];
            bool inWindow = wraps
                ? (ev.beatPosition >= beatStart || ev.beatPosition < beatEnd)
                : (ev.beatPosition >= beatStart && ev.beatPosition < beatEnd);

            if (inWindow)
                applyEvent(ev, out);
            else
                tempBuf[tempCount++] = ev;  // keep for next cycle
        });
    }  // finishedRead() called here

    // Re-insert non-fired events
    if (tempCount > 0)
    {
        auto scope = fifo_.write(tempCount);
        int i = 0;
        scope.forEach([&](int idx) { eventBuf_[idx] = tempBuf[i++]; });
    }

    return out;
}
```

**IMPORTANT DESIGN NOTE:** The snapshot-scan-reinsert pattern is the correct approach for a looper (as opposed to a one-shot event queue) but it is not simple. An alternative that avoids the complexity is to NOT use AbstractFifo as a queue at all for playback, but instead use it only for the RECORD side (audio thread writes events in), and for playback, maintain a separate `std::array` of events that is swapped in from the FIFO when recording stops. This "double-array" approach decouples the record queue from the playback store.

### Example 3: AudioPlayHead ppqPosition (JUCE 8 API)

```cpp
// Source: docs.juce.com/master/classAudioPlayHead.html
// Source: docs.juce.com/master/classAudioPlayHead_1_1PositionInfo.html
// Called at top of processBlock

double LooperEngine::getPpqPosition(juce::AudioPlayHead* head, bool& isDawPlaying)
{
    isDawPlaying = false;
    if (head == nullptr) return -1.0;

    auto posInfo = head->getPosition();  // Returns juce::Optional<PositionInfo>
    if (!posInfo.hasValue()) return -1.0;

    isDawPlaying = posInfo->getIsPlaying();

    auto ppq = posInfo->getPpqPosition();  // Returns Optional<double>
    if (!ppq.hasValue()) return -1.0;

    return *ppq;
}

// Time signature access:
auto ts = posInfo->getTimeSignature();  // Optional<TimeSignature>
int numerator   = ts.hasValue() ? ts->numerator   : 4;
int denominator = ts.hasValue() ? ts->denominator : 4;
```

### Example 4: CMakeLists.txt TSAN Option (Windows-safe)

```cmake
# Source: Microsoft ASAN docs + Google sanitizers project
option(ENABLE_TSAN "Enable ThreadSanitizer (Linux/Clang only)" OFF)

if(ENABLE_TSAN AND NOT MSVC AND CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(ChordJoystickTests PRIVATE -fsanitize=thread -g)
    target_link_options(ChordJoystickTests PRIVATE -fsanitize=thread)
    message(STATUS "ThreadSanitizer enabled for tests")
elseif(ENABLE_TSAN AND MSVC)
    message(WARNING "ENABLE_TSAN=ON ignored on MSVC — ThreadSanitizer not supported on Windows")
endif()
```

### Example 5: Thread-Guard Macro (MSVC Fallback for TSAN)

```cpp
// In LooperEngine.h
#if JUCE_DEBUG
  // Assert we are NOT on the JUCE message thread
  // (Audio thread must not be the message thread)
  #define ASSERT_AUDIO_THREAD() \
      jassert(!juce::MessageManager::existsAndIsCurrentThread())
#else
  #define ASSERT_AUDIO_THREAD() ((void)0)
#endif

// In LooperEngine::process() — first line:
ASSERT_AUDIO_THREAD();
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `std::vector` + `std::mutex` in processBlock | `juce::AbstractFifo` + `std::array` | This phase | Eliminates blocking on audio thread; prevents priority inversion |
| `AudioPlayHead::getCurrentPosition()` (fills CurrentPositionInfo) | `AudioPlayHead::getPosition()` returning `Optional<PositionInfo>` | JUCE 7 (deprecated old API) | Forces explicit null-checking; `PositionInfo` getters return Optional<T> for fields not provided by all hosts |
| Intel Inspector for data race detection on Windows | Intel Inspector reached end-of-life Oct 2024 | Oct 2024 | No free GUI race detector on Windows; must use TSan on Linux/WSL2 or Debug-mode assertions |
| `AbstractFifo::prepareToWrite/finishedWrite` (manual) | `fifo_.write(N)` returning `ScopedWrite` with RAII `forEach` | JUCE 5+ | Eliminates forgotten `finishedWrite()` calls (a common bug) |

**Deprecated/outdated:**
- `AudioPlayHead::getCurrentPosition(bool&)` — deprecated since JUCE 7, removed/discouraged in JUCE 8. Use `getPosition()` returning `Optional<PositionInfo>`.
- Intel Inspector — end-of-life October 2024, downloads removed as of January 2026.
- Raw `prepareToWrite/finishedWrite` pattern — still works but ScopedWrite/forEach is the recommended API in JUCE 7+.

---

## Open Questions

1. **Snapshot-scan vs double-buffer for playback**
   - What we know: AbstractFifo is a queue, not a random-access store. Using it for looper playback (scan all, re-insert non-fired) is functional but adds complexity.
   - What's unclear: Whether the snapshot-scan-reinsert pattern introduces any correctness issues under the sequential model at loop wrap boundaries.
   - Recommendation: Use the double-array design: AbstractFifo for recording (producer=audio thread during record), and a separate `std::array<LooperEvent, 2048> playbackStore_` + `std::atomic<int> playbackCount_` that the audio thread copies the FIFO into when recording stops. Playback scans `playbackStore_` directly (no FIFO involved). This is the cleanest separation and matches the sequential model exactly.

2. **std::atomic<double> lock-freedom on the project's target (Windows 64-bit)**
   - What we know: On 64-bit Windows with MSVC, `std::atomic<double>` is typically lock-free (8-byte aligned, single `CMPXCHG8B` or SSE instruction). Not guaranteed by the standard.
   - What's unclear: Whether VS 18 2026 toolchain guarantees this.
   - Recommendation: Add `static_assert(std::atomic<double>{}.is_lock_free(), "atomic<double> must be lock-free")` to LooperEngine.cpp. If it fails, switch to `std::atomic<float>`.

3. **Ableton crash on plugin instantiation (known blocker from Phase 01)**
   - What we know: Plugin crashes in Ableton on instantiation. Root cause unresolved since Phase 01.
   - What's unclear: Whether Phase 05's changes affect this (LooperEngine is not the likely cause, but DAW sync testing requires Ableton to load).
   - Recommendation: Validate Ableton in Phase 05 only after Reaper passes. If Ableton still crashes, defer Ableton DAW sync validation to Phase 06 and document the blocker. Do not block Phase 05 completion on Ableton if Reaper passes.

4. **Punch-in implementation complexity**
   - What we know: The punch-in requirement says "new events replace old at touched beat positions." With a FIFO-based store, "replace at position" requires clearing old events at those beat positions.
   - What's unclear: How to identify "touched beat positions" precisely given block-granular recording.
   - Recommendation: Define "touched" as any beat position within a `±(blockBeats/2)` window of a newly recorded event. During punch-in, before writing new events, scan the playback store and remove (tombstone) events at matching positions. This is safe because punch-in only activates when playback is stopped (audio thread is sole FIFO user).

---

## Sources

### Primary (HIGH confidence)
- [docs.juce.com — AbstractFifo Class Reference](http://docs.juce.com/master/classAbstractFifo.html) — API: constructor, prepareToWrite/finishedWrite, ScopedWrite/ScopedRead, reset() thread-safety note
- [docs.juce.com — AbstractFifo::ScopedReadWrite](https://docs.juce.com/master/classAbstractFifo_1_1ScopedReadWrite.html) — forEach API, RAII destructor behavior, startIndex1/2, blockSize1/2
- [docs.juce.com — AudioPlayHead](https://docs.juce.com/master/classAudioPlayHead.html) — getPosition() vs deprecated getCurrentPosition, Optional<PositionInfo>
- [docs.juce.com — AudioPlayHead::PositionInfo](https://docs.juce.com/master/classAudioPlayHead_1_1PositionInfo.html) — getPpqPosition() Optional<double>, getIsPlaying() bool, getTimeSignature() Optional<TimeSignature>
- [github.com/juce-framework/JUCE — juce_AbstractFifo.h](https://github.com/juce-framework/JUCE/blob/master/modules/juce_core/containers/juce_AbstractFifo.h) — ScopedRead/ScopedWrite type aliases, read()/write() factory methods

### Secondary (MEDIUM confidence)
- [JUCE Forum — AbstractFifo thread safety (SPSC)](https://forum.juce.com/t/abstractfifo-single-consumer-single-producer-thread-safety/50749) — confirmed SPSC guarantee, finishedRead advancement behavior, multi-consumer limitation
- [JUCE Forum — PPQ Position and transport start](https://forum.juce.com/t/ppq-position-and-actual-transport-start-point/65933) — negative ppqPosition (Cubase pre-roll), sample-counting reliability, loop wrap detection patterns
- [timur.audio — Using locks in real-time audio processing safely](https://timur.audio/using-locks-in-real-time-audio-processing-safely) — try_lock() risks, SPSC FIFO pattern, immutable data structures for UI→audio
- [Google sanitizers / ThreadSanitizerCppManual](https://github.com/google/sanitizers/wiki/ThreadSanitizerCppManual) — TSAN Linux-only, no Windows support
- [Intel Inspector end-of-life announcement](https://www.intel.com/content/www/us/en/developer/tools/oneapi/inspector.html) — final version Oct 2024, downloads removed Jan 2026
- [Microsoft — ASAN for MSVC](https://devblogs.microsoft.com/cppblog/address-sanitizer-for-msvc-now-generally-available/) — ASAN available on MSVC, TSAN is NOT

### Tertiary (LOW confidence — from WebSearch, not directly verified against official source)
- [Catch2 GitHub issue #99](https://github.com/catchorg/Catch2/issues/99) — REQUIRE not thread-safe; confirmed in multiple issues but thread-safety.md not directly fetched
- [Catch2 GitHub issue #246](https://github.com/catchorg/Catch2/issues/246) — serial test execution confirmed; secondary-thread assertion workaround pattern

---

## Metadata

**Confidence breakdown:**
- Standard stack (AbstractFifo API): HIGH — directly verified against JUCE official docs and source
- Architecture patterns (FIFO write/read, atomic flags): HIGH — derived from official API docs + well-established audio thread patterns
- ppqPosition anchor/drift: MEDIUM — JUCE forum discussion verified, but negative-ppq pitfall is host-specific (confirmed for Cubase, may not apply in Reaper/Ableton)
- TSAN/Windows constraint: HIGH — confirmed from Google sanitizers project + Microsoft ASAN docs
- Catch2 threading: MEDIUM — confirmed from GitHub issues, not from official thread-safety.md directly
- Punch-in design: LOW (novel design, no prior art found in public JUCE ecosystem)
- Intel Inspector EOL: HIGH — confirmed from Intel developer site

**Research date:** 2026-02-22
**Valid until:** 2026-05-22 (AbstractFifo API stable; JUCE 8.x unlikely to break this; TSAN Windows status unlikely to change)
