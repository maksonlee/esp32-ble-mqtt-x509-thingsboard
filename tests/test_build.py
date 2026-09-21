"""Check the generated firmware against the board and provisioning requirements.

Run after `idf.py build`, using `python -m unittest discover -s tests -v`.
These checks do not access hardware or require device credentials.
"""

import json
from pathlib import Path
import struct
import unittest


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build"


class BuildContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.project = json.loads((BUILD / "project_description.json").read_text())
        cls.config = json.loads((BUILD / "config" / "sdkconfig.json").read_text())
        cls.partitions = {}
        data = (BUILD / "partition_table" / "partition-table.bin").read_bytes()
        for offset in range(0, len(data), 32):
            record = data[offset:offset + 32]
            # Ignore the checksum and erased padding after the partition entries.
            if record[:2] != b"\xaa\x50":
                break
            _, kind, subtype, start, size, label, _ = struct.unpack("<HBBII16sI", record)
            name = label.split(b"\0", 1)[0].decode()
            cls.partitions[name] = (kind, subtype, start, size)

    def test_idf_version_target_and_managed_components(self):
        self.assertRegex(self.project["git_revision"], r"^v6\.1(?:\.\d+)?$")
        self.assertEqual(self.project["target"], "esp32")
        self.assertTrue({"espressif__mqtt", "espressif__network_provisioning"}
                        <= set(self.project["build_components"]))

    def test_ble_provisioning_and_mqtt_tls_enabled(self):
        for option in ("BT_ENABLED", "BT_NIMBLE_ENABLED",
                       "NETWORK_PROV_NETWORK_TYPE_WIFI",
                       "ESP_PROTOCOMM_SUPPORT_SECURITY_VERSION_1",
                       "MQTT_TRANSPORT_SSL"):
            with self.subTest(option=option):
                self.assertIs(self.config[option], True)

    def test_existing_partition_layout_and_flash_capacity(self):
        self.assertEqual(self.config["ESPTOOLPY_FLASHSIZE"], "4MB")
        self.assertEqual(self.config["PARTITION_TABLE_OFFSET"], 0x8000)
        self.assertEqual(self.partitions, {
            "nvs": (1, 2, 0x9000, 0x4000),
            "otadata": (1, 0, 0xD000, 0x2000),
            "phy_init": (1, 1, 0xF000, 0x1000),
            "ota_0": (0, 0x10, 0x10000, 0x140000),
            "ota_1": (0, 0x11, 0x150000, 0x140000),
            "spiffs": (1, 0x82, 0x290000, 0x40000),
        })
        for _, _, start, size in self.partitions.values():
            self.assertLessEqual(start + size, 4 * 1024 * 1024)

    def test_firmware_fits_both_ota_slots(self):
        firmware = BUILD / self.project["app_bin"]
        size = firmware.stat().st_size
        self.assertGreater(size, 0)
        for name in ("ota_0", "ota_1"):
            with self.subTest(partition=name):
                self.assertLessEqual(size, self.partitions[name][3])


if __name__ == "__main__":
    unittest.main()
