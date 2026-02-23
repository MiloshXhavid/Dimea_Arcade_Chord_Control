# Phase 07: DAW Compatibility, Distribution, and Release - Context

**Gathered:** 2026-02-23
**Status:** Ready for planning

<domain>
## Phase Boundary

Produce a shippable v1.0 release of ChordJoystick for **both Windows and macOS simultaneously**.
Phase 07 ends when:
- Windows installer (DimaChordJoystick-Setup.exe) is built and passes pluginval
- macOS build (VST3 + AU) is verified working on the MacBook Pro
- Both artifacts are tested on a clean machine / fresh environment
- Artifacts are ready to upload to Gumroad

**Nothing ships until both platforms are done.** This is the user's explicit requirement.

Note: Ableton 11.3.43 routing is already confirmed working (fixed in pre-07 commit 41d75db —
optional inactive stereo buses + prevIsDawPlaying_ DAW-stop fix).

</domain>

<decisions>
## Implementation Decisions

### Code Signing
- **No certificate yet** — ship unsigned for v1; SmartScreen warning is acceptable short-term
- Structure the build + installer so signing can be retrofitted in one step (signing script placeholder)
- When signing is added later: sign **both** the VST3 DLL and the installer EXE
- Mac: no Apple Developer account yet — ship unnotarized; Gatekeeper warning acceptable for v1

### pluginval (Windows)
- Target: **strictness 5** preferred; level 4 acceptable if level 5 reveals truly obscure edge cases
- Policy: **fix everything pluginval reports** — zero tolerance, no known-benign exceptions
- Not installed yet — plan must include download + setup steps
- Run against **installed path** (`%COMMONPROGRAMFILES%\VST3\ChordJoystick.vst3`) — production-like validation

### Windows Installer (Inno Setup)
- **Name:** `DimaChordJoystick-Setup.exe`
- **Version:** 1.0.0
- **Install path:** offer both system-wide (`%COMMONPROGRAMFILES%\VST3\`) and per-user during install wizard; system-wide as default
- **Uninstaller:** yes — registered in Windows Add/Remove Programs
- **Wizard pages:** standard 4-5 page flow: welcome, license agreement, install path, install progress, done
- **Post-install:** success dialog only — no browser launch, no README popup
- **Signing:** placeholder `signtool.exe` call commented out, ready to activate when cert acquired

### macOS Build
- **Hardware:** MacBook Pro available — **fresh machine** (Xcode and CMake not yet installed)
- **Plan must include:** Xcode CLI tools install, CMake install, SDL2 macOS dependency, JUCE FetchContent on Mac
- **Formats:** VST3 + AU (both targets)
- **Installer:** `.pkg` (guided installer, places files automatically)
- **Notarization:** skip for v1 — user gets Gatekeeper warning; notarization deferred until Apple Developer account acquired
- **Signing:** unnotarized/unsigned for now — document workaround for users (right-click → Open, or System Settings → Allow)

### Distribution
- **Scope:** artifact only — produce `DimaChordJoystick-Setup.exe` (Windows) and a `.pkg` (Mac) ready to upload
- **Gumroad product listing:** manual — out of scope for Phase 07
- **Clean install test:** required before release — both Windows and Mac must be tested on a machine without dev tools

### Claude's Discretion
- Exact Inno Setup script structure and section ordering
- SDL2 macOS approach (Homebrew vs FetchContent static lib vs pre-built framework)
- pkgbuild / productbuild vs third-party Mac packaging tool
- How to document the Gatekeeper workaround for Mac users

</decisions>

<specifics>
## Specific Requirements

- Installer EXE filename is exactly `DimaChordJoystick-Setup.exe` (not ChordJoystick)
- Version string: 1.0.0 throughout (CMakeLists.txt, installer, plugin metadata)
- Mac build must include both AU and VST3 — not VST3 only
- Both platforms must complete before either artifact is uploaded to Gumroad
- pluginval run must target the installed VST3, not the build artefact path
- The Mac MacBook Pro is a fresh machine — all dev tooling install steps must be included in the plan

</specifics>

<deferred>
## Deferred Ideas

- **Apple Developer Program enrollment + notarization** — deferred post-v1. When enrolled:
  sign VST3 bundle + .pkg, notarize with `xcrun notarytool`, staple ticket to .pkg
- **Windows code signing** — deferred post-v1. Infrastructure placeholder in installer script.
  When cert acquired: `signtool sign` on VST3 DLL + EXE, timestamp server
- **macOS v2 polish** — AU parameter naming, Logic Pro UI validation, Rosetta 2 compatibility check

</deferred>

---

*Phase: 07-daw-compatibility-distribution-and-release*
*Context gathered: 2026-02-23*
