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
├── device.crt            # Device X.509 certificate
├── device.key            # Device private key
└── root_ca.crt           # Root CA certificate
```

---

## Generate SPIFFS image manually

Activate the ESP-IDF v6.1 environment first (see below). From the project root on Linux:

```bash
mkdir -p spiffs_image
python "$IDF_PATH/components/spiffs/spiffsgen.py" \
  0x40000 spiffs_root spiffs_image/spiffs.bin
```

This requires all three certificate/key files listed above. Firmware compilation itself does not require these files.

The existing `partitions.csv` places SPIFFS at `0x290000`, with size `0x40000`:

```
spiffs, data, spiffs, 0x290000, 0x40000
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

Then write the SPIFFS image separately:

```bash
python -m esptool --chip esp32 --port /dev/ttyUSB0 \
  write-flash 0x290000 spiffs_image/spiffs.bin
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
│   ├── app_event.c / .h           # Event loop abstraction
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
- DHT11 timing uses `esp_rom_delay_us`; the existing GPIO, telemetry payload, broker setting, reconnect behavior, and partition offsets are preserved.
- The two OTA application slots remain `0x140000` bytes each. The build must fit these slots; do not enlarge them without reviewing the SPIFFS offset and flash layout.

Validation on Ubuntu 26.04 with ESP-IDF v6.1 and Python 3.14.4: a clean build from `sdkconfig.defaults` and all four automated checks passed. The application image is 1,256,896 bytes, leaving 53,824 bytes (about 4%) in each OTA slot. ESP-IDF itself emits CMake private-include dependency warnings between `esp_wifi` and `wpa_supplicant`; no unknown Kconfig symbols remain. Hardware provisioning and MQTT TLS operation have not yet been validated for this migration.

Build checks do not replace hardware testing. Before using the migrated firmware, verify fresh BLE provisioning, reconnecting after reboot with saved Wi-Fi credentials, recovery after Wi-Fi/MQTT interruption, SPIFFS certificate loading, ThingsBoard X.509 authentication, and DHT11 telemetry. A failed sensor read does not publish telemetry. End-to-end MQTT testing requires the device certificates and a configured ThingsBoard test environment.

Migration references: [5.4 to 5.5, 5.5 to 6.0, and 6.0 to 6.1](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32/migration-guides/index.html).

---

## License

MIT License
