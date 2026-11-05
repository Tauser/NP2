"""Pure tests: no serial import, device access or writes."""
import binascii
from pathlib import Path
import struct
import unittest
from flash_p4_app_checked import active_slot, verify_layout


class FlashGateTest(unittest.TestCase):
    def test_ota_selection(self):
        erased = b"\xff" * 8192
        self.assertEqual(active_slot(erased), 0)
        data = bytearray(erased)
        def entry(offset, sequence, state=2):
            struct.pack_into("<I", data, offset, sequence)
            crc = binascii.crc32(data[offset:offset+4], 0xFFFFFFFF) & 0xFFFFFFFF
            struct.pack_into("<II", data, offset+24, state, crc)
        entry(0, 1)
        self.assertEqual(active_slot(data), 0)
        entry(4096, 2)
        self.assertEqual(active_slot(data), 1)
        entry(4096, 2, 4)
        self.assertEqual(active_slot(data), 0)
        data[28] ^= 1
        with self.assertRaises(ValueError):
            active_slot(data)

    def test_layout_match_and_wrong_address(self):
        table = bytearray(b"\xff" * 3072)
        for index, row in enumerate(((1,2,0x11000,0x8000,b"nvs"),
                                    (1,0,0x1b000,0x2000,b"otadata"),
                                    (0,0x10,0x20000,0x800000,b"ota_0"),
                                    (0,0x11,0x820000,0x800000,b"ota_1"),
                                    (1,0x82,0x1020000,0x900000,b"storage"))):
            struct.pack_into("<HBBII16sI", table, index*32, 0x50aa, *row, 0)
        verify_layout(table+b"\xff"*1024, table)
        wrong=bytearray(table);struct.pack_into("<I",wrong,2*32+4,0x200000)
        with self.assertRaises(ValueError):
            verify_layout(wrong+b"\xff"*1024,table)
        with self.assertRaises(ValueError):
            verify_layout(wrong+b"\xff"*1024,wrong)


if __name__ == "__main__":
    unittest.main()
