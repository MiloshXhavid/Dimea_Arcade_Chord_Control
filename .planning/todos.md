# Project TODOs

Captured feature requests and improvement ideas for upcoming phases.

---

## TODO: Quantize UI Grouping Polish

**Requested:** 2026-02-25
**Status:** completed

- Add a visible "Quantize Trigger" section label above the Off/Live/Post buttons in the Looper section
- Widen the quantize subdivision ComboBox (quantizeSubdivBox_) so "1/32" text is fully visible — currently shows "..." because the box is too narrow
- Ensure the quantize controls area is visually grouped/distinct from the rest of the looper controls

**Files:** Source/PluginEditor.cpp (resized() + possibly constructor for label component)
**Phase:** Consider adding to Phase 11 (UI Polish) or as a standalone gap closure

---

## TODO: Joystick -> LFO freq: UI fader doesn't track joystick movement visually

**Requested:** 2026-03-01
**Status:** pending

When the gamepad left stick is routed to control LFO frequency, moving the stick changes the LFO rate but the frequency fader in the LFO panel doesn't visually update to reflect the current value. Fix: ensure the joystick->LFO freq routing updates the APVTS parameter (or at least the UI display value) so the fader tracks in sync.

**Files:** Source/PluginEditor.cpp, Source/PluginProcessor.cpp (wherever joystick->LFO freq routing is applied)
**Phase:** LFO bug fix — candidate for a gap closure or v1.1 patch

---

## TODO: Joystick -> LFO freq: slider value = center offset (joystick modulates around slider position)

**Requested:** 2026-03-01
**Status:** pending

For the gamepad left stick -> LFO freq routing: the LFO frequency slider's current value should define the "center" — i.e., with joystick at rest (center), LFO freq = slider value. Moving joystick up/down offsets above/below that center. This allows players with spring-loaded joysticks to set a useful base rate via the slider and then modulate live around it.

**Files:** Source/PluginProcessor.cpp (joystick->LFO freq routing logic), Source/PluginEditor.cpp (slider display)
**Phase:** LFO behavior refinement — pairs with the visual tracking fix above

---

## TODO: Joystick gate % parameter is not smooth — add slew/smoothing

**Requested:** 2026-03-01
**Status:** pending

Moving the joystick gate % knob/slider causes audible zipper noise or stepped changes. The arpeggiator gate length is smooth — the joystick gate % should use the same smoothing approach.

**Fix direction:** Apply the same parameter smoothing technique used for arpeggiator gate length to the joystick gate % parameter. Likely needs a `SmoothedValue<float>` or similar slew applied in processBlock before the value is used for note-off timing.

**Files:** Source/PluginProcessor.cpp (where gate % is applied to note duration)
**Phase:** Candidate for a gap closure or v1.1 patch

---

## TODO: Random trigger "Population" — grain spray on top of probability hits

**Requested:** 2026-03-01
**Status:** completed

A "Population" control that determines how many rapid-fire grain pulses fire on top of each probability hit — like a granulator spray. When a random trigger fires, instead of a single note-on, it fires N closely-spaced sub-triggers in a short burst. Density/probability controls WHEN hits occur; Population controls HOW MANY grains spray per hit.

**UX:** Population knob (1–8 grains), grain spacing sub-parameter (time between grains in ms or as a sub-subdivision fraction).

**Relation to existing system:** Extends the Random Trigger System (Phase 20) — builds on top of `randomDensity` / `randomSubdiv` APVTS params. The existing parameters remain unchanged; Population layers a burst-firing mode on top of each scheduled hit.

**Files:** Source/PluginProcessor.cpp (random trigger dispatch), Source/PluginEditor.cpp (UI knob), APVTS parameter additions
**Phase:** Candidate for a future v1.1 phase
