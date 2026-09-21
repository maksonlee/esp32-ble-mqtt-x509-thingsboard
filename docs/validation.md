# Reliability review validation — 2026-09-21

## Automated checks

Run after building in the activated ESP-IDF v6.1 environment:

```bash
/home/administrator/.local/bin/codex-build-limited -- \
  python -m unittest discover -s tests -v
```

21 test cases passed:

- Five build contracts: SDK/target/components, BLE/TLS options including date
  validation, contiguous full-4-MiB partition layout, firmware fit in both OTA
  slots, and sensor sampling constraints.
- Four native C scenarios with AddressSanitizer and UndefinedBehaviorSanitizer:
  file-read fault paths; valid/truncated/corrupt DHT frames; BOOT hold/release
  behavior; MQTT initialization/registration/start failures and disconnect races.
  Each scenario contains multiple assertions against the actual C implementation.
- Eight certificate tests using disposable synthetic certificates and keys:
  correct chain, incorrect ordering, mismatched key, wrong CN, insufficient
  remaining lifetime, private-key permissions, mixed PEM content, and symlinks.
- Four mocked USB/deployment safeguards: wrong-board refusal, old-layout refusal,
  certificate-only write boundaries, and production rejection for SPIFFS secrets.

No production private key or enrollment secret is used by these tests. Synthetic
PKI fixtures and native binaries are deleted at test completion.

## Physical board and ThingsBoard

The USB tool verified target MAC `94:b9:7e:fa:85:64`, generated a protected SPIFFS
image, and completed esptool-verified writes on the ESP32-D0WDQ6-V3, 4 MiB board.
Existing Wi-Fi credentials survived. After reset, serial output confirmed:

- Connection with saved Wi-Fi settings and DHCP address acquisition.
- NTP synchronization before MQTT startup with certificate date checks enabled.
- Loading the original working X.509 identity from the 320 KiB SPIFFS partition.
- MQTT connection, successful RMT samples, and separate broker acknowledgements.
- Approximately five-second telemetry intervals; the first read includes the
  one-second GPIO settling period. No read errors occurred in the final 45-second
  reset observation, after an earlier first-read timeout motivated that delay.

ThingsBoard's latest telemetry for `ESP32 DHT11 01` independently confirmed
23 degrees Celsius and 48 percent humidity after the update. The application
image is 1,283,600 bytes, leaving 616,944 bytes in each 1,900,544-byte OTA slot.

## Remaining physical/deployment acceptance

The existing working Wi-Fi configuration was not erased for testing. Before
deploying additional boards, manually verify BOOT long-press enrollment with the
new PoP, incorrect PoP rejection, incorrect Wi-Fi credentials followed by corrected
credentials, and provisioning completion/Bluetooth-memory release. The button
state machine is tested on the host; these full BLE flows are not yet physically
validated with the updated firmware.

Also exercise sensor unplug/replug, prolonged broker/Wi-Fi outage, DHCP renewal,
NTP unavailability at cold boot, long-duration memory usage, and interrupted
certificate replacement on isolated hardware. MQTT startup fault paths and USB
write boundaries have automated coverage; that is not a substitute for these
physical/network failure tests. Production services were not stopped or altered
to simulate failures.

No CA certificate was reissued/revoked, no hardware-security eFuses were changed,
and no remote OTA mechanism was added. See [security.md](security.md) for remaining
hardware-security/storage decisions and [maintenance.md](maintenance.md) for the
authorized USB update and renewal workflow.
