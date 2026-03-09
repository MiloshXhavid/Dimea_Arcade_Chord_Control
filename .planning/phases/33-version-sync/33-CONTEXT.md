# Phase 33: Version Sync & Distribution — Context

**Gathered:** 2026-03-05
**Status:** Ready for planning

<domain>
## Phase Boundary

Publish v1.7 of ChordJoystick MK2 as the official stable Latest release on GitHub. Bump version strings in CMakeLists.txt and installer .iss to 1.7, build a clean Release binary and installer, create the GitHub v1.7 release with release notes, and back up to desktop. Also mark TRIP-01/TRIP-02 as complete in REQUIREMENTS.md (triplet subdivisions confirmed implemented in codebase).

The deferred v1.6 GitHub release (Phase 30-02 / DIST-06) is **skipped** — v1.7 supersedes v1.6 and will become the new Latest.

No new features.

</domain>

<decisions>
## Implementation Decisions

### GitHub Release Designation
- v1.7 is a **full stable release** — mark as Latest on GitHub (replaces v1.6 installer as current)
- Tag: `v1.7`
- Release title: `DIMEA PLUGIN - CHORD JOYSTICK MK2 v1.7`
- Installer filename: `DIMEA-ChordJoystickMK2-v1.7-Setup.exe`
- v1.6 GitHub release: **not published** (skipped — superseded by v1.7)

### Release Notes Style
- Same format as v1.5/v1.6: short narrative intro (1–3 sentences) followed by feature bullets grouped by category
- v1.7 feature groupings:
  1. **Visual Overhaul** — space background, milky way particle field, BPM-synced glow ring, semitone grid on joystick
  2. **Joystick Physics** — spring-damper cursor with inertia settle, perimeter arc indicator, note-label compass at cardinal positions
  3. **Gamepad SWAP/INV** — stick swap (left↔right) and axis invert routing via APVTS params, visual indicators in gamepad UI
  4. **Bug Fixes & Triplets** — 6-bug batch (arp step counting, gate length on joystick triggers, CC mode expansion, left-stick deadzone rescaling, looper reset syncs random phases, population knob deterministic), triplet subdivisions in random trigger and quantize selectors

### Version Bump Locations
- `CMakeLists.txt` — `VERSION` field (or `PLUGIN_VERSION` / version string, wherever set)
- `installer/DimaChordJoystick-Setup.iss` — `AppVersion`, `AppComments`, `OutputBaseFilename`

### Installer Changes
- `AppVersion` 1.6 → 1.7
- `AppComments` updated to v1.7 feature list
- `OutputBaseFilename` → `DIMEA-ChordJoystickMK2-v1.7-Setup`
- All other fields unchanged (AppName, AppPublisher, AppPublisherURL, welcome text, no LicenseFile)
- VST3 bundle source path: `Chord Joystick Control (BETA).vst3` (confirmed in Phase 30-01 — PRODUCT_NAME in CMakeLists.txt, DestDir still installs to `DIMEA CHORD JOYSTICK MK2.vst3`)

### Build Requirements
- Clean Release build required — verify `.vst3` binary timestamp is newer than last commit before publishing
- Installer rebuilt with updated `.iss` file

### Desktop Backup
- Target: `%USERPROFILE%\Desktop\Dima_plug-in\v1.7\`
- Contents: installer `.exe`, `.vst3` bundle, `source-v1.7.zip`

### Requirements Housekeeping
- TRIP-01 and TRIP-02 must be marked complete in REQUIREMENTS.md (triplet subdivisions confirmed implemented)
- DIST-05 is already complete (Phase 30-01); DIST-06 is superseded/skipped
- A new DIST-07 + DIST-08 pair should be minted for v1.7 GitHub release and desktop backup

</decisions>

<code_context>
## Existing Code Insights

### Reusable Assets
- `installer/DimaChordJoystick-Setup.iss` — updated to v1.6 in Phase 30-01; only AppVersion + AppComments + OutputBaseFilename need updating to v1.7
- `installer/Output/` — build output directory for installer binary
- Prior release workflow established in Phase 25 and Phase 30 (build → installer → GitHub release → desktop backup)

### Established Patterns
- GitHub release via `gh release create` CLI
- Desktop backup via PowerShell copy
- Git tag applied before GitHub release creation
- `gh release edit v1.x --prerelease=false` to promote prior releases if needed

### Integration Points
- `gh` CLI for GitHub release creation
- CMake Release build: `cmake --build build --config Release`
- ISCC.exe for Inno Setup installer compilation
- VST3 bundle source: `build/ChordJoystick_artefacts/Release/Chord Joystick Control (BETA).vst3`

### Known Gotchas
- VST3 bundle folder name is `Chord Joystick Control (BETA).vst3` (from PRODUCT_NAME), not `DIMEA CHORD JOYSTICK MK2.vst3` — DestDir in .iss installs to branded name
- RC file (`build/ChordJoystick_artefacts/JuceLibraryCode/ChordJoystick_resources.rc`) has CompanyName baked at cmake configure time — Reaper reads this for vendor display; verify it shows "DIMEA"
- GitHub `isLatest` JSON field not supported in installed gh CLI — use `isPrerelease` field for verification

</code_context>

<deferred>
## Deferred Ideas

- Phase 34: not yet defined — no directory exists. If new features are planned for v1.8, that would be a new milestone.
- v1.6 GitHub release (DIST-06): permanently skipped — v1.7 supersedes it.

</deferred>

---

*Phase: 33-version-sync*
*Context gathered: 2026-03-05*
