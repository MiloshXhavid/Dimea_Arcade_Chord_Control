# Phase 16: Distribution — Context

**Phase goal:** v1.4 publicly released on GitHub with installer binary + release notes; desktop backup created.
**Requirements:** DIST-01, DIST-02
**Created:** 2026-02-26

---

## 1. Release Version & Scope

**Tag:** `v1.4`
- The "v1.5" label in STATE.md was informal bookkeeping, not a version boundary. Everything in phases 12–15 ships under a single `v1.4` tag.

**GitHub release title:** `ChordJoystick v1.4 — LFO + Beat Clock`

**Release scope:** All phases 12–15 are first-class features in this release:
- Phase 12–13: Dual-axis LFO engine (7 waveforms, sync, distortion, level)
- Phase 14: LFO UI panel + beat clock dot
- Phase 15: Gamepad Preset Control (Option button → MIDI Program Change)

No "v1.3 previously" section — release notes are stand-alone for v1.4.

---

## 2. Release Notes Content

**Product name:** Dimea Chord Joystick MK2
**Company:** Dimea Sound

**Format:** Narrative paragraph (1–3 sentences intro) followed by bullet list grouped by feature category.

**Feature groupings:**
1. LFO System — dual-axis, 7 waveforms, sync/free, phase, distortion, level
2. Beat Clock — flashing dot on BPM knob, follows DAW or free tempo
3. Gamepad Preset Control — Option button toggle, D-pad sends MIDI PC, UI indicator

**Required installation note:** Include a note that because this is an unsigned binary, Windows SmartScreen will show a security warning. Users should click "More info" → "Run anyway" to proceed with installation.

**History:** Stand-alone release — do not reference v1.3 changes.

---

## 3. Build & Installer

**Verification step (required before release):**
- Check that `installer/Output/DimaChordJoystickMK2-Setup.exe` was built after the last commit (phase 15-02: `1720a71`).
- If binary is older than the last commit, perform a clean Release build first.

**Manual smoke test (required before publishing):**
- Load the built VST3 in DAW
- Confirm LFO modulation is audible on both axes
- Confirm gamepad Option button toggles preset-scroll mode and UI shows "OPTION" indicator

**Release asset filename:** `DimeaChordJoystickMK2-v1.4-Setup.exe`
- Rename the existing `DimaChordJoystickMK2-Setup.exe` before attaching to the release.

---

## 4. Desktop Backup

**Target path:** `%USERPROFILE%\Desktop\Dima_plug-in\v1.4\`

**Contents of `v1.4\` folder:**
- `DimeaChordJoystickMK2-v1.4-Setup.exe` — the installer binary
- The built VST3 bundle (`.vst3` folder from build output)
- `source-v1.4.zip` — zip of the repository at the `v1.4` git tag

**If `Dima_plug-in\` already exists:** overwrite/replace existing contents (do not create a parallel folder).

---

## Deferred Ideas

None captured during this discussion.
