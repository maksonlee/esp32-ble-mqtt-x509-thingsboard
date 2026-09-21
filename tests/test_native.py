"""Compile and run host-side C fault tests; invoke through codex-build-limited."""
from pathlib import Path
import subprocess
import tempfile
import unittest

NATIVE = Path(__file__).parent / "native"


class NativeTests(unittest.TestCase):
    def test_spiffs_faults(self):
        self.run_native("test_spiffs.c")

    def run_native(self, source):
        with tempfile.TemporaryDirectory() as directory:
            executable = Path(directory) / "test"
            subprocess.run([
                "cc", "-std=c11", "-g", "-fsanitize=address,undefined",
                "-I", str(NATIVE), str(NATIVE / source), "-o", str(executable),
            ], check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()
