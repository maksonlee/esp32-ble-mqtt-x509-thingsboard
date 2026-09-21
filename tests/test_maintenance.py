import importlib.util
import json
from pathlib import Path
from types import SimpleNamespace
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location("maintenance_device", Path(__file__).parents[1] / "tools/device.py")
device = importlib.util.module_from_spec(spec)
spec.loader.exec_module(device)


class MaintenanceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        root = Path(self.temp.name)
        (root / "partition_table").mkdir()
        (root / "partition_table/partition-table.bin").write_bytes(b"expected table")
        self.args = SimpleNamespace(build=root, cert_only=True, port="FAKE",
                                    expected_mac="94:b9:7e:fa:85:64", image=root / "image.bin")

    def test_wrong_board_never_writes(self):
        with patch.object(device, "make_image", return_value={}), \
             patch.object(device, "command", return_value=b"MAC: 00:00:00:00:00:00") as command:
            with self.assertRaisesRegex(ValueError, "MAC"):
                device.flash(self.args)
            self.assertEqual(command.call_count, 1)

    def test_certificate_update_rejects_old_partition_table(self):
        def command(args):
            if "read-mac" in args: return b"MAC: 94:b9:7e:fa:85:64"
            self.assertIn("read-flash", args)
            Path(args[-1]).write_bytes(b"old table")
            return b""
        with patch.object(device, "make_image", return_value={}), patch.object(device, "command", side_effect=command):
            with self.assertRaisesRegex(ValueError, "partition table differs"):
                device.flash(self.args)

    def test_certificate_update_writes_only_spiffs(self):
        writes = []
        def command(args):
            if "read-mac" in args: return b"MAC: 94:b9:7e:fa:85:64"
            if "read-flash" in args:
                Path(args[-1]).write_bytes(b"expected table" + b"\xff" * 16)
            else: writes.append(args)
            return b""
        with patch.object(device, "make_image", return_value={}), patch.object(device, "command", side_effect=command):
            device.flash(self.args)
        self.assertEqual(len(writes), 1)
        self.assertEqual(writes[0][-3:], ["write-flash", "0x3b0000", str(self.args.image)])

    def test_production_gate_rejects_unprotected_secret_storage(self):
        config = self.args.build / "config"
        config.mkdir()
        (config / "sdkconfig.json").write_text(json.dumps({"SECURE_BOOT": True, "SECURE_FLASH_ENC_ENABLED": True}))
        with self.assertRaisesRegex(ValueError, "SPIFFS"):
            device.production_check(self.args.build)
