# Phase 16: Distribution — Research

**Researched:** 2026-02-26
**Domain:** GitHub Release publishing, Windows binary packaging, local backup
**Confidence:** HIGH

<user_constraints>
## User Constraints (from CONTEXT.md)

### Locked Decisions

**Tag:** `v1.4`
- The "v1.5" label in STATE.md was informal bookkeeping. Everything in phases 12–15 ships under a single `v1.4` tag.

**GitHub release title:** `ChordJoystick v1.4 — LFO + Beat Clock`

**Release scope:** All phases 12–15 are first-class features:
- Phase 12–13: Dual-axis LFO engine (7 waveforms, sync, distortion, level)
- Phase 14: LFO UI panel + beat clock dot
- Phase 15: Gamepad Preset Control (Option button → MIDI Program Change)

No "v1.3 previously" section — release notes are stand-alone for v1.4.

**Product name:** Dimea Chord Joystick MK2 / **Company:** Dimea Sound

**Release notes format:** Narrative paragraph (1–3 sentences intro) + bullet list grouped by feature category:
1. LFO System — dual-axis, 7 waveforms, sync/free, phase, distortion, level
2. Beat Clock — flashing dot on BPM knob, follows DAW or free tempo
3. Gamepad Preset Control — Option button toggle, D-pad sends MIDI PC, UI indicator

**Required installation note:** Windows SmartScreen warning — click "More info" → "Run anyway".

**Release asset filename:** `DimeaChordJoystickMK2-v1.4-Setup.exe`
- Rename `DimaChordJoystickMK2-Setup.exe` before attaching.

**Desktop backup path:** `%USERPROFILE%\Desktop\Dima_plug-in\v1.4\`
**Backup contents:**
- `DimeaChordJoystickMK2-v1.4-Setup.exe` — the installer binary
- The built VST3 bundle (`.vst3` folder from build output)
- `source-v1.4.zip` — zip of the repository at the `v1.4` git tag

**Build verification required before release:**
- Check `installer/Output/DimaChordJoystickMK2-Setup.exe` was built after last commit (phase 15-02: `1720a71`)
- If older, perform a clean Release build first

**Manual smoke test required before publishing:**
- Load VST3 in DAW
- Confirm LFO modulation audible on both axes
- Confirm Option button toggles preset-scroll mode + UI shows "OPTION" indicator

**If `Dima_plug-in\` already exists:** overwrite/replace (do not create a parallel folder)

### Claude's Discretion

None captured.

### Deferred Ideas (OUT OF SCOPE)

None captured during this discussion.
</user_constraints>

<phase_requirements>
## Phase Requirements

| ID | Description | Research Support |
|----|-------------|-----------------|
| DIST-01 | GitHub v1.4 release created with built installer binary and release notes | gh CLI release create flow; plugin remote is `MiloshXhavid/Dima_Plugin_Chrdmachine`; v1.4 tag already pushed to plugin remote; v1.3 release is the exact template |
| DIST-02 | Full plugin copy backed up to Desktop/Dima_plug-in | `%USERPROFILE%\Desktop\Dima_Plug-in\` already exists; backup needs `v1.4\` subfolder with 3 items; git archive for source zip |
</phase_requirements>

---

## Summary

Phase 16 is a pure operations/release phase — no code changes, no new build system. The work is: verify the installer is current, rebuild if stale, rename the binary, push a git tag to the plugin remote, create the GitHub release with the renamed binary attached, write structured release notes, and copy artifacts to a versioned desktop folder.

The project has a clear prior art template: the v1.3 release at `https://github.com/MiloshXhavid/Dima_Plugin_Chrdmachine/releases/tag/v1.3` used `gh release create` with a `--notes` body and a binary asset upload. That exact flow applies here. The key operational risk is that **the installer binary is stale**: `DimaChordJoystickMK2-Setup.exe` is dated Feb 25 12:44, but phase 15 code commits landed on Feb 26 07:33-07:34. The installer must be rebuilt before publishing.

The `v1.4` git tag already exists on the `plugin` remote (`ff146af05bac816bf00974ebc00d0216a0d43fff`) but no GitHub release has been created yet (`gh release view v1.4` returns "release not found"). The installer packaging uses Inno Setup 6 with the `.iss` file at `installer/DimaChordJoystick-Setup.iss`, and the CMake-built product name is `DIMEA CHORD JOYSTICK MK2`.

**Primary recommendation:** Rebuild the installer first (Inno Setup recompile), rename the output binary, then use `gh release create` targeting the `plugin` remote with the binary as an attached asset. Then copy all three artifacts (installer, VST3 bundle, source zip) to `Desktop\Dima_Plug-in\v1.4\`.

---

## Standard Stack

### Core Tools

| Tool | Version | Purpose | Why Standard |
|------|---------|---------|--------------|
| `gh` CLI | system | Create GitHub release, attach binary asset | Already authenticated, used for v1.0–v1.3; official GitHub CLI |
| Inno Setup 6 | 6.7.1 | Rebuild installer from `.iss` script | Existing `.iss` at `installer/DimaChordJoystick-Setup.iss`; produces `DimaChordJoystickMK2-Setup.exe` |
| CMake + VS 2026 | existing build | Release build if installer is stale | Existing `build/` config; project VERSION 2.0.0 in CMakeLists.txt |
| `git archive` | system git | Produce `source-v1.4.zip` from tag | Standard git subcommand; no extra tools needed |
| PowerShell / bash | system | Copy, rename, mkdir for desktop backup | No additional tooling needed |

### Existing Artifacts (as of research date)

| Artifact | Path | Status |
|----------|------|--------|
| Installer EXE | `installer/Output/DimaChordJoystickMK2-Setup.exe` | **STALE** — dated Feb 25 12:44, predates phase 15 commits (Feb 26 07:33) |
| Release VST3 | `build/ChordJoystick_artefacts/Release/VST3/DIMEA CHORD JOYSTICK MK2.vst3/` | Latest — dated Feb 26 07:11, post phase 15 build |
| Plugin remote | `https://github.com/MiloshXhavid/Dima_Plugin_Chrdmachine` | `v1.4` tag pushed, no GitHub release yet |
| Desktop backup | `C:\Users\Milosh Xhavid\Desktop\Dima_Plug-in\` | Exists; contains `ChordJoystick_MK2.zip` and `README.txt` from earlier — needs `v1.4\` subfolder |

---

## Architecture Patterns

### Pattern 1: Installer Rebuild (Inno Setup)

**What:** Recompile `.iss` script to produce a fresh installer that bundles the current Release VST3.
**When to use:** Installer EXE is older than the most recent build output — which is the case here.
**The `.iss` configuration:**
- Source VST3: `"..\build\ChordJoystick_artefacts\Release\VST3\ChordJoystick MK2.vst3\*"` — **NOTE: this references `ChordJoystick MK2.vst3`, but the current PRODUCT_NAME is `DIMEA CHORD JOYSTICK MK2`**. The `.iss` file must be updated to reference the correct VST3 bundle name before recompiling, or the installer will bundle the wrong (or missing) plugin.
- Output filename: `DimaChordJoystickMK2-Setup` → output lands in `installer/Output/`
- Requires admin: yes (`PrivilegesRequired=admin`)

**CRITICAL ISS FIX NEEDED:** The current `DimaChordJoystick-Setup.iss` sources from `ChordJoystick MK2.vst3` but the CMakeLists PRODUCT_NAME is `DIMEA CHORD JOYSTICK MK2` (so the Release VST3 is at `DIMEA CHORD JOYSTICK MK2.vst3`). The `.iss` must be updated to:
- `AppVersion=1.4`
- `AppComments=v1.4 features: ...`
- `Source: "..\build\ChordJoystick_artefacts\Release\VST3\DIMEA CHORD JOYSTICK MK2.vst3\*"`
- `DestDir: "{commoncf64}\VST3\DIMEA CHORD JOYSTICK MK2.vst3"`

### Pattern 2: GitHub Release Creation

**What:** `gh release create` on the `plugin` remote, targeting the already-pushed `v1.4` tag, with the renamed binary as an asset.
**Command pattern (verified from v1.3 precedent):**

```bash
# Rename binary first
cp "installer/Output/DimaChordJoystickMK2-Setup.exe" \
   "installer/Output/DimeaChordJoystickMK2-v1.4-Setup.exe"

# Create release on plugin remote with asset upload
gh release create v1.4 \
  --repo MiloshXhavid/Dima_Plugin_Chrdmachine \
  --title "ChordJoystick v1.4 — LFO + Beat Clock" \
  --notes "$(cat <<'EOF'
[release notes body]
EOF
)" \
  "installer/Output/DimeaChordJoystickMK2-v1.4-Setup.exe"
```

**Key flag:** `--repo MiloshXhavid/Dima_Plugin_Chrdmachine` — the `gh` command must target the plugin repo, not the gsd `origin`.

**Tag already exists on remote:** `v1.4` is already pushed (`ff146af05bac816bf00974ebc00d0216a0d43fff`). Do NOT use `--verify-tag` as the tag commit may differ from local HEAD (it was pushed ahead of phase 15 documentation commits). Use `gh release create v1.4` directly without `--target`; `gh` will use the existing remote tag.

### Pattern 3: Source Archive

**What:** `git archive` to produce a clean source zip from the `v1.4` tag.
**Command:**

```bash
git -C "C:/Users/Milosh Xhavid/get-shit-done" archive \
  --format=zip \
  --prefix=chordjoy-v1.4/ \
  v1.4 \
  -o "source-v1.4.zip"
```

Note: `git archive` of a tag produces a clean archive (no `.git/`). The `--prefix` sets the top-level folder name inside the zip. The output path should be a temp location; then copy to desktop backup.

**WARNING:** `git archive` only archives commits reachable from the tag. If the local `v1.4` tag points to an older commit (before phase 15 documentation), the archive will not include phase 15 docs. Verify the local tag or use the remote tag commit hash directly. Alternatively, archive `HEAD` and rename — but that may include uncommitted files. Safest: push current HEAD to plugin remote, retag `v1.4` locally to HEAD, then archive.

### Pattern 4: Desktop Backup

**What:** Create versioned subfolder and copy three artifacts.
**Target:** `C:\Users\Milosh Xhavid\Desktop\Dima_Plug-in\v1.4\`

```powershell
# Create v1.4 subfolder (parent Dima_Plug-in already exists)
New-Item -ItemType Directory -Force -Path "$env:USERPROFILE\Desktop\Dima_Plug-in\v1.4"

# Copy installer (renamed)
Copy-Item "installer\Output\DimeaChordJoystickMK2-v1.4-Setup.exe" `
          "$env:USERPROFILE\Desktop\Dima_Plug-in\v1.4\"

# Copy VST3 bundle (entire directory)
Copy-Item -Recurse -Force `
  "build\ChordJoystick_artefacts\Release\VST3\DIMEA CHORD JOYSTICK MK2.vst3" `
  "$env:USERPROFILE\Desktop\Dima_Plug-in\v1.4\"

# Copy source zip
Copy-Item "source-v1.4.zip" "$env:USERPROFILE\Desktop\Dima_Plug-in\v1.4\"
```

Note: CONTEXT.md says "overwrite/replace existing contents" — but since `v1.4\` is a new subfolder, there is nothing to overwrite. The existing `Dima_Plug-in\` root (with `ChordJoystick_MK2.zip` and `README.txt`) is left untouched.

### Anti-Patterns to Avoid

- **Uploading the stale installer:** The existing `DimaChordJoystickMK2-Setup.exe` (Feb 25) does not include phase 15 code. Must rebuild.
- **Using wrong VST3 source path in .iss:** The `.iss` currently references `ChordJoystick MK2.vst3` — this is the v1.1/v1.2 era name. The current build output is `DIMEA CHORD JOYSTICK MK2.vst3`. Using the old path either bundles wrong plugin or fails compilation.
- **Creating release against wrong repo:** The `origin` remote is `glittercowboy/get-shit-done` (the GSD framework). The plugin remote is `plugin` → `MiloshXhavid/Dima_Plugin_Chrdmachine`. Must use `--repo MiloshXhavid/Dima_Plugin_Chrdmachine`.
- **Omitting SmartScreen note:** CONTEXT.md requires the unsigned-binary SmartScreen warning in release notes. Missing it leads to user confusion.
- **Archiving uncommitted files in source zip:** Use `git archive` from the tag, not a directory copy.

---

## Don't Hand-Roll

| Problem | Don't Build | Use Instead | Why |
|---------|-------------|-------------|-----|
| GitHub release creation | Custom API calls | `gh release create` | Already proven for v1.0–v1.3; handles asset upload, tag association, and publish atomically |
| Installer generation | Manual zip/copy | Inno Setup `.iss` recompile | Existing `.iss` already configured; handles VST3 dir recursion, default install path, uninstaller |
| Source archive | Manual file copy | `git archive` | Produces clean archive from tag with no `.git/` or build artifacts |

---

## Common Pitfalls

### Pitfall 1: Stale Installer Binary
**What goes wrong:** Release is published with Feb 25 installer that lacks phase 15 Gamepad Preset Control code. Users download a plugin missing the OPTION indicator and PC routing.
**Why it happens:** Installer was last rebuilt during phase 11 work and not rebuilt after phase 12–15 code changes.
**How to avoid:** Before any rename or upload, check file timestamp: `DimaChordJoystickMK2-Setup.exe` must be newer than 2026-02-26 07:34 (the feat(15-02) commit). If not, recompile `.iss` in Inno Setup.
**Warning signs:** Installer dated Feb 25 (confirmed by `ls -la installer/Output/`).

### Pitfall 2: Wrong VST3 Source Path in .iss
**What goes wrong:** Inno Setup compilation either fails (source path not found) or bundles the old `ChordJoystick MK2.vst3` binary from Feb 23 instead of the current `DIMEA CHORD JOYSTICK MK2.vst3`.
**Why it happens:** The `.iss` was last updated during v1.3 when the product name was `ChordJoystick MK2`. CMakeLists now uses `DIMEA CHORD JOYSTICK MK2` as PRODUCT_NAME.
**How to avoid:** Update `.iss` Source path to `DIMEA CHORD JOYSTICK MK2.vst3` and DestDir to match. Also bump `AppVersion=1.4`.
**Warning signs:** `.iss` line 26 reads `ChordJoystick MK2.vst3` — must be changed.

### Pitfall 3: Targeting Wrong GitHub Repository
**What goes wrong:** `gh release create` creates the release in `glittercowboy/get-shit-done` (the GSD framework repo) instead of the plugin repo.
**Why it happens:** The default `origin` remote is the GSD repo. `gh` uses the current directory's `origin` by default.
**How to avoid:** Always pass `--repo MiloshXhavid/Dima_Plugin_Chrdmachine` explicitly.
**Warning signs:** Check `gh release list` output — if it shows GSD framework releases, wrong repo.

### Pitfall 4: Local v1.4 Tag Points to Wrong Commit
**What goes wrong:** `git archive v1.4` produces a zip that misses phase 15 documentation commits (3b4bb79 etc.) because the local tag predates those commits.
**Why it happens:** Tag may have been created before phase 15 planning docs were committed.
**How to avoid:** Run `git log v1.4..HEAD --oneline` to see how many commits are ahead of the tag. If commits exist, decide whether to retag (force-push `v1.4` to HEAD) or accept the gap. For a source archive, the user may prefer the HEAD state.
**Warning signs:** `git describe --tags` shows `v1.4-N-gXXXXXXX` where N > 0.

### Pitfall 5: Desktop Path Case Sensitivity
**What goes wrong:** Scripts create `Dima_plug-in` (lowercase 'p') instead of using the existing `Dima_Plug-in` (capital 'P') folder, resulting in a duplicate folder.
**Why it happens:** CONTEXT.md uses `Dima_plug-in` (lowercase) but the actual Desktop folder is `Dima_Plug-in` (capital P, confirmed by `ls Desktop/`).
**How to avoid:** Use the actual folder name `Dima_Plug-in`. On Windows, PowerShell `mkdir` is case-insensitive at filesystem level but the displayed name matters. Target: `$env:USERPROFILE\Desktop\Dima_Plug-in\v1.4\`.

---

## Code Examples

### Release Notes Body (v1.4 template — follows v1.3 structure)

```markdown
## Dimea Chord Joystick MK2 v1.4

Dimea Chord Joystick MK2 v1.4 adds a full dual-axis LFO system, a beat clock
indicator, and gamepad preset navigation — the biggest update since launch.
All features ship as a single installer with no breaking changes to existing presets.

### New in v1.4

**LFO System**
- Dual-axis LFO independently controls X and Y joystick modulation
- 7 waveforms: Sine, Triangle, Saw Up, Saw Down, Square, Sample & Hold, Random
- Sync mode locks LFO phase to DAW BPM; Free mode runs at any rate (0.01–20 Hz)
- Per-LFO controls: Phase shift (0–360°), Distortion (additive jitter), Level (depth)

**Beat Clock**
- Flashing dot on the Free BPM knob pulses once per beat
- Follows DAW transport when DAW sync is active; follows Free BPM otherwise

**Gamepad Preset Control**
- Option / Guide button toggles preset-scroll mode
- In preset-scroll mode, D-pad Up/Down send MIDI Program Change +1 / −1
- Plugin UI shows "OPTION" indicator when preset-scroll mode is active

### Install

1. Download `DimeaChordJoystickMK2-v1.4-Setup.exe`
2. Run the installer — Windows SmartScreen will show a security warning because
   this binary is unsigned. Click **More info** → **Run anyway** to proceed.
3. Admin privileges required (installs to the system VST3 folder)
4. Rescan plugins in your DAW

### Requirements

- Windows 10/11 64-bit
- VST3-compatible DAW (Ableton Live, REAPER, etc.)
- Xbox / PS4 / PS5 controller optional (for gamepad input)
```

### gh Release Create Command

```bash
gh release create v1.4 \
  --repo MiloshXhavid/Dima_Plugin_Chrdmachine \
  --title "ChordJoystick v1.4 — LFO + Beat Clock" \
  --notes-file /tmp/release-notes-v1.4.md \
  "C:/Users/Milosh Xhavid/get-shit-done/installer/Output/DimeaChordJoystickMK2-v1.4-Setup.exe#DimeaChordJoystickMK2-v1.4-Setup.exe"
```

The `#label` suffix after the asset path sets the display name on the release page.

### Inno Setup .iss Updates Required

```ini
; Update these lines in DimaChordJoystick-Setup.iss before recompiling:
AppVersion=1.4
AppComments=v1.4 features: Dual-axis LFO engine (7 waveforms, sync, distortion, level), Beat clock dot on BPM knob, Gamepad Preset Control (Option button + MIDI Program Change)

; Update Source path — was "ChordJoystick MK2.vst3", now:
Source: "..\build\ChordJoystick_artefacts\Release\VST3\DIMEA CHORD JOYSTICK MK2.vst3\*"; \
    DestDir: "{commoncf64}\VST3\DIMEA CHORD JOYSTICK MK2.vst3"; \
    Flags: ignoreversion recursesubdirs createallsubdirs

; Update UninstallDelete name too:
Type: filesandordirs; Name: "{commoncf64}\VST3\DIMEA CHORD JOYSTICK MK2.vst3"
```

### git archive for Source Zip

```bash
# Check how far HEAD is ahead of v1.4 tag
git -C "C:/Users/Milosh Xhavid/get-shit-done" log v1.4..HEAD --oneline

# Archive from HEAD (includes all phase 15 docs) — safer than the stale tag
git -C "C:/Users/Milosh Xhavid/get-shit-done" archive \
  --format=zip \
  --prefix=ChordJoystick-v1.4/ \
  HEAD \
  -o "C:/Users/Milosh Xhavid/Desktop/Dima_Plug-in/v1.4/source-v1.4.zip"
```

---

## State of the Art

| Aspect | Current State | Note |
|--------|--------------|-------|
| GitHub release tooling | `gh release create` with asset file path arg | Proven for v1.0–v1.3 on this project |
| Installer | Inno Setup 6.7.1 `.iss` script | Last compiled for v1.3; needs version + source path update |
| VST3 product name | `DIMEA CHORD JOYSTICK MK2` | Changed from `ChordJoystick MK2` (v1.1–v1.3 era) |
| Plugin remote tag | `v1.4` already pushed | No GitHub release attached yet |
| Desktop backup | `Dima_Plug-in\` exists (root) | Needs `v1.4\` subfolder created |

---

## Open Questions

1. **Does the local v1.4 tag point to HEAD or an earlier commit?**
   - What we know: `plugin` remote has `v1.4` at `ff146af05bac816bf00974ebc00d0216a0d43fff`; HEAD is at `3b4bb79`
   - What's unclear: Whether local `v1.4` and remote `v1.4` point to the same commit, and whether the planner should retag HEAD as `v1.4` before archiving source
   - Recommendation: Planner task should run `git log v1.4..HEAD --oneline` first. If commits exist (likely: phase 15 docs), retag `v1.4` to HEAD locally, force-push to `plugin` remote, then archive. The GitHub release tag will update to point at the correct commit.

2. **Does Inno Setup need to be invoked from command line or GUI?**
   - What we know: Inno Setup 6 ships `ISCC.exe` (command-line compiler) and Compil32 (GUI)
   - What's unclear: Whether `ISCC.exe` is on PATH in this environment
   - Recommendation: Planner task should attempt `ISCC.exe installer/DimaChordJoystick-Setup.iss`; if not found on PATH, provide the full default path `C:\Program Files (x86)\Inno Setup 6\ISCC.exe`. Fallback: user opens Compil32 manually.

3. **Smoke test: can the planner automate it?**
   - What we know: Smoke test requires loading VST3 in DAW and manually verifying LFO + OPTION indicator
   - What's unclear: Whether this can be scripted
   - Recommendation: Treat smoke test as a manual human verification step in the plan. Mark it as a blocking gate before the `gh release create` command runs.

---

## Sources

### Primary (HIGH confidence)
- Direct inspection of `installer/Output/` — file timestamps confirm installer staleness
- Direct inspection of `build/ChordJoystick_artefacts/Release/VST3/` — confirms current VST3 bundle name
- `gh release list -R MiloshXhavid/Dima_Plugin_Chrdmachine` — confirms v1.3 is latest release, v1.4 not yet created
- `gh release view v1.3 -R MiloshXhavid/Dima_Plugin_Chrdmachine` — exact v1.3 release structure as template
- `git ls-remote --tags plugin` — confirms `v1.4` tag is already on plugin remote
- `installer/DimaChordJoystick-Setup.iss` — confirmed source path mismatch with current VST3 name
- `CMakeLists.txt` line 109 — confirmed PRODUCT_NAME is `DIMEA CHORD JOYSTICK MK2`
- `Desktop/Dima_Plug-in/` listing — folder exists with older content; no `v1.4\` subfolder yet
- `gh release create --help` — confirmed asset upload syntax with `#label` suffix
- `.planning/phases/16-distribution/16-CONTEXT.md` — all locked decisions

### Secondary (MEDIUM confidence)
- Inno Setup ISCC.exe command-line interface — standard feature of Inno Setup 6; default install path `C:\Program Files (x86)\Inno Setup 6\ISCC.exe` is well-documented but not verified in this environment

---

## Metadata

**Confidence breakdown:**
- Standard stack: HIGH — all tools (gh CLI, Inno Setup, git archive) confirmed present/used in project
- Architecture: HIGH — v1.3 release is a verified template; exact commands known
- Pitfalls: HIGH — stale installer and wrong VST3 path confirmed by direct file inspection; wrong-repo pitfall confirmed by remote listing
- Open questions: MEDIUM — Inno Setup CLI availability is inferred, not confirmed; tag retagging decision depends on plan author

**Research date:** 2026-02-26
**Valid until:** 2026-03-28 (30 days — stable tooling; gh CLI syntax unlikely to change)
