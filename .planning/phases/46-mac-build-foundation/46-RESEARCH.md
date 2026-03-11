# Phase 46: Mac Build Foundation - Research

**Researched:** 2026-03-11
**Domain:** CMake/Xcode universal binary builds, JUCE AU validation, macOS audio plugin toolchain
**Confidence:** HIGH (most findings verified against official sources and JUCE forum)

---

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions
- Wipe `build-mac/` entirely and start fresh — stale CMakeCache from the failed attempt must not carry forward
- Reconfigure from scratch with `cmake -G Xcode`
- Root cause confirmed: `configure_file` for Inno Setup `.iss.in` (CMakeLists.txt lines 5-9) is NOT wrapped in `if(WIN32)` — fix this first (MAC-05)
- `CMAKE_OSX_ARCHITECTURES "arm64;x86_64"` MUST be set before the FetchContent calls (before line 19)
- Use Xcode 26.3 as-is. After build, run `lipo -info` on both binaries. If single-arch, that confirms Xcode regression and will be handled as a follow-up
- All APVTS parameters need version hints set (`setVersionHint(1)` or equivalent) before AU can pass auval with zero warnings
- Manufacturer code `MxCJ`, plugin code `DCJM` → `auval -v aumu DCJM MxCJ`
- AU main type is already `kAudioUnitType_MusicDevice` in CMakeLists.txt
- All 3 target DAWs are on this Mac: Logic Pro (AU), Reaper (VST3), Ableton Live (VST3)
- PS4/Xbox controller via USB must be detected: `SDL_NumJoysticks() > 0` and controller type label visible

### Claude's Discretion
- How to set APVTS parameter version hints (exact method/location in PluginProcessor.cpp)
- Whether DEPLOYMENT_TARGET needs to be set (e.g. macOS 11.0 minimum)
- SDL2 compile flags on Apple Silicon (should work via FetchContent with OSX_ARCHITECTURES set)

### Deferred Ideas (OUT OF SCOPE)
- None — discussion stayed within phase scope
</user_constraints>

---

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| MAC-01 | Plugin builds as universal binary (arm64 + x86_64) via `cmake -G Xcode` with both VST3 and AU targets | CMake `CMAKE_OSX_ARCHITECTURES` placement, Xcode 16.x destination flag, `lipo -info` verification |
| MAC-02 | AU plugin passes `auval` validation with zero errors (NEEDS_MIDI_OUTPUT TRUE, uppercase manufacturer code, APVTS version hints on all params) | ParameterID constructor syntax, version hint = 1 on all ~78 parameters, deployment target recommendation |
| MAC-03 | Plugin loads and produces correct MIDI in Logic Pro (AU), Reaper (VST3), and Ableton (VST3) on macOS | Logic AU MIDI output limitation clarified; Reaper/Ableton VST3 MIDI routing documented |
| MAC-04 | SDL2 gamepad (PS4/PS5/Xbox) works on macOS — controller detected, all buttons and axes functional | SDL 2.30.9 GCController regression, HIDAPI=0 hint behavior on macOS, run loop workaround |
| MAC-05 | Inno Setup configure_file in CMakeLists.txt wrapped in `if(WIN32)` — Mac `cmake` configure runs clean | Confirmed root cause; exact fix is a 2-line `if(WIN32)` wrap at lines 5-9 of CMakeLists.txt |
</phase_requirements>

---

## Summary

This phase addresses five tightly scoped build-system and validation problems. All root causes are already understood from prior investigation. The code changes are small but order-sensitive: MAC-05 (the CMake configure fix) must be applied first because the project cannot even generate an Xcode project without it. MAC-01 (universal binary flag placement) must be confirmed correct — the `CMAKE_OSX_ARCHITECTURES` setting must precede all `FetchContent_Declare` calls to ensure JUCE and SDL2 static libs are built fat. MAC-02 (auval) requires updating every parameter constructor in `createParameterLayout()` to use `juce::ParameterID { "id", 1 }` syntax rather than bare string IDs. MAC-03, MAC-04, and MAC-05 are verification and smoke test tasks once the build is clean.

One significant risk discovered in research: the project currently runs on **Xcode 26.3** (macOS 26.3.1 / Sequoia). The AU universal binary regression affects **Xcode 16.x** builds where Xcode ignores `CMAKE_OSX_ARCHITECTURES` for AU targets when build destination is "My Mac". The confirmed workaround is to set the build destination to "Any Mac" — which in `xcodebuild` command-line terms means passing `ONLY_ACTIVE_ARCH=NO`. The VST3 target is not affected by this regression.

A second risk: **SDL 2.30.9** introduced a regression where Xbox controllers on macOS require the Core Foundation run loop to be processed before `SDL_Init()`. The current `SdlContext.cpp` already disables HIDAPI (`SDL_HINT_JOYSTICK_HIDAPI=0`), which may actually help by forcing IOKit fallback for PS4 controllers — but Xbox controllers under the GCController path still need run loop processing. This is a **MAC-04 risk** to verify empirically.

**Primary recommendation:** Apply MAC-05 fix, set CMAKE_OSX_ARCHITECTURES before FetchContent, update all parameter ParameterIDs with version hint 1, build with `ONLY_ACTIVE_ARCH=NO`, verify with `lipo -info` and `auval`, then run DAW smoke tests.

---

## Standard Stack

### Core
| Library | Version | Purpose | Why Standard |
|---------|---------|---------|--------------|
| JUCE | 8.0.4 (tag) | Audio plugin framework — already in project | Project-mandated; already fetched via FetchContent |
| SDL2 | release-2.30.9 | Gamepad input — already in project | Project-mandated; already fetched via FetchContent |
| CMake | 3.22+ | Build system — already required | Project-mandated by cmake_minimum_required |
| Xcode | 26.3 (installed) | macOS build generator | Only option on this Mac |

### Supporting
| Library | Version | Purpose | When to Use |
|---------|---------|---------|-------------|
| auval / auvaltool | macOS built-in | AU plugin validation | After each AU build — macOS 15+ auval is a symlink to auvaltool |
| lipo | macOS built-in | Architecture verification | After build to confirm universal binary |
| killall | macOS built-in | Audio daemon reset before auval | `killall -9 AudioComponentRegistrar` before auval if plugin not found |

### Alternatives Considered
| Instead of | Could Use | Tradeoff |
|------------|-----------|----------|
| `cmake -G Xcode` + xcodebuild | `cmake --build` with Ninja | Ninja does not support macOS code signing and AU bundle structure needed for auval |

**Build commands:**
```bash
# Delete stale build directory first
rm -rf build-mac

# Configure (all on one line)
cmake -G Xcode -B build-mac -S . \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0

# Build Release (ONLY_ACTIVE_ARCH=NO is the Xcode 16.x universal binary workaround)
xcodebuild -project build-mac/ChordJoystick.xcodeproj \
  -scheme ChordJoystick_All \
  -configuration Release \
  ONLY_ACTIVE_ARCH=NO

# Verify architectures
lipo -info "build-mac/ChordJoystick_artefacts/Release/VST3/Arcade Chord Control (BETA-Test).vst3/Contents/MacOS/Arcade Chord Control (BETA-Test)"
lipo -info "$HOME/Library/Audio/Plug-Ins/Components/Arcade Chord Control (BETA-Test).component/Contents/MacOS/Arcade Chord Control (BETA-Test)"
```

---

## Architecture Patterns

### Recommended Project Structure
No new directories needed — all changes are to existing files:
```
CMakeLists.txt              # Fix: wrap configure_file in if(WIN32), add CMAKE_OSX_ARCHITECTURES
Source/PluginProcessor.cpp  # Fix: update all parameter constructors to use ParameterID with version hint 1
Source/SdlContext.cpp       # Review: HIDAPI hint behavior on macOS — may need platform guard
```

### Pattern 1: CMAKE_OSX_ARCHITECTURES Placement

**What:** The `CMAKE_OSX_ARCHITECTURES` variable instructs CMake and the Xcode generator to build fat binaries. It MUST be set as a cache variable before any `FetchContent_Declare` call, because FetchContent downloads and configures dependencies at configure time — if the variable is set after, the dependency libraries will be built for the host architecture only, causing a linker failure when the plugin tries to link fat JUCE/SDL2 against a fat plugin binary.

**When to use:** Always, for universal binary builds.

**Correct placement — BEFORE line 19 (FetchContent_Declare juce):**
```cmake
# After project() declaration, before include(FetchContent):
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures for macOS")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment target")
```

### Pattern 2: APVTS Parameter Version Hints

**What:** JUCE's AU wrapper uses `ParameterID::getVersionHint()` to order parameters within the AU format. Logic Pro and GarageBand sort parameters by version hint first, then by string hash. A version hint of 0 causes an assertion in Debug builds and produces auval warnings because the AU spec requires consistent parameter ordering. For the first release of a plugin, ALL parameters must use version hint `1`.

**The two mechanisms:**
1. **`juce::ParameterID { "id", 1 }` constructor** — used directly in `std::make_unique<AudioParameterFloat>(ParameterID{...}, ...)` calls
2. **Helper lambdas** — the project's `addInt`, `addFloat`, `addBool`, `addChoice` lambdas currently pass bare string IDs; they must be updated to wrap in `ParameterID`

**Exact fix for the helper lambdas in `createParameterLayout()`:**
```cpp
// Source: https://docs.juce.com/master/classParameterID.html
// Change addInt lambda from:
layout.add(std::make_unique<juce::AudioParameterInt>(id, name, min, max, def));
// To:
layout.add(std::make_unique<juce::AudioParameterInt>(
    juce::ParameterID { id, 1 }, name, min, max, def));

// Change addFloat lambda from:
layout.add(std::make_unique<juce::AudioParameterFloat>(
    id, name, juce::NormalisableRange<float>(min, max, 0.01f), def));
// To:
layout.add(std::make_unique<juce::AudioParameterFloat>(
    juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float>(min, max, 0.01f), def));

// Change addBool lambda from:
layout.add(std::make_unique<juce::AudioParameterBool>(id, name, def));
// To:
layout.add(std::make_unique<juce::AudioParameterBool>(
    juce::ParameterID { id, 1 }, name, def));

// Change addChoice lambda from:
layout.add(std::make_unique<juce::AudioParameterChoice>(id, name, choices, def));
// To:
layout.add(std::make_unique<juce::AudioParameterChoice>(
    juce::ParameterID { id, 1 }, name, choices, def));
```

**Inline `layout.add` calls (not using lambdas) — also need updating.** There are approximately 30 direct `layout.add` calls in `createParameterLayout()` that bypass the lambdas. Each one must also use `juce::ParameterID { "id", 1 }` syntax. The exception: `globalTranspose` already uses `juce::ParameterID { ParamID::globalTranspose, 1 }` (line 127) — this one is already correct and must not be double-wrapped.

**Version hint strategy for this plugin:**
- All existing parameters: version hint `1` (this is the first AU release)
- Any parameters added in future phases: version hint `2` or higher
- NEVER change version hints on shipped parameters — AU parameter ordering is stable once released

### Pattern 3: Xcode 16.x Universal Binary Workaround

**What:** Xcode 16.1+ has a confirmed regression where AU plugin targets ignore `CMAKE_OSX_ARCHITECTURES` and build only for the host architecture when the Xcode build destination is "My Mac". VST3 targets are NOT affected. The fix is to set `ONLY_ACTIVE_ARCH=NO` on the xcodebuild command line, which forces Xcode to build all declared architectures regardless of the "active" host.

**Source:** JUCE forum thread "AU universal binary macOS Sequoia and Xcode 16.2" — confirmed working fix

**Command-line equivalent:**
```bash
xcodebuild ... ONLY_ACTIVE_ARCH=NO
```

**Or via CMake build command:**
```bash
cmake --build build-mac --config Release -- ONLY_ACTIVE_ARCH=NO
```

### Anti-Patterns to Avoid
- **Setting CMAKE_OSX_ARCHITECTURES after FetchContent:** Produces single-arch JUCE/SDL2 libs, causes linker failure with "building for macOS-arm64 but attempting to link with file built for macOS-x86_64"
- **Using bare string IDs in AudioParameter constructors:** Causes auval warnings "parameter version hint not set" and may affect Logic Pro parameter ordering
- **Changing version hints on existing parameters after shipping:** Breaks Logic Pro automation recall and preset compatibility
- **Running auval without registering the plugin first:** auval won't find the plugin; copy to `~/Library/Audio/Plug-Ins/Components/` first (build system already does this via POST_BUILD command)
- **Using `killall coreaudiod` to refresh AU registry:** Unnecessary; `killall -9 AudioComponentRegistrar` is sufficient and less disruptive

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| Universal binary creation | Custom lipo merge scripts | `CMAKE_OSX_ARCHITECTURES` + Xcode generator | CMake handles fat binary generation natively; lipo-merge approach requires separate arch builds and is error-prone |
| Parameter version hints | Custom parameter ordering metadata | `juce::ParameterID { id, version }` | JUCE AU wrapper reads these directly; custom approaches bypass the framework's AU parameter table generation |
| AU plugin registration | Manual `.auinfo` files or `coreaudiod` manipulation | Build system POST_BUILD copy to `~/Library/Audio/Plug-Ins/Components/` | Already implemented in CMakeLists.txt `elseif(APPLE)` block |
| auval output parsing | Custom validation scripts | `auval -v aumu DCJM MxCJ` exit code | auval exit code 0 = pass; any other value = fail |

**Key insight:** The AU validation ecosystem is fully prescribed by Apple. Every step from parameter declaration to plugin registration to validation has an official tool. Do not invent alternatives.

---

## Common Pitfalls

### Pitfall 1: CMAKE_OSX_ARCHITECTURES Set Too Late
**What goes wrong:** Build appears to succeed but `lipo -info` shows only `arm64` (or only `x86_64`). Linker may also fail with architecture mismatch error.
**Why it happens:** FetchContent runs during CMake configure phase. If `CMAKE_OSX_ARCHITECTURES` is not set as a cache variable before `FetchContent_Declare(juce)`, JUCE's CMakeLists.txt inherits an empty architecture list and compiles for the host only. The plugin target then sets the fat arch list but cannot link against single-arch JUCE libs.
**How to avoid:** Set `CMAKE_OSX_ARCHITECTURES` as a `CACHE STRING` immediately after `project()`, before any `include(FetchContent)` or `FetchContent_Declare`.
**Warning signs:** CMake configure completes without error but build output shows only one `-arch` flag in compile commands; `lipo -info` on JUCE static lib shows single arch.

### Pitfall 2: auval Cannot Find Plugin
**What goes wrong:** `auval -v aumu DCJM MxCJ` reports "AU not found" immediately without running any tests.
**Why it happens:** The Core Audio component registry has not been updated. The `.component` bundle must be present in `~/Library/Audio/Plug-Ins/Components/` AND the system must have scanned it. The POST_BUILD copy command runs during the Xcode build, but the scanner may not have run yet.
**How to avoid:** After build, either (a) wait a few seconds for background scanner, or (b) run `killall -9 AudioComponentRegistrar` to force rescan, or (c) verify with `auval -a | grep -i chord` before running the full auval.
**Warning signs:** `auval -a` does not list the plugin even after build completes.

### Pitfall 3: Logic Pro Cannot Show MIDI Output from AU Instrument
**What goes wrong:** Logic Pro loads the AU plugin in the Software Instrument slot, but the MIDI output is not visible in Logic's MIDI monitor or cannot be routed to a MIDI track.
**Why it happens:** `kAudioUnitType_MusicDevice` (the AU type used) is an audio-generating instrument in Logic's model. Logic does not expose MIDI output from Software Instrument slots. This is a fundamental Logic AU architecture limitation — not a JUCE bug.
**How to avoid / adapt the smoke test:** The MAC-03 success criterion for Logic Pro must be interpreted as: "the AU plugin loads in Logic's Software Instrument slot without error, the plugin UI appears, and Logic routes incoming MIDI to the plugin (the plugin receives MIDI input)." MIDI output emission from Logic is NOT verifiable via the Logic MIDI monitor for AU instrument type. Reaper and Ableton (VST3) are where MIDI output is confirmed.
**Warning signs:** Attempting to see chord note output in Logic's MIDI monitor from an instrument slot will fail — this is expected behavior, not a plugin bug.

### Pitfall 4: SDL 2.30.9 Xbox Controller Not Detected on macOS
**What goes wrong:** `SDL_NumJoysticks()` returns 0 even with a USB Xbox controller connected.
**Why it happens:** SDL 2.30.9 introduced commit 63aff8e which forces Xbox controllers to use Apple's GCController framework on macOS. GCController requires the Core Foundation run loop to process at least one event before it reports connected controllers. DAW plugins run in a windowless context and do not have a dedicated CF run loop.
**How to avoid:** PS4 controllers use IOKit HIDAPI for detection and are less affected. For Xbox controllers, the existing `SDL_HINT_JOYSTICK_HIDAPI=0` hint may actually help by forcing IOKit fallback — this must be tested empirically. If Xbox detection fails, add a brief run loop pump before `SDL_Init`:
```cpp
// Source: https://github.com/libsdl-org/SDL/issues/11742
// Add to SdlContext::acquire(), Apple-platform only, before SDL_Init:
#if JUCE_MAC
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, false);
#endif
```
**Warning signs:** `SDL_NumJoysticks()` returns 0 on macOS with a connected controller; controller type label stays blank in plugin UI. Test with PS4 controller first (more reliable than Xbox on 2.30.9).

### Pitfall 5: auval Parameter Version Hint Warnings
**What goes wrong:** `auval` runs but reports warnings: "parameter X has no version hint" — causes auval to exit non-zero if warnings are treated as errors, or leaves non-zero warning count.
**Why it happens:** Any `AudioParameter` constructor that receives a bare `juce::String` (not a `juce::ParameterID`) gets version hint 0, which JUCE treats as unset.
**How to avoid:** Update ALL four helper lambdas AND all direct `layout.add()` calls that use bare string IDs. Use `juce::ParameterID { id, 1 }` universally. The `globalTranspose` parameter (line 127) already uses the correct form.
**Warning signs:** auval output includes lines mentioning "version hint" before the summary; Debug build produces `JUCE_ASSERT` failures when plugin loads in Logic.

### Pitfall 6: Stale CMakeCache from Failed Previous Build
**What goes wrong:** Even after fixing CMakeLists.txt, cmake re-reads cached values from `build-mac/CMakeCache.txt` including the empty `CMAKE_OSX_ARCHITECTURES:STRING=` from the previous failed configure.
**Why it happens:** CMake caches all variables. `-D` flags on the command line override cache, but the old empty value may conflict if the project file also sets it.
**How to avoid:** Delete `build-mac/` entirely before reconfiguring. The CONTEXT.md decision is correct: wipe and start fresh.
**Warning signs:** `cmake -G Xcode -B build-mac` completes suspiciously fast (using cached values); `build-mac/CMakeCache.txt` shows the old `CMAKE_OSX_ARCHITECTURES:STRING=` (empty).

---

## Code Examples

Verified patterns from official sources:

### MAC-05: Wrap configure_file in if(WIN32)
```cmake
# Source: confirmed CMakeLists.txt line 5-9 — add guard
if(WIN32)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/installer/DimeaArcade-ChordControl-Setup.iss.in"
        "${CMAKE_CURRENT_SOURCE_DIR}/installer/DimeaArcade-ChordControl-Setup.iss"
        @ONLY
    )
endif()
```

### MAC-01: CMAKE_OSX_ARCHITECTURES placement
```cmake
# Source: CMake documentation — set BEFORE include(FetchContent)
cmake_minimum_required(VERSION 3.22)
project(ChordJoystick VERSION 1.9.0)

# MAC: Universal binary — MUST be set before FetchContent so JUCE/SDL2 static libs are built fat
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures for macOS")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS deployment target")
endif()

# Static CRT — Windows/MSVC only
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

include(FetchContent)
# ... FetchContent_Declare(juce ...) follows
```

### MAC-02: Helper lambda version hint update pattern
```cpp
// Source: https://docs.juce.com/master/classParameterID.html
// All four helper lambdas in createParameterLayout() — add ParameterID wrapper:

auto addInt = [&](const juce::String& id, const juce::String& name,
                  int min, int max, int def)
{
    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID { id, 1 }, name, min, max, def));
};

auto addFloat = [&](const juce::String& id, const juce::String& name,
                    float min, float max, float def)
{
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { id, 1 }, name,
        juce::NormalisableRange<float>(min, max, 0.01f), def));
};

auto addBool = [&](const juce::String& id, const juce::String& name, bool def)
{
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID { id, 1 }, name, def));
};

auto addChoice = [&](const juce::String& id, const juce::String& name,
                     juce::StringArray choices, int def)
{
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID { id, 1 }, name, choices, def));
};
```

### MAC-02: Direct layout.add calls with version hint (non-lambda)
Direct calls (not using helpers) also need updating. Pattern for each type:
```cpp
// AudioParameterFloat with NormalisableRange (non-lambda):
layout.add(std::make_unique<juce::AudioParameterFloat>(
    juce::ParameterID { ParamID::joystickXAtten, 1 },
    "Joy X Attenuator",
    juce::NormalisableRange<float>(0.0f, 127.0f, 1.0f), 24.0f));

// AudioParameterChoice (non-lambda):
layout.add(std::make_unique<juce::AudioParameterChoice>(
    juce::ParameterID { "randomSubdiv0", 1 },
    "Random Subdiv Root", subdivChoices, 8));

// Already-correct example (do NOT change):
layout.add(std::make_unique<juce::AudioParameterInt>(
    juce::ParameterID { ParamID::globalTranspose, 1 },  // already correct
    "Global Transpose", 0, 11, 0,
    juce::AudioParameterIntAttributes{} ... ));
```

### MAC-02: auval invocation and expected output
```bash
# After copying .component to ~/Library/Audio/Plug-Ins/Components/
# Force AU registry rescan:
killall -9 AudioComponentRegistrar 2>/dev/null; sleep 2

# Verify plugin is registered:
auval -a | grep -i "DCJM"

# Run full validation:
auval -v aumu DCJM MxCJ

# Expected success output ends with:
# "AU VALIDATION SUCCEEDED."
# Exit code: 0
# Errors: 0
# Warnings: 0 (after version hints are set)
```

### MAC-04: SDL2 run loop pump for macOS (if Xbox detection fails)
```cpp
// Source: SDL issue #11742 — add to SdlContext::acquire() before SDL_Init, Apple only
// Only needed if SDL_NumJoysticks() returns 0 for Xbox controllers
#if JUCE_MAC
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, false);
#endif
const bool ok = (SDL_Init(SDL_INIT_GAMECONTROLLER) == 0);
```

---

## State of the Art

| Old Approach | Current Approach | When Changed | Impact |
|--------------|------------------|--------------|--------|
| `altool` for notarization | `xcrun notarytool` | Nov 2023 (Apple retired altool) | Phase 48 must use notarytool |
| bare string parameter IDs | `juce::ParameterID { id, version }` | JUCE 7+ | Required for AU zero-warning auval |
| auval → separate tool | `auval` is symlink to `auvaltool` | macOS 15+ | Same command works; no change needed |

**Deprecated/outdated:**
- `altool`: Retired November 2023 — do not use for notarization (Phase 48 concern, not Phase 46)
- JUCE Projucer for CMake projects: Not applicable — this project uses CMake directly

---

## Open Questions

1. **Xcode 26.3 AU universal binary behavior**
   - What we know: Xcode 16.x has a confirmed regression where AU targets ignore `CMAKE_OSX_ARCHITECTURES` when destination is "My Mac". The fix is `ONLY_ACTIVE_ARCH=NO` on xcodebuild. Xcode 26.3 = Xcode 16.3 internal versioning (macOS 26 = Sequoia wave).
   - What's unclear: Whether the Apple fix for FB17019201 landed in Xcode 26.3 or whether `ONLY_ACTIVE_ARCH=NO` is still needed.
   - Recommendation: Always pass `ONLY_ACTIVE_ARCH=NO` and verify with `lipo -info`. If both VST3 and AU binaries report `x86_64 arm64`, success. If AU is single-arch after `ONLY_ACTIVE_ARCH=NO`, escalate to installing Xcode 15.x.

2. **SDL 2.30.9 PS4 vs Xbox detection on macOS**
   - What we know: The current `SdlContext.cpp` disables HIDAPI globally (`SDL_HINT_JOYSTICK_HIDAPI=0`). On macOS, HIDAPI=0 forces the IOKit driver for PS4 controllers. For Xbox, SDL 2.30.9 routes through GCController even with HIDAPI=0 (the commit changes the routing logic independently of the HIDAPI hint).
   - What's unclear: Whether `SDL_HINT_JOYSTICK_HIDAPI=0` is sufficient for PS4 USB detection on macOS without a run loop pump. PS4 uses IOKit directly; Xbox uses GCController.
   - Recommendation: Test PS4 first. If detected without changes — MAC-04 pass for PS4. If Xbox must also be tested, add the `CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, false)` call inside `#if JUCE_MAC` guard. The CONTEXT.md success criterion says "PS4 or Xbox controller" — PS4 first is the lower-risk path.

3. **Logic Pro AU MIDI output smoke test interpretation**
   - What we know: `kAudioUnitType_MusicDevice` AU plugins cannot route MIDI output through Logic Pro's MIDI monitor. This is a Logic architectural limitation, not a bug.
   - What's unclear: The MAC-03 success criterion says "emit MIDI output — confirmed by DAW MIDI monitor." This is achievable in Reaper and Ableton (VST3). For Logic, the test must be redefined as: plugin loads in Software Instrument slot, UI visible, plugin receives MIDI input and processes without crash.
   - Recommendation: MAC-03 for Logic = load test + UI visible + no crash. MIDI output verification happens only via Reaper and Ableton VST3. Document this distinction in verification.

4. **macOS Deployment Target**
   - What we know: The current CMakeCache shows `CMAKE_OSX_DEPLOYMENT_TARGET:STRING=` (empty). JUCE 8 supports macOS 10.13+. AU format has no special deployment target requirement beyond 10.9.
   - What's unclear: Whether Xcode 26.3 on macOS 26.3 will warn or fail if no deployment target is set.
   - Recommendation: Set `CMAKE_OSX_DEPLOYMENT_TARGET=11.0`. macOS 11 is the first universal binary-capable release, aligns with Apple Silicon support, and matches the REQUIREMENTS.md note "macOS 11+ requirement."

---

## Validation Architecture

### Test Framework
| Property | Value |
|----------|-------|
| Framework | Catch2 v3.8.1 (opt-in via BUILD_TESTS=ON) |
| Config file | CMakeLists.txt — `option(BUILD_TESTS ...)` |
| Quick run command | `cmake --build build-mac --config Release --target ChordJoystickTests && ./build-mac/Release/ChordJoystickTests` |
| Full suite command | Same as quick run — test suite is small |

**Note:** Phase 46 requirements are infrastructure and integration tests, not unit test territory. The Catch2 suite tests `ScaleQuantizer`, `ChordEngine`, `LooperEngine`, etc. — none of these cover build system configuration, AU validation, or DAW smoke tests. All MAC-01 through MAC-05 validations are manual or CLI commands, not automated Catch2 tests.

### Phase Requirements → Test Map
| Req ID | Behavior | Test Type | Automated Command | File Exists? |
|--------|----------|-----------|-------------------|-------------|
| MAC-05 | cmake -G Xcode completes with zero errors | smoke | `cmake -G Xcode -B build-mac -S . 2>&1; echo "Exit: $?"` | ✅ (CMakeLists.txt exists) |
| MAC-01 | Both binaries report x86_64 arm64 via lipo | smoke | `lipo -info <vst3-binary>` + `lipo -info <au-binary>` | ❌ Wave 0 — binaries must be built first |
| MAC-02 | auval exits 0 with zero errors, zero warnings | smoke | `auval -v aumu DCJM MxCJ; echo "Exit: $?"` | ❌ Wave 0 — AU must be registered first |
| MAC-03 | Plugin loads in Logic/Reaper/Ableton, MIDI flows | manual | N/A — requires DAW UI interaction | ❌ manual-only |
| MAC-04 | SDL_NumJoysticks() > 0 with USB controller | manual | N/A — requires physical hardware + UI inspection | ❌ manual-only |

**Manual-only justification (MAC-03, MAC-04):** Both require physical DAW application interaction and physical USB hardware. These cannot be automated without DAW scripting infrastructure (not in scope). Verification is human-in-the-loop: open DAW, load plugin, observe.

### Sampling Rate
- **Per task commit:** `cmake -G Xcode -B build-mac -S . 2>&1 | tail -5` (confirm configure succeeds)
- **Per wave merge:** `lipo -info` on both binaries + `auval -v aumu DCJM MxCJ`
- **Phase gate:** All CLI validations pass + manual DAW smoke tests documented before `/gsd:verify-work`

### Wave 0 Gaps
- No new test files needed — this phase has no Catch2-testable logic
- Smoke commands documented above run against build artifacts created during the phase

*(The existing test infrastructure covers pre-existing unit-testable logic. Phase 46 is pure build/configuration work.)*

---

## Sources

### Primary (HIGH confidence)
- [JUCE docs — juce::ParameterID Class Reference](https://docs.juce.com/master/classParameterID.html) — `ParameterID { id, versionHint }` constructor confirmed
- [JUCE forum — "How to set a parameter version hint?"](https://forum.juce.com/t/how-to-set-a-parameter-version-hint/52011) — confirmed constructor syntax, version 1 for first-release parameters
- [JUCE forum — "AU universal binary macOS Sequoia and Xcode 16.2"](https://forum.juce.com/t/au-universal-binary-macos-sequoia-and-xcode-16-2/65337) — "Any Mac" / `ONLY_ACTIVE_ARCH=NO` workaround confirmed
- CMakeLists.txt lines 1-220 — read directly from project; confirmed exact line numbers for all fixes
- Source/PluginProcessor.cpp lines 1-407 — read directly; confirmed helper lambda structure and parameter count
- Source/SdlContext.cpp — read directly; confirmed HIDAPI=0 hint is already set globally

### Secondary (MEDIUM confidence)
- [SDL issue #11742 — Xbox Controller Detection Fails on macOS Without Run Loop Processing](https://github.com/libsdl-org/SDL/issues/11742) — GCController regression in 2.30.9, run loop workaround documented; closed as "COMPLETED" with documentation update
- [Apple Developer Forums — Xcode 16.1 Stopped Building Universal Binaries](https://developer.apple.com/forums/thread/769777) — regression confirmed, `ONLY_ACTIVE_ARCH=NO` mentioned as fix direction
- [JUCE forum — "Why does ParameterID::version default to 0 and assert?"](https://forum.juce.com/t/why-does-parameterid-version-default-to-0-and-assert-like-theres-no-tomorrow/55865) — version hint behavior confirmed
- [JUCE forum — "AU plugin sending both Audio and MIDI"](https://forum.juce.com/t/au-plugin-sending-both-audio-and-midi/57765) — confirmed Logic Pro cannot output MIDI from kAudioUnitType_MusicDevice

### Tertiary (LOW confidence)
- Multiple WebSearch results about SDL2 HIDAPI=0 behavior on macOS — PS4 uses IOKit fallback when HIDAPI disabled, Xbox uses GCController (not fully verified with a single authoritative source)

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all libraries are already in project, no new dependencies
- Architecture (CMake fix): HIGH — exact line numbers read from source, fix is well-understood
- Architecture (ParameterID): HIGH — JUCE official docs confirm exact constructor syntax
- Architecture (Xcode universal binary workaround): HIGH — JUCE forum post with user confirmation
- SDL2 macOS controller behavior: MEDIUM — regression confirmed, exact HIDAPI=0 behavior on macOS needs empirical verification
- Logic Pro AU MIDI output limitation: HIGH — consistent across multiple sources (Apple, JUCE forum, KVR)

**Research date:** 2026-03-11
**Valid until:** 2026-04-11 (stable APIs; SDL/JUCE versions locked in CMakeLists.txt)
