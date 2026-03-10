# Architecture Research

**Domain:** JUCE VST3/AU MIDI Plugin — Cross-Platform Launch + License Validation
**Researched:** 2026-03-10
**Confidence:** HIGH (CMake/JUCE facts from official docs + source), MEDIUM (LemonSqueezy integration pattern), MEDIUM (SDL2 macOS threading)

---

## Standard Architecture

### System Overview

```
┌──────────────────────────────────────────────────────────────────┐
│                        DAW Host Process                           │
├──────────────────────────────────────────────────────────────────┤
│  ┌──────────────┐  ┌──────────────────────────────────────────┐  │
│  │ PluginEditor │  │              PluginProcessor              │  │
│  │  (UI thread) │  │           (audio/MIDI thread)             │  │
│  │              │  │  ChordEngine · ScaleQuantizer             │  │
│  │ LicensePanel │  │  TriggerSystem · LooperEngine             │  │
│  │  (deferred   │  │  LfoEngine · GamepadInput                 │  │
│  │   overlay)   │  │                                           │  │
│  └──────┬───────┘  └───────────────┬──────────────────────────┘  │
│         │                          │                              │
│  ┌──────▼──────────────────────────▼──────────────────────────┐  │
│  │               LicenseManager (process-level singleton)      │  │
│  │   PropertiesFile · juce::URL HTTPS · LicenseStatus enum     │  │
│  └─────────────────────────────────────────────────────────────┘  │
├──────────────────────────────────────────────────────────────────┤
│                   Platform Adapters                               │
│  ┌─────────────────┐  ┌──────────────────┐  ┌────────────────┐  │
│  │  SdlContext      │  │  CMake platform   │  │  Entitlements  │  │
│  │  (process-level  │  │  guards           │  │  .plist (AU)   │  │
│  │   singleton)     │  │  WIN32 / APPLE    │  │  Win RC file   │  │
│  └─────────────────┘  └──────────────────┘  └────────────────┘  │
└──────────────────────────────────────────────────────────────────┘
```

### Component Responsibilities

| Component | Responsibility | Status |
|-----------|----------------|--------|
| LicenseManager | Singleton: validate key via LemonSqueezy HTTPS, persist state, expose status enum | **New file** |
| LicensePanel | JUCE Component overlay: key entry field, activate button, status label | **New file** |
| PluginEditor | Deferred license check on first paint via `juce::Timer::callAfterDelay` | Modify existing |
| PluginProcessor | Reads `LicenseManager::getStatus()` to gate any feature-locked behaviour | Modify existing (minimal) |
| SdlContext | Process-level singleton — `SDL_INIT_GAMECONTROLLER` only, no `SDL_INIT_VIDEO` | Unchanged |
| CMakeLists.txt | Add `APPLE` guards, `CMAKE_OSX_ARCHITECTURES`, AU entitlements, guard Inno Setup | Modify existing |

---

## Recommended Project Structure

```
Source/
├── PluginProcessor.h/.cpp       # existing — add LicenseManager query
├── PluginEditor.h/.cpp          # existing — add deferred LicensePanel trigger
├── LicenseManager.h/.cpp        # NEW — singleton, HTTPS, PropertiesFile
├── LicensePanel.h/.cpp          # NEW — JUCE Component for key entry overlay
├── ChordEngine.h/.cpp           # unchanged
├── ScaleQuantizer.h/.cpp        # unchanged
├── TriggerSystem.h/.cpp         # unchanged
├── LooperEngine.h/.cpp          # unchanged
├── LfoEngine.h/.cpp             # unchanged
├── GamepadInput.h/.cpp          # unchanged (SDL_INIT_GAMECONTROLLER only)
└── SdlContext.h/.cpp            # unchanged
installer/
├── DimaChordJoystick-Setup.iss.in   # Windows only — wrap in if(WIN32) in CMake
└── mac/
    └── build-pkg.sh                 # NEW — macOS .pkg / .dmg packaging script
```

### Structure Rationale

- **LicenseManager separate from PluginProcessor:** License state outlives any single plugin instance (singleton). Must be readable from both processor and editor without coupling them together. Both components query it read-only; only LicensePanel writes to it.
- **LicensePanel separate from PluginEditor:** Keeps dialog lifecycle isolated. PluginEditor decides when to show it, not how it works.
- **Installer split by platform:** Inno Setup is Windows-only; macOS packaging is `pkgbuild`/`productbuild` or `create-dmg`. Keep in separate subdirectories and guard the CMake `configure_file` call.

---

## Architectural Patterns

### Pattern 1: AU Format in juce_add_plugin

**What:** Add `AU` to the `FORMATS` list and supply `AU_MAIN_TYPE`. JUCE auto-generates the AU wrapper, the Info.plist keys, and a separate `.component` bundle target named `ChordJoystick_AU`.

**Current CMakeLists.txt already has (confirmed from file read):**
```cmake
FORMATS                  VST3 AU
AU_MAIN_TYPE             "kAudioUnitType_MusicDevice"
HARDENED_RUNTIME_ENABLED TRUE
BUNDLE_ID                "com.dimeaarcade.chordcontrol"
PLUGIN_MANUFACTURER_CODE "MxCJ"
PLUGIN_CODE              "DCJM"
```

These are all the Info.plist-driving fields JUCE needs. No hand-crafted plist is required — JUCE generates it from these parameters at configure time.

**AU format requirements checklist (HIGH confidence — JUCE CMake API docs):**

| Field | Value | Status | Why |
|-------|-------|--------|-----|
| `AU_MAIN_TYPE` | `kAudioUnitType_MusicDevice` | Already set | Correct for IS_SYNTH TRUE instrument |
| `BUNDLE_ID` | `com.dimeaarcade.chordcontrol` | Already set | Required for AU bundle identity |
| `PLUGIN_MANUFACTURER_CODE` | `"MxCJ"` | Already set | 4-char OSType, AU registration requirement |
| `PLUGIN_CODE` | `"DCJM"` | Already set | 4-char OSType, AU registration requirement |
| `HARDENED_RUNTIME_ENABLED` | `TRUE` | Already set | Required for notarization |
| `HARDENED_RUNTIME_OPTIONS` | `"com.apple.security.network.client"` | **MUST ADD** | Needed for LemonSqueezy HTTPS calls |
| `SUPPRESS_AU_PLIST_RESOURCE_USAGE` | (omit) | Not needed | Only needed for PACE-protected plugins |

**Company name constraint (HIGH confidence — JUCE CMake API docs):** For AU compatibility the company name must contain at least one upper-case letter. GarageBand 10.3 requires the first letter to be upper-case with remaining letters lower-case. `"Dimea Arcade"` satisfies this (D uppercase, remainder lowercase per word).

**One CMake edit needed — add HARDENED_RUNTIME_OPTIONS:**
```cmake
juce_add_plugin(ChordJoystick
    ...
    HARDENED_RUNTIME_ENABLED TRUE
    HARDENED_RUNTIME_OPTIONS "com.apple.security.network.client"
    ...
)
```

### Pattern 2: Mac-Specific CMake Guards

**What:** The existing `if(MSVC)` static CRT block is safe — it is a no-op on Apple Clang and never entered on Mac. No interaction with Mac builds. The `if(WIN32)` RC file block is already guarded. Several additions are needed.

**Changes needed in CMakeLists.txt:**

```cmake
# Guard Inno Setup configure_file — currently runs unconditionally (top of file)
# MODIFY — wrap in WIN32 guard:
if(WIN32)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/installer/DimaChordJoystick-Setup.iss.in"
        "${CMAKE_CURRENT_SOURCE_DIR}/installer/DimaChordJoystick-Setup.iss"
        @ONLY
    )
endif()

# ADD — universal binary for Mac, BEFORE FetchContent_MakeAvailable calls
# (must precede SDL2 and JUCE fetches so static libs are also fat binaries)
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()
```

**Critical ordering rule (HIGH confidence — JUCE forum confirmed):** `CMAKE_OSX_ARCHITECTURES` must be set **before** `FetchContent_MakeAvailable(SDL2)` and `FetchContent_MakeAvailable(juce)`. If set after, the static libs are single-arch and the linker fails with "building for macOS-arm64 but attempting to link with file built for macOS-x86_64".

The `if(MSVC)` block is already above FetchContent; the new `if(APPLE)` block must also be placed above FetchContent, immediately after or alongside the `if(MSVC)` block.

**Xcode generator command (Mac build machine):**
```bash
cmake -S . -B build-mac -G Xcode \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="11.0"
cmake --build build-mac --config Release --target ChordJoystick_VST3
cmake --build build-mac --config Release --target ChordJoystick_AU
```

**Caution (MEDIUM confidence — JUCE forum Feb 2025):** Xcode 16.2 was reported to stop producing universal AU binaries despite settings when using Projucer. With CMake + `CMAKE_OSX_ARCHITECTURES` this should be unaffected. After first Mac build, verify: `lipo -info build-mac/ChordJoystick_artefacts/Release/AU/...component/Contents/MacOS/...` should report `arm64 x86_64`.

**Already-guarded blocks that need no changes:**
- `configure_file(... ChordJoystick_resources.rc ...)` — inside `if(WIN32)`, correct.
- Reaper cache wipe — inside `if(WIN32)`, correct.
- The `elseif(APPLE)` post-build copy block — already present and correct.

### Pattern 3: LicenseManager — Singleton with PropertiesFile Persistence

**What:** A process-level singleton that owns the license state machine, persists the validated key and instance ID to `juce::PropertiesFile`, and exposes a simple status enum. Both `PluginProcessor` and `PluginEditor` query it read-only; only `LicensePanel` writes to it by calling `activate()`.

**Why singleton:** JUCE plugins can have multiple `PluginProcessor` + `PluginEditor` instances in the same process (multiple plugin windows). License state should be shared and validated once per process lifetime.

**PropertiesFile storage path (cross-platform — HIGH confidence from JUCE docs):**
- Windows: `%APPDATA%\DimeaArcade\ChordControl\settings.xml`
- macOS: `~/Library/Application Support/DimeaArcade/ChordControl/settings.xml`

JUCE `PropertiesFile::Options` resolves the OS-specific path automatically:

```cpp
// In LicenseManager constructor:
juce::PropertiesFile::Options opts;
opts.applicationName     = "ChordControl";
opts.folderName          = "DimeaArcade";
opts.filenameSuffix      = "xml";
opts.osxLibrarySubFolder = "Application Support";  // not "Preferences" — Apple compliance
propertiesFile_ = std::make_unique<juce::PropertiesFile>(opts);
```

The `osxLibrarySubFolder = "Application Support"` is the JUCE-recommended value for macOS compliance (JUCE docs note that `~/Library/Preferences` is discouraged for non-plist data).

**Status enum:**
```cpp
enum class LicenseStatus { Unknown, Valid, Invalid, NetworkError, Expired };
```

**Keys to persist in PropertiesFile:**
- `licenseKey` — the raw key string entered by the user
- `instanceId` — UUID returned by LemonSqueezy activate endpoint (needed for validate and deactivate)
- `cachedStatus` — last known status as int (allows offline use without API call on every launch)

Never store the LemonSqueezy store bearer token in the binary or on disk. The license-API endpoints (`/v1/licenses/activate`, `/v1/licenses/validate`) are authenticated by the `license_key` alone — no bearer token required.

**File:** `Source/LicenseManager.h` + `Source/LicenseManager.cpp` (new files to create).

### Pattern 4: LemonSqueezy API Integration via juce::URL

**What:** Two API calls needed — activate (first launch) and validate (subsequent launches). Both are HTTPS POST to `https://api.lemonsqueezy.com/v1/licenses/`.

**LemonSqueezy endpoint summary (HIGH confidence — official docs):**

| Action | Endpoint | Body fields | When |
|--------|----------|-------------|------|
| Activate | `POST /v1/licenses/activate` | `license_key`, `instance_name` | First time user enters key |
| Validate | `POST /v1/licenses/validate` | `license_key`, `instance_id` | Every launch (background thread) |
| Deactivate | `POST /v1/licenses/deactivate` | `license_key`, `instance_id` | Optional: on uninstall |

Response JSON includes `activated: true/false`, `license_key.status` (`"active"/"inactive"/"expired"`), and `instance.id` (UUID to persist).

**juce::URL HTTPS POST pattern (MEDIUM confidence — JUCE docs + forum):**

```cpp
// Always on a background juce::Thread, never on the audio thread or message thread
juce::URL url ("https://api.lemonsqueezy.com/v1/licenses/activate");
url = url.withPOSTData ("license_key=" + key + "&instance_name=ChordControl-" + machineId);

juce::URL::InputStreamOptions opts (juce::URL::ParameterHandling::inPostData);
opts = opts.withExtraHeaders ("Accept: application/vnd.api+json\r\n"
                               "Content-Type: application/x-www-form-urlencoded");

auto stream = url.createInputStream (opts);
if (stream != nullptr)
{
    juce::String response = stream->readEntireStreamAsString();
    auto parsed = juce::JSON::parse (response);
    // read parsed["activated"], parsed["instance"]["id"], etc.
}
```

**JUCE_WEB_BROWSER and JUCE_USE_CURL — current CMake settings are correct for Mac:**
- `JUCE_WEB_BROWSER=0` — correct, no embedded browser needed.
- `JUCE_USE_CURL=0` — correct. On macOS, `juce::URL` uses `NSURLSession` (Foundation) when CURL is disabled. This is the right approach for plugin contexts — no extra libcurl dependency and works inside sandboxed hosts.
- On Windows, `juce::URL` uses `WinINet` when CURL is disabled — also correct, no change needed.
- Do NOT enable `JUCE_USE_CURL=1` on Mac — it requires bundling libcurl and complicates signing.

**Threading rule:** Run the `juce::URL` network call on a `juce::Thread` subclass (or `juce::ThreadPool`). When done, deliver the result to the message thread via `juce::MessageManager::callAsync`. Never call from `processBlock` (audio thread). The message thread should not block waiting for the network call either — use the async callback pattern.

**macOS entitlements for network (HIGH confidence):**
`com.apple.security.network.client` is required for any outgoing TCP connection from a hardened-runtime binary on macOS. Without it, `NSURLSession` calls are silently blocked by the OS. Since `HARDENED_RUNTIME_ENABLED TRUE` is already set in CMakeLists.txt, this entitlement must be added via `HARDENED_RUNTIME_OPTIONS`.

Plugins running inside a sandboxed DAW (Logic Pro, GarageBand) inherit the host's sandbox. Logic Pro has `com.apple.security.network.client` in its own entitlements, so the plugin's outgoing network calls are permitted. GarageBand is more restrictive — if GarageBand support is required, add `SUPPRESS_AU_PLIST_RESOURCE_USAGE TRUE` to the `juce_add_plugin` call and test separately in GarageBand.

### Pattern 5: License Dialog in PluginEditor — Deferred, Non-Blocking Overlay

**What:** Show the license entry panel the first time the editor is opened on an unlicensed machine. Do not call it in the constructor — JUCE components are not fully on-screen at construction time, and blocking modal loops in plugin constructors cause DAW crashes on some hosts.

**Correct pattern — deferred overlay via `juce::Timer::callAfterDelay`:**

```cpp
// In PluginEditor constructor, after all normal UI setup:
if (LicenseManager::getInstance().getStatus() != LicenseStatus::Valid)
{
    juce::Timer::callAfterDelay (200, [this]
    {
        licensePanel_ = std::make_unique<LicensePanel>(
            [this](bool activated) {
                if (activated) licensePanel_.reset();
            });
        addAndMakeVisible (*licensePanel_);
        licensePanel_->setBounds (getLocalBounds());
        licensePanel_->toFront (true);
    });
}
```

**Why 200 ms delay:** Ensures the host has finished embedding the plugin window before any overlay is shown. Some hosts (especially AU hosts on macOS, including Logic Pro) complete the window embedding asynchronously after the editor is constructed.

**Dialog style — overlay Component, not modal window:** Use a JUCE `Component` that covers the full editor bounds rather than `DialogWindow::showModalDialog()`. Reasons:
- `showModalDialog` spawns a nested event loop which AU hosts on macOS do not support safely.
- An overlay Component integrates with the existing editor layout and respects the plugin window lifecycle.
- Non-blocking: the overlay is removed from PluginEditor when the user activates successfully.

**Second-launch validation:** On subsequent launches, validate silently in a background thread (no dialog). If validation fails due to network error, use the cached `cachedStatus` from PropertiesFile and show a small "offline" indicator in the editor rather than blocking the UI. Only show the license dialog if `cachedStatus` is `Invalid` or `Expired` — not on transient `NetworkError`.

**New member in PluginEditor.h:**
```cpp
std::unique_ptr<LicensePanel> licensePanel_;
```

### Pattern 6: SDL2 on macOS — Confirmed Safe, No Changes Required

**What:** The existing `SdlContext` singleton uses `SDL_INIT_GAMECONTROLLER` only (no `SDL_INIT_VIDEO`). This is exactly correct for a plugin context on macOS.

**Critical macOS finding (HIGH confidence — confirmed SDL GitHub issue #11437, filed Nov 2024):**
`SDL_Init(SDL_INIT_VIDEO)` called off the main thread on macOS throws an uncaught Objective-C exception. The existing code avoids this entirely — `SDL_INIT_VIDEO` is never passed. No changes needed.

**`SDL_INIT_GAMECONTROLLER` thread safety on macOS:** The gamecontroller subsystem does not require the OS main thread. SDL timer callbacks run on a dedicated internal timer thread. The existing 60 Hz timer-based polling is safe on macOS.

**Universal binary and SDL2 static lib:** SDL 2.30.9 supports universal `arm64;x86_64` builds via CMake. Because `CMAKE_OSX_ARCHITECTURES` is set before `FetchContent_MakeAvailable(SDL2)`, SDL2 is compiled as a fat static lib and linked correctly into the universal plugin bundle.

**SDL2 macOS deployment target:** SDL 2.30.9 supports macOS 10.9+ for x86_64 and macOS 11.0+ for arm64. Setting `CMAKE_OSX_DEPLOYMENT_TARGET "11.0"` covers both slices cleanly and is the minimum for a universal binary.

**Controller detection on macOS vs Windows:** No differences in the `SDL_GAMECONTROLLER` API. PS4/PS5/Xbox controllers detected identically. No changes needed in `GamepadInput.cpp`.

**One macOS SDL issue to monitor (MEDIUM confidence):** macOS Ventura 13.2+ with Apple Silicon had crash reports in SDL_Init (GitHub issue #8696) related to Objective-C method cache corruption. SDL 2.30.9 was released after those reports and includes fixes. No action needed beyond staying on 2.30.9 or later.

---

## Data Flow

### License Validation Flow

```
PluginEditor constructed
    |
    v
juce::Timer::callAfterDelay(200ms)
    |
    v
LicenseManager::getInstance().getStatus()
    |
    +--[ Valid ]---------> no dialog, normal UI
    |
    +--[ Unknown/Invalid/Expired ]
          |
          v
    LicensePanel shown as overlay (full editor bounds)
          |
    user enters key, presses Activate
          |
          v
    LicenseManager::activate(key)
    [background juce::Thread]
          |
          v
    POST https://api.lemonsqueezy.com/v1/licenses/activate
    body: license_key=..., instance_name=ChordControl-<machineId>
          |
          v
    JSON response: activated=true, instance.id=<uuid>
          |
          v
    PropertiesFile.setValue("licenseKey", key)
    PropertiesFile.setValue("instanceId", uuid)
    PropertiesFile.setValue("cachedStatus", Valid)
          |
    juce::MessageManager::callAsync callback
          |
          v
    LicensePanel removed from PluginEditor
```

### Subsequent Launch Flow

```
PluginProcessor created
    |
    v
LicenseManager::getInstance()
  reads PropertiesFile on first access
  loads key + instanceId + cachedStatus
    |
    v
Background juce::Thread:
  POST /v1/licenses/validate
  body: license_key=..., instance_id=<uuid>
    |
    +--[ success ]------> status = Valid, update PropertiesFile
    |
    +--[ network error ]-> status = cachedStatus (allow offline use)
    |
    +--[ expired ]-------> status = Expired, show small indicator

PluginEditor opens normally — no dialog unless status is Invalid/Expired
```

### CMake Build Order (Mac)

```
Set CMAKE_OSX_ARCHITECTURES "arm64;x86_64"  (before FetchContent)
Set CMAKE_OSX_DEPLOYMENT_TARGET "11.0"
    |
    v
FetchContent_MakeAvailable(juce)    -- fat static libs
FetchContent_MakeAvailable(SDL2)    -- fat static lib
    |
    v
juce_add_binary_data(ChordJoystickFonts)
    |
    v
juce_add_plugin(ChordJoystick) --> ChordJoystick_VST3 + ChordJoystick_AU targets
    |
    v
target_sources: add LicenseManager.cpp, LicensePanel.cpp
    |
    v
POST_BUILD (APPLE block, already in CMakeLists.txt):
  .vst3 -> ~/Library/Audio/Plug-Ins/VST3/
  .component -> ~/Library/Audio/Plug-Ins/Components/
```

---

## Integration Points

### New Files to Create

| File | Purpose | Integration |
|------|---------|-------------|
| `Source/LicenseManager.h/.cpp` | Singleton, HTTPS calls, PropertiesFile | Included by PluginEditor.h, PluginProcessor.h |
| `Source/LicensePanel.h/.cpp` | Overlay Component for key entry | Instantiated by PluginEditor (deferred) |

Both files must be added to `target_sources(ChordJoystick PRIVATE ...)` in CMakeLists.txt.

### Modified Files (all changes are small and localized)

| File | Change | Lines |
|------|--------|-------|
| `CMakeLists.txt` | Add `HARDENED_RUNTIME_OPTIONS`, wrap Inno Setup in `if(WIN32)`, add `if(APPLE)` arch/deployment block before FetchContent, add new source files to `target_sources` | ~12 lines |
| `Source/PluginEditor.h` | Add `#include "LicenseManager.h"`, `#include "LicensePanel.h"`, `std::unique_ptr<LicensePanel> licensePanel_` | 3 lines |
| `Source/PluginEditor.cpp` | Add deferred license check in constructor body | ~10 lines |
| `Source/PluginProcessor.h` | Add `#include "LicenseManager.h"` (for optional feature gating) | 1 line |
| `Source/PluginProcessor.cpp` | Query `LicenseManager::getStatus()` at any feature gates (if implemented) | Minimal or zero |

### External Services

| Service | Integration Pattern | Notes |
|---------|---------------------|-------|
| LemonSqueezy License API | `juce::URL` HTTPS POST, background `juce::Thread`, `juce::JSON::parse` | Two endpoints: activate + validate. No bearer token needed. |
| macOS AU host (Logic/GarageBand) | Standard JUCE AU wrapper, `kAudioUnitType_MusicDevice` | No custom AU code needed — JUCE generates the AU adapter |

### Internal Boundaries

| Boundary | Communication | Notes |
|----------|---------------|-------|
| LicenseManager ↔ PluginEditor | `getStatus()` query + lambda callback when status changes | PluginEditor never touches PropertiesFile directly |
| LicenseManager ↔ PluginProcessor | `getStatus()` read-only query | Only needed if feature gating is implemented |
| LicensePanel ↔ LicenseManager | Calls `activate(key, callback)` | LicensePanel has no network knowledge — delegates entirely to LicenseManager |
| SdlContext ↔ macOS platform | No change — GAMECONTROLLER only | `SDL_INIT_VIDEO` never called; safe on macOS |

---

## Anti-Patterns

### Anti-Pattern 1: Network I/O on the Audio Thread

**What people do:** Put license validation in `processBlock` or `prepareToPlay` for convenience.
**Why it's wrong:** Network calls block for 100–5000 ms. `processBlock` has a ~5 ms budget. The DAW will produce dropouts or kill the plugin process.
**Do this instead:** All network calls on a `juce::Thread` subclass. Deliver results to the message thread via `juce::MessageManager::callAsync`.

### Anti-Pattern 2: Modal Dialog in PluginEditor Constructor

**What people do:** Call `DialogWindow::showModalDialog()` or `AlertWindow::showModalDialog()` directly in the PluginEditor constructor.
**Why it's wrong:** The plugin window is not yet embedded in the host at construction time. Modal event loops inside plugin windows cause hangs or crashes in Logic Pro, Ableton, and AU hosts on macOS.
**Do this instead:** `juce::Timer::callAfterDelay(200, ...)` to show an overlay Component after the host finishes embedding the window.

### Anti-Pattern 3: CMAKE_OSX_ARCHITECTURES Set After FetchContent

**What people do:** Set `CMAKE_OSX_ARCHITECTURES` near the bottom of CMakeLists.txt, or only as a target property.
**Why it's wrong:** FetchContent dependencies (JUCE, SDL2) are compiled with whatever architecture was active at `FetchContent_MakeAvailable` time. The result is single-arch static libs that cause a linker error: "building for macOS-arm64 but attempting to link with file built for macOS-x86_64".
**Do this instead:** Set `CMAKE_OSX_ARCHITECTURES "arm64;x86_64"` in the `if(APPLE)` block before any `FetchContent_MakeAvailable` call — alongside the existing `if(MSVC)` block.

### Anti-Pattern 4: Storing LemonSqueezy Store Bearer Token in the Plugin Binary

**What people do:** Embed the store-level API bearer token to make store-management API calls from the plugin.
**Why it's wrong:** The binary is extractable. The store bearer token grants full API access (customer list, refunds, etc.). LemonSqueezy's License API endpoints do not require the bearer token — they are authenticated by the `license_key` alone.
**Do this instead:** Use only the license-key-only endpoints (`/v1/licenses/activate`, `/v1/licenses/validate`). No API secret in the binary.

### Anti-Pattern 5: SDL_INIT_VIDEO in a Plugin Context on macOS

**What people do:** Copy SDL tutorial code calling `SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER)`.
**Why it's wrong:** `SDL_INIT_VIDEO` on macOS off the main thread throws an Objective-C exception (confirmed SDL issue #11437, Nov 2024). Plugin initialization never happens on the OS main thread.
**Do this instead:** `SDL_Init(SDL_INIT_GAMECONTROLLER)` only. The existing `SdlContext.cpp` already does this correctly — no change needed.

### Anti-Pattern 6: Unconditional Inno Setup configure_file on Mac

**What people do:** Leave the `configure_file(... .iss.in ...)` call at the top of CMakeLists.txt without a platform guard.
**Why it's wrong:** On macOS the `.iss` file is generated pointlessly, and if the template references Windows-specific variables that expand incorrectly, CMake configure silently produces a malformed file.
**Do this instead:** Wrap the `configure_file` call in `if(WIN32) ... endif()`.

---

## Scaling Considerations

This is a desktop plugin, not a web service. Scaling concerns are limited to multiple simultaneous instances and API robustness.

| Concern | Approach |
|---------|----------|
| Multiple plugin instances in same process | LicenseManager is a singleton — one PropertiesFile, one background validation thread per process |
| Network unavailable at launch | Use `cachedStatus` from PropertiesFile; allow offline use; show unobtrusive indicator |
| LemonSqueezy API downtime | Treat `NetworkError` as "allow with cached status" — never block the plugin due to a third-party API outage |
| PropertiesFile multi-process DAW isolation | JUCE PropertiesFile is not multi-process safe; acceptable since license writes happen rarely (once at activation) |
| LicenseManager singleton lifetime | Destroyed on process exit; PropertiesFile flush called in destructor |

---

## Sources

- JUCE CMake API — `FORMATS`, `AU_MAIN_TYPE`, `HARDENED_RUNTIME_ENABLED/OPTIONS`, `SUPPRESS_AU_PLIST_RESOURCE_USAGE`, company name constraint: [JUCE/docs/CMake API.md](https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md)
- JUCE `juce::URL` + `InputStreamOptions` reference: [docs.juce.com — URL::InputStreamOptions](https://docs.juce.com/master/classURL_1_1InputStreamOptions.html)
- JUCE `PropertiesFile` reference: [docs.juce.com — PropertiesFile](https://docs.juce.com/master/classjuce_1_1PropertiesFile.html)
- JUCE `ApplicationProperties` (osxLibrarySubFolder guidance): [docs.juce.com — ApplicationProperties](https://docs.juce.com/master/classjuce_1_1ApplicationProperties.html)
- LemonSqueezy License API — activate: [docs.lemonsqueezy.com/api/license-api/activate-license-key](https://docs.lemonsqueezy.com/api/license-api/activate-license-key)
- LemonSqueezy License API — validate: [docs.lemonsqueezy.com/api/license-api/validate-license-key](https://docs.lemonsqueezy.com/api/license-api/validate-license-key)
- SDL2 macOS `SDL_Init(SDL_INIT_VIDEO)` off-main-thread crash: [libsdl-org/SDL issue #11437](https://github.com/libsdl-org/SDL/issues/11437)
- SDL2 arm64 macOS Ventura crash (fixed in 2.30.x): [libsdl-org/SDL issue #8696](https://github.com/libsdl-org/SDL/issues/8696)
- JUCE CMake universal binary: [JUCE forum — CMake plugin OS 11 Universal Binary](https://forum.juce.com/t/cmake-plugin-and-os-11-universal-binary/41997)
- JUCE AU universal binary Xcode 16.2 regression note: [JUCE forum — AU universal binary macOS Sequoia Xcode 16.2](https://forum.juce.com/t/au-universal-binary-macos-sequoia-and-xcode-16-2/65337)
- macOS network client entitlement for plugins: [JUCE forum — network connection from audio plugin](https://forum.juce.com/t/network-connection-from-audio-plugin/38105)
- JUCE modal dialog risks in plugins: [JUCE forum — dialogs on plugins, modal, non-modal](https://forum.juce.com/t/dialogs-on-plugins-modal-non-modal/11889)
- JUCE cross-platform CMake + GitHub Actions: [Reilly Spitzfaden blog 2025](https://reillyspitzfaden.com/posts/2025/08/plugins-for-everyone-crossplatform-juce-with-cmake-github-actions/)
- Direct file inspection: `CMakeLists.txt` (this repository) — confirmed existing FORMATS, AU_MAIN_TYPE, HARDENED_RUNTIME_ENABLED, BUNDLE_ID, elseif(APPLE) post-build block
- Direct file inspection: `.planning/PROJECT.md` — v2.0 requirements verbatim

---
*Architecture research for: JUCE VST3/AU plugin — macOS cross-platform + LemonSqueezy license*
*Researched: 2026-03-10*
