# Feature Research

**Domain:** Cross-platform paid JUCE VST3/AU plugin — license key system, Mac distribution, Windows code signing
**Researched:** 2026-03-10
**Milestone:** v2.0 Cross-Platform Launch
**Confidence:** MEDIUM-HIGH
- LemonSqueezy API contract: HIGH (official docs)
- Mac DAW format matrix: HIGH (Ableton/Steinberg/Apple official docs)
- macOS signing/notarization requirements: HIGH (Apple Developer docs + melatonin.dev JUCE-specific guide)
- Windows code signing landscape: MEDIUM — Microsoft changed SmartScreen/EV policy in March 2024; Azure Trusted Signing eligibility changed again in April 2025; verify current eligibility

> **Scope note:** This document covers ONLY what is new for v2.0 (license key system, Mac distribution, code signing).
> Existing plugin features (chord engine, LFO, looper, arpeggiator, gamepad, UI) are already built and are not re-researched here.

---

## Feature Landscape

### Table Stakes (Users Expect These)

Features that must exist. Missing any of these = product feels broken or unprofessional.

| Feature | Why Expected | Complexity | Notes |
|---------|--------------|------------|-------|
| License key entry dialog on first launch | Every paid plugin requires a key before use; no dialog = feels broken or abandoned | LOW | Modal overlay shown in `PluginEditor` constructor; check persisted key first, only show if absent or invalid |
| Persistent local license storage | Users expect to re-open DAW without re-entering key every session | LOW | `juce::PropertiesFile` at platform-appropriate path (`%APPDATA%` / `~/Library/Application Support`); NOT APVTS — APVTS is DAW-session state, not machine state |
| Graceful offline mode after initial activation | Studio machines are routinely offline during sessions; plugin must not block audio | MEDIUM | Cache `instance_id` + key locally; re-validate on a background thread at most weekly; never touch audio thread for network; see grace period below |
| Clear error messages for each failure case | Users paste wrong keys constantly; cryptic "activation failed" causes support tickets | LOW | Map each LemonSqueezy error string to a user-visible sentence (see LemonSqueezy section below) |
| Machine count display | Multi-machine users need to know how many activations are used before they're blocked | LOW | LemonSqueezy returns `activation_usage` and `activation_limit` in every activate/validate response; show "2 of 3 machines activated" |
| AU format on macOS | Logic Pro and GarageBand only load AU — shipping VST3-only excludes the two most popular Mac DAWs | HIGH | JUCE `FORMATS AU` in CMakeLists; only compiled on macOS; requires Apple Developer ID Application cert + notarization |
| VST3 on macOS | Ableton Live, Reaper, FL Studio, Cubase on Mac use VST3 | LOW (incremental) | Same JUCE build target produces VST3 alongside AU; no additional DSP work |
| Universal binary (arm64 + x86_64) | Apple Silicon Macs are now the majority; Rosetta works but triggers "not native" support complaints | MEDIUM | CMake: `-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64`; known Xcode 16.2 regression with AU universal binary — verify before shipping |
| PKG installer for macOS | DMG drag-to-folder fails on macOS Sonoma for system plugin paths; PKG handles permissions automatically | MEDIUM | `pkgbuild` + `productbuild`; wrap PKG in DMG as distribution container; sign PKG with Developer ID Installer cert (separate from Application cert) |
| Notarized macOS distribution | macOS Gatekeeper silently blocks unnotarized plugins — users see nothing load and file support tickets | HIGH | Requires Apple Developer Program ($99/year); `codesign --deep -o runtime` on .component + .vst3; submit via `notarytool`; staple ticket to PKG |
| Windows code signing (SmartScreen-free installer) | SmartScreen "Windows protected your PC" warning on unsigned EXE causes a large fraction of users to abandon install | MEDIUM | Azure Artifact Signing ($9.99/month, instant reputation) if eligible; OV cert as fallback — see code signing section |
| Offline grace period after activation | Users working in studios without internet access should not be locked out mid-session | LOW | Record `last_validated_timestamp` locally; allow use for 14 days after last successful validate; only show re-validation prompt after grace period expires |

### Differentiators (Competitive Advantage)

Features that raise quality perception above baseline indie plugin expectations.

| Feature | Value Proposition | Complexity | Notes |
|---------|-------------------|------------|-------|
| "Deactivate this machine" button in plugin settings | Users can transfer license to new computer without emailing support — rare for indie plugins at this price point | LOW | Button in plugin About/Settings panel; calls `POST /v1/licenses/deactivate` with cached `instance_id`; deletes local cache on success; shows license entry dialog on next launch |
| Named machine display ("Studio Laptop") | Users feel ownership of their activations; makes the machine list feel intentional not bureaucratic | LOW | Use `juce::SystemStats::getComputerName()` as default `instance_name` when calling activate; display it back in the UI |
| Remaining activations display after deactivation | Shows "1 of 3 machines now active" — confirms the deactivation worked without requiring user to check a web dashboard | LOW | LemonSqueezy returns updated `activation_usage` in deactivate response; display it in a result label |
| Architecture badge in About panel ("Native Apple Silicon") | Apple Silicon users notice and comment positively; signals quality and up-to-date build | LOW | Compile-time `#if JUCE_ARM` check; show "Native Apple Silicon" or "Intel x86_64" in About panel |

### Anti-Features (Commonly Requested, Often Problematic)

| Feature | Why Requested | Why Problematic | Alternative |
|---------|---------------|-----------------|-------------|
| iLok / hardware dongle requirement | "Industry standard" for flagship plugins | $50+ hardware barrier; alienates hobbyist / bedroom producer market; iLok account creation friction | LemonSqueezy machine-based activation (3 machines) |
| Subscription licensing | Predictable revenue for developer | Music software users strongly prefer perpetual licenses; subscription churn + refund rate is high for tools priced under $100 | Perpetual license with optional paid upgrade for v3 |
| Challenge-response offline activation | Requested by air-gapped studio users | LemonSqueezy does not provide this natively; requires manual support workflow; adds significant implementation complexity | 14-day offline grace period handles 99% of real-world offline studio scenarios |
| Feature-gating / crippled trial mode | Lets users preview without paying | Removes features users rely on for evaluation; creates resentment if they buy and discover what was hidden; MIDI plugin is focused enough that a time-limited full trial is less confusing | Time-limited full trial (e.g., 14-day, full features, then license dialog blocks further use) |
| Blocking audio thread during license validation | Simplest implementation | Causes audio dropouts, DAW freezes, pluginval failures; immediately destroys professional credibility | Validate on a background `juce::Thread`; cache result; audio thread reads cached bool only |
| Server-side feature flags via license API | Granular per-user feature control | LemonSqueezy License API is binary pass/fail only (activated/not); building feature gating on top adds fragility | Keep feature set fixed per version; use licensed/unlicensed binary state only |
| AAX / Pro Tools format | Broad DAW coverage | Requires Avid certification + iLok integration; significant effort for a format used by <10% of the hobbyist/bedroom producer target market | Defer to v3; AU + VST3 covers Logic, GarageBand, Ableton, Reaper, FL Studio, Cubase |

---

## LemonSqueezy License API — Specific Endpoints

This section documents the actual API contract. The requirements writer needs exact field names and error behaviors.

**Base URL:** `https://api.lemonsqueezy.com/v1/licenses/`
**Rate limit:** 60 requests per minute

### Endpoint 1: Activate

```
POST https://api.lemonsqueezy.com/v1/licenses/activate
Content-Type: application/x-www-form-urlencoded

license_key=<key>
instance_name=<label>   # use computer name, e.g. "MacBook Pro (Studio)"
```

**Success response — key fields:**
- `activated: true`
- `error: null`
- `license_key.status` — "active" | "inactive" | "expired" | "disabled"
- `license_key.activation_limit` — integer (null = unlimited)
- `license_key.activation_usage` — integer, count of currently activated instances
- `license_key.expires_at` — ISO timestamp or null
- `instance.id` — UUID — **persist this to disk; required for validate and deactivate**
- `instance.name` — echoes the `instance_name` you sent
- `meta.product_id`, `meta.customer_email` — useful for support lookup

**Failure response:**
- `activated: false`
- `error: "<message string>"` — common messages: "This license key does not exist.", "This license key has reached its activation limit.", "This license key has been disabled."

**User-visible error mapping (implement in plugin):**
| LemonSqueezy error | User-facing text |
|--------------------|-----------------|
| "does not exist" | "Key not found. Check for typos and try again." |
| "activation limit" | "This key has already been activated on 3 machines. Deactivate another machine first." |
| "disabled" | "This key has been disabled. Contact support." |
| Network unreachable | "Could not reach the activation server. Check your internet connection and try again." |

### Endpoint 2: Validate

```
POST https://api.lemonsqueezy.com/v1/licenses/validate
Content-Type: application/x-www-form-urlencoded

license_key=<key>
instance_id=<uuid>    # the instance.id saved from activation
```

**Success response:** same structure as activate; top-level `valid: true` instead of `activated`
**Use:** Periodic background re-validation (weekly); called on launch if `last_validated_timestamp` is older than 7 days

### Endpoint 3: Deactivate

```
POST https://api.lemonsqueezy.com/v1/licenses/deactivate
Content-Type: application/x-www-form-urlencoded

license_key=<key>
instance_id=<uuid>
```

**Success response:** `deactivated: true`; no `instance` object (it has been deleted server-side)
**After success:** Delete local `instance_id` and key from `PropertiesFile`; show license entry dialog on next plugin open

### What LemonSqueezy does NOT provide (handle in-app)

| Gap | In-app mitigation |
|-----|-------------------|
| No offline activation (challenge/response) | 14-day grace period using local `last_validated_timestamp` |
| No machine fingerprinting | Plugin tracks `instance_id` (UUID assigned by LemonSqueezy at activation); `instance_name` is freeform label only |
| No webhook for key revocation | Weekly validate call surfaces revocation; grace period cap is 14 days worst-case |
| No grace period enforcement | App must implement: compare `last_validated_timestamp` to current time before blocking |

---

## Mac DAW Compatibility Matrix

| DAW | macOS Formats Accepted | Notes |
|-----|------------------------|-------|
| Logic Pro | **AU only** | VST3 not supported; AU is mandatory for Logic users |
| GarageBand | **AU only** | Same plugin engine as Logic; free, large hobbyist user base |
| Ableton Live (Mac) | AU + VST3 (+ VST2) | Both work; VST3 preferred for new plugins |
| Reaper (Mac) | AU + VST3 (+ VST2) | Both work |
| FL Studio (Mac) | AU + VST3 | Both work |
| Cubase / Nuendo (Mac) | VST3 (+ VST2) | Steinberg host — VST3 is the native format |
| Pro Tools (Mac) | AAX only | Out of scope for v2; requires Avid certification + iLok |

**Conclusion:** AU + VST3 dual-format covers all major Mac DAWs except Pro Tools. Shipping only VST3 on Mac would lock out Logic Pro and GarageBand — a significant portion of the Mac musician market.

### macOS Plugin Install Paths

| Format | System path (PKG installs here) | User path (no admin needed) |
|--------|--------------------------------|-----------------------------|
| AU (.component) | `/Library/Audio/Plug-Ins/Components/` | `~/Library/Audio/Plug-Ins/Components/` |
| VST3 (.vst3) | `/Library/Audio/Plug-Ins/VST3/` | `~/Library/Audio/Plug-Ins/VST3/` |

PKG installer should target system paths (requires admin password prompt, which users expect for audio software). User-path install skips the prompt but may not be found by all DAWs without custom scan-path configuration.

---

## macOS Distribution: Signing and Notarization

### Requirements for unblocked distribution (macOS Gatekeeper)

1. **Apple Developer Program** — $99/year; provides Developer ID certificates
2. **Developer ID Application certificate** — signs .component and .vst3 bundles
3. **Developer ID Installer certificate** — signs .pkg installer (separate cert from Application)
4. **Hardened Runtime** — `codesign -o runtime` flag required on all binaries for notarization
5. **Notarization** — submit signed PKG to Apple via `notarytool submit`; attach staple ticket
6. **Stapling** — `stapler staple` attaches the notarization ticket to the PKG so Gatekeeper trusts it offline

### Distribution container recommendation

**PKG inside DMG** — industry standard for audio plugins:
- PKG handles automated install to `/Library/Audio/Plug-Ins/` (handles admin prompt, creates target directories)
- DMG is the download artifact (familiar to Mac users; easy to distribute via GitHub Releases)
- Notarize the DMG (outermost container); everything inside is implicitly trusted

**Why not DMG-only (drag-to-folder):** On macOS Sonoma, dragging a plugin to a system path from a DMG fails silently — the system does not let the user write to `/Library/Audio/Plug-Ins/VST3/` via drag. PKG handles this correctly.

### Ad-hoc signing (beta testing only)

Ad-hoc signed code (`codesign --sign -`) only works on the machine that built it. Copying to another machine causes Gatekeeper to kill it. For beta distribution, either use a full Developer ID signing workflow or document the `xattr -rd com.apple.quarantine` override command for testers.

---

## Windows Code Signing — OV vs EV vs Azure Artifact Signing

### Current landscape (2025)

| Option | SmartScreen behavior | Cost | Key storage | Recommendation |
|--------|---------------------|------|-------------|----------------|
| OV Certificate | Reputation builds organically over months/years of downloads — SmartScreen warning shows until threshold; Microsoft does not publish the threshold | ~$200–400/year | FIPS 140-2 Level 2 USB token (required since June 2023) | Viable long-term; unsuitable for initial release without warning documentation |
| EV Certificate | As of March 2024: no longer provides instant SmartScreen trust — reputation builds same way as OV; EV OIDs removed from Microsoft Trusted Root in August 2024 | ~$300–600/year | FIPS 140-2 Level 2 USB token (required) | No longer justifies premium over OV; skip EV |
| Azure Artifact Signing (Trusted Signing) | **Instant SmartScreen reputation** tied to verified publisher identity; certificate rotation does not reset reputation; FIPS 140-2 Level 3 cloud HSM, no physical token | $9.99/month (5,000 signatures) | Cloud-managed (no USB token) | **Recommended if eligible** — lowest cost, instant reputation, no hardware |

**Critical Azure Artifact Signing eligibility note (April 2025):** Currently restricted to US or Canadian businesses with 3+ years of history. Individual developers and non-US/non-Canadian entities are not eligible until General Availability. Verify current eligibility at [azure.microsoft.com/products/artifact-signing](https://azure.microsoft.com/en-us/products/artifact-signing) before planning this path.

**If Azure Artifact Signing is not available:** Use OV cert. EV provides no material SmartScreen advantage over OV post-March 2024. With OV, SmartScreen warnings will appear on early installs — document this for users in release notes: "Windows may show a SmartScreen warning for new software. Click 'More info' → 'Run anyway'." This is expected and common for indie plugins.

---

## Feature Dependencies

```
LemonSqueezy License Check (first launch)
    └──requires──> Network HTTP client (juce::URL over HTTPS; no third-party HTTP lib needed)
    └──requires──> Local key + instance_id persistence (juce::PropertiesFile)
    └──requires──> Background thread for validation (juce::Thread; never audio thread)
    └──enables──>  Deactivate This Machine (needs stored instance_id)
    └──enables──>  Machine count display (needs activation_usage from API response)
    └──enables──>  Offline grace period (needs last_validated_timestamp in PropertiesFile)

AU Format (macOS)
    └──requires──> Apple Developer Program ($99/year)
    └──requires──> Developer ID Application certificate
    └──requires──> macOS build machine with Xcode installed
    └──requires──> Notarization (Gatekeeper blocks without it)
    └──requires──> Verify: Xcode 16.2 has known AU universal binary regression — check JUCE forum before building

macOS PKG Installer
    └──requires──> Developer ID Installer certificate (separate from Application cert)
    └──requires──> pkgbuild + productbuild CLI tools (included with Xcode)
    └──requires──> Notarization of the PKG / DMG container

macOS Notarization
    └──requires──> Apple Developer Program
    └──requires──> All binaries signed with Hardened Runtime (-o runtime)
    └──requires──> notarytool + stapler (Xcode CLI tools)
    └──enables──>  Gatekeeper trust (plugin loads without xattr override)
    └──enables──>  macOS GitHub Release distribution

Universal Binary
    └──requires──> CMake -DCMAKE_OSX_ARCHITECTURES=arm64;x86_64
    └──requires──> SDL2 universal fat library (or SDL2 not linked on Mac — verify if gamepad needed on Mac)
    └──note──>     Xcode 16.2 regression: AU universal binary may not build; track JUCE forum issue

Windows Code Signing
    └──requires──> Azure Artifact Signing account (if eligible) OR OV cert USB token
    └──requires──> Sign both the .exe installer and the .vst3 bundle
    └──enables──>  SmartScreen-free install experience
```

---

## MVP Definition

### Launch With (v2.0)

Minimum required for a paid public cross-platform release.

- [ ] License key entry dialog — shown on first launch when no valid local key exists
- [ ] Local license persistence — `juce::PropertiesFile` stores key + `instance_id`; dialog skipped on valid re-open
- [ ] 14-day offline grace period — `last_validated_timestamp` stored locally; background re-validation on launch if stale
- [ ] Activation error messages — user-readable text for each LemonSqueezy failure code
- [ ] Machine count display — show `activation_usage / activation_limit` in the dialog
- [ ] AU format build — universal binary; required for Logic/GarageBand users
- [ ] VST3 macOS build — universal binary; required for Ableton/Reaper/FL Studio on Mac
- [ ] Notarized PKG installer for macOS — no Gatekeeper block; installs to system plugin paths
- [ ] Windows code signing — Azure Artifact Signing (preferred) or OV cert; no SmartScreen block on installer

### Add After Validation (v2.x)

Features to add once the v2.0 launch is proven stable.

- [ ] "Deactivate this machine" button — reduces support volume as user base grows
- [ ] Machine name display in settings panel — shows which `instance_name` is active on this machine
- [ ] Silent weekly background re-validation — surfaces key revocation within 7 days without user action

### Future Consideration (v3+)

- [ ] Offline activation / challenge-response — only warranted if enterprise/air-gapped segment is significant
- [ ] AAX / Pro Tools support — requires Avid certification + iLok; separate milestone
- [ ] Linux support — JUCE supports it; commercial plugin sales volume is low on Linux

---

## Feature Prioritization Matrix

| Feature | User Value | Implementation Cost | Priority |
|---------|------------|---------------------|----------|
| License key dialog (first launch) | HIGH | LOW | P1 |
| Local key persistence (no re-entry) | HIGH | LOW | P1 |
| 14-day offline grace period | HIGH | LOW | P1 |
| Activation error messages | HIGH | LOW | P1 |
| AU format (Logic/GarageBand access) | HIGH | HIGH | P1 |
| VST3 macOS + universal binary | HIGH | MEDIUM | P1 |
| Notarized PKG installer | HIGH | MEDIUM | P1 |
| Windows code signing | HIGH | LOW–MEDIUM | P1 |
| Machine count display | MEDIUM | LOW | P1 |
| Deactivate this machine button | MEDIUM | LOW | P2 |
| Machine name label in settings | LOW | LOW | P2 |
| Weekly background re-validation | MEDIUM | LOW | P2 |
| Apple Silicon architecture badge | LOW | LOW | P3 |

---

## Sources

- [LemonSqueezy License API Overview](https://docs.lemonsqueezy.com/api/license-api) — endpoints list
- [LemonSqueezy Activate Endpoint](https://docs.lemonsqueezy.com/api/license-api/activate-license-key) — request/response schema, instance.id
- [LemonSqueezy Validate Endpoint](https://docs.lemonsqueezy.com/api/license-api/validate-license-key) — `valid`, `activation_usage`, `activation_limit` fields
- [LemonSqueezy Deactivate Endpoint](https://docs.lemonsqueezy.com/api/license-api/deactivate-license-key) — `instance_id` required, instance deleted on success
- [LemonSqueezy License Key Guide](https://docs.lemonsqueezy.com/guides/tutorials/license-keys) — full activation/deactivation flow walkthrough
- [Ableton: Using AU and VST plug-ins on macOS](https://help.ableton.com/hc/en-us/articles/209068929-Using-AU-and-VST-plug-ins-on-macOS) — AU + VST3 format support confirmed
- [Steinberg: VST3 plug-in locations on macOS](https://helpcenter.steinberg.de/hc/en-us/articles/115000171310-VST-plug-in-locations-on-Mac-OS-X-and-macOS) — system install paths
- [JUCE Forum: CMake + macOS Universal Binary](https://forum.juce.com/t/cmake-plugin-and-os-11-universal-binary/41997) — `-DCMAKE_OSX_ARCHITECTURES` flag
- [JUCE Forum: AU universal binary Xcode 16.2 regression](https://forum.juce.com/t/au-universal-binary-macos-sequoia-and-xcode-16-2/65337) — active issue in 2025
- [Melatonin: Code sign and notarize macOS audio plugins in CI](https://melatonin.dev/blog/how-to-code-sign-and-notarize-macos-audio-plugins-in-ci/) — JUCE-specific practical guide
- [KVR Audio: Mac plugin distribution — DMG or PKG](https://www.kvraudio.com/forum/viewtopic.php?t=393088) — DMG limitations on Sonoma
- [JUCE Forum: Signing Windows installers with OV cert](https://forum.juce.com/t/signing-windows-installers-with-ov-certificate-how-long-to-get-smartscreen-reputation/55245) — community experience with reputation build time
- [Azure Artifact Signing](https://azure.microsoft.com/en-us/products/artifact-signing) — $9.99/month, instant SmartScreen reputation
- [Microsoft Q&A: EV vs OV SmartScreen post-March 2024](https://learn.microsoft.com/en-us/answers/questions/417016/reputation-with-ov-certificates-and-are-ev-certifi) — parity confirmed
- [JUCE Forum: Current options for code signing on Windows (2025)](https://forum.juce.com/t/current-options-for-code-signing-on-windows/65812) — community roundup
- [Melatonin: Code signing on Windows with Azure Trusted Signing](https://melatonin.dev/blog/code-signing-on-windows-with-azure-trusted-signing/) — step-by-step for JUCE developers

---

## Appendix: v1.5 Feature Research (Historical — 2026-02-28)

> Archived. All v1.5 features validated and shipped as of v1.9.

Key decisions preserved for reference:
- Single-channel routing: reference-count collision guard per pitch per channel
- Sub octave: fixed -12 semitones; per-voice APVTS bool; fires in same NoteCallback as parent
- LFO recording: 4096-entry beat-indexed float buffer; 1 loop cycle; Distort stays live
- Left joystick expansion: filterXMode/filterYMode APVTS enum extended; LFO targets + arp gate
- Gamepad Option Mode 1: face-button remapping; R3+pad = sub oct toggle; R3 standalone panic removed in mode 1 only

---
*Feature research for: ChordJoystick v2.0 Cross-Platform Launch — license key UX, Mac DAW distribution, Windows code signing*
*Researched: 2026-03-10*
