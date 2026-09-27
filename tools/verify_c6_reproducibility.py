"""Compare two audited C6 builds made in different build directories."""

import argparse
import json
from pathlib import Path

ARTIFACTS = ("app", "bootloader", "partition_table", "sdkconfig", "lock")
FIELDS = ("target", "idf", "hosted", "partitions", "effective_config")


def compare_build_audits(left, right):
    for field in FIELDS:
        if left.get(field) != right.get(field):
            raise ValueError(f"Build audit mismatch: {field}")
    for artifact in ARTIFACTS:
        try:
            lhs = left["artifacts"][artifact]
            rhs = right["artifacts"][artifact]
        except KeyError as error:
            raise ValueError(f"Missing audited artifact: {artifact}") from error
        if lhs["bytes"] != rhs["bytes"] or lhs["sha256"] != rhs["sha256"]:
            raise ValueError(f"Build artifact mismatch: {artifact}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--left", type=Path, required=True)
    parser.add_argument("--right", type=Path, required=True)
    args = parser.parse_args()
    left = json.loads((args.left / "build-audit.json").read_text())
    right = json.loads((args.right / "build-audit.json").read_text())
    compare_build_audits(left, right)
    print("C6 reproducibility verified: app, bootloader, table, config and lock match")


if __name__ == "__main__":
    main()
