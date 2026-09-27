"""Negative tests for the gates that run before/after the C6 build."""
import struct
import unittest

from audit_c6_idf_patch import NEW, OLD, normalized, verify_source
from verify_c6_reproducibility import compare_build_audits
from verify_c6_build import REQUIRED, check_config, config_values, partitions


class PatchGateTest(unittest.TestCase):
    def setUp(self):
        self.original = "// preamble\n" + OLD + "\n// remainder\n"
        self.patched = self.original.replace(OLD, NEW)

    def test_only_official_patch(self):
        self.assertEqual(verify_source(self.original, self.patched), self.patched)
        self.assertEqual(verify_source(self.original, self.original, True), self.patched)

    def test_missing_patch(self):
        with self.assertRaises(ValueError):
            verify_source(self.original, self.original)

    def test_unrelated_sdk_edit(self):
        with self.assertRaises(ValueError):
            verify_source(self.original, self.patched + "// unrelated modification\n", True)

    def test_no_guard_or_duplicate(self):
        for original in ("// unrelated", self.original + OLD):
            with self.assertRaises(ValueError):
                verify_source(original, self.patched, True)

    def test_line_endings_only(self):
        self.assertEqual(normalized(self.original.replace("\n", "\r\n").encode()), self.original)
        with self.assertRaises(ValueError):
            verify_source(self.original, self.patched.replace("remainder", "changed"))


class ConfigGateTest(unittest.TestCase):
    def test_disabled_parser(self):
        self.assertEqual(config_values('# CONFIG_SECURE_BOOT is not set\nCONFIG_IDF_TARGET="esp32c6"'),
                         {"CONFIG_SECURE_BOOT": "n", "CONFIG_IDF_TARGET": '"esp32c6"'})

    def test_required_symbols(self):
        check_config(REQUIRED)
        for key in REQUIRED:
            candidate = dict(REQUIRED)
            del candidate[key]
            with self.subTest(key=key), self.assertRaises(ValueError):
                check_config(candidate)

    def test_wrong_target_and_security(self):
        for key, value in (("CONFIG_IDF_TARGET", '"esp32p4"'),
                           ("CONFIG_SECURE_BOOT", "y"),
                           ("CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE", "y"),
                           ("CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT", "y")):
            candidate = dict(REQUIRED)
            candidate[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError):
                check_config(candidate)

    def test_partition_layout(self):
        entries = [("nvs", 1, 2, 0x9000, 0x4000), ("otadata", 1, 0, 0xD000, 0x2000),
                   ("phy_init", 1, 1, 0xF000, 0x1000),
                   ("ota_0", 0, 0x10, 0x10000, 0x1C0000),
                   ("ota_1", 0, 0x11, 0x1D0000, 0x1C0000)]
        blob = b"".join(struct.pack("<HBBII16sI", 0x50AA, kind, subtype, offset, size,
                                    label.encode(), 0) for label, kind, subtype, offset, size in entries)
        self.assertEqual(len(partitions(blob)), 5)
        for corrupted in (blob[:-32], blob + blob[-32:], blob[:12] + b"bad" + blob[15:]):
            with self.assertRaises(ValueError):
                partitions(corrupted)


class ReproducibilityTest(unittest.TestCase):
    def setUp(self):
        self.audit = {
            "target": "esp32c6", "idf": "5.5.4", "hosted": "3.0.6",
            "partitions": [], "effective_config": {"CONFIG_APP_REPRODUCIBLE_BUILD": "y"},
            "artifacts": {name: {"bytes": 1, "sha256": name} for name in
                          ("app", "bootloader", "partition_table", "sdkconfig", "lock")},
        }

    def test_matching_audits(self):
        compare_build_audits(self.audit, dict(self.audit))

    def test_rejects_artifact_or_configuration_change(self):
        changed = dict(self.audit)
        changed["artifacts"] = dict(self.audit["artifacts"])
        changed["artifacts"]["app"] = {"bytes": 2, "sha256": "changed"}
        with self.assertRaises(ValueError):
            compare_build_audits(self.audit, changed)
        changed = dict(self.audit)
        changed["target"] = "esp32p4"
        with self.assertRaises(ValueError):
            compare_build_audits(self.audit, changed)


if __name__ == "__main__":
    unittest.main()
