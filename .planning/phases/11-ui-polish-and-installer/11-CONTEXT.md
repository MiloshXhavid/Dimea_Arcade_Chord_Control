# Phase 11: UI Polish and Installer - Context

**Gathered:** 2026-02-25
**Status:** Ready for planning

<domain>
## Phase Boundary

Polish the plugin UI with rounded-rect section panels (Filter Mod / Looper / Gamepad), add a looper position progress bar inside the Looper panel, upgrade the gamepad status label to show specific controller type names, and rebuild the installer as version 1.3.

Technical contract (from success criteria, locked):
- Filter Mod, Looper, and Gamepad sections each have a visible rounded-rect panel with a label floating on the top border
- Gamepad status label shows "PS4 Connected", "PS5 Connected", "Xbox Connected", "Controller Connected" (generic fallback), or "No controller" (disconnected)
- Looper position bar sweeps left-to-right continuously during playback; does not jump backward at loop wrap; resets to empty when stopped
- Installer version string = 1.3 (GitHub release will be tagged v1.3)

</domain>

<decisions>
## Implementation Decisions

### Section grouping — rounded rect panels

**Visual treatment:**
- Each section (LOOPER, FILTER MOD, GAMEPAD) gets a rounded rectangle panel
- Panel fill: `Clr::panel` slightly brighter (e.g. `Clr::panel.brighter(0.12f)`) — same approach used elsewhere (e.g. pill toggle background)
- Panel border: `Clr::accent.brighter(0.5f)` — consistent with existing outer border style
- Corner radius: Claude's discretion — 6–8px recommended to match the pill/button aesthetic

**Section label:**
- Position: centered, inset on the top edge of the panel border (floating/knockout style)
- The label sits centered over the top border line, drawn on top of the border (bg color behind text to create the knockout)
- Font: `Clr::textDim`, 9–10px bold
- Text: "LOOPER", "FILTER MOD", "GAMEPAD"

**Layout reorganization:**
- GAMEPAD and FILTER MOD controls are currently mixed in the same row — they must be moved into separate panels with distinct visual zones
- LOOPER section: left column — wraps PLAY/REC/RST/DEL buttons, mode buttons, subdiv/length controls, and the new position bar
- FILTER MOD section: right column bottom — wraps FILTER MOD ON/OFF, REC FILTER, filterXModeBox_, filterYModeBox_, and the filter attenuator/offset knobs
- GAMEPAD section: right column bottom — wraps GAMEPAD ON/OFF button, the PANIC! button (from Phase 09), and the gamepadStatusLabel_
- Exact pixel layout of the reorganization is Claude's discretion; honor existing control sizes and spacing conventions

### Looper position bar

**Placement:** Full-width strip inside the Looper panel, between the mode buttons row (REC GATES/REC JOY/REC TOUCH/DAW SYNC) and the subdiv/length controls row

**Dimensions:** Full width of the Looper panel, 10px tall (Claude may adjust ±2px to fit neatly)

**Visual style:**
- Filled sweep: `Clr::gateOn` (bright green, `0xFF00E676`) fill from left edge to current position fraction
- Background track: dark fill (e.g. `Clr::gateOff` or `Clr::panel`)
- No text overlay — purely visual

**Behavior:**
- Idle / stopped: empty (no fill), position at 0
- Playing: fill sweeps left-to-right proportional to `LooperEngine::getPlaybackPosition()` (fraction 0.0–1.0 of loop length)
- Loop wrap: position resets smoothly to 0 (no backward jump visible to user — since getPlaybackPosition() returns 0 at wrap boundary, the bar simply starts over from the left)
- Recording: show position sweep same as playback (so user can see current record head)

**Implementation notes:**
- Drawn in `paint()` using the existing 30 Hz editor timer for updates (no new timer)
- A dedicated member `juce::Rectangle<int> looperPositionBarBounds_` should be stored in `resized()` and read in `paint()`

### Gamepad controller name detection

**Label widget:** Reuse existing `gamepadStatusLabel_` — update text only, no new widget

**Text strings (locked):**
| State | Label text | Color |
|-------|-----------|-------|
| No controller connected | "No controller" | `Clr::textDim` |
| PS4 DualShock 4 | "PS4 Connected" | `Clr::gateOn` |
| PS5 DualSense | "PS5 Connected" | `Clr::gateOn` |
| Xbox (any variant) | "Xbox Connected" | `Clr::gateOn` |
| Any other controller | "Controller Connected" | `Clr::gateOn` |

**Color behavior:** Permanent state change — connected = `Clr::gateOn` (green), disconnected = `Clr::textDim` (grey). No flash animation.

**Detection logic (SDL2 name matching):**
- Check `SDL_GameControllerName()` — case-insensitive substring match
  - Contains "DualSense" → PS5
  - Contains "DualShock 4" or "PS4" → PS4
  - Contains "Xbox" or "xinput" → Xbox
  - Anything else connected → generic fallback

**Integration point:** `GamepadInput::onConnectionChangeUI` callback — pass controller name string (or empty string on disconnect) to the label update lambda in PluginEditor

### Installer — version 1.3

**Version string:** `1.3` (not 1.1 — GitHub release will be tagged v1.3)

**Install path:** No change — same VST3 destination, same product name, same GUID as v1.0 installer

**Information page — v1.3 feature list:**
- Section visual grouping (LOOPER / FILTER MOD / GAMEPAD panels)
- Gamepad controller name display (PS4/PS5/Xbox detection)
- Trigger quantization (live and post-record snap-to-grid)
- MIDI Panic (full 16-channel sweep)

**No other installer changes** — no EULA update, no "What's New" wizard page, no path changes

**File to update:** `installer/installer.iss` — bump `AppVersion`, update the feature list in the info/description text

</decisions>

<specifics>
## Specific Ideas

- The knockout label technique: draw a small filled rect in `Clr::bg` (or panel fill) behind the label text, then draw the text — this visually "breaks" the top border line at the label position, creating the inset look
- The looper position bar bounds should be calculated in `resized()` and cached as `looperPositionBarBounds_` — avoids recalculating geometry on every paint call
- SDL2 name check: `SDL_GameControllerName()` returns a const char* (may be null if not a game controller); use `juce::String::containsIgnoreCase()` for the pattern matching
- MIDI Panic button from Phase 09 should be co-located in the GAMEPAD panel — the CONTEXT.md for Phase 09 places it in the same row as the status label: `PS4 Connected         [ PANIC! ]`
- The ARP block at the bottom of the left column is separate from the Looper section — it does not need a panel (it's a distinct feature)

</specifics>

<deferred>
## Deferred Ideas

- MIDI Panic feature list entry in installer: included in v1.3 list even though it's Phase 09 (user confirmed it's a v1.1/v1.3 milestone feature)

</deferred>

---

*Phase: 11-ui-polish-and-installer*
*Context gathered: 2026-02-25*
