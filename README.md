# ESP32 BLE Wi-Fi Provisioning + MQTT over TLS with X.509

This ESP32 firmware enables BLE-based Wi-Fi provisioning and connects to ThingsBoard over MQTTs using X.509 client certificates. It's built with **ESP-IDF v5.4.1** and designed for production IoT scenarios.

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
└── server_chain.pem      # Server certificate chain (ThingsBoard)
```

---

## Generate SPIFFS image manually

ESP-IDF already includes Python and all required tools.  
You do **not** need to install anything or run `.\export.ps1`.

Run this command from PowerShell:

```powershell
python $env:IDF_PATH/components/spiffs/spiffsgen.py 0x40000 .\spiffs_root .\spiffs_image\spiffs.bin
```

> This generates `spiffs.bin` at `.\spiffs_image\spiffs.bin`.

Make sure your partition table has an entry like:

```
spiffs, data, spiffs, 0x290000, 0x40000
```

---

## Build Firmware

From the root directory:

```powershell
idf.py build
```

> The SPIFFS image will **not** be built automatically. You must run the above `spiffsgen.py` command yourself.

---

## Flash Everything

```powershell
idf.py -p COMX flash
```

Then write the SPIFFS image separately:

```powershell
esptool.py --chip esp32 --port COMX write_flash 0x290000 .\spiffs_image\spiffs.bin
```

Replace `COMX` with your actual serial port (e.g., `COM3`).

---

## Provision Wi-Fi over BLE

Download **ESP BLE Provisioning** app from:

- [Android (Play Store)](https://play.google.com/store/apps/details?id=com.espressif.provble)
- [iOS (App Store)](https://apps.apple.com/us/app/esp-ble-provisioning/id1470163477)

Scan for devices, tap your ESP32, and follow the app instructions to provision Wi-Fi.

---

## ThingsBoard Configuration

This project requires ThingsBoard to be configured for:

- X.509 client certificate authentication
- Device auto-provisioning using `CN` from the certificate
- Trusting your Root + Intermediate CA certificate chain

For a full step-by-step guide, see:  
[Secure ThingsBoard MQTTS with X.509 Certificate Chain and Auto-Provisioning](https://www.maksonlee.com/secure-thingsboard-mqtts-with-x-509-certificate-chain-and-auto-provisioning/)

> The ESP32 uses `server_chain.pem` in SPIFFS to verify the ThingsBoard MQTT server certificate.

---

## Project Structure

```
esp32-ble-mqtt-x509-thingsboard/
├── main/
│   ├── app_event.c / .h           # Event loop abstraction
│   ├── app_main.c                 # Entry point (main task)
│   ├── cert_manager.c / .h       # Loads X.509 certs from SPIFFS
│   ├── dht11.c / .h              # Sensor logic (optional)
│   ├── mqtt_client_handler.c / .h# MQTT connection and telemetry
│   ├── spiffs_utils.c / .h       # Mount/read SPIFFS filesystem
│   ├── wifi_provisioning.c / .h  # BLE provisioning using protocomm
│   ├── CMakeLists.txt
│   └── Kconfig.projbuild
├── spiffs_root/            # SPIFFS content (not in version control)
├── spiffs_image/           # Output folder for spiffs.bin
├── CMakeLists.txt
├── sdkconfig
└── README.md
```

---

## Notes

- Device certificates are usually valid for **365 days** (adjust as needed).
- The MQTT client verifies the ThingsBoard server certificate using `server_chain.pem` in SPIFFS.
- Telemetry is sent to ThingsBoard once per second via MQTT.

---

## License

MIT License