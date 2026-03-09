# Phase 25: Distribution — Context

**Phase goal:** v1.5 publicly released on GitHub with installer binary + release notes; desktop backup created.
**Requirements:** DIST-03, DIST-04
**Created:** 2026-03-01

---

## 1. Release Notes Content

**Format:** Same as v1.4 — short narrative intro (1–3 sentences) followed by feature bullets grouped by category.

**Feature groupings:**
1. **Routing** — single-channel routing, sub-octave per voice
2. **Arpeggiator** — 6 modes, gate length, bar-boundary launch
3. **LFO Recording** — arm/record/playback state machine
4. **Expression** — random trigger extensions, left joystick modulation expansion
5. **Gamepad** — Option Mode 1 visual feedback (knob color indication), face-button dispatch
6. **Bug fix** — Ableton MIDI port startup crash fix

**No SmartScreen warning** in the release notes — that information is handled inside the installer instead.

---

## 2. Release Title & Naming

**GitHub release title:** `DIMEA PLUGIN - CHORD JOYSTICK MK2 v1.5`
**Git tag:** `v1.5`
**Installer filename:** `DIMEA-ChordJoystickMK2-v1.5-Setup.exe`
**GitHub release type:** Pre-release (mark as pre-release, not full release)

---

## 3. Build & Installer

**Always require a clean Release build** before publishing — no delta builds.
- Build must be performed after Phase 24 is fully complete
- Verify the `.vst3` binary timestamp is newer than the last commit before proceeding

**Inno Setup `.iss` file changes required** (`installer/DimaChordJoystick-Setup.iss`):

| Field | Old value | New value |
|-------|-----------|-----------|
| `AppVersion` | `1.4` | `1.5` |
| `AppName` | `ChordJoystick MK2` | `DIMEA CHORD JOYSTICK MK2` |
| `AppPublisher` | `Dima Xhavid` | `DIMEA` |
| `AppPublisherURL` | `https://gumroad.com` | `https://www.dimea.com` |
| `AppComments` | v1.4 feature list | v1.5 feature list |
| `OutputBaseFilename` | `DimaChordJoystickMK2-Setup` | `DIMEA-ChordJoystickMK2-v1.5-Setup` |
| `LicenseFile` | present | remove (skip license step) |

**Welcome page text** (override via `[Messages]` → `WelcomeLabel2`):
```
Hi, I am DIMEA. I am currently no certified developer by Microsoft nor MAC OS and this Plugin is currently in Beta-Phase. That's why you will see the warning PopUP by installing this File. Ignore it ;) The Plugin will enroll with all necessary certificates once it's out for the market. How cares about these warnings anyway. Have fun exploring Harmonization and give me your feedback under dimitri.daehler@dimea.com!
```

**Welcome page headline** (override via `[Messages]` → `WelcomeLabel1`):
```
Welcome to DIMEA — congratulations for unlocking musical skills. Let's play some chords! Check out the Website: www.DIMEA.com
```

**Disclaimer location:** Welcome page only (first screen). No separate info page.
**License step:** Removed (no `LicenseFile` directive).

---

## 4. Desktop Backup

**Target path:** `%USERPROFILE%\Desktop\Dima_plug-in\v1.5\`

**If `v1.5\` folder already exists:** create `v1.5.1\` instead (do not overwrite).

**Contents:**
1. `DIMEA-ChordJoystickMK2-v1.5-Setup.exe` — the renamed installer binary
2. The built `.vst3` bundle (full directory from build output)
3. `source-v1.5.zip` — zip of the repository at the `v1.5` git tag

**Order:** Backup and GitHub release order does not matter — can be done in either sequence.

---

## Deferred Ideas

These were raised during discussion but belong in **Phase 24** (or a pre-distribution hotfix phase), not Phase 25:

1. **Option mode visual feedback** — When pressing Option on the gamepad, knobs inside the plugin show subtle color indication for Option Mode 1 and Option Mode 2 editable features. Must be implemented and verified before Phase 25 ships.

2. **Ableton MIDI port startup crash** — Ableton shows error: *"Lives letzter Start wurde während des Versuchs, den MIDI-Port 'M8U eX 10' zu laden, vorzeitig beendet."* — this only occurs when the plugin is loaded or present in a default template. Root cause is likely SDL2 or plugin initialization interfering with Ableton's MIDI port loading sequence. Must be fixed before Phase 25 ships.
