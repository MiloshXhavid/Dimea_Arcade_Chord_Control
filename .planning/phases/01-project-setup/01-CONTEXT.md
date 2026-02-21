# Phase 1: Project Setup - Context

**Gathered:** 2026-02-21
**Status:** Ready for planning

<domain>
## Phase Boundary

Establish JUCE build pipeline with CMake and define all APVTS parameters. This phase creates the foundational project structure and parameter system that all subsequent phases depend on. No MIDI output or GUI in this phase — just the buildable shell with parameter definitions.

</domain>

<decisions>
## Implementation Decisions

### Project Structure
- **Folder organization:** By Type (standard JUCE folders)
- **Component folders:** Source/, Components/, DSP/, Models/
- **Header files:** Flat structure (same folder as source)
- **Tests:** Root tests/ folder

### Build Targets
- **Plugin formats:** VST3 + AUv3 + Standalone from day one
- **CI/CD:** GitHub Actions with Windows + macOS
- **Architectures:** Universal (x64 + ARM)
- **Build configurations:** Both Debug and Release

### Parameter Layout
- **Organization:** By Function (grouped by feature)
- **Parameter groups (6 total):**
  1. **Quantizer** — 12 buttons for scale selection (piano roll style)
  2. **Intervals** — 8 knobs for intervals + scale preset dropdown
  3. **Triggers** — trigger selection switches
  4. **Joystick** — joystick + 2 attenuators (X and Y)
  5. **Switches** — 5 switches (trigger source selection, clock source)
  6. **Looper** — transport controls + dropdowns for length/subdivision
- **Value ranges:** Normalized (0.0-1.0), convert in code
- **Automation:** Full automation support from day one

### Shell Scope
- **Processor + Editor shells:** Minimal empty implementations that compile
- **Basic window:** Empty editor window that opens in DAW

</decisions>

<specifics>
## Specific Ideas

- 8 knobs for intervals: Root (transpose), Third, Fifth, Tension intervals (0-12 each), plus octave knobs for each of 4 notes
- Scale preset dropdown: Major, Minor, Min Pent, Maj Pent, Harmonic Minor, Melodic Minor, Dorian, Phrygian, Lydian, Mixolydian, Locrian
- Looper dropdowns: subdivisions (3/4, 4/4, 5/4, 7/8, 9/8, 11/8), lengths (1-16 bars)
- 5 switches: trigger source selection (touchplate/joystick/random), clock source (internal/external)

**No specific requirements — open to standard approaches**

</specifics>

<deferred>
## Deferred Ideas

None — discussion stayed within phase scope

</deferred>

---

*Phase: 01-project-setup*
*Context gathered: 2026-02-21*
