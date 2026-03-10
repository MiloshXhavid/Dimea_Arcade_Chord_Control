# Project Research Summary

**Project:** ChordJoystick v2.0 — Cross-Platform Launch
**Domain:** JUCE VST3+AU MIDI Plugin — macOS universal binary, license key validation, macOS installer, cross-platform code signing
**Researched:** 2026-03-10
**Confidence:** MEDIUM-HIGH

## Executive Summary

ChordJoystick v2.0 adds three capabilities to the existing Windows-only v1.9 plugin: macOS distribution (AU + VST3, universal binary, notarized PKG installer), a license key validation system (LemonSqueezy REST API), and cross-platform code signing (macOS Developer ID + Windows Azure Trusted Signing or OV cert). The core plugin DSP, UI, looper, LFO, arpeggiator, and gamepad code requires zero changes — all new work is infrastructure and a thin licensing layer on top. Two new C++ files are introduced (`LicenseManager` and `LicensePanel`); CMakeLists.txt requires approximately 12 lines of additions; and the macOS installer pipeline is a new shell script with no new CMake dependencies.

The recommended approach uses zero new external libraries. macOS HTTP calls use JUCE's native `juce::URL` with `NSURLSession` transport (already configured via `JUCE_USE_CURL=0`). License state persists via `juce::PropertiesFile`. The AU format target already exists in `CMakeLists.txt` — the primary CMake work is adding `CMAKE_OSX_ARCHITECTURES "arm64;x86_64"` before FetchContent calls and wrapping the Inno Setup `configure_file` in a `WIN32` guard. One critical CMake line is currently missing: `HARDENED_RUNTIME_OPTIONS "com.apple.security.network.client"` — without it, all LemonSqueezy API calls silently fail in notarized builds.

The dominant risks are sequencing and operational, not algorithmic. Four mistakes have catastrophic consequences: (1) running network I/O on the audio thread — this is unfixable post-ship without an architectural rewrite and causes dropouts or DAW freezes; (2) setting `CMAKE_OSX_ARCHITECTURES` after FetchContent — produces single-arch static libs and a linker failure; (3) signing plugin bundles with `codesign --deep` — re-signs inner components and causes notarization rejection; and (4) omitting entitlements for HID access — silently disables gamepad detection in all notarized builds with no error message. All four are fully avoidable by strict phase ordering with explicit verification gates.

## Key Findings

### Recommended Stack

The Windows stack (JUCE 8.0.4, SDL2 2.30.9 static, CMake FetchContent, C++17, MSVC, Catch2, Inno Setup 6) is locked and unchanged. For macOS, the only CMake additions are two cache variables — `CMAKE_OSX_ARCHITECTURES "arm64;x86_64"` and `CMAKE_OSX_DEPLOYMENT_TARGET "11.0"` — both placed before FetchContent so all static libs (JUCE, SDL2, Catch2) compile as fat binaries. The Xcode generator is mandatory on macOS; JUCE's AU wrapper requires it for correct `.component` bundle generation. Xcode 15.x is preferred — Xcode 16.2 has a community-confirmed regression where AU bundles build as single-arch despite the architecture flag, while VST3 builds are unaffected.

For code signing, macOS requires the Apple Developer Program ($99/year), which provides both a Developer ID Application certificate (signs plugin bundles) and a separate Developer ID Installer certificate (signs the `.pkg`). For Windows, Azure Trusted Signing ($9.99/month, cloud HSM, instant SmartScreen reputation) is preferred over traditional EV/OV certificates — EV no longer provides instant SmartScreen reputation as of March 2024, eliminating its cost premium. Azure Trusted Signing is currently restricted to US/Canadian organizations with 3+ years of history; verify eligibility before planning this path. OV certificate with a USB hardware token is the fallback.

**Core technologies (all new additions for v2.0):**
- `CMAKE_OSX_ARCHITECTURES "arm64;x86_64"` + Xcode generator: universal binary — must be set before all FetchContent calls
- `juce::URL` + `juce_network` (already transitively linked): LemonSqueezy HTTPS POST — zero new libraries, native transport on both platforms
- `juce::PropertiesFile` (already in juce_core): license key + instance_id persistence — resolves cross-platform OS-appropriate path automatically
- `HARDENED_RUNTIME_OPTIONS "com.apple.security.network.client"`: one missing CMake line that silently blocks all license validation in notarized builds
- `pkgbuild` + `productbuild` (macOS built-in CLT): PKG installer — handles system plugin path permissions that DMG drag-to-folder cannot on macOS Sonoma
- `xcrun notarytool` (Xcode 13+ built-in): notarization — `altool` was retired November 2023, do not use
- Apple Developer Program ($99/year): Developer ID Application + Installer certificates
- Azure Trusted Signing (preferred) or OV cert: Windows SmartScreen-free installer signing

### Expected Features

**Must have — v2.0 table stakes (all P1):**
- License key entry dialog on first launch — overlay Component (not `DialogWindow`), shown via 200ms deferred `callAfterDelay` to allow host window embedding to complete
- Persistent local license storage — `juce::PropertiesFile` at platform-appropriate path; key + instance_id survive DAW restarts
- 14-day offline grace period — `last_validated_timestamp` in PropertiesFile; background re-validation on launch if stale; audio thread never touches network
- User-readable error messages — map each LemonSqueezy error string to plain-language text (key not found, activation limit reached, key disabled, network unreachable)
- Machine count display — show `activation_usage / activation_limit` from LemonSqueezy response
- AU format on macOS — mandatory for Logic Pro and GarageBand; VST3-only would lock out the two most-used Mac DAWs
- VST3 macOS universal binary — for Ableton, Reaper, FL Studio, Cubase on Mac
- Notarized PKG installer (macOS) — Gatekeeper silently blocks unnotarized plugins; PKG handles system path permissions DMG cannot
- Windows code signing — Azure Trusted Signing or OV cert to suppress SmartScreen warning on installer

**Should have — v2.x after launch validation (P2):**
- "Deactivate this machine" button — calls `POST /v1/licenses/deactivate` with stored `instance_id`; reduces support load as user base grows
- Named machine display — `juce::SystemStats::getComputerName()` as `instance_name` at activation; shows which activation belongs to which device
- Silent weekly background re-validation — surfaces key revocation within 7 days without user interaction

**Defer (v3+ or never):**
- AAX / Pro Tools format — Avid certification + iLok required; less than 10% of target market
- Challenge-response offline activation — LemonSqueezy does not support it natively; 14-day grace period covers real-world scenarios
- iLok / hardware dongle — hardware barrier alienates bedroom producer segment
- Linux support — JUCE supports it; commercial plugin sales volume is low

### Architecture Approach

The architecture introduces two new source files and makes small, localized additions to three existing files. `LicenseManager` is a process-level singleton — because multiple plugin instances share one OS process, license state must be shared and validated once per process lifetime. It owns the `juce::PropertiesFile`, runs background `juce::Thread` validation, and exposes a `LicenseStatus` enum (`Unknown`, `Valid`, `Invalid`, `NetworkError`, `Expired`). `LicensePanel` is a JUCE Component overlay that covers the full editor bounds; it delegates all network calls to `LicenseManager` and has no HTTP knowledge of its own. `PluginEditor` checks license status in its constructor and shows the panel via `juce::Timer::callAfterDelay(200ms)` — the 200ms delay is critical for AU hosts (Logic, GarageBand) which complete window embedding asynchronously after the editor is constructed. `PluginProcessor` reads only a cached atomic flag from `LicenseManager`; it never executes network code. `SdlContext` and `GamepadInput` are unchanged; `SDL_INIT_GAMECONTROLLER`-only initialization is already correct for macOS and requires no threading changes.

**Major components:**
1. `LicenseManager` (new singleton) — HTTPS calls to LemonSqueezy via `juce::URL`, `juce::PropertiesFile` persistence, `LicenseStatus` enum, background `juce::Thread` management, `juce::MessageManager::callAsync` result delivery
2. `LicensePanel` (new JUCE Component) — key entry field, activate button, status/error label, machine count display; full-editor overlay; lambda callback on success
3. `PluginEditor` (modified, ~10 lines) — `licensePanel_` unique_ptr member, deferred 200ms timer check in constructor body
4. `CMakeLists.txt` (modified, ~12 lines) — add `HARDENED_RUNTIME_OPTIONS`, add `if(APPLE)` arch/deployment block before FetchContent, wrap Inno Setup in `if(WIN32)`, add new source files to `target_sources`
5. `installer/mac/build-pkg.sh` (new shell script) — `codesign`, `pkgbuild`, `productbuild`, `xcrun notarytool submit`, `xcrun stapler staple` pipeline

### Critical Pitfalls

1. **Network I/O on the audio thread** — Any blocking HTTP call in `processBlock`, `prepareToPlay`, or a parameter callback causes audio dropouts or DAW freeze; this is an architectural defect, unfixable post-ship without a rewrite. `processBlock` must read only an atomic bool from `LicenseManager`; all `juce::URL::createInputStream()` calls run on a `juce::Thread` subclass with results delivered to the message thread via `juce::MessageManager::callAsync`.

2. **Missing macOS entitlements silently disable features** — Hardened runtime blocks both outgoing network (`com.apple.security.network.client`) and USB/Bluetooth HID access (`com.apple.security.device.usb`, `com.apple.security.device.bluetooth`) without these entries in the entitlements plist. Both failures are silent — no crash, no error, feature simply stops working. Development builds bypass hardened runtime checks entirely, making this invisible until a notarized build is tested on a clean Mac.

3. **`codesign --deep` invalidates nested bundle signatures** — The `--deep` flag re-signs inner components, breaking their existing signatures and causing notarization to reject the submission with "The signature of the binary is invalid." Sign leaf-to-root without `--deep`: inner dylibs first, then the plugin binary, then the outer bundle directory.

4. **`CMAKE_OSX_ARCHITECTURES` set after FetchContent** — FetchContent compiles static libs using whatever architecture setting is active at `FetchContent_MakeAvailable` time. Setting `CMAKE_OSX_ARCHITECTURES` afterward produces single-arch JUCE and SDL2 static libs, causing a linker error: "building for macOS-arm64 but attempting to link with file built for macOS-x86_64." The `if(APPLE)` block must precede all FetchContent calls.

5. **AU parameter version hints omitted from first release** — JUCE AU sorts parameters by hash of their string ID unless explicit `versionHint` values are supplied. Adding any new parameter in v2.1 without first setting `versionHint = 1` on all existing parameters will corrupt Logic Pro automation lanes and saved presets for existing users. This must be done before the first AU build reaches any user.

## Implications for Roadmap

The phase structure is dictated by hard dependencies: a working macOS universal binary must exist before AU testing, AU validation must pass before signing, signing must be correct before notarization, and the entitlements plist content is determined by the license system's network requirements. The license system implementation belongs after the macOS build is verified so it can be tested on both platforms simultaneously.

### Phase 1: macOS CMake Foundation and AU Build

**Rationale:** Everything depends on a working universal binary. The `CMAKE_OSX_ARCHITECTURES` ordering constraint is a hard build-system prerequisite for all subsequent phases. AU parameter version hints must be set before any AU user touches the plugin — impossible to fix retroactively without breaking existing presets. `auval` must pass before investing time in signing or distribution.

**Delivers:** Universal binary VST3 + AU builds on macOS; `auval -v aumu DCJM MxCJ` clean pass; SDL2 fat lib verified with `lipo -info`; Inno Setup `configure_file` guarded in `if(WIN32)`; `HARDENED_RUNTIME_OPTIONS "com.apple.security.network.client"` added to `juce_add_plugin`; `versionHint = 1` set on all existing APVTS parameters; Logic Pro load test with MIDI output confirmed.

**Addresses:** AU format table-stakes; VST3 macOS table-stakes; universal binary (arm64 + x86_64).

**Avoids:** `CMAKE_OSX_ARCHITECTURES` ordering pitfall; AU plugin type mapping pitfall; AU parameter version hint pitfall; manufacturer code case pitfall (PLUGIN_MANUFACTURER_CODE "MxCJ" has uppercase — already correct); SDL2 fat binary pitfall.

**Verification gate:** `lipo -info ChordJoystick.vst3/.../MacOS/ChordJoystick` reports `x86_64 arm64`; same for `.component`; `auval -v aumu DCJM MxCJ` exits with zero errors; Logic Pro detects plugin in instrument slot and emits MIDI.

**Research flag:** Standard, well-documented patterns. Skip `/gsd:research-phase`. Monitor JUCE forum for Xcode 16.2 AU universal binary regression resolution before selecting build machine Xcode version.

### Phase 2: License Validation System

**Rationale:** License implementation has one inviolable internal constraint — the threading architecture must be finalized before any code is written, because audio-thread network calls are unfixable post-ship. Placing this phase after Phase 1 allows simultaneous testing on Windows and macOS and ensures the `HARDENED_RUNTIME_OPTIONS` entitlement is already in place before network calls are tested in a signed build.

**Delivers:** `LicenseManager` singleton with `juce::PropertiesFile` persistence, LemonSqueezy activate/validate API calls on background `juce::Thread`, `LicenseStatus` enum, 14-day offline grace period via `last_validated_timestamp`; `LicensePanel` overlay Component with key entry, activate button, error label, machine count display; `PluginEditor` deferred 200ms check; user-readable error message mapping for all LemonSqueezy failure codes.

**Uses:** `juce::URL` + `juce_network`, `juce::JSON::parse`, `juce::PropertiesFile`, `juce::Thread` + `juce::MessageManager::callAsync`, `juce::Timer::callAfterDelay`, `juce::SystemStats::getComputerName`.

**Implements:** `LicenseManager.h/.cpp` (new), `LicensePanel.h/.cpp` (new), PluginEditor modifications (~10 lines).

**Avoids:** Audio-thread network call pitfall (critical, unfixable post-ship); modal dialog in constructor pitfall (AU host crash on Logic/GarageBand); LemonSqueezy bearer token embedded in binary; re-activation on every launch (consuming activation quota).

**Verification gate:** Key activates on first launch; key persists across DAW restarts; disconnecting network after activation allows normal plugin use for configured grace period; audio thread never blocks on network I/O (verify by running network round-trip under 5ms load).

**Research flag:** One gap — LemonSqueezy deactivation endpoint server-side behavior is LOW confidence (what the validate response looks like after `instance_id` is deleted server-side is not explicitly documented). Run a manual API test before building any deactivation flow. The "Deactivate this machine" button is a v2.x feature — do not block v2.0 on this. Separately: resolve the macOS Keychain vs PropertiesFile storage question before writing LicenseManager — PITFALLS.md recommends Keychain (`SecItemAdd` with `kSecAttrAccessibleAfterFirstUnlock`) over PropertiesFile (plaintext XML), but Keychain adds platform-specific code. Decide policy before implementation begins.

### Phase 3: Code Signing and Notarization

**Rationale:** Cannot precede Phase 1 (binary must exist) or Phase 2 (entitlements content is determined by what the license system requires). This phase is intentionally its own dedicated milestone because the signing pipeline — entitlements plist, leaf-to-root `codesign` order, `pkgbuild`, `productbuild`, `xcrun notarytool`, `xcrun stapler` — has multiple independent failure modes each requiring a clean-Mac verification. It is the most operationally complex phase despite involving the least code.

**Delivers:** `entitlements.plist` with `com.apple.security.network.client`, `com.apple.security.device.usb`, `com.apple.security.device.bluetooth`; signed .vst3 and .component bundles (leaf-to-root, no `--deep`); notarized and stapled PKG installer for macOS; Windows Azure Trusted Signing (or OV cert fallback) applied to Inno Setup .exe.

**Avoids:** Missing network entitlement (silently breaks license validation in notarized builds); missing HID entitlement (silently disables gamepad in notarized builds); incorrect bundle signing order with `--deep`; ad-hoc signing for public distribution.

**Verification gate (macOS):** `codesign --verify --deep --strict` passes on all bundles; `spctl --assess -vvv --type install` reports "accepted source=Notarized Developer ID"; `xcrun stapler validate` passes; `SDL_NumJoysticks()` returns non-zero with PS4/Xbox controller in the notarized build on a clean Mac that has never run the unsigned build; LemonSqueezy validate succeeds in the notarized build on clean Mac. **Verification gate (Windows):** Signed .exe installs without SmartScreen warning on a machine with no prior reputation for this binary.

**Research flag:** Azure Trusted Signing eligibility must be verified before this phase begins — if the entity is not US/Canadian with 3+ years of business history, the Windows signing toolchain changes to OV cert + USB hardware token. Plan procurement time accordingly. Apple Developer Program enrollment takes 1–5 business days for identity verification if not already enrolled.

### Phase 4: macOS PKG Distribution Polish

**Rationale:** After the signing and notarization pipeline is proven, the installer can be polished into a release-ready artifact. Separated from Phase 3 because investing time in distribution container polish before knowing the signing pipeline works is wasted effort.

**Delivers:** `installer/mac/build-pkg.sh` wrapping the full `pkgbuild` + `productbuild` + notarize + staple pipeline; `distribution.xml` with title, choice groups, and `<domains enable_localSystem="true"/>` for system-path installation; optionally a DMG as the outer download container (industry standard for audio plugins; DMG wrapping the signed PKG, not a drag-to-folder DMG).

**Avoids:** DMG-only drag-to-folder distribution (fails silently on macOS Sonoma for system plugin paths); `altool` notarization (retired November 2023).

**Verification gate:** Clean-Mac install (no prior version) via PKG succeeds and DAW detects the plugin at `/Library/Audio/Plug-Ins/` without requiring a manual scan path addition.

**Research flag:** Standard patterns with multiple JUCE-specific walkthroughs. Skip `/gsd:research-phase`.

### Phase Ordering Rationale

- Phase 1 must come first: `CMAKE_OSX_ARCHITECTURES` ordering is a hard build-system constraint; AU parameter version hints must be set before any AU user receives the plugin
- Phase 2 must precede Phase 3: the entitlements plist content (which network and HID entitlements are needed) is determined by what the license system requires; signing without this information wastes a notarization submission cycle
- Phase 3 must precede public release: Gatekeeper silently blocks unnotarized plugins — this is not optional
- Phase 4 follows Phase 3 naturally; packaging polish is independent of license system correctness

### Research Flags

Phases needing deeper research or pre-phase verification:
- **Phase 3 (Code Signing):** Azure Trusted Signing eligibility is restricted by geography and entity age. Verify at `azure.microsoft.com/products/artifact-signing` before planning Windows signing toolchain. If ineligible, switch to OV cert and document SmartScreen warning expectation in release notes.
- **Phase 2 (License, pre-implementation decision):** Resolve Keychain vs PropertiesFile security posture before writing `LicenseManager`. LemonSqueezy deactivation endpoint behavior requires a manual API test before building any deactivation flow (LOW confidence in current research).

Phases with standard patterns (skip research-phase):
- **Phase 1 (macOS CMake build):** JUCE CMake API docs are authoritative; universal binary CMake pattern is extensively documented and confirmed working on JUCE 8 with FetchContent.
- **Phase 4 (PKG installer):** `pkgbuild`/`productbuild` toolchain has multiple JUCE-specific walkthroughs (moonbase.sh, JUCE forum) with verified command sequences.

## Confidence Assessment

| Area | Confidence | Notes |
|------|------------|-------|
| Stack | HIGH | CMake/JUCE universal binary from official JUCE CMake API docs; pkgbuild/productbuild from Apple official CLT docs; LemonSqueezy API from official REST API docs; Azure Trusted Signing from Microsoft official docs |
| Features | MEDIUM-HIGH | LemonSqueezy API contract HIGH (official docs); Mac DAW format matrix HIGH (Ableton/Steinberg/Apple official); Windows SmartScreen/EV policy MEDIUM (Microsoft changed policy March 2024; Azure Trusted Signing eligibility changed April 2025 — verify current state before executing) |
| Architecture | HIGH (JUCE patterns) / MEDIUM (LemonSqueezy desktop integration) | JUCE singleton, PropertiesFile, juce::URL, callAfterDelay patterns are from official JUCE docs; LemonSqueezy desktop integration pattern is inferred from REST API docs + community practice, not an official JUCE integration guide |
| Pitfalls | MEDIUM-HIGH | Most pitfalls confirmed across multiple JUCE forum threads, melatonin.dev JUCE-specific guides, and Apple official docs; LemonSqueezy offline grace period behavior is LOW (sparse native desktop plugin documentation) |

**Overall confidence:** MEDIUM-HIGH

### Gaps to Address

- **Azure Trusted Signing eligibility:** Must be verified before Phase 3. If ineligible, plan for OV cert USB token — budget 1–3 weeks for certificate procurement and hardware token delivery.
- **Keychain vs PropertiesFile for license key storage:** PITFALLS.md recommends macOS Keychain (`SecItemAdd`, `kSecAttrAccessibleAfterFirstUnlock`) over plaintext PropertiesFile XML. Architecture.md recommends PropertiesFile for simplicity. Decide policy before Phase 2 implementation. Keychain is the correct security posture for a paid product but requires `Security.framework` linkage and platform-specific code.
- **Xcode 16.2 AU universal binary regression:** Community-confirmed (Feb 2025 JUCE forum) but not confirmed fixed. After first macOS build, immediately run `lipo -info` on the `.component` binary. If single-arch, install Xcode 15.x and verify the regression.
- **LemonSqueezy deactivation endpoint behavior:** Official docs do not explicitly document how the validate endpoint behaves after `instance_id` is deleted server-side, or how key revocation surfaces in the validate response body. Run manual API tests (revoke a test key, observe validate response) before building any grace period enforcement or deactivation flow. This is LOW confidence in current research.
- **GarageBand sandbox and network access:** GarageBand may be more restrictive than Logic Pro regarding plugin network access from inside the host's app sandbox. If GarageBand is a target, test license activation specifically inside GarageBand (not just Logic) after notarization. May require `SUPPRESS_AU_PLIST_RESOURCE_USAGE TRUE` in `juce_add_plugin`.

## Sources

### Primary (HIGH confidence)
- JUCE CMake API docs — `juce_add_plugin` FORMATS, AU_MAIN_TYPE, HARDENED_RUNTIME_ENABLED/OPTIONS, BUNDLE_ID, company name constraint: `github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md`
- LemonSqueezy License API — activate, validate, deactivate endpoints, request/response schema: `docs.lemonsqueezy.com/api/license-api`
- Apple Developer: Signing Mac Software with Developer ID — certificate types and requirements: `developer.apple.com/developer-id/`
- Apple Developer: Customizing the notarization workflow — notarytool, stapler: `developer.apple.com/documentation/security/customizing-the-notarization-workflow`
- Apple Developer: Resolving common notarization issues: `developer.apple.com/documentation/security/resolving-common-notarization-issues`
- SDL2 CMake README — `CMAKE_OSX_ARCHITECTURES` propagation to SDL2 FetchContent builds: `github.com/libsdl-org/SDL/blob/main/docs/README-cmake.md`
- Direct file inspection: `CMakeLists.txt` (this repository) — confirmed existing FORMATS VST3 AU, AU_MAIN_TYPE kAudioUnitType_MusicDevice, HARDENED_RUNTIME_ENABLED TRUE, BUNDLE_ID com.dimeaarcade.chordcontrol, PLUGIN_MANUFACTURER_CODE MxCJ, PLUGIN_CODE DCJM, elseif(APPLE) post-build block, JUCE_USE_CURL=0

### Secondary (MEDIUM confidence)
- JUCE forum: AU universal binary macOS Sequoia and Xcode 16.2 regression — `forum.juce.com/t/au-universal-binary-macos-sequoia-and-xcode-16-2/65337`
- JUCE forum: CMake plugin OS 11 universal binary — `forum.juce.com/t/cmake-plugin-and-os-11-universal-binary/41997`
- JUCE forum: Network connection from audio plugin (entitlements) — `forum.juce.com/t/network-connection-from-audio-plugin/38105`
- JUCE forum: AU parameter order and JUCE_FORCE_USE_LEGACY_PARAM_IDS — `forum.juce.com/t/au-parameter-order-vs-vst-and-vst3-juce-force-use-legacy-param-ids/40555`
- JUCE forum: pkgbuild and productbuild tutorial + pamplejuce example — `forum.juce.com/t/pkgbuild-and-productbuild-a-tutorial-pamplejuce-example/64977`
- Melatonin: Code sign and notarize macOS audio plugins in CI — `melatonin.dev/blog/how-to-code-sign-and-notarize-macos-audio-plugins-in-ci/`
- Melatonin: Code signing on Windows with Azure Trusted Signing — `melatonin.dev/blog/code-signing-on-windows-with-azure-trusted-signing/`
- moonbase.sh: macOS installers with pkgbuild and productbuild for JUCE — `moonbase.sh/articles/how-to-make-macos-installers-for-juce-projects-with-pkgbuild-and-productbuild/`
- moonbase.sh: Code signing audio plugins in 2025 — `moonbase.sh/articles/code-signing-audio-plugins-in-2025-a-round-up/`
- pamplejuce reference template — JUCE 8 + macOS notarization + Azure Trusted Signing: `github.com/sudara/pamplejuce`
- SDL2 GitHub issue #11437: SDL_Init(SDL_INIT_VIDEO) off-main-thread Objective-C crash on macOS (Nov 2024)
- SDL2 GitHub issue #8696: arm64 macOS Ventura crash (fixed in SDL 2.30.x)
- Microsoft: EV vs OV SmartScreen parity post-March 2024 — `learn.microsoft.com/en-us/answers/questions/417016/reputation-with-ov-certificates`

### Tertiary (LOW confidence)
- LemonSqueezy offline grace period behavior — inferred from REST API docs + standard indie plugin practice; LemonSqueezy has no official native desktop plugin integration guide
- GarageBand plugin sandbox network behavior — JUCE forum anecdotes; requires empirical testing on actual GarageBand install post-notarization
- Azure Trusted Signing April 2025 eligibility changes — verify current requirements at `azure.microsoft.com/en-us/products/artifact-signing` before executing Phase 3

---
*Research completed: 2026-03-10*
*Ready for roadmap: yes*
