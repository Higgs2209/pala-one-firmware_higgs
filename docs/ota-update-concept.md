# OTA Firmware Update — Concept

## Goal

Let a Pala One device update its own firmware over Wi-Fi, pulling the correct binary from the same GitHub Pages channels (`stable` / `dev`) that the web installer already publishes to. No toolchain, no USB cable, no computer required after the initial flash.

## Non-goals

- Automatic background updates (user must trigger the check explicitly).
- Differential / delta updates (full binary only).
- Rollback UI exposed to the user (automatic silent rollback on boot failure only).
- Updating the bootloader or partition table over-the-air (not supported by the IDF OTA mechanism).
- Changing the firmware language via OTA — the language is baked in at compile time and the update always fetches the binary for the same language that is already installed.

---

## Background: how firmware is already distributed

The CI workflow publishes four firmware binaries to GitHub Pages after every qualifying build:

```
https://paullagier.github.io/pala-one-firmware/{channel}/firmware-{board}-{lang}.bin
```

| Placeholder | Values                   |
|-------------|--------------------------|
| `{channel}` | `stable`, `dev`          |
| `{board}`   | `v1_1`, `v1_2`           |
| `{lang}`    | `en`, `es`               |

The same directory also contains a manifest JSON per variant (e.g. `manifest-v1_2-en.json`) whose `version` field carries the channel label (`vX.Y.Z` for stable, `dev-<sha>` for dev). This manifest is already parseable JSON and is small enough to fetch cheaply before committing to a full download.

---

## Partition table changes

This is the single most disruptive change. The current `partitions.csv` uses a plain `factory` app partition with no OTA support:

```
# current (no OTA)
nvs,    data, nvs,     0x9000,  0x5000
app0,   app,  factory, 0x10000, 0x280000   ← 2.5 MB, single bank
spiffs, data, spiffs,  0x290000,0x570000   ← 5.6 MB
```

ESP-IDF's OTA mechanism requires:
- An **`otadata`** partition (8 KB) that records which app bank is currently active.
- Two equal-size **`ota_0` / `ota_1`** app partitions so the running firmware can write the incoming image to the idle bank and then hand boot control to it.

The Heltec Wireless Paper (ESP32-S3FN8) has 8 MB flash. The current layout fills it exactly, so introducing two OTA banks forces a reallocation. A workable new layout:

```
# proposed (OTA-capable)
nvs,     data, nvs,   0x9000,   0x5000    ←  20 KB  (unchanged)
otadata, data, ota,   0xE000,   0x2000    ←   8 KB  (new)
ota_0,   app,  ota_0, 0x10000,  0x1C0000  ← 1.75 MB (was 2.5 MB)
ota_1,   app,  ota_1, 0x1D0000, 0x1C0000  ← 1.75 MB (new second bank)
spiffs,  data, spiffs,0x390000, 0x470000  ← 4.44 MB (was 5.6 MB)
```

**Consequences:**

- **App bank shrinks from 2.5 MB to 1.75 MB.** Confirmed workable — `firmware.bin` is 1.3 MB, leaving ~450 KB of headroom.
- **spiffs shrinks by ~1.1 MB.** Devices that already have books loaded will keep them — the spiffs content is separate from the app partitions and a partition-table reflash does not erase LittleFS data unless the spiffs region's start offset or size changes enough to overlap used blocks. However, the reduced ceiling means slightly fewer books can be stored in total.
- **One-time migration flash required.** The partition table itself cannot be updated OTA. Every device needs one USB reflash (via the web installer) to land on the new partition table before OTA becomes available. After that, all future updates can be wireless.

---

## Runtime variant detection

The firmware knows its board revision and language at compile time via preprocessor defines (`DISPLAY_V1_1` / `DISPLAY_V1_2`, `LANG_EN` / `LANG_ES_LA`). To construct the correct download URL at runtime, these must be translated into the filename tokens `v1_1` / `v1_2` and `en` / `es`.

Both mappings are static and determined at build time, so they can be resolved with simple `#if` guards in a small helper that returns the URL components:

```
board_token : "v1_1" | "v1_2"
lang_token  : "en"   | "es"   ← read-only; OTA always fetches the same language
```

No runtime detection is needed — the defines already encode the answer, and neither value is user-configurable. OTA updates in-place: an English device stays English, a Spanish device stays Spanish.

---

## Version check before download

Downloading a full 1.5–1.75 MB binary just to find out the device is already up-to-date would be wasteful on a slow connection. The manifest JSON is the right checkpoint:

1. Fetch `{channel_url}/manifest-{board}-{lang}.json` (a few hundred bytes).
2. Parse the `version` field.
3. Compare against the running `FW_VERSION` (already available from `src/config.h`).
4. If they match, show "Already up to date" and skip the download.
5. If they differ, show the available version and ask the user to confirm before downloading.

The manifest is already in the right shape for this — no server-side changes needed.

---

## Channel selection

The device needs to remember whether the user wants `stable` or `dev` updates. This is a single string value that fits naturally in NVS alongside existing settings (the web settings UI already reads/writes NVS via the `KeyValueStore`).

Default: `stable`. The user selects the channel directly on the Update Screen (see below). The channel preference should be visible in the web UI as well.

---

## Update Screen

The Update Screen is a dedicated screen reached from **Settings**. It is the single place where the user selects a channel and triggers a check or install.

```
┌──────────────────────────────┐
│  Firmware Update             │
│                              │
│  Current version: v1.2.3     │
│                              │
│  Channel:  < Stable | Dev >  │
│                              │
│       [Check for update]     │
└──────────────────────────────┘
```

- The **channel selector** (`< Stable | Dev >`) is a left/right toggle navigated with the normal input buttons. Changing the selection immediately persists the new value to NVS so the preference survives a reboot.
- **Check for update** is the only action button. It is greyed out (non-interactive) when the device is not in station mode.
- Once a check completes and an update is available, the screen updates in place to show the remote version and replaces the button label with **Install update**:

```
┌──────────────────────────────┐
│  Firmware Update             │
│                              │
│  Current:   v1.2.3           │
│  Available: v1.3.0  (Stable) │
│                              │
│  ████████░░░░░░░░  48 %      │  ← during download
│                              │
│       [Install update]       │
└──────────────────────────────┘
```

- While downloading, the button is replaced by a progress bar showing percentage.
- On success the screen shows a **"Reboot to apply"** prompt. On error it shows a short message and re-enables the button.
- A `dev` channel warning is shown inline when the user switches to Dev: *"Dev builds may be unstable."*

---

## Internet connectivity check

Being connected to a Wi-Fi network (station mode) is a necessary but not sufficient condition — the router may have no internet access, or DNS may be unavailable. Before fetching the manifest the firmware must verify that GitHub Pages is reachable, since that is the only host that matters for OTA.

The check is a lightweight HTTPS HEAD request to the firmware distribution root:

```
HEAD https://paullagier.github.io/pala-one-firmware/
```

A `200` (or any non-error) response confirms the host is reachable. Any other outcome (DNS failure, TCP timeout, TLS error, 5xx) is treated as unreachable and surfaces as *"Cannot reach update server"* on screen, without attempting the manifest fetch.

```cpp
// GitHub Pages reachability probe (demo)
bool isUpdateServerReachable() {
    WiFiClientSecure client;
    client.setCACertBundle(arduino_esp32_bundle);  // IDF built-in Mozilla root store
    HTTPClient http;
    http.begin(client, "https://paullagier.github.io/pala-one-firmware/");
    http.setTimeout(5000);
    int code = http.sendRequest("HEAD");
    http.end();
    return code > 0 && code < 500;
}
```

Using the actual distribution host means the probe also validates DNS resolution and TLS for the domain the OTA will use — a general internet check would pass even if GitHub Pages were down or blocked.

---

## User-facing update flow

```
Settings → Firmware Update screen
    │
    ├─ [WiFi not in station mode] → button greyed out, hint shown
    │
    └─ User presses "Check for update"
           │
           ├─ [Connectivity probe fails] → "Cannot reach update server"
           │
           ├─ [Fetching manifest…] → progress indicator
           │
           ├─ [Already up to date] → "v1.2.3 is current"
           │
           └─ [Update available: v1.3.0]
                  User presses "Install update"
                       │
                       ├─ [Content-Length > partition size] → "Update file too large"
                       │   (no data written, device untouched)
                       │
                       ├─ Progress bar (percentage)
                       │
                       ├─ [Download or write error] → "Update failed, try again"
                       │   (device stays on current firmware; ota_1 bank left invalid)
                       │
                       └─ [Success] → "Reboot to apply?" → confirm → esp_restart()
```

The download and write happen entirely within the device's Wi-Fi station session. The device must be in station mode (connected to the user's home network), not SoftAP-only mode, because it needs outbound internet access to reach GitHub Pages.

---

## Libraries

No new library is needed for the OTA write itself — `esp_ota_ops.h` ships with IDF 5.x, which is already bundled in the arduino-esp32 platform in use. The manifest fetch needs an HTTPS client and a JSON parser.

| Library | Source | Purpose |
|---------|--------|---------|
| `esp_ota_ops.h` | IDF built-in (already present) | Partition write, image validation, boot-partition swap |
| `HTTPClient` + `WiFiClientSecure` | arduino-esp32 built-in | HTTPS fetch for manifest JSON and binary stream |
| `ArduinoJson` | New `lib_dep` | Parse the `version` field from the manifest JSON |

`ArduinoJson` is the only addition to `platformio.ini`.

### Fetching the manifest (demo)

```cpp
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

String fetchRemoteVersion(const char* manifestUrl) {
    WiFiClientSecure client;
    client.setCACertBundle(arduino_esp32_bundle);  // IDF built-in Mozilla root store
    HTTPClient http;
    http.begin(client, manifestUrl);
    if (http.GET() != HTTP_CODE_OK) {
        http.end();
        return "";
    }
    JsonDocument doc;
    deserializeJson(doc, http.getString());
    http.end();
    return doc["version"].as<String>();
}
```

### Writing the firmware binary (demo)

```cpp
#include <esp_ota_ops.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

bool downloadAndApply(const char* binUrl) {
    const esp_partition_t* target = esp_ota_get_next_update_partition(nullptr);
    esp_ota_handle_t handle;
    if (esp_ota_begin(target, OTA_WITH_SEQUENTIAL_WRITES, &handle) != ESP_OK)
        return false;

    WiFiClientSecure client;
    client.setCACertBundle(arduino_esp32_bundle);  // IDF built-in Mozilla root store
    HTTPClient http;
    http.begin(client, binUrl);
    if (http.GET() != HTTP_CODE_OK) { http.end(); return false; }

    WiFiClient* stream = http.getStreamPtr();
    uint8_t buf[1024];
    int len;
    while ((len = stream->readBytes(buf, sizeof(buf))) > 0)
        esp_ota_write(handle, buf, len);

    http.end();
    if (esp_ota_end(handle) != ESP_OK) return false;
    esp_ota_set_boot_partition(target);
    return true;
}
```

These snippets show the essential calls. The real `ota.cpp` will add error handling, a progress callback, and the `Content-Length` size check before writing.

---

## Implementation outline

The following new pieces are needed. None of these exist yet — this section describes the work, not current code.

### `src/hal/ota.h / ota.cpp`

Thin wrapper around `esp_ota_ops.h`:

- `OTA::isUpdateServerReachable()` — sends an HTTPS HEAD to `paullagier.github.io/pala-one-firmware/` with a short timeout; returns `true` on any non-5xx response. Called before any manifest or binary fetch.
- `OTA::checkAvailable(channel, board, lang)` — fetches the manifest, parses version, returns a struct with `{available: bool, remoteVersion: String}`.
- `OTA::download(url, progressCallback)` — reads the `Content-Length` header before writing any data; aborts immediately if the reported size exceeds the target partition's capacity. If the size check passes, streams the binary into the idle OTA partition via `esp_ota_write`, validates the image, calls `esp_ota_set_boot_partition`, returns success/failure.

HTTPS is required (GitHub Pages enforces it). All requests use `HTTPClient` + `WiFiClientSecure` with `setCACertBundle(arduino_esp32_bundle)` — the IDF Mozilla root store compiled into the arduino-esp32 platform.

### Settings key

`cfg_ota_channel` → `"stable"` or `"dev"`, stored in NVS. Default `"stable"` if absent.

### UI integration

A new **Firmware Update** entry in the Settings list opens the dedicated Update Screen described above. The screen owns both the channel selector and the check/install action. The connectivity probe runs when the user presses "Check for update", not on screen entry.

### Web UI integration (optional / later)

The captive portal Settings page could expose the same two controls so a user can trigger an update from a browser without navigating the e-ink menus.

---

## Rollback

ESP-IDF's OTA machinery provides automatic silent rollback:

- If a newly applied firmware fails to call `esp_ota_mark_app_valid_cancel_rollback()` before the watchdog fires (or before a deliberate reboot), the bootloader marks the new partition as invalid and boots from the previous one.
- The firmware should call this mark function early in `setup()`, after verifying enough of the system is functional.
- No user-visible rollback UI is needed beyond a toast on the first boot after rollback: "Update failed — reverted to previous firmware."

Detecting "we just rolled back" can be done by comparing the active OTA partition against `esp_ota_get_running_partition()` and checking the `esp_ota_img_states_t` of the previously pending partition.

---

## Security considerations

- **Transport**: GitHub Pages is HTTPS. Fetching over plain HTTP must be refused.
- **Image integrity**: `esp_ota_write` + `esp_ota_end` use the ESP-IDF secure boot verification path if secure boot is enabled. Without secure boot, anyone who can perform a MITM on the TLS connection could push arbitrary firmware. The IDF Mozilla root bundle (`esp_crt_bundle_attach`) pins the trust anchor without requiring device-side certificate management.
- **Source pinning**: The download URL is constructed from the firmware's own baked-in base URL (`paullagier.github.io/pala-one-firmware`). It cannot be overridden at runtime, which limits the attack surface to the GitHub Pages hosting itself.

---

## Open questions

1. ~~**App binary headroom**: What is the current maximum release binary size? If it is already above ~1.6 MB the proposed 1.75 MB per bank is too tight and the spiffs partition needs to shrink further (or a different flash allocation must be found).~~ **Resolved** — current `firmware.bin` is 1.3 MB, leaving ~450 KB of headroom in the proposed 1.75 MB bank. Partition sizing is confirmed workable.
2. ~~**HTTPS root CA**: Use `esp_crt_bundle_attach` (IDF's bundled Mozilla root store) or pin a specific CA? The bundled store is simpler; pinning is more robust against CA compromise.~~ **Resolved** — use `esp_crt_bundle_attach`. It is built into IDF 5.x / arduino-esp32 3.x, requires one line to enable, and needs no maintenance. Pinning a specific CA would break silently if GitHub rotates their certificate chain, requiring a new OTA to recover — an unacceptable circular dependency. The ~70 KB bundle overhead is acceptable given the confirmed 450 KB headroom.
3. ~~**spiffs impact**: Should the concept acknowledge a migration guide / script that pre-backs up books to the web UI before a user does the one-time repartition flash?~~ **Deferred** — not needed for the initial implementation.
4. ~~**Channel label in UI**: Should the channel selection live only on-device or also in the web UI settings page?~~ **Resolved** — channel selector lives on the Update Screen (on-device). Web UI exposure is listed as optional/later.
5. ~~**dev channel safety warning**: The dev channel may contain unstable builds. Should the UI show a warning when switching to dev?~~ **Resolved** — warning *"Dev builds may be unstable"* is shown inline on the Update Screen when the user switches to Dev.

---

## Implementation plan

Each step below builds on the previous one. Steps 1–2 must be completed before any code is written, because they constrain the partition layout and the TLS approach that everything else depends on.

### Step 1 — Resolve prerequisites

- ~~Build a release binary for each board variant and record the `.bin` size.~~ **Done** — `firmware.bin` is 1.3 MB, ~450 KB under the 1.75 MB bank limit. Partition sizing stands.
- ~~Decide on the CA certificate approach.~~ **Done** — `esp_crt_bundle_attach` / `setCACertBundle(arduino_esp32_bundle)` selected. No cert files to manage.

### Step 2 — Update the partition table

- Edit `Pala_One_2_1/partitions.csv` to the proposed OTA-capable layout (`otadata` + `ota_0` + `ota_1`).
- Verify the new layout compiles cleanly and that the binary still fits within `ota_0`.
- This is a **breaking change** for existing devices — it requires a one-time USB reflash via the web installer. Prepare a short migration note for users (relates to open question 3).

### Step 3 — Add the ArduinoJson dependency

- Add `ArduinoJson` to `lib_deps` in `platformio.ini`.
- Confirm the library resolves and the project still builds on all four board/language combinations.

### Step 4 — Enable the CA bundle

- Call `setCACertBundle(arduino_esp32_bundle)` on every `WiFiClientSecure` instance used by the OTA module. No additional build config or certificate files are needed — the bundle is compiled into the arduino-esp32 platform already in use.
- Verify a test HTTPS connection to `paullagier.github.io` succeeds on-device before writing any OTA logic.

### Step 5 — Implement `src/hal/ota.h / ota.cpp`

Work through the three public functions in order, testing each before moving to the next:

1. `isUpdateServerReachable()` — HEAD probe to `paullagier.github.io/pala-one-firmware/`. Confirm it returns the right result when connected and when offline.
2. `checkAvailable(channel, board, lang)` — fetch and parse the manifest JSON, compare version strings. Confirm "already up to date" and "update available" paths both work.
3. `download(url, progressCallback)` — read `Content-Length` from the response headers and compare against the target partition size before writing a single byte; abort with a clear error if it does not fit. If the check passes, stream the binary into the idle OTA partition and set the boot partition. Test with a known-good binary, a deliberately corrupted one (verify `esp_ota_end` rejects it), and a response with a `Content-Length` that exceeds the partition (verify the download never starts).

### Step 6 — Add the NVS settings key

- Register `cfg_ota_channel` in the existing settings/NVS layer with a default of `"stable"`.
- Confirm the value persists across reboots and that an absent key falls back to `"stable"` without crashing.

### Step 7 — Add the rollback mark to `setup()`

- Call `esp_ota_mark_app_valid_cancel_rollback()` early in `setup()`, after the core systems (display, NVS, Wi-Fi driver) have initialised successfully.
- Add first-boot-after-rollback detection: check the OTA state of the previously pending partition and show a toast if it was rolled back.
- Test by flashing a firmware that deliberately does not call the mark function, confirming the bootloader reverts to the previous bank on the next boot.

### Step 8 — Implement the Update Screen

- Create `src/ui/screens/update_screen.h / update_screen.cpp` following the same pattern as existing screens.
- Implement the three visual states: idle (channel selector + "Check for update"), result (version diff + "Install update"), and progress (progress bar).
- Wire the channel selector to read/write `cfg_ota_channel` via the settings layer.
- Connect the button actions to the HAL functions from step 5.
- Add a **"Dev builds may be unstable"** warning when the user switches to the Dev channel (open question 5).

### Step 9 — Wire the screen into Settings

- Add a **Firmware Update** entry to the Settings list that opens the Update Screen.
- Confirm the entry is reachable, the screen opens correctly, and the back/exit navigation works.

### Step 10 — End-to-end testing

Go through the full flow on a real device for each case:

- [ ] Device offline → button greyed out.
- [ ] Device online, server unreachable (simulate with DNS block) → "Cannot reach update server".
- [ ] Already up to date → correct version shown, no install offered.
- [ ] Update available, stable channel → download, flash, reboot, new version running.
- [ ] Update available, dev channel → same flow, warning shown when switching channel.
- [ ] Download interrupted mid-way → device boots normally from the previous bank.
- [ ] Rollback scenario → new firmware does not mark itself valid → previous bank restored on next boot, toast shown.
