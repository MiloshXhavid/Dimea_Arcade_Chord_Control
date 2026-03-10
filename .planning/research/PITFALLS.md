# Pitfalls Research

**Domain:** JUCE 8 VST3/AU plugin — Windows-to-macOS port with universal binary, AU format, LemonSqueezy licensing, code signing, and notarization
**Researched:** 2026-03-10
**Confidence:** MEDIUM — WebSearch + JUCE forum community findings verified across multiple sources; Apple entitlement specifics are HIGH (official Apple docs); LemonSqueezy offline behavior is LOW (limited native desktop documentation)

---

## Critical Pitfalls

Mistakes that cause build failures, DAW rejection, notarization rejection, or audio-thread crashes.

---

### Pitfall 1: Network Call on the Audio Thread for License Validation

**What goes wrong:**
The license check fires inside `processBlock` (or from a constructor, which may run on a real-time thread in some DAWs). `processBlock` is called on the audio thread, which is a real-time thread. Any blocking I/O — including a synchronous HTTP request to `api.lemonsqueezy.com` — blocks the audio thread. The OS can preempt the network stack for arbitrarily long periods (DNS resolution, TLS handshake, TCP timeout). This causes an audio dropout in the best case and a DAW freeze or crash in the worst case.

**Why it happens:**
Developers initialize a `juce::URL::InputStreamOptions` or equivalent HTTP fetch in `PluginProcessor::prepareToPlay()` or a parameter callback, not realising those run on the audio thread in some DAWs. JUCE's `URL::createInputStream()` is a blocking call.

**How to avoid:**
License validation must run exclusively on the message thread (JUCE's main UI thread) or a dedicated background `juce::Thread`. The correct pattern:

1. On first `PluginEditor` construction (message thread), launch a background `juce::Thread` that performs the HTTP request to LemonSqueezy.
2. The thread stores the result via an atomic flag or a lock-free queue visible to the editor.
3. The editor polls this result in its `timerCallback()` and updates UI.
4. `processBlock` reads only the atomic flag (`licenseValid_.load()`). It never touches network code.

Never call `juce::URL::createInputStream()`, `juce::WebInputStream`, or any LemonSqueezy HTTP wrapper from `processBlock`, `prepareToPlay()`, or any `AudioProcessorParameter::Listener` callback.

**Warning signs:**
- Audio glitches or dropout on first plugin load
- DAW logs showing "audio thread blocked for Xms"
- Debug assertion: "This function should only be called from the message thread"

**Phase to address:** License validation implementation phase — must be the first architectural decision before writing any validation code.

---

### Pitfall 2: AU Validation Fails Due to Wrong Plugin Type Mapping

**What goes wrong:**
ChordJoystick is currently `IS_SYNTH TRUE`, `IS_MIDI_EFFECT FALSE`, with a silent stereo output bus. The AU type for this configuration is `aumu` (musical instrument). However `auval` also checks: the output bus layout must be consistent with the declared type, and MIDI output capability must be declared correctly.

In JUCE's CMake AU wrapper, `NEEDS_MIDI_OUTPUT TRUE` is required for the plugin to emit MIDI in the AU format. Without it, `auval` reports "No MIDI output" or Logic Pro silently ignores all MIDI. Setting `IS_MIDI_EFFECT TRUE` instead causes `auval` to use type `aumi` (MIDI processor) and Logic places the plugin on a MIDI channel strip, not an instrument slot — changing the workflow users expect.

**Why it happens:**
Developers copy CMake configuration from a VST3-only target and do not realise the AU wrapper requires additional CMake flags to expose MIDI output.

**How to avoid:**
In `CMakeLists.txt`, the AU target must declare:
```
IS_SYNTH TRUE
IS_MIDI_EFFECT FALSE
NEEDS_MIDI_INPUT TRUE
NEEDS_MIDI_OUTPUT TRUE
```
The silent stereo output bus must remain enabled (JUCE's AU wrapper requires at least one output bus for the `aumu` type — even if it outputs silence). Verify with `auval -v aumu [SUBT] [MANU]` on the command line before any DAW testing.

**Warning signs:**
- `auval` passes but Logic Pro shows no MIDI output from the plugin
- Logic places plugin on audio channel strip instead of instrument slot
- `auval -v aufx` (effect type) finds the plugin when `aumu` was expected

**Phase to address:** Mac build phase — CMake AU target configuration must be verified before any AU DAW testing.

---

### Pitfall 3: AU Parameter IDs Break Saved State Between VST3 and AU Versions

**What goes wrong:**
JUCE generates AU parameter IDs differently from VST3 parameter IDs by default. In JUCE 8, AU parameters are ordered by a hash of their string identifier (not by registration order). If a user saves an AU preset and then you add new parameters in a subsequent build, Logic Pro remaps automation using parameter indices (not IDs), which means adding parameters can silently corrupt saved presets.

Additionally: if `JUCE_FORCE_USE_LEGACY_PARAM_IDS` is not set, JUCE's AU wrapper sorts existing parameters by hash and adds new ones via a version hint. Failing to supply a `versionHint` to every `AudioProcessorValueTreeState::Parameter` constructor means all parameters get hint `0`, and their AU order is hash-based — unpredictable and not the same as VST3 order.

**Why it happens:**
VST3 parameter IDs are handled by JUCE automatically. AU is a different format with different identity rules. Developers test on VST3, ship AU, and only notice the problem when Logic users report broken presets after an update.

**How to avoid:**
From the first AU build: supply explicit `versionHint = 1` to every `APVTS::ParameterLayout` entry. For any new parameters added in v2.1+, supply `versionHint = 2`. Never remove or reorder existing parameters between releases. Run `auval` after any parameter change. Document the version hint assignment in code comments.

Also set `JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING 1` only if you have deliberately diverged VST3 and AU IDs — do not set this to silence legitimate warnings.

**Warning signs:**
- `auval` warning: "Parameter did not retain default value when set"
- Logic Pro automation lanes point to wrong parameters after a plugin update
- APVTS `restoreFromXML` produces incorrect parameter values when loading a VST3 preset in the AU version

**Phase to address:** Mac build phase — set all version hints before the first AU release to any user.

---

### Pitfall 4: Manufacturer Code Must Have at Least One Uppercase Character

**What goes wrong:**
`auval` rejects AU plugins with a manufacturer OSType (4-character code) that is entirely lowercase. The error message is: `ERROR: Manufacturer OSType should have at least one non-lower case character`. JUCE reads this from `PLUGIN_MANUFACTURER_CODE` in `CMakeLists.txt`. If the existing Windows build used a code like `"dimea"` or `"juce"`, the AU build will fail `auval`.

**Why it happens:**
VST3 does not enforce this rule, so it was never caught during Windows development.

**How to avoid:**
Set `PLUGIN_MANUFACTURER_CODE "Dima"` (or similar) in `CMakeLists.txt`. Verify with `auval -v aumu [SUBT] [MANU]`. The 4-character code must also not conflict with any existing Apple-registered manufacturer code. Check current registration at the Apple AU manufacturer code registry.

**Warning signs:**
- `auval` output: `ERROR: Manufacturer OSType should have at least one non-lower case character`
- AU plugin does not appear in Logic's plugin manager after installation

**Phase to address:** Mac build phase — before any `auval` run.

---

### Pitfall 5: SDL2 Initialization Conflicts with macOS Main Thread Requirements

**What goes wrong:**
SDL2 on macOS requires `SDL_Init` and all event processing to run on the main thread. ChordJoystick's `GamepadInput` currently initialises SDL2 in a background timer thread (60 Hz `juce::Timer` callback). On Windows this works because Win32 allows SDL event polling from non-main threads. On macOS, SDL2's Cocoa backend calls into AppKit APIs (`NSApplication`, `NSEvent`) which must run on the main thread. Calling these from a JUCE timer callback (which runs on the message thread — which IS the main thread in a plugin editor context, but is NOT the main thread if the editor is not open) causes a crash or silent SDL failure.

Additionally, `SDL_INIT_JOYSTICK`/`SDL_INIT_GAMECONTROLLER` on macOS uses the IOKit HID Manager internally, which also requires main-thread context.

**Why it happens:**
The Windows port works because SDL2's Windows backend does not use a GUI toolkit. The macOS port exposes SDL2's Cocoa/IOKit threading requirements that were never exercised.

**How to avoid:**
On macOS, wrap SDL2 initialization in `juce::MessageManager::callAsync()` to ensure it runs on the main message thread. The 60 Hz timer callback in `GamepadInput` may need to be started after the editor is opened (ensuring the message loop is running). Test with the plugin editor closed — the gamepad must not crash the host when the UI is not visible.

Also: when building as a universal binary, ensure the SDL2 static library is built as a fat binary (`lipo -create`) containing both `arm64` and `x86_64` slices. A single-arch SDL2 lib will produce a link error during the universal binary build. Use SDL2's `build-scripts/clang-fat.sh` or build SDL2 twice (once per arch) and combine with `lipo`.

**Warning signs:**
- Plugin crashes on macOS when joystick is connected but editor is closed
- SDL2 returns error on `SDL_Init`: "No displays available" or AppKit threading assertion
- `lipo -info libSDL2.a` shows single architecture when universal binary is expected

**Phase to address:** Mac build phase — SDL2 universal binary is a build-system blocker; threading fix must be verified in headless host scenario.

---

### Pitfall 6: Hardened Runtime Breaks SDL2 HID/Gamepad Access Without Entitlements

**What goes wrong:**
macOS notarization requires hardened runtime (`--options runtime` in `codesign`). Hardened runtime by default blocks access to USB HID devices (gamepads use the IOKit HID Manager). Without the correct entitlements, `SDL_Init(SDL_INIT_GAMECONTROLLER)` silently returns 0 (success) but `SDL_NumJoysticks()` returns 0 — no gamepads detected. No error is reported; the feature silently stops working on notarized builds.

**Why it happens:**
Unsigned/ad-hoc-signed builds during development bypass hardened runtime checks. The problem only appears after proper code signing with hardened runtime enabled, which is the last step before distribution.

**How to avoid:**
Create an entitlements `.plist` file that includes:
```xml
<key>com.apple.security.cs.disable-library-validation</key><true/>
```
For USB HID access (gamepads), add:
```xml
<key>com.apple.security.device.usb</key><true/>
```
For Bluetooth gamepads (PS4/PS5 via Bluetooth), add:
```xml
<key>com.apple.security.device.bluetooth</key><true/>
```
Pass this entitlements file to `codesign`: `codesign --force -s "$DEV_ID" --options runtime --entitlements entitlements.plist Plugin.vst3`.

Test the signed build on a separate Mac (not the build Mac) to confirm gamepads are detected. The build Mac's development provisioning may mask the entitlement check.

**Warning signs:**
- `SDL_NumJoysticks()` returns 0 in notarized build, non-zero in unsigned build
- No crash, no error message — gamepad feature silently non-functional
- Console.app shows IOKit access denied messages for the plugin process

**Phase to address:** Code signing phase — entitlements file must be defined before notarization attempt; test on clean Mac.

---

### Pitfall 7: Notarization Rejection Due to Incorrect Signing Order in Nested Bundles

**What goes wrong:**
A VST3 plugin is a bundle containing a binary. An AU `.component` is also a bundle. If the plugin bundle is signed before inner resources are signed, or if the outer bundle is signed with `--deep` (which Apple now discourages for complex bundles), Apple's notarization service rejects with "The signature of the binary is invalid" or "The binary was not signed with a valid Developer ID certificate."

The correct order: sign innermost binaries first, then sign the outer bundle without `--deep`. Using `--deep` on a bundle that contains another already-signed bundle re-signs the inner bundle and invalidates it.

**Why it happens:**
The `--deep` flag looks like it handles nested signing automatically. It does not — it re-signs inner components, which breaks their signatures.

**How to avoid:**
Sign in leaf-to-root order:
1. Sign any frameworks or dylibs inside the bundle.
2. Sign the plugin binary itself (`MacOS/ChordJoystick`).
3. Sign the outer bundle directory (`ChordJoystick.vst3`).
4. Zip the signed bundle.
5. Submit zip to `xcrun notarytool submit`.
6. After Apple approves, `xcrun stapler staple ChordJoystick.vst3`.

Do not use `--deep` in step 3. Verify each level with `codesign --verify --deep --strict ChordJoystick.vst3` after signing.

The `.pkg` installer must also be signed with a Developer ID Installer certificate (separate from the Developer ID Application certificate used for plugin binaries) and notarized separately.

**Warning signs:**
- `notarytool log` shows `"The signature of the binary is invalid"`
- `codesign --verify --deep --strict` shows "sealed resource modified"
- Plugin is rejected by Gatekeeper on clean Mac even after notarization

**Phase to address:** Code signing and notarization phase — this is its own dedicated phase due to the complexity of the signing pipeline.

---

### Pitfall 8: Network Entitlement Missing for LemonSqueezy API Calls

**What goes wrong:**
With hardened runtime enabled, outgoing network connections require the `com.apple.security.network.client` entitlement. Without it, `juce::URL::createInputStream()` (or any HTTP client) silently fails — the connection attempt returns null with no user-visible error. The license validation background thread receives a null stream, interprets it as "network unavailable," and may either fail open (grant access anyway) or fail closed (block the user permanently).

**Why it happens:**
Network access works freely in development builds (no hardened runtime). The restriction is invisible until the notarized build is tested.

**How to avoid:**
Add to the entitlements `.plist`:
```xml
<key>com.apple.security.network.client</key><true/>
```
Test the signed build's network access explicitly: validate a real license key against the LemonSqueezy API in the notarized build, on a machine that has never run the unsigned build.

**Warning signs:**
- License validation always fails in notarized build, always succeeds in dev build
- `juce::URL::createInputStream()` returns `nullptr` with no exception
- System log shows network sandbox denial for the plugin process

**Phase to address:** Code signing phase — entitlement must be in the entitlements file before any notarization attempt.

---

## Technical Debt Patterns

Shortcuts that seem reasonable but create long-term problems.

| Shortcut | Immediate Benefit | Long-term Cost | When Acceptable |
|----------|-------------------|----------------|-----------------|
| Ad-hoc signing for beta Mac builds | Skip Apple Developer account cost | Gatekeeper blocks distribution; every user must `xattr -rd com.apple.quarantine` manually | Beta testers only; never for public release |
| Skipping `auval` and testing only in Logic | Faster AU validation loop | Logic's scanner is more permissive than `auval`; ships broken plugin | Never — `auval` before every AU release |
| Storing license key in `~/Library/Preferences` plist in plaintext | Trivial implementation | License key is readable by any process with user access; trivially shared | Never — use macOS Keychain |
| Synchronous LemonSqueezy validation at every plugin load | Simple implementation | Blocks message thread on slow network; plugin appears to hang in DAW | Never |
| Single-arch (arm64-only) macOS build | Faster build, smaller binary | Excludes Intel Mac users (Rosetta works but adds compatibility matrix complexity) | Only if Intel support is explicitly out of scope |
| Using `--deep` for bundle signing | Single codesign command | Re-signs inner bundles, invalidates their signatures, notarization fails | Never for complex plugin bundles |

---

## Integration Gotchas

Common mistakes when connecting to external services.

| Integration | Common Mistake | Correct Approach |
|-------------|----------------|------------------|
| LemonSqueezy Validate API | Call `/v1/licenses/validate` on every `processBlock` | Call once on first editor open, then re-validate at configurable interval (e.g., 24 hours) from a background thread |
| LemonSqueezy Activate API | Activate a new instance every time the plugin loads | Activate once, store `instance_id` in Keychain alongside the key; use that `instance_id` for subsequent validates |
| LemonSqueezy offline handling | Treat HTTP failure as license invalid, block user | Cache last-known-valid state with a timestamp; allow configurable offline grace period (e.g., 7 days) before restricting features |
| macOS Keychain | Use `kSecAttrAccessibleAlways` for stored license key | Use `kSecAttrAccessibleAfterFirstUnlock` — works after device boot without user interaction, does not expose key before first unlock |
| Apple notarytool | Submit using deprecated `altool` | Use `xcrun notarytool submit` — `altool` was removed in Xcode 14 |

---

## Performance Traps

Patterns that work at small scale but fail as usage grows.

| Trap | Symptoms | Prevention | When It Breaks |
|------|----------|------------|----------------|
| Blocking license network call in `PluginProcessor` constructor | DAW scan hangs for the duration of DNS + TLS + HTTP round-trip (~200–2000ms) | Background thread with async result | Any network latency above ~50ms; especially noticeable during DAW plugin scan of all installed plugins |
| Polling `juce::URL` on message thread in `timerCallback` | UI freezes during HTTP response | Non-blocking `juce::WebInputStream` with async notification or dedicated `juce::Thread` | Any response time above one timer tick (~30ms at 30 Hz) |
| Re-signing entire bundle with `codesign --force --deep` in CI | Works when bundle is simple | Breaks nested bundle signatures; notarization fails | Any bundle with signed inner components |

---

## Security Mistakes

Domain-specific security issues beyond general C++ advice.

| Mistake | Risk | Prevention |
|---------|------|------------|
| Storing LemonSqueezy license key in `UserDefaults` / plist | Key readable in plaintext by any process; easily copied to unlock pirated copies | macOS Keychain (`SecItemAdd` with `kSecClassGenericPassword`) |
| Storing license key in the JUCE `ApplicationProperties` file | Same as plist — plaintext on disk in `~/Library/Preferences/` | macOS Keychain |
| Embedding LemonSqueezy API secret key in plugin binary | API secret extractable with `strings` command; enables mass license generation | LemonSqueezy validation API is public — it does not require a secret key. Only use public Validate/Activate endpoints. Never embed a store API key. |
| Fail-open on network error without grace-period limit | Any network failure permanently unlocks all features | Explicit grace period: store `lastValidatedTimestamp`; allow up to N days offline; after N days, restrict features until re-validated |
| Not verifying LemonSqueezy response fields | Attacker returns spoofed `{"valid": true}` from a local proxy | Verify `license_key.status == "active"` and `meta.store_id` matches your store; do not trust only the top-level `valid` field |

---

## UX Pitfalls

Common user experience mistakes in this domain.

| Pitfall | User Impact | Better Approach |
|---------|-------------|-----------------|
| Blocking the plugin UI while license validates on first launch | Plugin appears frozen; user force-quits DAW | Show "Checking license..." state immediately; validate in background; unlock UI when result arrives |
| Showing license dialog in `PluginProcessor` constructor | Constructor may run during DAW plugin scan (headless); dialog in headless context crashes or blocks indefinitely | Show dialog only from `PluginEditor` constructor on the message thread |
| Requiring online activation for beta testers who don't have a LemonSqueezy account | Beta testers cannot use the plugin | Issue beta license keys via LemonSqueezy's key generation, or add a separate "beta mode" flag |
| Gatekeeper block on first launch with no user guidance | Users see "can't be opened because Apple cannot verify it" and abandon the product | Notarize the installer `.pkg` and the plugin bundles; staple the notarization ticket so Gatekeeper passes without network; include install instructions for quarantine removal if distributing unsigned beta builds |
| AU preset format incompatible with VST3 presets | Users cannot move presets between formats | Document clearly that presets are format-specific; JUCE APVTS state is portable but AU/VST3 host preset containers are not |

---

## "Looks Done But Isn't" Checklist

Things that appear complete but are missing critical pieces.

- [ ] **Mac build compiles:** Verify `lipo -info ChordJoystick.vst3/Contents/MacOS/ChordJoystick` shows both `x86_64` and `arm64` — a single-arch build compiles without error
- [ ] **AU format works:** Run `auval -v aumu [SUBT] [MANU]` — a plugin that loads in Logic may still fail `auval`
- [ ] **Gamepad detected:** Test `SDL_NumJoysticks()` with a signed (hardened runtime) build — unsigned build always works
- [ ] **License validation works:** Test the full validation flow in a notarized build on a clean Mac — dev builds bypass network entitlement checks
- [ ] **Notarization stapled:** Run `xcrun stapler validate ChordJoystick.vst3` — notarization without stapling fails Gatekeeper offline
- [ ] **Installer pkg signed:** Verify with `pkgutil --check-signature Installer.pkg` — plugin signing and installer signing are separate
- [ ] **Offline grace period:** Disconnect Mac from network after validation, restart DAW — plugin must function for configured grace period
- [ ] **License key in Keychain:** Run `security find-generic-password -a ChordJoystick` in Terminal — key must not appear in plaintext in any plist

---

## Recovery Strategies

When pitfalls occur despite prevention, how to recover.

| Pitfall | Recovery Cost | Recovery Steps |
|---------|---------------|----------------|
| AU parameter IDs broken by parameter reorder | HIGH | Add version hints to all existing params (hint=1) and new params (hint=2); publish a migration note; user re-saves presets |
| Notarization rejected (invalid signature) | MEDIUM | Re-sign in leaf-to-root order without `--deep`; re-submit to notarytool; budget ~30 min for re-sign + Apple processing time |
| Hardened runtime breaks gamepad (missing entitlement) | LOW | Add entitlement to `.plist`; re-sign all bundles; re-notarize |
| Network entitlement missing (LemonSqueezy fails silently) | LOW | Same as above — add `com.apple.security.network.client` to entitlements and re-notarize |
| SDL2 fat binary missing arm64 | MEDIUM | Rebuild SDL2 for both arches; `lipo -create`; re-link plugin; rebuild universal binary |
| Audio thread network call causing dropout | HIGH | Architectural fix required — move validation to background thread; cannot be patched with a flag |

---

## Pitfall-to-Phase Mapping

How roadmap phases should address these pitfalls.

| Pitfall | Prevention Phase | Verification |
|---------|------------------|--------------|
| Network call on audio thread (P1) | License implementation phase | Unit test: call `licenseCheck()` from a mock processBlock — must not block |
| AU plugin type mapping wrong (P2) | Mac build phase | `auval -v aumu [SUBT] [MANU]` passes; Logic shows MIDI output |
| AU parameter IDs break presets (P3) | Mac build phase | Set version hints before first AU user release; `auval` parameter retention tests pass |
| Manufacturer code all-lowercase (P4) | Mac build phase | `auval` first run — fails immediately with explicit error |
| SDL2 threading on macOS (P5) | Mac build phase | Plugin loads with editor closed and gamepad connected — no crash |
| SDL2 fat binary missing arch (P5) | Mac build phase | `lipo -info libSDL2.a` shows both arches before linking |
| Hardened runtime blocks gamepad (P6) | Code signing phase | Test notarized build with PS4/Xbox controller on clean Mac |
| Incorrect bundle signing order (P7) | Code signing phase | `codesign --verify --deep --strict` passes; notarytool accepts submission |
| Missing network entitlement (P8) | Code signing phase | LemonSqueezy validate call succeeds in notarized build on clean Mac |
| License key in plaintext storage | License implementation phase | `security find-generic-password` returns key; no plaintext in `~/Library/Preferences/` |

---

## Sources

- JUCE forum: AU parameter order and `JUCE_FORCE_USE_LEGACY_PARAM_IDS` — https://forum.juce.com/t/au-parameter-order-vs-vst-and-vst3-juce-force-use-legacy-param-ids/40555
- JUCE forum: AU validation failures — https://forum.juce.com/t/validation-fail-on-au-plugin/27228
- JUCE forum: AU auval manufacturer code uppercase requirement — https://forum.juce.com/t/au-validation-errors/15719
- JUCE forum: `auval` + MIDI output for instrument plugins — https://forum.juce.com/t/audio-units-auval-and-midi/9954
- JUCE forum: JUCE AU plugin "No types" (bus layout) — https://forum.juce.com/t/pluginval-au-plugins-returning-no-types-but-play-fine-in-logic/36687
- JUCE forum: Universal binary CMake — https://forum.juce.com/t/cmake-plugin-and-os-11-universal-binary/41997
- JUCE forum: AU universal binary Xcode 16.2 issue (Feb 2025) — https://forum.juce.com/t/au-universal-binary-macos-sequoia-and-xcode-16-2/65337
- JUCE forum: Network connection from audio plugin — https://forum.juce.com/t/network-connection-from-audio-plugin/38105
- JUCE forum: Checking license validation at plugin load — https://forum.juce.com/t/checking-license-validation-at-plugin-load/66298
- JUCE forum: License key storage — https://forum.juce.com/t/license-key-storage/51046
- JUCE forum: Message thread / audio thread deadlock — https://forum.juce.com/t/message-thread-and-audio-thread-stuck-waiting-for-eachother/32755
- JUCE forum: Gatekeeper notarised distributables — https://forum.juce.com/t/apple-gatekeeper-notarised-distributables/29952
- Melatonin blog: How to code sign and notarize macOS audio plugins in CI — https://melatonin.dev/blog/how-to-code-sign-and-notarize-macos-audio-plugins-in-ci/
- Moonbase: Code signing audio plugins in 2025 — https://moonbase.sh/articles/code-signing-audio-plugins-in-2025-a-round-up/
- Moonbase: Debugging AU plugin with auval — https://moonbase.sh/articles/debugging-your-audio-unit-plugin-with-auval-aka-auvaltool/
- KVR Audio: HOWTO macOS notarization (plugins) — https://www.kvraudio.com/forum/viewtopic.php?t=531663
- Apple Developer Docs: Resolving common notarization issues — https://developer.apple.com/documentation/security/resolving-common-notarization-issues
- Apple Developer Docs: Disable Library Validation Entitlement — https://developer.apple.com/documentation/bundleresources/entitlements/com.apple.security.cs.disable-library-validation
- Syntheway: De-quarantine VST/AU plugins macOS — https://syntheway.com/fix-au-vst-vst3-macos.htm
- SDL2 Wiki: macOS README — https://github.com/letoram/SDL2/blob/master/docs/README-macosx.md
- SDL2 Discord/Forum: Compile macOS universal binary — https://discourse.libsdl.org/t/compile-macos-universal/28759
- LemonSqueezy Docs: License API — https://docs.lemonsqueezy.com/api/license-api
- LemonSqueezy Docs: Validate a License Key — https://docs.lemonsqueezy.com/api/license-api/validate-license-key
- Reillyspitzfaden.com: Cross-Platform JUCE with CMake & GitHub Actions (2025) — https://reillyspitzfaden.com/posts/2025/08/plugins-for-everyone-crossplatform-juce-with-cmake-github-actions/

---
*Pitfalls research for: ChordJoystick v2.0 — Windows-to-macOS port, AU format, LemonSqueezy licensing, code signing, notarization*
*Researched: 2026-03-10*
