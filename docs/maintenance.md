# Reliability and maintenance

## Certificate storage

Certificate files must be nonempty and no larger than 16 KiB each. The loader
rejects seek failures, invalid sizes, short reads, and stream errors. A failed
mount never formats the certificate partition. Mount and cleanup operations
are idempotent, and released private-key buffers are wiped.

Run host fault-injection tests through the host build limiter:

```bash
/home/administrator/.local/bin/codex-build-limited -- \
  python3 -m unittest discover -s tests -p test_native.py -v
```

These tests compile the actual file loader with failing I/O substitutes and
AddressSanitizer/UndefinedBehaviorSanitizer. They do not access device secrets.

## MQTT diagnostics

Submission and broker acknowledgement are separate log events. Negative publish
results are failures, not successful uploads. A QoS 1 acknowledgement confirms
broker receipt; verify ThingsBoard telemetry separately for application receipt.
Transport failures include TLS verification flags, TLS errors, socket errno,
and CONNACK status without printing keys or credentials.

Sampling runs in a dedicated task, not the shared ESP Timer task. Connection
events wake that task, and a sample is discarded if MQTT disconnected while it
was being acquired. MQTT enqueue keeps socket writes in the MQTT task. Its
outbox is bounded to 16 KiB; full-queue failures are logged instead of consuming
unbounded heap while acknowledgements are unavailable.

## Wi-Fi recovery

Wi-Fi event handlers never sleep to implement reconnect delays. A worker retries
saved credentials every five seconds and logs connection API errors. During BLE
enrollment the provisioning manager owns attempts, so application retries do not
race it. IP events remain registered across reconnects.

On `NETWORK_PROV_END`, the manager is deinitialized and the BLE/BT memory is
released. Reprovisioning uses a restart, so released controller memory never
needs to be reallocated during the same boot.

After normal startup, release BOOT once, then hold it for five seconds to clear
Wi-Fi configuration and restart into BLE provisioning. Device credentials in
SPIFFS are retained. Do not hold BOOT while resetting/powering on: GPIO 0 is a
boot strap and may enter the ROM downloader. Short presses and a pin held low at
task startup do not trigger reset. `CONFIG_REPROVISION_GPIO` is configurable and
must differ from the sensor pin. Failed enrollment retries three times, then
clears the failed Wi-Fi configuration and accepts a new BLE credential attempt.

The MQTT worker starts at boot and receives network availability directly through
an event group. No one-shot application event can be lost to a full event queue.
Missing certificate files, client allocation failures, event-registration failures,
and client-start failures retry every five seconds while Wi-Fi has an address.
After successful startup, ESP-MQTT owns transport reconnection. There is one
client and one sampling task; connection events do not create additional tasks.

Losing the DHCP address also pauses sampling even if the Wi-Fi radio remains
associated. A sample is not enqueued until both IP availability and MQTT
connectivity are present, including a connection change during a sensor read.

## DHT11 timing

The data pin is configurable (default GPIO 23). RMT captures pulses at 1 MHz,
so Wi-Fi interrupts do not turn loop iterations into incorrect pulse widths.
The host uses an open-drain release and guarantees at least a 20 ms start pulse.
The decoder checks the response, all 40 bits, checksum, and integer output ranges;
invalid frames never modify the caller's reading. The driver has one task owner.
Use a suitable external pull-up as required by the sensor/module wiring.

Sampling and publication default to once every five seconds. Change
`CONFIG_TELEMETRY_INTERVAL_SECONDS` in menuconfig (minimum three seconds).
Connection changes restart the waiting interval; failed reads are skipped and
never trigger an immediate retry or publication of cached data.

## Clock and TLS validation

The MQTT worker starts SNTP only after Wi-Fi has an address and waits for initial
clock synchronization before starting TLS. `CONFIG_TIME_SERVER` defaults to
`time.cloudflare.com`; UDP/123 must be reachable. Failed synchronization is
retried without bypassing certificate validation. mbedTLS checks not-before and
not-after dates as well as the chain and hostname. SNTP itself is unauthenticated;
use a controlled internal time source where the deployment requires it.

Existing sdkconfig files must enable `CONFIG_MBEDTLS_HAVE_TIME_DATE=y`; defaults
do not override a saved disabled setting. The build contract test enforces this.

## USB firmware updates and certificate renewal

Activate ESP-IDF and build first. The maintenance tool uses Python's standard
library, OpenSSL 3, and the active IDF esptool/spiffsgen; no extra Python package
is required. It rejects mismatched keys, wrong CN, unordered/invalid chains,
wrong client usage, expired/near-expiry certificates, unsafe private-key
permissions, unexpected directory contents, and unsupported partition layouts.
The default expiry margin is 30 days, configurable with `--min-days`.

```bash
python tools/device.py check --device-name 'ESP32 DHT11 01'
/home/administrator/.local/bin/codex-build-limited -- \
  python tools/device.py image --device-name 'ESP32 DHT11 01' --generate-pop
/home/administrator/.local/bin/codex-build-limited -- \
  python tools/device.py flash --device-name 'ESP32 DHT11 01' \
  --port /dev/ttyUSB0 --expected-mac 94:b9:7e:fa:85:64
```

`--generate-pop` creates a per-device secret only if absent; it never prints or
replaces an existing one. Keep that file with the device's private credentials.
Image generation includes exactly `device.crt`, `device.key`, `root_ca.crt`, and
`provisioning.pop`, with SPIFFS geometry taken from the build. Images are written
atomically with mode 0600. Flash always regenerates the image, verifies the
connected MAC, and relies on esptool write verification. A full update writes
bootloader, partition table, OTA metadata, application, and SPIFFS together; NVS
is preserved. Resetting OTA metadata intentionally selects the freshly written
OTA slot 0. Do not use this tool on an encrypted or secure-boot production board.

For certificate renewal:

1. Obtain a new device key/certificate using the existing EJBCA recovery-enabled
   profile. Keep the device CN unchanged. This tool does not issue or revoke
   certificates or change ThingsBoard settings.
2. Prepare a private staging directory under ignored `certs/`, containing the new
   ordered `device.crt`, matching `device.key`, server trust `root_ca.crt`, and a
   copy of the existing `provisioning.pop`. Use directory mode 0700 and key/PoP
   mode 0600. Keep the previous working bundle available for rollback.
3. Run `check` with `--certs-dir certs/staged`, then run `flash` with that same
   option and `--cert-only`, plus the target name, port, and MAC. Certificate-only
   updates first compare the actual board partition table to the build. They
   refuse an old layout instead of writing to the wrong location.
4. Verify boot, clock sync, MQTT acknowledgement, and fresh ThingsBoard telemetry.
   Only then promote the staged bundle to the local `spiffs_root` used for future
   updates. Revoke/archive the superseded identity according to CA policy.

SPIFFS replacement is not atomic across power loss. If interrupted, restore the
previous validated bundle over USB; Wi-Fi NVS is not part of the write. This is
the supported maintenance workflow for now. Remote OTA, including update-source
authentication and rollback policy, is intentionally a separate feature.
