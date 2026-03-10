# Technology Stack

**Project:** ChordJoystick v2.0 — Cross-Platform Launch
**Domain:** JUCE VST3+AU MIDI plugin — macOS universal binary, license key validation, macOS installer, cross-platform code signing
**Researched:** 2026-03-10
**Confidence:** MEDIUM-HIGH (CMake/JUCE claims HIGH from official docs; signing claims MEDIUM from community sources; LemonSqueezy API HIGH from official docs)

> **Scope:** This file covers ONLY the new v2.0 additions. The locked Windows stack
> (JUCE 8.0.4, SDL2 2.30.9 static, CMake FetchContent, C++17, MSVC, Catch2, Inno Setup 6,
> static CRT MultiThreaded, GitHub releases via gh CLI) is NOT re-researched here.

---

## Area 1: macOS Universal Binary Build

### CMake Configuration

The single flag that makes every FetchContent dependency (JUCE, SDL2, Catch2) build as a fat binary:

```cmake
# Must be set BEFORE project() — set once, propagates to all FetchContent subdirs
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
```

- `CMAKE_OSX_ARCHITECTURES "arm64;x86_64"` — the standard CMake universal binary flag; propagates automatically to SDL2 and JUCE when set before `project()` or at configure time via `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`
- `CMAKE_OSX_DEPLOYMENT_TARGET "11.0"` — macOS 11 (Big Sur) is the minimum that supports Apple Silicon natively; required for arm64 slice; JUCE 8.0.4 supports 10.13+ but 11.0 is the practical floor for universal binaries targeting M1+ users

### Xcode Generator

```bash
cmake -S . -B build-mac -G Xcode \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="11.0"

cmake --build build-mac --config Release --target ChordJoystick_VST3
cmake --build build-mac --config Release --target ChordJoystick_AU
```

Use the Xcode generator (`-G Xcode`) on macOS — not `Unix Makefiles` or `Ninja`. JUCE's AU wrapper generation requires the Xcode toolchain for the post-build `auval`-compatible bundle structure.

### Known Issue: AU Universal Binary on Xcode 16.2 + JUCE 8.0.6

A February 2025 JUCE forum report ([AU universal binary macOS Sequoia and Xcode 16.2](https://forum.juce.com/t/au-universal-binary-macos-sequoia-and-xcode-16-2/65337)) states that AU plugins built with Xcode 16.2 compile only for arm64 despite `CMAKE_OSX_ARCHITECTURES` being set, while VST3 compiles correctly as universal. VST3 builds are unaffected. The workaround is to verify the output with `lipo -info PluginName.component/Contents/MacOS/PluginName` after build and, if single-arch, use Xcode 15.x or pin to a known-good Xcode version in CI. This bug may be resolved in a later JUCE or Xcode patch.

### JUCE CMakeLists.txt: HARDENED_RUNTIME_ENABLED

The existing `CMakeLists.txt` already sets `HARDENED_RUNTIME_ENABLED TRUE` in `juce_add_plugin`. This is **required** for macOS notarization — Apple rejects binaries that are not built with the hardened runtime. No change needed; this is already correct.

### JUCE CMakeLists.txt: AU Format

The existing `CMakeLists.txt` already includes `FORMATS VST3 AU`, `AU_MAIN_TYPE kAudioUnitType_MusicDevice`, and macOS copy commands in the `elseif(APPLE)` block. The `AU` format target is already wired. No structural CMake change is needed; the AU target exists but has never been built on macOS.

### SDL2 Universal Binary

SDL2 2.30.9 respects `CMAKE_OSX_ARCHITECTURES` when built via FetchContent. Because `CMAKE_OSX_ARCHITECTURES` is set before `FetchContent_MakeAvailable(SDL2)`, SDL2-static will be compiled as a fat library automatically. No SDL2-specific CMake changes are needed.

**Verification command after build:**
```bash
lipo -info build-mac/ChordJoystick_artefacts/Release/VST3/"Arcade Chord Control (BETA-Test).vst3"/Contents/MacOS/"Arcade Chord Control (BETA-Test)"
# Expected: Architectures in the fat file: x86_64 arm64
```

---

## Area 2: AU Plugin Format

### What JUCE Already Provides

JUCE 8.0.4 generates a full AudioUnit v2 (AUv2) component bundle from the same source files that produce the VST3. The `AU_MAIN_TYPE kAudioUnitType_MusicDevice` (already set in `CMakeLists.txt`) is correct for an instrument that sends MIDI — it registers the plugin as a music device in the CoreAudio component system, equivalent to VST3's `Instrument` category.

### No Source Code Changes Required

The AU wrapper is generated entirely by JUCE. `PluginProcessor.cpp` and `PluginEditor.cpp` do not need platform guards or AU-specific code. The existing `IS_SYNTH TRUE`, `NEEDS_MIDI_OUTPUT TRUE`, and silent stereo bus setup work identically for AU.

### auval Validation

Before shipping, validate the AU component with Apple's `auval` tool:
```bash
auval -v aumu DCJM MxCJ
# PLUGIN_CODE=DCJM, PLUGIN_MANUFACTURER_CODE=MxCJ (from CMakeLists.txt)
```
A clean `auval` pass is required for Logic Pro and GarageBand compatibility.

### AU Installation Path

System-wide (requires sudo, installer-friendly):
`/Library/Audio/Plug-Ins/Components/`

User-only (no elevation, matches existing macOS copy command in CMakeLists.txt):
`~/Library/Audio/Plug-Ins/Components/`

The production installer should install to `/Library/Audio/Plug-Ins/Components/` using `pkgbuild` (see Area 4). The CMakeLists.txt post-build copy to `~/Library/` is for local dev iteration only.

---

## Area 3: LemonSqueezy License Key Validation

### API Overview

LemonSqueezy provides a REST License API separate from their main store API. No API key is required from the client side — only the license key (and optionally the instance ID). All requests go to `https://api.lemonsqueezy.com/v1/licenses/`.

### Two-Step Flow

**Step 1: Activate** (first launch on a new machine)

```
POST https://api.lemonsqueezy.com/v1/licenses/activate
Content-Type: application/x-www-form-urlencoded

license_key=XXXX-XXXX-XXXX-XXXX&instance_name=MacBook-Pro-M2
```

Response includes `instance.id` (UUID string) — save this to disk alongside the license key. The `instance_id` is needed to validate on subsequent launches.

**Step 2: Validate** (every subsequent launch)

```
POST https://api.lemonsqueezy.com/v1/licenses/validate
Content-Type: application/x-www-form-urlencoded

license_key=XXXX-XXXX-XXXX-XXXX&instance_id=f90ec370-fd83-46a5-8bbd-44a241e78665
```

Response:
```json
{
  "valid": true,
  "error": null,
  "license_key": { "status": "active", "activation_limit": 2, "activation_usage": 1 },
  "instance": { "id": "...", "name": "..." },
  "meta": { "store_id": 12345, "product_id": 67890, "variant_id": 11111 }
}
```

Always verify `meta.store_id`, `meta.product_id`, and `meta.variant_id` match your product's IDs (hard-code these values in the plugin). The `valid` field is `true` only when the key is active and the instance is valid.

### C++ HTTP Implementation: juce::URL

**Recommended: `juce::URL` with `juce_network` module.** Do not add libcurl or any third-party HTTP library.

Rationale:
- The existing `CMakeLists.txt` sets `JUCE_USE_CURL=0` — this disables curl on all platforms and tells JUCE to use native HTTP (WinHTTP on Windows, NSURLSession on macOS). This is the correct setting for a plugin that does not want a libcurl dependency.
- `juce_network` is already an implicit transitive dependency of `juce_audio_processors`; no new `target_link_libraries` entry is needed.
- `juce::URL` supports HTTPS POST on both Windows (WinHTTP) and macOS (NSURLSession) natively with `JUCE_USE_CURL=0`.

**Required CMake change:** Add `JUCE_WEB_BROWSER=0` is already set. Add `juce::juce_core` to ensure `juce_network` is linked — in practice `juce_audio_processors` pulls it in, but explicit is safer:

```cmake
target_link_libraries(ChordJoystick PRIVATE
    ...
    juce::juce_core     # ensures juce_network is available for URL HTTP calls
)
```

Also remove (or conditionally set) `JUCE_USE_CURL=0` — it must remain `0` on Windows and macOS since we rely on native transports. This is already correctly set.

**HTTP call pattern:**

```cpp
// Activate (first launch)
juce::URL url("https://api.lemonsqueezy.com/v1/licenses/activate");
url = url.withPOSTData("license_key=" + key + "&instance_name=" + machineName);

int statusCode = 0;
juce::StringPairArray responseHeaders;
auto stream = url.createInputStream(
    juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
        .withStatusCode(&statusCode)
        .withResponseHeaders(&responseHeaders));

if (stream != nullptr && statusCode == 200)
{
    juce::String body = stream->readEntireStreamAsString();
    // parse JSON with juce::JSON::parse(body)
}
```

**JSON parsing:** Use `juce::JSON::parse()` (from `juce_core`) — already available, no additional library needed. The LemonSqueezy response is shallow JSON (one level of nesting for the main fields, two levels for `license_key` and `instance` objects).

**Local persistence:** Store `license_key` and `instance_id` in `juce::PropertiesFile` (stored in the OS app data directory). On macOS: `~/Library/Application Support/DimeaArcade/ChordControl/`. On Windows: `%APPDATA%\DimeaArcade\ChordControl\`. `juce::PropertiesFile` handles the platform-specific path automatically.

**Offline fallback:** Cache the last-known-good validation result with a timestamp. Allow a grace period (e.g., 7 days) before requiring re-validation. Store `last_validated_at` in the PropertiesFile.

**Thread safety:** Make the HTTP call from a background thread (`juce::Thread` subclass or `juce::ThreadPool`). Never call `juce::URL::createInputStream()` from the audio thread or the message thread directly (it blocks). Show a non-blocking "Validating license..." UI while the thread runs.

### What NOT to Use

| Avoid | Why | Use Instead |
|-------|-----|-------------|
| libcurl via FetchContent | Adds ~500KB binary weight, conflicts with `JUCE_USE_CURL=0`, requires OpenSSL or system TLS linkage on macOS | `juce::URL` with native transport |
| Boost.Asio | Heavyweight dependency for a single POST call | `juce::URL` |
| JUCE's `OnlineUnlockStatus` | Tied to JUCE's proprietary key format and server; not compatible with LemonSqueezy's REST API | Direct `juce::URL` POST |

---

## Area 4: macOS Installer

### Recommendation: pkgbuild + productbuild (.pkg)

Use Apple's first-party `pkgbuild` and `productbuild` command-line tools. Do NOT use create-dmg for plugin distribution.

Rationale: DMGs cannot create directories that don't exist on the target machine. Plugin installers must write to `/Library/Audio/Plug-Ins/VST3/` and `/Library/Audio/Plug-Ins/Components/`, which may not exist. `pkgbuild` is a native macOS tool (included in Xcode Command Line Tools), requires no additional installs, and produces `.pkg` files that can be notarized with `xcrun notarytool`.

### Toolchain

| Tool | Source | Purpose |
|------|--------|---------|
| `pkgbuild` | macOS built-in (Xcode CLT) | Create a `.pkg` component package for each plugin format |
| `productbuild` | macOS built-in (Xcode CLT) | Assemble component packages into a signed distribution installer |
| `codesign` | macOS built-in | Sign each plugin bundle before packaging |
| `xcrun notarytool` | macOS built-in (Xcode 13+) | Submit `.pkg` to Apple's notarization service |
| `xcrun stapler` | macOS built-in | Staple the notarization ticket to the final `.pkg` |

No third-party tools are required. This is the current community-standard workflow for JUCE plugin installers (confirmed by [moonbase.sh pkgbuild guide](https://moonbase.sh/articles/how-to-make-macos-installers-for-juce-projects-with-pkgbuild-and-productbuild/) and [JUCE forum pkgbuild tutorial](https://forum.juce.com/t/pkgbuild-and-productbuild-a-tutorial-pamplejuce-example/64977)).

### Workflow Summary

```bash
# 1. Sign each plugin bundle (requires Developer ID Application cert)
codesign --force --timestamp --options runtime \
  -s "Developer ID Application: Dimea Arcade (TEAMID)" \
  "build-mac/.../Arcade Chord Control (BETA-Test).vst3"

codesign --force --timestamp --options runtime \
  -s "Developer ID Application: Dimea Arcade (TEAMID)" \
  "build-mac/.../Arcade Chord Control (BETA-Test).component"

# 2. Create component packages
pkgbuild --component "Arcade Chord Control (BETA-Test).vst3" \
  --install-location "/Library/Audio/Plug-Ins/VST3" \
  --identifier "com.dimeaarcade.chordcontrol.vst3" \
  --version "2.0.0" \
  ChordControl-VST3.pkg

pkgbuild --component "Arcade Chord Control (BETA-Test).component" \
  --install-location "/Library/Audio/Plug-Ins/Components" \
  --identifier "com.dimeaarcade.chordcontrol.au" \
  --version "2.0.0" \
  ChordControl-AU.pkg

# 3. Assemble distribution installer (requires distribution.xml)
productbuild --distribution distribution.xml \
  --package-path . \
  --sign "Developer ID Installer: Dimea Arcade (TEAMID)" \
  DimeaArcade-ChordControl-v2.0.0-macOS.pkg

# 4. Notarize
xcrun notarytool submit DimeaArcade-ChordControl-v2.0.0-macOS.pkg \
  --keychain-profile "notarytool-profile" --wait

# 5. Staple
xcrun stapler staple DimeaArcade-ChordControl-v2.0.0-macOS.pkg
```

### distribution.xml Structure

A minimal `distribution.xml` must specify:
- `<title>` — installer window title
- `<pkg-ref>` entries for each component package
- `<choice>` elements to make them selectable (or mandatory)
- `<domains enable_localSystem="true"/>` — required for `/Library/` installation

### Beta Distribution Without Notarization

For beta testers before signing infrastructure is set up, distribute with ad-hoc signing:
```bash
codesign --force --deep -s - "PluginName.vst3"
```
Recipients must right-click and "Open" to bypass Gatekeeper, or run:
```bash
xattr -dr com.apple.quarantine "PluginName.vst3"
```
This is not suitable for public release.

---

## Area 5: Code Signing

### macOS Code Signing

**Certificate requirements:**

| Certificate | Purpose | Source |
|-------------|---------|--------|
| Developer ID Application | Sign plugin bundles (.vst3, .component) | Apple Developer Program ($99/year) |
| Developer ID Installer | Sign .pkg installer built with productbuild | Apple Developer Program (same membership) |

Both certificates are obtained from the Apple Developer Program. One $99/year membership covers both. Enroll at https://developer.apple.com/programs/.

**Required: Hardened Runtime** — already set in CMakeLists.txt via `HARDENED_RUNTIME_ENABLED TRUE`. The `--options runtime` flag in `codesign` activates it at signing time.

**Notarization tool:** `xcrun notarytool` (Xcode 13+). `altool` was deprecated by Apple; as of November 2023 Apple's notary service no longer accepts `altool` uploads. Use only `notarytool`.

**Credential storage for CI:**
```bash
# Store once on the CI machine or in Keychain
xcrun notarytool store-credentials "notarytool-profile" \
  --key ~/private_keys/AuthKey_XXXXXXXXXX.p8 \
  --key-id XXXXXXXXXX \
  --issuer xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
```

**Verification after notarization:**
```bash
spctl --assess -vvv --type install DimeaArcade-ChordControl-v2.0.0-macOS.pkg
# Expected: "accepted" source=Notarized Developer ID
```

### Windows Code Signing

**2025 Status:** As of June 2023, both EV and OV code signing certificates require hardware token or cloud signing (two-factor requirement). As of March 2024, EV certificates no longer provide instant SmartScreen reputation — both OV and EV now build reputation organically through download volume.

**Recommended: Azure Trusted Signing** (formerly Azure Code Signing)

| Approach | Cost | Hardware Token | SmartScreen Reputation | CI/CD |
|----------|------|---------------|----------------------|-------|
| Azure Trusted Signing | ~$9.99/month | No (cloud) | Builds organically | Native GitHub Actions integration |
| Traditional EV Cert (DigiCert, Sectigo) | $300-600/year | Yes (USB token) | Builds organically (same as ATS since March 2024) | Requires token or cloud signing workaround |
| OV Cert | $100-300/year | Yes (USB token) | Builds organically | Same issues as EV |

Azure Trusted Signing is preferred for v2.0 because:
1. No hardware USB token — signing happens in the cloud via `signtool.exe` with the `Azure.CodeSigning.Dlib` plugin
2. GitHub Actions integration via the `azure/trusted-signing-action` action
3. $9.99/month vs $300-600/year for EV
4. Automatic certificate renewal (no annual cert expiry)

**Limitation (as of 2025):** Azure Trusted Signing requires a US or Canadian organization with 3+ years of verifiable business history, OR individual developers in the US/Canada. EU/rest-of-world availability is in progress. If not eligible, fall back to an OV cert from DigiCert, Sectigo, or SSL.com.

**Azure Trusted Signing signtool invocation:**
```bash
signtool sign /v /fd SHA256 /tr http://timestamp.acs.microsoft.com /td SHA256 \
  /dlib "Azure.CodeSigning.Dlib.dll" \
  /dmdf "metadata.json" \
  "DimeaArcade-ChordControl-v2.0.0-Setup.exe"
```

**Traditional EV/OV signtool invocation (fallback):**
```bash
signtool sign /v /fd SHA256 /tr http://timestamp.digicert.com /td SHA256 \
  /sha1 "CERTIFICATE_THUMBPRINT" \
  "DimeaArcade-ChordControl-v2.0.0-Setup.exe"
```

**What to sign on Windows:** The Inno Setup output `.exe`. The JUCE-built `.vst3` bundle is already inside the installer payload; signing the installer is sufficient for SmartScreen. Optionally sign the `.vst3` DLL directly if distributing it outside the installer.

---

## Consolidated: New Stack Additions

| Technology | Version | Purpose | Existing? |
|------------|---------|---------|-----------|
| `CMAKE_OSX_ARCHITECTURES` | CMake built-in | Universal binary fat build | No — must add |
| `CMAKE_OSX_DEPLOYMENT_TARGET` | CMake built-in | macOS 11.0 minimum | No — must add |
| Xcode | 15.x recommended (see AU bug note) | macOS build generator | No — need Mac build machine |
| `juce::URL` + `juce_network` | JUCE 8.0.4 (already available) | LemonSqueezy HTTP POST | Already in JUCE; `JUCE_USE_CURL=0` already set |
| `juce::JSON` | JUCE 8.0.4 (already available) | Parse LemonSqueezy response | Already in `juce_core` |
| `juce::PropertiesFile` | JUCE 8.0.4 (already available) | Persist license key + instance_id | Already in `juce_core` |
| `pkgbuild` | macOS built-in | Build AU+VST3 component packages | macOS built-in, no install |
| `productbuild` | macOS built-in | Assemble signed distribution installer | macOS built-in, no install |
| `codesign` | macOS built-in | Sign plugin bundles | macOS built-in, no install |
| `xcrun notarytool` | Xcode 13+ (macOS built-in) | Submit .pkg to Apple notarization | macOS built-in (Xcode CLT) |
| Apple Developer Program | $99/year | Developer ID Application + Installer certs | No — must enroll |
| Azure Trusted Signing | $9.99/month | Windows EXE/DLL signing (no hardware token) | No — must set up |

**No new FetchContent dependencies.** Zero new libraries added to CMakeLists.txt for the license key feature. All HTTP, JSON, and persistence needs are covered by existing JUCE modules.

---

## CMake Changes Summary

The full set of CMakeLists.txt changes for macOS support:

```cmake
# Add near top, before project()
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)

# Remove the JUCE_USE_CURL=0 from Catch2 test target definition,
# or keep it — it doesn't affect the plugin target (already JUCE_USE_CURL=0 there)

# Add to plugin target_compile_definitions (macOS network entitlement)
# JUCE_USE_CURL=0 is already set — no change needed
# JUCE_WEB_BROWSER=0 is already set — no change needed

# The APPLE elseif block in CMakeLists.txt already handles post-build copies.
# The AU format is already in FORMATS.
# HARDENED_RUNTIME_ENABLED is already TRUE.
# No structural CMake changes are needed for the AU format itself.
```

The existing `elseif(APPLE)` block already handles post-build copies for VST3 and AU. The existing `FORMATS VST3 AU` already declares both targets. The only true additions are the two `CMAKE_OSX_*` cache variables.

---

## Version Compatibility

| Component | Version | Compatibility Note |
|-----------|---------|-------------------|
| JUCE | 8.0.4 | AU v2 generation confirmed working; AU universal binary issue with Xcode 16.2 is post-8.0.4 (8.0.6 report) |
| SDL2 | 2.30.9 | Respects `CMAKE_OSX_ARCHITECTURES`; macOS gamepad support via IOKit (no change needed) |
| Xcode | 15.x preferred | 16.2 has known AU universal binary bug; use 15.x until patched |
| macOS deployment target | 11.0 | Minimum for arm64 native (Apple Silicon); JUCE 8 supports 10.13+ |
| Catch2 | 3.8.1 | No macOS-specific changes; tests run on both arches |
| notarytool | Xcode 13+ | `altool` is deprecated since Nov 2023; must use `notarytool` |

---

## Alternatives Considered

| Recommended | Alternative | Why Not |
|-------------|-------------|---------|
| `juce::URL` for LemonSqueezy HTTP | libcurl via FetchContent | Conflicts with `JUCE_USE_CURL=0`; adds binary weight; native transports are sufficient for a single POST |
| `juce::URL` for LemonSqueezy HTTP | JUCE `OnlineUnlockStatus` | Designed for JUCE's proprietary XML key format; incompatible with LemonSqueezy JSON REST API |
| pkgbuild + productbuild | create-dmg | DMGs cannot create install directories; plugins must install to `/Library/Audio/Plug-Ins/` which may not exist |
| pkgbuild + productbuild | Packages.app (GUI) | GUI tool is not CI-scriptable; official CLI tools are sufficient |
| Azure Trusted Signing | Traditional EV cert (DigiCert) | EV no longer provides instant SmartScreen reputation since March 2024; ATS is cheaper, no hardware token, CI-native |
| Azure Trusted Signing | OV cert | Same SmartScreen reputation curve; more expensive and requires hardware token |
| Xcode generator | Ninja generator on macOS | JUCE AU wrapper requires Xcode build system for proper bundle generation and Info.plist embedding |

---

## Sources

- [JUCE CMake API docs](https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md) — `juce_add_plugin` FORMATS, AU_MAIN_TYPE, HARDENED_RUNTIME_ENABLED (HIGH)
- [JUCE forum: AU universal binary macOS Sequoia and Xcode 16.2](https://forum.juce.com/t/au-universal-binary-macos-sequoia-and-xcode-16-2/65337) — AU single-arch bug on Xcode 16.2 (MEDIUM — community report)
- [LemonSqueezy License API: Validate](https://docs.lemonsqueezy.com/api/license-api/validate-license-key) — validate endpoint, request format, response JSON (HIGH — official docs)
- [LemonSqueezy License API: Activate](https://docs.lemonsqueezy.com/api/license-api/activate-license-key) — activate endpoint, instance_id in response (HIGH — official docs)
- [JUCE forum: Using juce::URL to send POST](https://forum.juce.com/t/solved-using-juce-url-to-send-post-with-body/59701) — confirmed POST with form-data body (MEDIUM)
- [JUCE forum: JUCE_USE_CURL=0 alternatives](https://forum.juce.com/t/juce-use-curl-0-what-does-juce-use-instead/60937) — WinHTTP on Windows, NSURLSession on macOS (MEDIUM)
- [moonbase.sh: macOS installers with pkgbuild and productbuild](https://moonbase.sh/articles/how-to-make-macos-installers-for-juce-projects-with-pkgbuild-and-productbuild/) — pkgbuild/productbuild workflow for JUCE plugins (MEDIUM — community, verified against Apple CLI docs)
- [JUCE forum: pkgbuild and productbuild tutorial + pamplejuce](https://forum.juce.com/t/pkgbuild-and-productbuild-a-tutorial-pamplejuce-example/64977) — community-standard macOS installer workflow (MEDIUM)
- [moonbase.sh: Code signing audio plugins in 2025](https://moonbase.sh/articles/code-signing-audio-plugins-in-2025-a-round-up/) — current state of both Windows and macOS signing (MEDIUM — community, 2025)
- [Apple Developer: Signing Mac Software with Developer ID](https://developer.apple.com/developer-id/) — Developer ID Application and Installer certificate types (HIGH — official)
- [Apple: Customizing the notarization workflow](https://developer.apple.com/documentation/security/customizing-the-notarization-workflow) — notarytool workflow (HIGH — official)
- [Melatonin: macOS plugin CI signing and notarization](https://melatonin.dev/blog/how-to-code-sign-and-notarize-macos-audio-plugins-in-ci/) — CI-oriented notarization guide (MEDIUM)
- [Melatonin: Azure Trusted Signing on GitHub Actions](https://melatonin.dev/blog/code-signing-on-windows-with-azure-trusted-signing/) — ATS setup for JUCE plugins (MEDIUM)
- [pamplejuce template](https://github.com/sudara/pamplejuce) — JUCE 8 + macOS notarization + Azure Trusted Signing reference implementation (MEDIUM)
- [SDL2 README-cmake.md](https://github.com/libsdl-org/SDL/blob/main/docs/README-cmake.md) — `CMAKE_OSX_ARCHITECTURES` propagation to SDL2 builds (HIGH — official)
- Source code verified: `CMakeLists.txt` (FORMATS VST3 AU, HARDENED_RUNTIME_ENABLED, elseif APPLE block, JUCE_USE_CURL=0 already set)

---

*Stack research for: ChordJoystick v2.0 — Cross-Platform Launch*
*Researched: 2026-03-10*
