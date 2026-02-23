# Phase 07: DAW Compatibility, Distribution, and Release - Context

**Gathered:** 2026-02-23
**Status:** Ready for planning

<domain>
## Phase Boundary

Produce a shippable v1.0 **Windows-only** release of ChordJoystick.
Phase 07 ends when:
- Windows installer (DimaChordJoystick-Setup.exe) is built and passes pluginval
- Installer tested on a clean Windows machine without dev tools
- Artifact is ready to upload to Gumroad

macOS support is explicitly deferred to v2.

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

### Distribution
- **Scope:** artifact only — produce `DimaChordJoystick-Setup.exe` ready to upload
- **Gumroad product listing:** manual — out of scope for Phase 07
- **Clean install test:** required before release — must be tested on a Windows machine without dev tools

### Claude's Discretion
- Exact Inno Setup script structure and section ordering

</decisions>

<specifics>
## Specific Requirements

- Installer EXE filename is exactly `DimaChordJoystick-Setup.exe` (not ChordJoystick)
- Version string: 1.0.0 throughout (CMakeLists.txt, installer, plugin metadata)
- pluginval run must target the installed VST3, not the build artefact path

</specifics>

<deferred>
## Deferred Ideas

- **macOS support** — explicitly v2. MacBook Pro available; will need Xcode + CMake setup,
  VST3 + AU targets, .pkg installer, Apple Developer account + notarization, Gatekeeper docs.
- **Windows code signing** — deferred post-v1. Infrastructure placeholder in installer script.
  When cert acquired: `signtool sign` on VST3 DLL + EXE, timestamp server.

</deferred>

---

*Phase: 07-daw-compatibility-distribution-and-release*
*Context gathered: 2026-02-23*
