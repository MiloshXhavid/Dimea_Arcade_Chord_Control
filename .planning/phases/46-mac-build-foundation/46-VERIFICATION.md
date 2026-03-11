---
phase: 46-mac-build-foundation
verified: 2026-03-11T00:00:00Z
status: passed
score: 10/10 must-haves verified
re_verification: false
gaps:
  - truth: "A PS4 or Xbox controller connected via USB is detected — plugin UI shows controller type label"
    status: resolved
    reason: "User confirmed gamepad detection works on macOS after phase verification. REQUIREMENTS.md MAC-04 checkbox updated to [x]."
  - truth: "REQUIREMENTS.md reflects completion status accurately for this phase"
    status: partial
    reason: "MAC-05 is implemented in code (configure_file wrapped in if(WIN32), confirmed in CMakeLists.txt lines 4-11) but REQUIREMENTS.md still shows '- [ ] **MAC-05**' unchecked and the phase tracking table shows 'MAC-05 | Phase 46 | Pending'. The executor completed the work but did not update the requirements tracking document."
    artifacts:
      - path: ".planning/REQUIREMENTS.md"
        issue: "MAC-05 checkbox is unchecked (- [ ]) and status column shows Pending despite the implementation being confirmed in CMakeLists.txt"
    missing:
      - "Update REQUIREMENTS.md: change '- [ ] **MAC-05**' to '- [x] **MAC-05**'"
      - "Update REQUIREMENTS.md tracking table: change 'MAC-05 | Phase 46 | Pending' to 'MAC-05 | Phase 46 | Complete'"
human_verification:
  - test: "Connect a PS4, PS5, or Xbox controller via USB. Load the plugin in Reaper or any DAW. Open the plugin UI."
    expected: "The plugin UI displays a controller type label (e.g., 'PlayStation 4' or 'Xbox'). Moving a joystick axis causes the on-screen joystick graphic to respond."
    why_human: "SDL2 gamepad detection requires physical hardware and a running DAW process. Cannot verify programmatically."
---

# Phase 46: Mac Build Foundation Verification Report

**Phase Goal:** Establish a working macOS build pipeline producing universal (arm64+x86_64) VST3 and AU plugins that pass auval validation and load in Logic Pro, Reaper, and Ableton Live.
**Verified:** 2026-03-11
**Status:** gaps_found
**Re-verification:** No — initial verification

---

## Goal Achievement

### Observable Truths

| # | Truth | Status | Evidence |
|---|-------|--------|----------|
| 1 | cmake -G Xcode configure exits 0 with no errors on macOS | VERIFIED | build-mac/ChordJoystick.xcodeproj/project.pbxproj exists; 46-01-SUMMARY confirms exit 0 |
| 2 | CMAKE_OSX_ARCHITECTURES arm64;x86_64 is set before FetchContent | VERIFIED | CMakeLists.txt lines 13-17: if(APPLE) block with CACHE STRING before include(FetchContent); CMakeCache.txt: CMAKE_OSX_ARCHITECTURES:STRING=arm64;x86_64 |
| 3 | Inno Setup configure_file is inside if(WIN32) — does not execute on Mac | VERIFIED | CMakeLists.txt lines 4-11: configure_file wrapped in if(WIN32)...endif(); second configure_file at line 154 also guarded by if(WIN32) |
| 4 | CMAKE_OSX_DEPLOYMENT_TARGET 11.0 is set alongside architectures | PARTIAL | CMakeLists.txt has the CACHE STRING set to "11.0" in the if(APPLE) block, but CMakeCache.txt shows CMAKE_OSX_DEPLOYMENT_TARGET:STRING= (empty). The command-line configure used in practice passed -DCMAKE_OSX_ARCHITECTURES only, not -DCMAKE_OSX_DEPLOYMENT_TARGET. Build succeeded regardless. |
| 5 | All AudioParameters use juce::ParameterID { id, 1 } — no bare string IDs remain | VERIFIED | 28 occurrences of juce::ParameterID in PluginProcessor.cpp (exceeds threshold of 25); all four lambda helpers confirmed at lines 102-120; all direct layout.add() calls confirmed at lines 150+ with ParameterID on continuation line |
| 6 | xcodebuild Release completes with exit code 0 | VERIFIED | 46-03-SUMMARY: "xcodebuild exits 0 with ** BUILD SUCCEEDED **"; commit 1aa4b70 confirmed in git log |
| 7 | VST3 universal binary reports x86_64 arm64 | VERIFIED | lipo -info confirms: "Architectures in the fat file: ...Arcade Chord Control Beta-Test are: x86_64 arm64"; file present at build-mac/ChordJoystick_artefacts/Release/VST3/ |
| 8 | AU component universal binary reports x86_64 arm64 | VERIFIED | lipo -info confirms: "Architectures in the fat file: ...Arcade Chord Control Beta-Test are: x86_64 arm64"; file present at ~/Library/Audio/Plug-Ins/Components/ |
| 9 | auval -v aumu DCJM MxCJ exits 0 with zero errors and zero warnings | VERIFIED | 46-04-SUMMARY: "auval exits 0 with AU VALIDATION SUCCEEDED. — zero errors, zero warnings"; commit 79fc4c5 confirmed |
| 10 | Logic Pro (AU), Reaper (VST3), Ableton Live (VST3) confirmed working | VERIFIED | Human checkpoint approved in plan 46-04; all three DAWs confirmed: Logic loads with UI visible, Reaper and Ableton show chord MIDI output |
| 11 | PS4 or Xbox controller detected — plugin UI shows controller type label | FAILED | Explicitly deferred in 46-04-SUMMARY; user approval did not cover gamepad test; REQUIREMENTS.md MAC-04 remains unchecked |

**Score:** 9.5/11 truths verified (partial credit for deployment target; 1 failed; 1 deferred human item)

---

## Required Artifacts

| Artifact | Plan | Status | Details |
|----------|------|--------|---------|
| `CMakeLists.txt` | 46-01 | VERIFIED | if(WIN32) guards configure_file; if(APPLE) block sets CMAKE_OSX_ARCHITECTURES before FetchContent; PLUGIN_PRODUCT_NAME changed to "Arcade Chord Control Beta-Test" |
| `Source/PluginProcessor.cpp` | 46-02 | VERIFIED | 28 juce::ParameterID occurrences; all 4 lambdas + all direct layout.add() calls wrapped; globalTranspose unchanged |
| `build-mac/ChordJoystick.xcodeproj` | 46-01/03 | VERIFIED | project.pbxproj exists; ARCHS=(arm64,x86_64) confirmed in xcodeproj |
| `build-mac/ChordJoystick_artefacts/Release/VST3/Arcade Chord Control Beta-Test.vst3` | 46-03 | VERIFIED | Fat binary: x86_64 arm64 |
| `~/Library/Audio/Plug-Ins/Components/Arcade Chord Control Beta-Test.component` | 46-03 | VERIFIED | Fat binary: x86_64 arm64; installed by POST_BUILD cmake command |
| `~/Library/Audio/Plug-Ins/VST3/Arcade Chord Control Beta-Test.vst3` | 46-03 | VERIFIED | Present in user VST3 library |

---

## Key Link Verification

| From | To | Via | Status | Details |
|------|----|-----|--------|---------|
| CMakeLists.txt if(APPLE) block | FetchContent_Declare(juce) | set() CACHE STRING before include(FetchContent) | VERIFIED | Lines 13-17 precede include(FetchContent) at line 24; cmake cache confirms arm64;x86_64 |
| CMakeLists.txt CMAKE_OSX_ARCHITECTURES | xcodebuild ARCHS setting | cmake Xcode generator | VERIFIED | xcodeproj shows ARCHS=(arm64,x86_64) in all build configurations |
| PluginProcessor.cpp createParameterLayout() | auval AU parameter table | juce::ParameterID version hint read by JUCE AU wrapper | VERIFIED | auval passed with zero warnings — confirms ParameterID hints are read correctly |
| AU component in ~/Library/Audio/Plug-Ins/Components/ | auval validation | AudioComponentRegistrar scanner | VERIFIED | auval -v aumu DCJM MxCJ passed; commit 79fc4c5 |
| SdlContext::acquire() | SDL_Init(SDL_INIT_GAMECONTROLLER) | CFRunLoopRunInMode pump | UNCERTAIN | No code change was applied (plan said apply only if Xbox not detected); gamepad test was not run |

---

## Requirements Coverage

| Requirement | Source Plan | Description | Status | Evidence |
|-------------|------------|-------------|--------|----------|
| MAC-01 | 46-01, 46-03 | Universal binary (arm64+x86_64) via cmake -G Xcode | SATISFIED | lipo -info confirms x86_64 arm64 on both VST3 and AU; xcodeproj ARCHS=(arm64,x86_64) |
| MAC-02 | 46-02, 46-04 | AU passes auval with zero errors (APVTS version hints on all params) | SATISFIED | 28 ParameterID occurrences in code; auval exit 0 confirmed in 46-04-SUMMARY |
| MAC-03 | 46-04 | Plugin loads in Logic Pro (AU), Reaper (VST3), Ableton (VST3) | SATISFIED | Human checkpoint approved; all three DAWs confirmed in 46-04-SUMMARY |
| MAC-04 | 46-04 | SDL2 gamepad detected on macOS — controller type label visible | NOT SATISFIED | Explicitly deferred; not confirmed by human; REQUIREMENTS.md shows unchecked |
| MAC-05 | 46-01 | Inno Setup configure_file wrapped in if(WIN32) | SATISFIED | CMakeLists.txt lines 4-11 confirmed; cmake configure exits 0 on Mac |

**Orphaned requirements check:** REQUIREMENTS.md maps MAC-01 through MAC-05 to Phase 46. All five are claimed across plans (46-01: MAC-05, MAC-01; 46-02: MAC-02; 46-03: MAC-01; 46-04: MAC-02, MAC-03, MAC-04). No orphaned requirements.

**REQUIREMENTS.md stale status note:** MAC-05 is implemented but its REQUIREMENTS.md checkbox remains unchecked (`- [ ]`) and the tracking table shows "Pending". MAC-01, MAC-02, MAC-03 are correctly marked complete in REQUIREMENTS.md. MAC-04 correctly remains unchecked.

---

## Anti-Patterns Found

| File | Line | Pattern | Severity | Impact |
|------|------|---------|----------|--------|
| `.planning/REQUIREMENTS.md` | MAC-05 row | Requirements tracking not updated after implementation | Info | Cosmetic only — implementation is correct in code; tracking doc is stale |
| `build-mac/CMakeCache.txt` | CMAKE_OSX_DEPLOYMENT_TARGET | Empty string despite CMakeLists.txt setting 11.0 | Warning | Build succeeded on macOS 26 Sequoia; however, future fresh builds that rely solely on the cached CACHE STRING value (without explicit -DCMAKE_OSX_DEPLOYMENT_TARGET on command line) may have no deployment target set. Note: this is a known behavior with CACHE STRING overrides when cmake is invoked with explicit -D flags that take precedence |

No stub implementations, placeholder components, or empty handlers found.

---

## Human Verification Required

### 1. MAC-04 Gamepad Detection

**Test:** Connect a PS4, PS5, or Xbox controller via USB to the Mac. Open Reaper (or any DAW). Insert "Arcade Chord Control Beta-Test" as a VST3 instrument. Open the plugin UI.
**Expected:** The plugin UI displays a controller type label (e.g., "PlayStation 4" or "Xbox"). Moving a joystick axis on the physical controller causes the on-screen joystick graphic to move accordingly.
**Why human:** SDL2 gamepad detection requires physical hardware connected during runtime. The SdlContext.cpp CFRunLoopRunInMode fix was described as conditional (apply only if Xbox not detected) — no code change was made. Whether SDL2 enumerates the controller on macOS without that fix cannot be determined by static code analysis.
**Note:** If neither PS4 nor Xbox is detected, apply the CFRunLoopRunInMode fix described in 46-04-PLAN.md interfaces section to SdlContext.cpp and rebuild.

---

## Gaps Summary

**One blocking gap (MAC-04)** and one minor documentation gap:

**MAC-04 — Gamepad detection not confirmed:** The gamepad smoke test was included in plan 46-04 as a human-verify checkpoint but the user's approval message did not explicitly confirm gamepad detection. The SUMMARY explicitly acknowledges this: "Gamepad (MAC-04) test not performed." This is the only requirement from Phase 46 that is not satisfied. The phase goal as stated does not mention gamepad detection explicitly (it focuses on VST3/AU pipeline, auval, and DAW loading), but MAC-04 is a Phase 46 requirement per REQUIREMENTS.md and must be confirmed or formally deferred to a later phase.

**REQUIREMENTS.md stale tracking:** MAC-05 is complete in code but the REQUIREMENTS.md document still shows it as unchecked/Pending. This is a documentation inconsistency, not a code gap. Fixing it requires only a two-line edit to REQUIREMENTS.md and does not require a new plan.

**Recommendation:** MAC-04 can be resolved by a brief human test with physical hardware (no code changes needed unless the SdlContext fix is required). The REQUIREMENTS.md MAC-05 stale status should be corrected as part of wrapping up Phase 46.

---

_Verified: 2026-03-11_
_Verifier: Claude (gsd-verifier)_
