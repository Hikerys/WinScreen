import os
import struct
import unittest

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BIN_DIR = os.path.join(PROJECT_ROOT, "bin")

IMAGE_FILE_MACHINE_AMD64 = 0x8664
IMAGE_FILE_MACHINE_ARM64 = 0xAA64


def get_pe_machine(file_path):
    with open(file_path, "rb") as f:
        # Check MZ header
        if f.read(2) != b"MZ":
            raise ValueError("Not a valid DOS/PE executable")
        # Read offset to PE signature at 0x3C
        f.seek(0x3C)
        pe_offset = struct.unpack("<I", f.read(4))[0]
        # Read PE signature
        f.seek(pe_offset)
        sig = f.read(4)
        if sig != b"PE\x00\x00":
            raise ValueError(f"Invalid PE signature: {sig}")
        # Read Machine field (2 bytes)
        machine = struct.unpack("<H", f.read(2))[0]
        return machine


class TestBinaries(unittest.TestCase):
    def test_x86_64_binary(self):
        exe_path = os.path.join(BIN_DIR, "WinScreen.exe")
        self.assertTrue(os.path.isfile(exe_path), "WinScreen.exe does not exist")
        self.assertGreater(os.path.getsize(exe_path), 100_000, "WinScreen.exe file size too small")
        machine = get_pe_machine(exe_path)
        self.assertEqual(machine, IMAGE_FILE_MACHINE_AMD64, f"Expected x86_64 (0x8664), got 0x{machine:04x}")

    def test_arm64_binary(self):
        exe_path = os.path.join(BIN_DIR, "WinScreen-ARM.exe")
        self.assertTrue(os.path.isfile(exe_path), "WinScreen-ARM.exe does not exist")
        self.assertGreater(os.path.getsize(exe_path), 100_000, "WinScreen-ARM.exe file size too small")
        machine = get_pe_machine(exe_path)
        self.assertEqual(machine, IMAGE_FILE_MACHINE_ARM64, f"Expected ARM64 (0xaa64), got 0x{machine:04x}")


if __name__ == "__main__":
    unittest.main()
