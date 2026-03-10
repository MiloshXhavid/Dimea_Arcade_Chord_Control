#include "GamepadInput.h"

// Include SDL2 only in the .cpp to avoid polluting headers
#include <SDL.h>
#include "SdlContext.h"

static constexpr int kTriggerThreshold = 8000;  // SDL axis > this = pressed

GamepadInput::GamepadInput()
{
    for (auto& a : voiceTrig_)    a.store(false);

    // Record construction time so timerCallback() can defer SDL init by kSdlInitDelayMs.
    // Deferring prevents SDL's DirectInput/XInput HID device scan from racing with
    // Ableton's WASAPI/ASIO audio init — both enumerate the Windows HID tree concurrently,
    // which can soft-deadlock Ableton at "Initializing audio inputs and outputs".
    constructTime_ = std::chrono::steady_clock::now();

    // Start the timer immediately so the plugin is responsive at 60 Hz.
    // timerCallback() gates SDL usage on sdlReady_ and gates thread spawn on
    // sdlThreadSpawned_ + the kSdlInitDelayMs elapsed window.
    startTimerHz(60);
}

GamepadInput::~GamepadInput()
{
    stopTimer();

    // Ensure the background SDL init thread has finished before we clean up.
    // (If SDL_Init is still running, closeController/SDL_Quit would be unsafe.)
    if (sdlInitThread_.joinable())
        sdlInitThread_.join();

    closeController();
    if (sdlInitialised_)
        SdlContext::release();
}

void GamepadInput::setMouseFilterOverride(float x, float y, bool active)
{
    mouseFilterX_.store(x, std::memory_order_relaxed);
    mouseFilterY_.store(y, std::memory_order_relaxed);
    mouseFilterActive_.store(active, std::memory_order_relaxed);
}

juce::String GamepadInput::getControllerName() const
{
    if (!controller_) return {};
    const char* name = SDL_GameControllerName(controller_);
    return name ? juce::String(name) : juce::String("Unknown Controller");
}

void GamepadInput::tryOpenController()
{
    const int n = SDL_NumJoysticks();
    for (int i = 0; i < n; ++i)
    {
        if (SDL_IsGameController(i))
        {
            controller_ = SDL_GameControllerOpen(i);
            if (controller_)
            {
                virtuallyConnected_ = true;
                // Fire both callback slots — processor handles pending flags, editor handles UI
                if (onConnectionChange)   onConnectionChange(true);
                if (onConnectionChangeUI)
                {
                    const char* rawName = SDL_GameControllerName(controller_);
                    onConnectionChangeUI(rawName ? juce::String(rawName)
                                                 : juce::String("Unknown Controller"));
                }
            }
            return;
        }
    }
}

void GamepadInput::closeController()
{
    if (controller_)
    {
        SDL_GameControllerClose(controller_);
        controller_ = nullptr;
        virtuallyConnected_ = false;
        // Fire both callback slots
        if (onConnectionChange)   onConnectionChange(false);
        if (onConnectionChangeUI) onConnectionChangeUI(juce::String{});  // empty = disconnected
    }
}

float GamepadInput::normaliseAxis(int16_t raw) const
{
    const float v = static_cast<float>(raw) / 32767.0f;
    return std::abs(v) < deadZone_.load(std::memory_order_relaxed) ? 0.0f : v;
}

bool GamepadInput::triggerPressed(int16_t axisValue) const
{
    return axisValue > kTriggerThreshold;
}

// ─── Timer callback (main thread, ~60 Hz) ────────────────────────────────────

void GamepadInput::timerCallback()
{
    // Deferred SDL init: wait kSdlInitDelayMs after construction before spawning the
    // background thread.  This ensures SDL's DirectInput/XInput HID scan does not race
    // with Ableton's WASAPI/ASIO audio init, which also scans the Windows HID tree and
    // can soft-deadlock the DAW startup if both run concurrently.
    if (!sdlThreadSpawned_)
    {
        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - constructTime_).count();
        if (elapsedMs < kSdlInitDelayMs)
            return;

        sdlThreadSpawned_ = true;
        sdlInitThread_ = std::thread([this]
        {
            sdlInitialised_ = SdlContext::acquire();

            if (!sdlInitialised_)
            {
                DBG("GamepadInput: SdlContext::acquire() failed — no gamepad support");
            }
            else
            {
                // Queue controller open for the timer thread (main thread).
                // Do NOT call tryOpenController() here — its callbacks (onConnectionChange,
                // onConnectionChangeUI) must fire on the main thread, not from here.
                pendingReopenTick_.store(true, std::memory_order_release);
            }

            // Signal timer that SDL is ready.
            sdlReady_.store(true, std::memory_order_release);
        });
        return;  // SDL not ready this tick; will start polling on the next tick after init completes
    }

    // SDL not ready yet (init is running on background thread) — skip this tick.
    if (!sdlReady_.load(std::memory_order_acquire))
        return;

    const uint32_t now = SDL_GetTicks();

    // Debounce helper: returns true on a confirmed state transition
    // (cur != prev AND at least kDebounceMsBtn ms have elapsed since last change).
    auto debounce = [&](bool cur, ButtonState& s) -> bool {
        if (cur != s.prev && (now - s.lastMs) >= kDebounceMsBtn)
        {
            s.prev   = cur;
            s.lastMs = now;
            return true;
        }
        return false;
    };

    // SDL event loop
    SDL_Event ev;
    while (SDL_PollEvent(&ev))
    {
        if (ev.type == SDL_CONTROLLERDEVICEADDED)
        {
            // Queue a reconnect. If a stale controller handle is still open (BT reconnect
            // where REMOVED was never fired or is delayed), close it first so
            // tryOpenController() starts from a clean state. A delayed REMOVED event
            // arriving later will be harmlessly ignored (controller_ == nullptr).
            // Defer the open by one tick so SDL_GameControllerOpen() is not called during
            // active BT enumeration (avoids crash on some BT stacks).
            if (controller_)
                closeController();  // clear stale handle + fire disconnect callbacks
            pendingReopenTick_.store(true, std::memory_order_relaxed);
        }
        else if (ev.type == SDL_CONTROLLERDEVICEREMOVED)
        {
            // Guard: only close if the removed device matches our open controller.
            // Prevents closing the wrong device or a null controller handle.
            // SDL_GameControllerGetJoystick() can return nullptr on BT reconnect
            // (SDL's joystick thread may invalidate the handle concurrently).
            // If joy is null the handle is already invalid — close unconditionally.
            if (controller_)
            {
                SDL_Joystick* joy = SDL_GameControllerGetJoystick(controller_);
                if (joy == nullptr || SDL_JoystickInstanceID(joy) == ev.cdevice.which)
                    closeController();
            }
        }
    }

    // Deferred open: one tick after ADDED event, BT enumeration has settled.
    if (pendingReopenTick_.load(std::memory_order_relaxed) && !controller_)
    {
        pendingReopenTick_.store(false, std::memory_order_relaxed);
        tryOpenController();
    }

    // Fallback disconnect check: SDL_CONTROLLERDEVICEREMOVED is unreliable on some
    // Windows USB drivers. Poll the attached state every tick to catch silent disconnects.
    if (controller_ && !SDL_GameControllerGetAttached(controller_))
        closeController();

    if (!controller_) return;

    // ── PS / Guide button: soft connect / disconnect toggle ───────────────────
    // Pressing PS while connected soft-disconnects the plugin (fires all-notes-off
    // + CC reset via onConnectionChange(false)) but keeps the SDL handle open so
    // the button can be detected again.  Pressing PS while soft-disconnected
    // re-enables input and fires onConnectionChange(true).
    {
        const bool cur = SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_GUIDE) != 0;
        if (debounce(cur, btnGuide_) && btnGuide_.prev)
        {
            virtuallyConnected_ = !virtuallyConnected_;
            if (virtuallyConnected_)
            {
                if (onConnectionChange)   onConnectionChange(true);
                if (onConnectionChangeUI)
                {
                    const char* n = SDL_GameControllerName(controller_);
                    onConnectionChangeUI(n ? juce::String(n) : juce::String("Unknown Controller"));
                }
            }
            else
            {
                if (onConnectionChange)   onConnectionChange(false);
                if (onConnectionChangeUI) onConnectionChangeUI(juce::String{});
            }
        }
    }
    if (!virtuallyConnected_)
    {
        buttonHeldMask_.store(0u, std::memory_order_relaxed);
        leftStickXLive_.store(0.0f, std::memory_order_relaxed);
        leftStickYLive_.store(0.0f, std::memory_order_relaxed);
        return;
    }

    // ── Right stick → pitch joystick (returns to center when released) ──────────
    // Rescaling applied (same as left stick): value is 0 inside the dead zone and ramps
    // smoothly to ±1 at stick extreme, eliminating the cursor jump when the stick first
    // exits the dead zone.
    {
        const float dz = deadZone_.load(std::memory_order_relaxed);
        auto rescaleRight = [dz](float raw) -> float {
            const float av = std::abs(raw);
            if (av < dz) return 0.0f;
            const float sign = (raw > 0.0f) ? 1.0f : -1.0f;
            return sign * (av - dz) / (1.0f - dz);
        };
        const float rawX =  normaliseAxis(SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_RIGHTX));
        const float rawY = -normaliseAxis(SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_RIGHTY));
        pitchX_.store(rescaleRight(rawX), std::memory_order_relaxed);
        pitchY_.store(rescaleRight(rawY), std::memory_order_relaxed);
    }

    // ── Left stick → filter (deadzone rescaling, no sample-and-hold) ────────
    // Rescaling: value starts at 0 at the deadzone boundary and ramps to ±1 at
    // the stick extreme, eliminating the jump when the stick exits the deadzone.
    // No S&H: when the stick is centered (inside deadzone), filterX_/filterY_ = 0
    // so the CC returns to the MOD FIX base knob position.
    {
        const float dz = deadZone_.load(std::memory_order_relaxed);
        auto rescaleLeft = [dz](int16_t rawInt, float invert) -> float {
            const float v  = static_cast<float>(rawInt) / 32767.0f * invert;
            const float av = std::abs(v);
            if (av < dz) return 0.0f;
            const float sign = (v > 0.0f) ? 1.0f : -1.0f;
            return sign * (av - dz) / (1.0f - dz);
        };

        const float scaledX = rescaleLeft(SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_LEFTX),  1.0f);
        const float scaledY = rescaleLeft(SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_LEFTY), -1.0f);

        filterX_.store(scaledX, std::memory_order_relaxed);   // X axis (left/right) → filterX
        filterY_.store(scaledY, std::memory_order_relaxed);   // Y axis (up/down)    → filterY
        filterYRaw_.store(scaledY, std::memory_order_relaxed); // 0 when in dead zone (sustain pedal)
        leftStickXLive_.store(scaledX, std::memory_order_relaxed);
        leftStickYLive_.store(scaledY, std::memory_order_relaxed);
    }

    // ── Voice triggers: L1/L2/R1/R2 (debounced) ──────────────────────────────
    // L1 = left  shoulder button  → voice 0 (Root)
    // L2 = left  trigger  (axis)  → voice 1 (Third)
    // R1 = right shoulder button  → voice 2 (Fifth)
    // R2 = right trigger (axis)   → voice 3 (Tension)
    const bool curVoice[4] =
    {
        SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_LEFTSHOULDER)  != 0,
        triggerPressed(SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_TRIGGERLEFT)),
        SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) != 0,
        triggerPressed(SDL_GameControllerGetAxis(controller_, SDL_CONTROLLER_AXIS_TRIGGERRIGHT)),
    };
    for (int i = 0; i < 4; ++i)
    {
        if (debounce(curVoice[i], btnVoice_[i]) && btnVoice_[i].prev)
            voiceTrig_[i].store(true);  // rising edge after debounce
        voiceHeld_[i].store(btnVoice_[i].prev, std::memory_order_relaxed);
    }

    // ── L3 (left stick click) → all-notes trigger + held state ──────────────
    {
        const bool cur = SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_LEFTSTICK) != 0;
        if (debounce(cur, btnAllNotes_) && btnAllNotes_.prev)
            allNotesTrig_.store(true);
        allNotesHeld_.store(btnAllNotes_.prev, std::memory_order_relaxed);
    }

    // ── Looper buttons: Cross(A)/Circle(B)/Square(X)/Triangle(Y) ─────────────
    // SDL A=Cross(X), B=Circle, X=Square, Y=Triangle
    {
        const bool curSS  = SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_A) != 0;
        const bool curDel = SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_B) != 0;
        const bool curRst = SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_X) != 0;
        const bool curRec = SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_Y) != 0;

        if (debounce(curSS,  btnStartStop_) && btnStartStop_.prev) looperStartStop_.store(true);
        if (debounce(curDel, btnDelete_)    && btnDelete_.prev)    looperDelete_.store(true);
        if (debounce(curRst, btnReset_)     && btnReset_.prev)     looperReset_.store(true);
        if (debounce(curRec, btnRecord_)    && btnRecord_.prev)    looperRecord_.store(true);
    }

    // ── R3 (right stick click) → MIDI panic ──────────────────────────────────
    {
        const bool cur = SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_RIGHTSTICK) != 0;
        if (debounce(cur, btnRightStick_) && btnRightStick_.prev)
            rightStickTrig_.store(true);
    }

    // ── Options button (PS4: ≡ = SDL START) → cycles option mode 0→1→2→3→4→0 ──
    optionFrameFired_ = false;
    {
        const bool cur = SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_START) != 0;
        if (debounce(cur, btnOption_) && btnOption_.prev)  // rising edge = button down
        {
            const int next = (optionMode_.load(std::memory_order_relaxed) + 1) % 5;
            optionMode_.store(next, std::memory_order_relaxed);
            optionFrameFired_ = true;  // lockout D-pad for this frame
        }
    }

    // ── D-pad: mode 0/3/4=BPM/looper, mode 1=octaves, mode 2=transpose+intervals ─
    if (!optionFrameFired_)
    {
        const bool curUp    = SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_DPAD_UP)    != 0;
        const bool curDown  = SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_DPAD_DOWN)  != 0;
        const bool curLeft  = SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_DPAD_LEFT)  != 0;
        const bool curRight = SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_DPAD_RIGHT) != 0;

        if (optionMode_.load(std::memory_order_relaxed) == 1 || optionMode_.load(std::memory_order_relaxed) == 2)
        {
            // Option modes 1 & 2: rising edge fires a press signal per direction.
            const bool curOptBtn[4] = { curUp, curDown, curLeft, curRight };
            ButtonState* btnOpt[4]  = { &btnDpadUp_, &btnDpadDown_, &btnDpadLeft_, &btnDpadRight_ };

            for (int i = 0; i < 4; ++i)
            {
                const bool changed = debounce(curOptBtn[i], *btnOpt[i]);
                if (changed && btnOpt[i]->prev)  // rising edge only
                {
                    const uint32_t elapsed = now - optDpadLastPressMs_[i];
                    // First press: +1 (and record timestamp).
                    // Second press within double-click window: -2 so that the net
                    // change from the original value is -1 (cancels the +1 already
                    // fired plus adds one decrement, regardless of whether the first
                    // delta was consumed before the second press arrives).
                    const int delta = (optDpadLastPressMs_[i] != 0 && elapsed < kDpadDoubleClickMs)
                                          ? -2 : +1;
                    pendingOptionDpadDelta_[i].fetch_add(delta, std::memory_order_relaxed);
                    // Record timestamp for first press; reset it after a double-click
                    optDpadLastPressMs_[i] = (delta == +1) ? now : 0;
                }
            }
        }
        else
        {
            // Normal mode: BPM delta (up/down) + looper rec state (left/right)
            if (debounce(curUp,    btnDpadUp_)    && btnDpadUp_.prev)    dpadUpTrig_.store(true);
            if (debounce(curDown,  btnDpadDown_)  && btnDpadDown_.prev)  dpadDownTrig_.store(true);
            if (debounce(curLeft,  btnDpadLeft_)  && btnDpadLeft_.prev)  dpadLeftTrig_.store(true);
            if (debounce(curRight, btnDpadRight_) && btnDpadRight_.prev) dpadRightTrig_.store(true);
        }

        // ── Mode 1 face-button arp dispatch ──────────────────────────────────────
        // Uses separate ButtonState trackers (btnMode1*) so the unconditional looper
        // button atoms above still fire in all modes — PluginProcessor gates consumption.
        // This block is inside !optionFrameFired_ so it cannot fire in the same frame
        // the Option button toggles mode.
        if (optionMode_.load(std::memory_order_relaxed) == 1)
        {
            // Circle (SDL B) → arp toggle
            {
                const bool cur = SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_B) != 0;
                if (debounce(cur, btnMode1Circle_) && btnMode1Circle_.prev)
                    pendingArpCircle_.store(true, std::memory_order_relaxed);
            }

            // Triangle (SDL Y) → arp rate delta (double-click pattern)
            {
                const bool cur = SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_Y) != 0;
                if (debounce(cur, btnMode1Triangle_) && btnMode1Triangle_.prev)
                {
                    const uint32_t elapsed = now - arpRateLastPressMs_;
                    const int delta = (arpRateLastPressMs_ != 0 && elapsed < kDpadDoubleClickMs) ? -2 : +1;
                    pendingArpRateDelta_.fetch_add(delta, std::memory_order_relaxed);
                    arpRateLastPressMs_ = (delta == +1) ? now : 0;
                }
            }

            // Square (SDL X) → arp order delta (double-click pattern)
            {
                const bool cur = SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_X) != 0;
                if (debounce(cur, btnMode1Square_) && btnMode1Square_.prev)
                {
                    const uint32_t elapsed = now - arpOrderLastPressMs_;
                    const int delta = (arpOrderLastPressMs_ != 0 && elapsed < kDpadDoubleClickMs) ? -2 : +1;
                    pendingArpOrderDelta_.fetch_add(delta, std::memory_order_relaxed);
                    arpOrderLastPressMs_ = (delta == +1) ? now : 0;
                }
            }

            // Cross (SDL A) → RND Sync toggle
            {
                const bool cur = SDL_GameControllerGetButton(controller_, SDL_CONTROLLER_BUTTON_A) != 0;
                if (debounce(cur, btnMode1Cross_) && btnMode1Cross_.prev)
                    pendingRndSyncToggle_.store(true, std::memory_order_relaxed);
            }
        }
    }

    // ── Button held-state bitmask for UI visualization ──────────────────────────
    {
        uint32_t mask = 0;
        if (btnVoice_[0].prev)   mask |= (1u <<  0);  // L1
        if (btnVoice_[1].prev)   mask |= (1u <<  1);  // L2
        if (btnVoice_[2].prev)   mask |= (1u <<  2);  // R1
        if (btnVoice_[3].prev)   mask |= (1u <<  3);  // R2
        if (btnAllNotes_.prev)   mask |= (1u <<  4);  // L3
        if (btnRightStick_.prev) mask |= (1u <<  5);  // R3
        if (btnStartStop_.prev)  mask |= (1u <<  6);  // Cross
        if (btnReset_.prev)      mask |= (1u <<  7);  // Square
        if (btnRecord_.prev)     mask |= (1u <<  8);  // Triangle
        if (btnDelete_.prev)     mask |= (1u <<  9);  // Circle
        if (btnDpadUp_.prev)     mask |= (1u << 10);  // DpadUp
        if (btnDpadDown_.prev)   mask |= (1u << 11);  // DpadDown
        if (btnDpadLeft_.prev)   mask |= (1u << 12);  // DpadLeft
        if (btnDpadRight_.prev)  mask |= (1u << 13);  // DpadRight
        if (btnOption_.prev)     mask |= (1u << 14);  // Options
        if (btnGuide_.prev)      mask |= (1u << 15);  // PS
        buttonHeldMask_.store(mask, std::memory_order_relaxed);
    }
}

// ── Battery level ─────────────────────────────────────────────────────────────

int GamepadInput::getBatteryLevel() const
{
    if (!controller_) return -2;
    SDL_Joystick* joy = SDL_GameControllerGetJoystick(controller_);
    if (!joy) return -2;
    return (int)SDL_JoystickCurrentPowerLevel(joy);
}

// ── Consume helpers ───────────────────────────────────────────────────────────

bool GamepadInput::consumeVoiceTrigger(int voice)
{
    if (voice < 0 || voice >= 4) return false;
    return voiceTrig_[voice].exchange(false);
}

bool GamepadInput::consumeAllNotesTrigger()   { return allNotesTrig_.exchange(false); }
bool GamepadInput::consumeLooperStartStop()   { return looperStartStop_.exchange(false); }
bool GamepadInput::consumeLooperRecord()      { return looperRecord_.exchange(false); }
bool GamepadInput::consumeLooperReset()       { return looperReset_.exchange(false); }
bool GamepadInput::consumeLooperDelete()      { return looperDelete_.exchange(false); }
