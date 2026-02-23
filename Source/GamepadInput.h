#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <cstdint>
#include <functional>

// SDL2 forward declarations (avoid including SDL headers in headers)
struct _SDL_GameController;
typedef struct _SDL_GameController SDL_GameController;

// ─── GamepadInput ─────────────────────────────────────────────────────────────
// Polls an SDL2 game controller at 60 Hz (via juce::Timer).
// Writes state to atomics so the audio thread and UI thread can read safely.
//
// PS Controller mapping:
//   Right stick X/Y  → pitch joystick
//   R1 (RightShoulder) → voice 0 (Root)
//   R2 (RightTrigger)  → voice 1 (Third)
//   L1 (LeftShoulder)  → voice 2 (Fifth)
//   L2 (LeftTrigger)   → voice 3 (Tension)
//   Left stick X       → filter cutoff (CC74)
//   Left stick Y       → filter resonance (CC71)
//   L3 (LeftStick btn) → all-notes trigger
//   Cross (A)          → looper start/stop
//   Square (X)         → looper reset
//   Triangle (Y)       → looper record
//   Circle (B)         → looper delete

class GamepadInput : private juce::Timer
{
public:
    GamepadInput();
    ~GamepadInput() override;

    // ── Normalised joystick values (-1..+1), updated at 60Hz ─────────────────
    float getPitchX()    const { return pitchX_.load(); }
    float getPitchY()    const { return pitchY_.load(); }
    float getFilterX()   const { return filterX_.load(); }
    float getFilterY()   const { return filterY_.load(); }

    // ── Voice trigger rising-edge flags (consume = clear after read) ──────────
    bool consumeVoiceTrigger(int voice);  // returns true once per press

    // ── Special button rising-edge flags ─────────────────────────────────────
    bool consumeAllNotesTrigger();
    bool consumeLooperStartStop();
    bool consumeLooperRecord();
    bool consumeLooperReset();
    bool consumeLooperDelete();

    // Whether a gamepad is connected
    bool isConnected() const { return controller_ != nullptr; }

    // ── Dead zone setter ──────────────────────────────────────────────────────
    // Called from processBlock via PluginProcessor — sets dead zone threshold
    // from the joystickThreshold APVTS parameter (single source of truth).
    void setDeadZone(float t) { deadZone_.store(t, std::memory_order_relaxed); }

    // ── Connection callbacks — two independent slots ──────────────────────────
    // Called on message thread when a controller connects or disconnects.
    // PluginProcessor ctor sets this slot to handle pendingAllNotesOff_ / pendingCcReset_.
    std::function<void(bool)> onConnectionChange;

    // Called on message thread when a controller connects or disconnects.
    // PluginEditor sets this slot to update the status label and gamepadActiveBtn_.
    // Kept separate from onConnectionChange so both fire independently.
    std::function<void(bool)> onConnectionChangeUI;

private:
    void timerCallback() override;
    void tryOpenController();
    void closeController();
    float normaliseAxis(int16_t raw) const;
    bool  triggerPressed(int16_t axisValue) const;  // L2/R2 are axes

    SDL_GameController* controller_ = nullptr;

    // Dead zone — set from processBlock via setDeadZone(); replaces hard-coded kDeadzone
    std::atomic<float> deadZone_ { 0.08f };  // conservative default until first setDeadZone call

    std::atomic<float> pitchX_  {0.0f};
    std::atomic<float> pitchY_  {0.0f};
    std::atomic<float> filterX_ {0.0f};
    std::atomic<float> filterY_ {0.0f};

    // Sample-and-hold: hold last non-dead-zone value when stick returns to center
    float lastPitchX_  = 0.0f;
    float lastPitchY_  = 0.0f;
    float lastFilterX_ = 0.0f;
    float lastFilterY_ = 0.0f;

    std::atomic<bool>  voiceTrig_[4]    {};
    std::atomic<bool>  allNotesTrig_    {false};
    std::atomic<bool>  looperStartStop_ {false};
    std::atomic<bool>  looperRecord_    {false};
    std::atomic<bool>  looperReset_     {false};
    std::atomic<bool>  looperDelete_    {false};

    // 20ms button debounce — each button tracks previous state and last-change timestamp
    struct ButtonState
    {
        bool     prev   = false;
        uint32_t lastMs = 0;  // SDL_GetTicks() ms timestamp of last state change
    };
    static constexpr uint32_t kDebounceMsBtn = 20;

    ButtonState btnVoice_[4];
    ButtonState btnAllNotes_;
    ButtonState btnStartStop_;
    ButtonState btnRecord_;
    ButtonState btnReset_;
    ButtonState btnDelete_;

    bool sdlInitialised_    = false;  // guard: SdlContext::release() only if acquire() succeeded
};
