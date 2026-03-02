# Phase 25: Distribution — Research

**Researched:** 2026-03-02
**Domain:** GitHub Release publishing, Inno Setup installer packaging, local desktop backup
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**GitHub release title:** `DIMEA PLUGIN - CHORD JOYSTICK MK2 v1.5`
**Git tag:** `v1.5`
**Installer filename:** `DIMEA-ChordJoystickMK2-v1.5-Setup.exe`
**GitHub release type:** Pre-release (mark as pre-release, not full release)

**Inno Setup .iss file changes required:**
| Field | Old value | New value |
|-------|-----------|-----------|
| `AppVersion` | `1.4` | `1.5` |
| `AppName` | `ChordJoystick MK2` | `DIMEA CHORD JOYSTICK MK2` |
| `AppPublisher` | `Dima Xhavid` | `DIMEA` |
| `AppPublisherURL` | `https://gumroad.com` | `https://www.dimea.com` |
| `AppComments` | v1.4 feature list | v1.5 feature list |
| `OutputBaseFilename` | `DimaChordJoystickMK2-Setup` | `DIMEA-ChordJoystickMK2-v1.5-Setup` |
| `LicenseFile` | `present (..\LICENSE.txt)` | remove entirely (no license step) |

**Welcome page headline** (`[Messages]` → `WelcomeLabel1`):
```
Welcome to DIMEA — congratulations for unlocking musical skills. Let's play some chords! Check out the Website: www.DIMEA.com
```

**Welcome page body** (`[Messages]` → `WelcomeLabel2`):
```
Hi, I am DIMEA. I am currently no certified developer by Microsoft nor MAC OS and this Plugin is currently in Beta-Phase. That's why you will see the warning PopUP by installing this File. Ignore it ;) The Plugin will enroll with all necessary certificates once it's out for the market. How cares about these warnings anyway. Have fun exploring Harmonization and give me your feedback under dimitri.daehler@dimea.com!
```

**Desktop backup path:** `%USERPROFILE%\Desktop\Dima_plug-in\v1.5\`
If `v1.5\` already exists: create `v1.5.1\` instead.

**Backup contents:**
1. `DIMEA-ChordJoystickMK2-v1.5-Setup.exe` — renamed installer binary
2. Built `.vst3` bundle (full directory from build output)
3. `source-v1.5.zip` — zip of repository at `v1.5` tag

**Release notes feature groupings:**
1. Routing — single-channel routing, sub-octave per voice
2. Arpeggiator — 6 modes, gate length, bar-boundary launch
3. LFO Recording — arm/record/playback state machine
4. Expression — random trigger extensions, left joystick modulation expansion
5. Gamepad — Option Mode 1 visual feedback (knob color indication), face-button dispatch
6. Bug fix — Ableton MIDI port startup crash fix

**No SmartScreen warning in release notes** — handled inside installer welcome page instead.

**Clean Release build required** before publishing. VST3 binary timestamp must be newer than the last commit.

### Claude's Discretion

None captured.

### Deferred Ideas (OUT OF SCOPE)

- Option mode visual feedback (Phase 24 / pre-distribution hotfix — must be done before Phase 25 ships)
- Ableton MIDI port startup crash (Phase 24 / pre-distribution hotfix — must be fixed before Phase 25 ships)
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| DIST-03 | GitHub v1.5 release created with built installer binary and release notes | gh release create flow verified; plugin remote is `MiloshXhavid/Dima_Plugin_Chrdmachine`; v1.5 tag does NOT yet exist locally or on remote; pre-release flag is `--prerelease`; v1.4 release is the exact template |
| DIST-04 | Full plugin copy backed up to Desktop | `%USERPROFILE%\Desktop\Dima_plug-in\` exists (lowercase 'p'); `v1.4\` subfolder present with 3 items; `v1.5\` does not exist yet; git archive for source zip |
</phase_requirements>

---

## Summary

Phase 25 is a pure operations/release phase — no code changes, no new build system. The work is: update the `.iss` file for v1.5 (7 field changes + add `[Messages]` section + remove `LicenseFile`), perform a clean Release build, recompile the installer, rename the binary, create a v1.5 git tag, push the tag to the plugin remote, create a GitHub pre-release with the renamed binary attached, write structured release notes, and copy all artifacts to a `v1.5\` subfolder on the desktop.

The Phase 16 (v1.4) release provides an exact template for every operation. The key differences for v1.5 are: pre-release flag (not full release), new branding (`DIMEA`, `dimea.com`), removal of the license step, addition of a `[Messages]` section with SmartScreen disclaimer moved inside the installer, and the expanded v1.5 feature set in release notes.

**Current state of play (verified 2026-03-02):** The `.iss` file still has v1.4 values (AppVersion=1.4, AppName=ChordJoystick MK2, AppPublisher=Dima Xhavid, etc.). The installer Output directory has the v1.4 binary (`DimeaChordJoystickMK2-v1.4-Setup.exe`) but no v1.5 binary. No v1.5 tag exists locally or on the plugin remote. No v1.5 GitHub release exists. The desktop `v1.5\` subfolder does not exist. ISCC.exe is confirmed at `C:\Program Files (x86)\Inno Setup 6\ISCC.exe`. The VST3 source path in the `.iss` already points to `DIMEA CHORD JOYSTICK MK2.vst3` (fixed in Phase 16) — no change needed there.

**Primary recommendation:** Edit the `.iss` file (7 field changes + new `[Messages]` section + remove `LicenseFile`), perform a clean CMake Release build, run ISCC.exe to compile the installer, then follow the v1.4 release pattern exactly — tagging, pushing, `gh release create --prerelease`, and desktop copy.

---

## Standard Stack

### Core Tools

| Tool | Version | Purpose | Why Standard |
|------|---------|---------|--------------|
| `gh` CLI | system | Create GitHub pre-release, attach binary asset | Authenticated, used for v1.0–v1.4; official GitHub CLI |
| Inno Setup 6 (ISCC.exe) | 6.x, at `C:\Program Files (x86)\Inno Setup 6\ISCC.exe` | Recompile `.iss` to produce installer EXE | Existing `.iss` at `installer/DimaChordJoystick-Setup.iss`; produces output in `installer/Output/` |
| CMake + VS 2026 | existing build config | Clean Release build | Existing `build/` directory; `cmake --build build --config Release` |
| `git archive` | system git | Produce `source-v1.5.zip` from tag | Standard git subcommand; no extra tools needed |
| PowerShell | system | Copy, rename, mkdir for desktop backup | No additional tooling needed |

### Existing Artifacts (as of research date 2026-03-02)

| Artifact | Path | Status |
|----------|------|--------|
| .iss script | `installer/DimaChordJoystick-Setup.iss` | NEEDS UPDATE — still has v1.4 values; no `[Messages]` section; has `LicenseFile` directive |
| Current installer Output | `installer/Output/DimeaChordJoystickMK2-v1.4-Setup.exe` | v1.4 only — stale for v1.5 |
| Release VST3 | `build/ChordJoystick_artefacts/Release/VST3/DIMEA CHORD JOYSTICK MK2.vst3/` | Present but MUST be rebuilt clean before release |
| Plugin remote | `https://github.com/MiloshXhavid/Dima_Plugin_Chrdmachine` | v1.4 is Latest release; no v1.5 tag, no v1.5 release |
| Desktop backup root | `C:\Users\Milosh Xhavid\Desktop\Dima_plug-in\` | Exists (lowercase 'p'); `v1.4\` subfolder complete; no `v1.5\` yet |
| ISCC.exe | `C:\Program Files (x86)\Inno Setup 6\ISCC.exe` | CONFIRMED present |

---

## Architecture Patterns

### Pattern 1: Inno Setup .iss Update

**What:** Edit `installer/DimaChordJoystick-Setup.iss` — 7 field changes, remove `LicenseFile`, add `[Messages]` section.
**When to use:** Before any installer recompile.

**Complete diff of required changes:**

```ini
; [Setup] section — change these lines:
AppName=DIMEA CHORD JOYSTICK MK2
AppVersion=1.5
AppPublisher=DIMEA
AppPublisherURL=https://www.dimea.com
AppComments=v1.5 features: Single-channel routing, Sub-octave per voice, Arpeggiator (6 modes), LFO Recording, Random trigger extensions, Left joystick modulation targets, Gamepad Option Mode 1
OutputBaseFilename=DIMEA-ChordJoystickMK2-v1.5-Setup

; Remove this line entirely:
LicenseFile=..\LICENSE.txt

; [Setup] section — UninstallDisplayName (optional cosmetic update):
UninstallDisplayName=DIMEA CHORD JOYSTICK MK2 VST3

; Add new [Messages] section at the end of the file:
[Messages]
WelcomeLabel1=Welcome to DIMEA — congratulations for unlocking musical skills. Let's play some chords! Check out the Website: www.DIMEA.com
WelcomeLabel2=Hi, I am DIMEA. I am currently no certified developer by Microsoft nor MAC OS and this Plugin is currently in Beta-Phase. That's why you will see the warning PopUP by installing this File. Ignore it ;) The Plugin will enroll with all necessary certificates once it's out for the market. How cares about these warnings anyway. Have fun exploring Harmonization and give me your feedback under dimitri.daehler@dimea.com!
```

**Source path stays the same** — already correct from Phase 16:
```ini
Source: "..\build\ChordJoystick_artefacts\Release\VST3\DIMEA CHORD JOYSTICK MK2.vst3\*"; \
    DestDir: "{commoncf64}\VST3\DIMEA CHORD JOYSTICK MK2.vst3"; \
    Flags: ignoreversion recursesubdirs createallsubdirs
```

### Pattern 2: Clean Release Build

**What:** Run CMake Release build to ensure the VST3 binary is current.
**Command (PowerShell):**
```powershell
cmake --build "C:\Users\Milosh Xhavid\get-shit-done\build" --config Release
```

**Verify:** Check timestamp of `build\ChordJoystick_artefacts\Release\VST3\DIMEA CHORD JOYSTICK MK2.vst3\Contents\x86_64-win\DIMEA CHORD JOYSTICK MK2.vst3` is newer than the most recent code commit.

### Pattern 3: Installer Recompile

**What:** Run ISCC.exe to compile updated `.iss` into `DIMEA-ChordJoystickMK2-v1.5-Setup.exe`.
**Command:**
```powershell
& "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" `
    "C:\Users\Milosh Xhavid\get-shit-done\installer\DimaChordJoystick-Setup.iss"
```

**Output lands at:** `installer\Output\DIMEA-ChordJoystickMK2-v1.5-Setup.exe`
(The `OutputBaseFilename=DIMEA-ChordJoystickMK2-v1.5-Setup` directive controls this.)

### Pattern 4: Git Tag and Push to Plugin Remote

**What:** Create v1.5 tag at current HEAD and push to the `plugin` remote.
**Command (PowerShell):**
```powershell
# Check which commits are ahead of v1.4 tag
git -C "C:\Users\Milosh Xhavid\get-shit-done" log v1.4..HEAD --oneline

# Create annotated tag at HEAD
git -C "C:\Users\Milosh Xhavid\get-shit-done" tag -a v1.5 -m "v1.5 — Routing + Expression"

# Push tag to plugin remote
git -C "C:\Users\Milosh Xhavid\get-shit-done" push plugin v1.5
```

**NOTE:** The plugin remote name must be verified — prior phases used `plugin` as the remote name. Check with `git remote -v` first. Alternatively, push directly by URL:
```powershell
git -C "C:\Users\Milosh Xhavid\get-shit-done" push https://github.com/MiloshXhavid/Dima_Plugin_Chrdmachine.git v1.5
```

### Pattern 5: GitHub Pre-Release Creation

**What:** `gh release create` on the plugin remote with `--prerelease` flag, using the v1.5 tag and binary asset.
**Command:**
```bash
gh release create v1.5 \
  --repo MiloshXhavid/Dima_Plugin_Chrdmachine \
  --title "DIMEA PLUGIN - CHORD JOYSTICK MK2 v1.5" \
  --prerelease \
  --notes-file /tmp/release-notes-v1.5.md \
  "C:/Users/Milosh Xhavid/get-shit-done/installer/Output/DIMEA-ChordJoystickMK2-v1.5-Setup.exe#DIMEA-ChordJoystickMK2-v1.5-Setup.exe"
```

**Key differences from v1.4 command:**
- `--prerelease` flag is NEW (v1.4 was a full release with `isPrerelease: false`)
- `--title` uses new DIMEA branding
- Binary filename uses new `DIMEA-ChordJoystickMK2-v1.5-Setup` prefix
- No SmartScreen note in release notes body (it's in the installer instead)

**The `#label` suffix** after the asset path sets the display name on the GitHub release page.

### Pattern 6: Source Archive

**What:** `git archive` to produce a clean `source-v1.5.zip` from the `v1.5` tag.
**Command:**
```bash
git -C "C:/Users/Milosh Xhavid/get-shit-done" archive \
  --format=zip \
  --prefix=ChordJoystick-v1.5/ \
  v1.5 \
  -o "C:/Users/Milosh Xhavid/Desktop/Dima_plug-in/v1.5/source-v1.5.zip"
```

**Note:** Tag the repo BEFORE running git archive so the archive captures the correct commit.

### Pattern 7: Desktop Backup

**What:** Create `v1.5\` subfolder and copy three artifacts.
**Target:** `C:\Users\Milosh Xhavid\Desktop\Dima_plug-in\v1.5\`

**The desktop folder is `Dima_plug-in` with lowercase 'p'** — confirmed by filesystem inspection. CONTEXT.md also uses lowercase.

```powershell
# Create v1.5 subfolder
New-Item -ItemType Directory -Force -Path "$env:USERPROFILE\Desktop\Dima_plug-in\v1.5"

# Copy installer (already named correctly)
Copy-Item "C:\Users\Milosh Xhavid\get-shit-done\installer\Output\DIMEA-ChordJoystickMK2-v1.5-Setup.exe" `
          "$env:USERPROFILE\Desktop\Dima_plug-in\v1.5\"

# Copy VST3 bundle (entire directory)
Copy-Item -Recurse -Force `
  "C:\Users\Milosh Xhavid\get-shit-done\build\ChordJoystick_artefacts\Release\VST3\DIMEA CHORD JOYSTICK MK2.vst3" `
  "$env:USERPROFILE\Desktop\Dima_plug-in\v1.5\"

# source zip: done as part of git archive (output path set directly to v1.5 folder)
```

### Anti-Patterns to Avoid

- **Using stale installer binary:** The existing v1.4 binary must not be renamed and passed off as v1.5. Must recompile `.iss` after changes.
- **Missing `--prerelease` flag:** v1.5 is a pre-release. Omitting the flag creates a full release — must be corrected manually on GitHub.
- **Creating release against wrong repo:** The `origin` remote points to the GSD framework (`glittercowboy/get-shit-done`). Must use `--repo MiloshXhavid/Dima_Plugin_Chrdmachine` explicitly.
- **Forgetting `LicenseFile` removal:** If `LicenseFile=..\LICENSE.txt` remains in the `.iss`, Inno Setup will show a license agreement step during install. CONTEXT.md requires this step removed.
- **Forgetting `[Messages]` section:** Without `WelcomeLabel1` and `WelcomeLabel2` overrides, the installer shows the default Inno Setup welcome text instead of the DIMEA branding + SmartScreen disclaimer.
- **Archiving before tagging:** `git archive v1.5` fails if the v1.5 tag does not yet exist. Tag first, then archive.
- **Wrong desktop path casing:** The actual Desktop folder is `Dima_plug-in` (lowercase 'p'). Using `Dima_Plug-in` creates a duplicate folder on case-insensitive NTFS but the displayed name matters.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| GitHub release creation | Custom API calls / form submission | `gh release create` | Already proven for v1.0–v1.4; handles asset upload, tag association, pre-release flag atomically |
| Installer generation | Manual zip/copy of VST3 | Inno Setup ISCC.exe recompile | Existing `.iss` handles VST3 dir recursion, default install path, uninstaller, welcome page |
| Source archive | Manual directory copy | `git archive` from tag | Produces clean archive with no `.git/` or build artifacts; standard practice |

---

## Common Pitfalls

### Pitfall 1: Installer Not Rebuilt After .iss Edit
**What goes wrong:** Release is published with a binary that has no DIMEA branding, old AppPublisher, and still shows the license agreement screen.
**Why it happens:** The EXE in `installer/Output/` is from the Phase 16 run. Editing the `.iss` does not auto-recompile.
**How to avoid:** After every `.iss` change, run `ISCC.exe installer\DimaChordJoystick-Setup.iss`. Verify the output filename is `DIMEA-ChordJoystickMK2-v1.5-Setup.exe` (not the old name).
**Warning signs:** `OutputBaseFilename` field in `.iss` controls output name — after edit, the filename in `installer/Output/` changes.

### Pitfall 2: Missing --prerelease Flag
**What goes wrong:** v1.5 is published as a full/stable release, bumping v1.4 from "Latest" position incorrectly.
**Why it happens:** v1.4 used no pre-release flag. The v1.4 `gh release create` template omits `--prerelease`.
**How to avoid:** Explicitly add `--prerelease` to the `gh release create` command.
**Warning signs:** After creation, `gh release list --repo MiloshXhavid/Dima_Plugin_Chrdmachine` should show v1.5 with "(Pre-release)" label and v1.4 retaining "Latest".

### Pitfall 3: v1.5 Tag Not Pushed Before Release Creation
**What goes wrong:** `gh release create v1.5` fails with "tag not found" or creates the release pointing to a non-existent remote tag.
**Why it happens:** The tag only exists locally. The plugin remote does not have v1.5 yet (confirmed: only v1.0–v1.4 on remote).
**How to avoid:** Push the tag to the plugin remote (`git push plugin v1.5`) BEFORE running `gh release create`. The `gh` CLI uses the already-pushed tag.
**Warning signs:** `gh api repos/MiloshXhavid/Dima_Plugin_Chrdmachine/git/refs/tags/v1.5` returns 404 — tag not yet on remote.

### Pitfall 4: Targeting Wrong GitHub Repository
**What goes wrong:** `gh release create` creates the release in `glittercowboy/get-shit-done` (the GSD framework repo).
**Why it happens:** The `origin` remote for the working directory is the GSD repo, not the plugin repo.
**How to avoid:** Always pass `--repo MiloshXhavid/Dima_Plugin_Chrdmachine` explicitly.

### Pitfall 5: LicenseFile Directive Left In
**What goes wrong:** Installer shows a license agreement screen that user must scroll through and accept — not desired for v1.5.
**Why it happens:** `LicenseFile=..\LICENSE.txt` is currently present in the `.iss` (confirmed by inspection).
**How to avoid:** Delete the entire `LicenseFile=` line from the `[Setup]` section. Do not leave it commented out.

### Pitfall 6: v1.5.1 Folder Decision
**What goes wrong:** If `v1.5\` already exists when the desktop backup runs, overwriting it destroys the original backup.
**Why it happens:** Not applicable yet (v1.5 folder does not exist). But the CONTEXT.md fallback rule must be followed.
**How to avoid:** Check for `v1.5\` existence before creating. If present, create `v1.5.1\` instead.
**Current status:** `v1.5\` does not exist on the desktop — create `v1.5\` directly.

---

## Code Examples

### Complete .iss File Changes (v1.4 → v1.5)

```ini
; installer/DimaChordJoystick-Setup.iss — v1.5 version
; Changes from v1.4:
;   AppName, AppVersion, AppPublisher, AppPublisherURL, AppComments,
;   OutputBaseFilename, UninstallDisplayName — updated
;   LicenseFile — REMOVED
;   [Messages] section — ADDED

[Setup]
AppName=DIMEA CHORD JOYSTICK MK2
AppVersion=1.5
AppPublisher=DIMEA
AppPublisherURL=https://www.dimea.com
AppComments=v1.5: Single-Channel Routing, Sub-Octave Per Voice, Arpeggiator (6 modes), LFO Recording, Random Trigger extensions, Left Joystick Modulation targets, Gamepad Option Mode 1
OutputBaseFilename=DIMEA-ChordJoystickMK2-v1.5-Setup
OutputDir=Output
DefaultDirName={commoncf64}\VST3\DIMEA CHORD JOYSTICK MK2.vst3
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog
DisableDirPage=yes
Compression=lzma
SolidCompression=yes
UninstallDisplayName=DIMEA CHORD JOYSTICK MK2 VST3

[Files]
Source: "..\build\ChordJoystick_artefacts\Release\VST3\DIMEA CHORD JOYSTICK MK2.vst3\*"; \
    DestDir: "{commoncf64}\VST3\DIMEA CHORD JOYSTICK MK2.vst3"; \
    Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\DIMEA CHORD JOYSTICK MK2.vst3"

[Messages]
WelcomeLabel1=Welcome to DIMEA — congratulations for unlocking musical skills. Let's play some chords! Check out the Website: www.DIMEA.com
WelcomeLabel2=Hi, I am DIMEA. I am currently no certified developer by Microsoft nor MAC OS and this Plugin is currently in Beta-Phase. That's why you will see the warning PopUP by installing this File. Ignore it ;) The Plugin will enroll with all necessary certificates once it's out for the market. How cares about these warnings anyway. Have fun exploring Harmonization and give me your feedback under dimitri.daehler@dimea.com!
```

### gh Release Create Command (v1.5)

```bash
gh release create v1.5 \
  --repo MiloshXhavid/Dima_Plugin_Chrdmachine \
  --title "DIMEA PLUGIN - CHORD JOYSTICK MK2 v1.5" \
  --prerelease \
  --notes-file /tmp/release-notes-v1.5.md \
  "C:/Users/Milosh Xhavid/get-shit-done/installer/Output/DIMEA-ChordJoystickMK2-v1.5-Setup.exe#DIMEA-ChordJoystickMK2-v1.5-Setup.exe"
```

### Release Notes Body (v1.5)

```markdown
## DIMEA CHORD JOYSTICK MK2 v1.5

DIMEA CHORD JOYSTICK MK2 v1.5 is the most comprehensive update yet — adding single-channel MIDI routing, per-voice sub-octave doubling, a 6-mode arpeggiator, LFO recording, expanded random trigger modes, left joystick modulation targets, and Gamepad Option Mode 1 face-button dispatch. All features are backward-compatible with existing presets.

### New in v1.5

**Routing**
- Single-Channel mode: all voices → one user-selected MIDI channel (default is Multi-Channel, voices on ch 1–4)
- Reference-counted note collisions in Single-Channel mode: note-off only fires when the last voice on a pitch releases
- Sub-Octave per voice: toggle on any pad to fire a second note-on exactly 12 semitones below on every trigger

**Arpeggiator**
- Steps through the 4-voice chord at configurable Rate (1/4, 1/8, 1/16, 1/32) and Order (Up, Down, Up-Down, Random)
- Unified Gate Length parameter shared with Random trigger source
- Bar-boundary launch: when DAW sync is active, Arp waits for the next bar before stepping begins

**LFO Recording**
- Arm button lets you record one loop cycle of LFO output into a ring buffer
- After one cycle, LFO switches to playback — replaying stored values in sync with the looper
- Distortion stays adjustable during playback; Shape/Freq/Phase/Level/Sync are locked
- Clear button returns to normal LFO mode

**Expression**
- Random trigger sources split into Random Free (continuous) and Random Hold (fires only while pad is held)
- Population knob (1–64) and Probability knob (0–100%) replace the old RND Density/Gate controls
- Subdivision options extended to include 1/64
- Left Joystick X and Y can target: Filter Cutoff, Filter Resonance, LFO Freq, LFO Phase, LFO Level, or Gate Length
- LFO sliders visually track the joystick in real time; releasing the stick returns to the user-set base value

**Gamepad — Option Mode 1**
- Circle: toggle Arp on/off (resets looper to beat 0 on turn-on; arms if DAW sync active)
- Triangle: step Arp Rate up (1 press = step up; 2 quick = step down)
- Square: step Arp Order (same two-press architecture)
- X: toggle RND Sync
- Holding pad button (R1/R2/L1/L2) + R3: toggle Sub-Octave for that voice (any mode)

**Bug Fix**
- Fixed: Ableton MIDI port startup crash when plugin is present in default template

### Install

1. Download `DIMEA-ChordJoystickMK2-v1.5-Setup.exe`
2. Run the installer — read the welcome screen note about Windows SmartScreen
3. Admin privileges required (installs to the system VST3 folder)
4. Rescan plugins in your DAW

### Requirements

- Windows 10/11 64-bit
- VST3-compatible DAW (Ableton Live, REAPER, etc.)
- Xbox / PS4 / PS5 controller optional (for gamepad input)
```

### git Tag and Push (PowerShell)

```powershell
# Verify current position
git -C "C:\Users\Milosh Xhavid\get-shit-done" log v1.4..HEAD --oneline

# Create annotated tag at HEAD
git -C "C:\Users\Milosh Xhavid\get-shit-done" tag -a v1.5 -m "v1.5 — Routing + Expression"

# Push tag to plugin remote (adjust remote name if needed)
git -C "C:\Users\Milosh Xhavid\get-shit-done" push plugin v1.5
# OR by URL if remote name differs:
git -C "C:\Users\Milosh Xhavid\get-shit-done" push https://github.com/MiloshXhavid/Dima_Plugin_Chrdmachine.git v1.5
```

---

## State of the Art

| Aspect | v1.4 State | v1.5 Change |
|--------|-----------|-------------|
| GitHub release type | Full release (`isPrerelease: false`) | Pre-release (`--prerelease`) |
| Release title | `ChordJoystick v1.4 — LFO + Beat Clock` | `DIMEA PLUGIN - CHORD JOYSTICK MK2 v1.5` |
| AppPublisher | `Dima Xhavid` | `DIMEA` |
| AppPublisherURL | `https://gumroad.com` | `https://www.dimea.com` |
| Installer filename | `DimeaChordJoystickMK2-v1.4-Setup.exe` | `DIMEA-ChordJoystickMK2-v1.5-Setup.exe` |
| License step | Present (`LicenseFile=..\LICENSE.txt`) | Removed |
| SmartScreen note | In release notes body | In installer welcome page (`[Messages]` section) |
| Plugin remote tag v1.5 | Does not exist | Must create and push |

**What is unchanged from v1.4 pattern:**
- VST3 source path in `.iss` (`DIMEA CHORD JOYSTICK MK2.vst3`) — already correct
- DestDir path — unchanged
- UninstallDelete entry — unchanged
- ISCC.exe location — unchanged
- gh CLI `--repo` flag — unchanged
- Desktop backup parent folder (`Dima_plug-in\`) — unchanged

---

## Open Questions

1. **What is the exact `plugin` remote name in this repo?**
   - What we know: Phase 16 RESEARCH.md refers to the remote as `plugin` (`plugin` → `MiloshXhavid/Dima_Plugin_Chrdmachine`). The bash shell cannot run `git remote -v` due to path space issue.
   - What's unclear: Whether the remote is still named `plugin` or has been renamed since Phase 16.
   - Recommendation: Planner task should run `git remote -v` (via PowerShell) as the first sub-step. If `plugin` is not found, push by URL directly: `git push https://github.com/MiloshXhavid/Dima_Plugin_Chrdmachine.git v1.5`.

2. **Should the source archive include the `.planning/` directory?**
   - What we know: `git archive` archives all tracked files at the commit. `.planning/` files are tracked in `.gitignore`-excluded vs tracked state is unclear.
   - What's unclear: Whether `.planning/` is in `.gitignore` or tracked.
   - Recommendation: `git archive` only includes tracked files, so this resolves itself. No action needed.

3. **Does the Phase 24.1 completion commit affect whether a rebuild is needed?**
   - What we know: The installer must be built AFTER Phase 24.1 is complete (per CONTEXT.md: "build must be performed after Phase 24 is fully complete"). The latest commit on the plugin remote is a Phase 15-era commit, confirming the plugin remote has NOT been updated with v1.5 code.
   - What's unclear: Whether Phase 24.1 is fully merged and verified by the time Phase 25 runs.
   - Recommendation: Planner gate: Phase 25 tasks must not start until Phase 24.1 human verification is confirmed. The first task of Phase 25 is the clean build — this naturally enforces the dependency.

---

## Sources

### Primary (HIGH confidence)
- Direct inspection of `installer/DimaChordJoystick-Setup.iss` — confirmed all current values
- `Glob installer/Output/*.exe` — confirmed current installer filenames in Output directory
- `Glob Desktop/Dima_plug-in/**` — confirmed folder name (lowercase 'p'), v1.4 subfolder complete, no v1.5 subfolder
- `gh release list --repo MiloshXhavid/Dima_Plugin_Chrdmachine` — confirmed v1.4 is Latest, no v1.5 release
- `gh release view v1.4 --json isPrerelease` — confirmed v1.4 was `isPrerelease: false`
- `gh api repos/MiloshXhavid/Dima_Plugin_Chrdmachine/git/refs/tags/v1.5` → 404 — confirmed no v1.5 tag on remote
- `gh api repos/MiloshXhavid/Dima_Plugin_Chrdmachine/git/refs/tags` — confirmed remote has only v1.0–v1.4
- `Glob C:/Program Files (x86)/ISCC.exe` — confirmed ISCC.exe is present
- `Glob build/ChordJoystick_artefacts/Release/VST3/*.vst3` — confirmed `DIMEA CHORD JOYSTICK MK2.vst3` exists in build output
- `.planning/phases/25-distribution/25-CONTEXT.md` — all locked decisions

### Secondary (MEDIUM confidence)
- `.planning/phases/16-distribution/16-RESEARCH.md` — v1.4 release pattern as template; exact gh CLI commands verified against that successful release
- Inno Setup 6 `[Messages]` section documentation — `WelcomeLabel1` and `WelcomeLabel2` are standard Inno Setup message constants for the welcome wizard page

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all tools confirmed present; ISCC.exe path verified
- Architecture: HIGH — v1.4 release is a proven template; every step has been done before
- Pitfalls: HIGH — all pitfalls derived from direct inspection of current artifact state
- .iss changes: HIGH — read current file, derived exact delta from CONTEXT.md requirements

**Research date:** 2026-03-02
**Valid until:** 2026-04-01 (30 days — stable tooling)
