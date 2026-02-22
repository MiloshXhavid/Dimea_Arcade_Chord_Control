# Phase 01: Build Foundation and JUCE Version Lock - Context

**Gathered:** 2026-02-22
**Status:** Ready for planning

<domain>
## Phase Boundary

Fix the build configuration so the plugin compiles and loads in Ableton Live 11. Pin JUCE to a specific release tag, configure static CRT, enable auto-copy to system VST3 folder. Verify APVTS parameter round-trip and basic MIDI output (routed to a soft synth, audible note). No new plugin logic — this is purely build + baseline verification.

</domain>

<decisions>
## Implementation Decisions

### DAW smoke test target
- **Ableton Live 11 only** — no Reaper available on the dev machine
- Test in Ableton from Phase 01 onward (not deferred to Phase 07)
- Pass criteria: plugin loads without crash + all APVTS params save/restore + at least one audible note via a routed soft synth

### JUCE version
- **Pin to 8.0.4** — do not use origin/master, do not check for newer tags
- Research validated 8.0.4; avoid introducing new variables while getting the first build working

### COPY_PLUGIN_AFTER_BUILD
- **Enabled** — auto-copy VST3 bundle to `%COMMONPROGRAMFILES%\VST3\` after each build
- Dev machine has full admin rights — this will work without elevation issues
- Makes Ableton iteration faster: rebuild → restart Ableton → done

### Unit testing (Catch2)
- **Do not add Catch2** in Phase 01 or Phase 02
- User position: testing a MIDI controller requires controlling a virtual synthesizer — standalone logic tests don't reflect real behavior
- Pure logic verification (ChordEngine, ScaleQuantizer correctness) will be validated manually in Ableton during Phase 03 testing

### Claude's Discretion
- Specific CMakeLists.txt structure and ordering
- Static CRT configuration (`set_property(TARGET ... PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")`)
- CMake generator to use (Visual Studio 17 2022 vs Ninja)
- How to handle the existing `GIT_SHALLOW TRUE` flag for JUCE FetchContent

</decisions>

<specifics>
## Specific Ideas

- JUCE 8.0.4 is a hard requirement — the `GIT_TAG origin/master` in CMakeLists.txt is a known critical issue from the research, must be fixed as the first change
- Plugin should be audible in Ableton via routing to a built-in soft synth (e.g., Ableton's Wavetable or any instrument on a MIDI track)
- Note: Ableton Live 11 MIDI routing for VST3 MIDI generators may require specific IS_SYNTH / VST3_CATEGORIES settings to appear correctly — the planner should investigate this as part of the plan (it's a known risk that research flagged for Phase 07, but since Ableton is the only DAW, it must be addressed now)

</specifics>

<deferred>
## Deferred Ideas

- Catch2 unit tests for ChordEngine / ScaleQuantizer — not needed, user prefers manual testing via Ableton
- Phase 02 (Core Engine Validation) scope needs adjustment since Catch2 is off the table — planner should revise Phase 02 when planning it
- Reaper testing — user doesn't have Reaper, skip entirely
- Ableton Live 12 upgrade consideration — no action needed for v1

</deferred>

---

*Phase: 01-build-foundation-and-juce-version-lock*
*Context gathered: 2026-02-22*
