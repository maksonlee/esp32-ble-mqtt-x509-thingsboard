# ESP32 BLE Wi-Fi Provisioning + MQTT over TLS with X.509

This ESP32 firmware enables BLE-based Wi-Fi provisioning and connects to ThingsBoard over MQTTs using X.509 client certificates. It targets **ESP-IDF v6.1** on the original **ESP32 with at least 4 MB flash**.

---

## Features

- BLE-based Wi-Fi provisioning (using ESP’s official provisioning app)
- MQTT over TLS (port `8883`)
- X.509 client certificate authentication
- Loads certificates from SPIFFS at runtime
- Sends telemetry every second
- Clean and modular ESP-IDF implementation

---

## SPIFFS Content Layout

We do **not** check in credentials or certificates to source control.  
You must manually place these files into the `spiffs_root` directory before building the SPIFFS image:

```
spiffs_root/
├── device.crt            # PEM chain: device -> issuing intermediate -> root
├── device.key            # Device private key
└── root_ca.crt           # Root CA certificate
```

### Prepare and verify the device certificate chain

`device.crt` must start with the device certificate, followed by its issuing
intermediate CA. If the root is included, put it last. The chain tested with
this deployment is **device -> IoTDeviceIssuingCA -> shared Root CA**.
Do not concatenate additional CA certificates in the order returned by a
PKCS#12 decoder without checking their issuer relationships. The downloaded
bundle used during setup returned the root before the intermediate; copying
that order into PEM caused ThingsBoard to reject the TLS client certificate.

After identifying the individual public certificates, assemble them explicitly
(the filenames below are placeholders for your exported certificates):

```bash
umask 077
cat device-leaf.pem issuing-ca.pem device-root.pem > spiffs_root/device.crt
openssl crl2pkcs7 -nocrl -certfile spiffs_root/device.crt |
  openssl pkcs7 -print_certs -noout
openssl verify -purpose sslclient -CAfile device-root.pem \
  -untrusted issuing-ca.pem device-leaf.pem
```

Inspect the printed sequence: each certificate's issuer must match the next
certificate's subject. `openssl verify` checks a trust path but can rebuild it
from an unordered collection; success alone does **not** validate PEM ordering.
Also check that the device certificate matches `device.key`, has the intended
CN, is currently valid, and permits TLS client authentication. Never print the
private key when checking it.

In the current deployment, Step CA's intermediate signs the MQTT **server**
certificate, while EJBCA's `IoTDeviceIssuingCA` signs **device** certificates;
both chain to the same root. `root_ca.crt` is the trust anchor for verifying the
server, not a replacement for the device's intermediate certificate.
The existing EJBCA end entity profile requires key recovery, so this device
uses an EJBCA-generated recoverable key rather than a locally generated CSR.

After changing any of these files, regenerate and flash the SPIFFS image.
Verify actual MQTT authentication and telemetry receipt in ThingsBoard;
a TLS handshake reported successful by a client alone is not sufficient.

---

## Generate SPIFFS image manually

Activate the ESP-IDF v6.1 environment first (see below). From the project root on Linux:

```bash
mkdir -p spiffs_image
python "$IDF_PATH/components/spiffs/spiffsgen.py" \
  0x50000 spiffs_root spiffs_image/spiffs.bin
```

This requires all three certificate/key files listed above. Firmware compilation itself does not require these files.

`partitions.csv` places SPIFFS at `0x3b0000`, with size `0x50000`:

```
spiffs, data, spiffs, 0x3b0000, 0x50000
```

---

## Build Firmware

Install ESP-IDF using the [official Linux installation guide](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32/get-started/linux-setup.html). With EIM installed:

```bash
eim install -i v6.1 -t esp32
```

Activate the environment using the command printed by EIM. With the default installation path:

```bash
source "$HOME/.espressif/tools/activate_idf_v6.1.sh"
idf.py --version
```

From the project root, build using this host's required resource limiter. Check that no Jenkins/Docker build is active before starting:

```bash
/home/administrator/.local/bin/codex-build-limited -- \
  python "$IDF_PATH/tools/idf.py" build
```

EIM exposes `idf.py` as a shell function, so the resource limiter invokes its Python script directly. On other machines without this host policy, use `idf.py build` in the activated environment.

`sdkconfig.defaults` selects ESP32, 4 MB flash, NimBLE, the custom partition table, and provisioning Security 1. For an existing local `sdkconfig` from v5.4.1, back it up outside the repository and regenerate it from these defaults, then reapply any intentional local settings (such as `CONFIG_BROKER_URI`).

The Component Manager fetches `espressif/network_provisioning` and `espressif/mqtt` using `main/idf_component.yml`; `dependencies.lock` records the resolved versions and hashes, including transitive dependencies. Generated component sources belong in the ignored `managed_components/` directory.

After a successful build, check the generated configuration and firmware:

```bash
/home/administrator/.local/bin/codex-build-limited -- \
  python -m unittest discover -s tests -v
```

> The SPIFFS image will **not** be built automatically. You must run the above `spiffsgen.py` command yourself.

---

## Flash Everything

With the ESP32 connected, replace `/dev/ttyUSB0` with its actual port. For a blank/erased board, use a full flash:

```bash
/home/administrator/.local/bin/codex-build-limited -- \
  python "$IDF_PATH/tools/idf.py" -p /dev/ttyUSB0 flash --all
```

When upgrading from the earlier layout (OTA slots `0x140000`, SPIFFS at `0x290000`), flash the new partition table, OTA metadata, and application together using the full-flash command above. An application-only OTA update cannot migrate this layout. Regenerate SPIFFS at the new size; keep the local certificate/key files available before flashing. Do not erase the whole chip if you want to retain Wi-Fi credentials in NVS.

Then write the regenerated SPIFFS image separately:

```bash
python -m esptool --chip esp32 --port /dev/ttyUSB0 \
  write-flash 0x3b0000 spiffs_image/spiffs.bin
```

The SPIFFS image contains the device private key: keep it local and out of source control. The serial-port user needs access to the device (usually membership in `dialout`, effective after a new login).

---

## Provision Wi-Fi over BLE

Download **ESP BLE Provisioning** app from:

- [Android (Play Store)](https://play.google.com/store/apps/details?id=com.espressif.provble)
- [iOS (App Store)](https://apps.apple.com/us/app/esp-ble-provisioning/id1470163477)

Scan for devices, tap your ESP32, and follow the app instructions to provision Wi-Fi.

The BLE name remains `PROV_` followed by six MAC-address hex digits. This migration retains Security 1 and the existing no-proof-of-possession configuration.

---

## ThingsBoard Configuration

This project requires ThingsBoard to be configured for:

- X.509 client certificate authentication
- Device auto-provisioning using `CN` from the certificate
- Trusting your Root + Intermediate CA certificate chain

For a full step-by-step guide, see:  
[Secure ThingsBoard MQTTS with X.509 Certificate Chain and Auto-Provisioning](https://www.maksonlee.com/secure-thingsboard-mqtts-with-x-509-certificate-chain-and-auto-provisioning/)

> The ESP32 uses `root_ca.crt` in SPIFFS to verify the ThingsBoard MQTT server certificate.

---

## Project Structure

```
esp32-ble-mqtt-x509-thingsboard/
├── main/
│   ├── app_main.c                 # Entry point (main task)
│   ├── cert_manager.c / .h       # Loads X.509 certs from SPIFFS
│   ├── dht11.c / .h              # DHT11 on GPIO 23
│   ├── mqtt_client_handler.c / .h# MQTT connection and telemetry
│   ├── spiffs_utils.c / .h       # Mount/read SPIFFS filesystem
│   ├── wifi_provisioning.c / .h  # BLE provisioning using protocomm
│   ├── CMakeLists.txt
│   ├── Kconfig.projbuild
│   └── idf_component.yml   # IDF and managed component requirements
├── spiffs_root/            # SPIFFS content (not in version control)
├── spiffs_image/           # Output folder for spiffs.bin
├── CMakeLists.txt
├── sdkconfig.defaults     # Shared defaults; sdkconfig is generated locally
├── tests/                 # Checks against the built configuration and images
└── README.md
```

---

## Notes

- Device certificates are usually valid for **365 days** (adjust as needed).
- The MQTT client verifies the ThingsBoard server certificate using `root_ca.crt` in SPIFFS.
- Telemetry is sent to ThingsBoard once per second via MQTT.

## ESP-IDF v6.1 Migration and Validation

- Wi-Fi provisioning uses the managed `network_provisioning` component and renamed APIs; MQTT uses the managed `mqtt` component.
- Security 1 is explicitly enabled because ESP-IDF 6.x disables it by default. Certificate verification and client certificate authentication remain enabled in the MQTT configuration.
- DHT11 timing uses `esp_rom_delay_us`; the existing GPIO, telemetry payload, broker setting, and reconnect behavior are preserved.
- The two OTA application slots are `0x1d0000` bytes (1,856 KiB) each, at `0x10000` and `0x1e0000`. SPIFFS occupies the final 320 KiB at `0x3b0000`; the layout uses all 4 MiB of flash. NVS, OTA metadata, and PHY offsets remain unchanged.

Validation on Ubuntu 26.04 with ESP-IDF v6.1 and Python 3.14.4: a clean build from `sdkconfig.defaults` and all four automated checks passed. The application image is 1,256,896 bytes, leaving 643,648 bytes (about 34%) in each expanded OTA slot. ESP-IDF itself emits CMake private-include dependency warnings between `esp_wifi` and `wpa_supplicant`; no unknown Kconfig symbols remain.

Hardware validation on 2026-09-21 with an ESP32-D0WDQ6-V3 and 4 MB flash passed: BLE Wi-Fi provisioning, reconnecting with saved Wi-Fi after restart, SPIFFS certificate loading, and X.509 MQTT authentication. After correcting the device PEM chain order, ThingsBoard automatically created `ESP32 DHT11 01` under the `IoTDevice` profile. DHT11 telemetry was published every second, and server-side latest telemetry confirmed temperature 23 degrees Celsius and humidity 49 percent. Recovery after a separate Wi-Fi or MQTT service interruption remains untested.

The expanded flash layout was subsequently rebuilt and all four updated checks passed. Flash write hashes and the boot log confirmed the new layout. Saved Wi-Fi credentials survived the migration, the relocated SPIFFS mounted successfully, and MQTT reconnected. ThingsBoard confirmed fresh telemetry at 23 degrees Celsius and 48 percent humidity.

Build checks do not replace hardware testing. Before using the migrated firmware, verify fresh BLE provisioning, reconnecting after reboot with saved Wi-Fi credentials, recovery after Wi-Fi/MQTT interruption, SPIFFS certificate loading, ThingsBoard X.509 authentication, and DHT11 telemetry. A failed sensor read does not publish telemetry. End-to-end MQTT testing requires the device certificates and a configured ThingsBoard test environment.

Migration references: [5.4 to 5.5, 5.5 to 6.0, and 6.0 to 6.1](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32/migration-guides/index.html).

---

## License

MIT License
