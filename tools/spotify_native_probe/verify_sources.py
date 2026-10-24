"""Fail closed on an unexpected or modified foreign checkout (no network)."""
import json
import subprocess
import sys
from pathlib import Path


def main():
    root = Path(sys.argv[1]).resolve().parent
    lock = json.loads(Path(__file__).with_name("source-lock.json").read_text())
    for item in lock["repositories"]:
        source = root / item["path"]
        revision = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
        if revision != item["commit"]:
            raise ValueError("Spotify source revision differs from source-lock.json")
        subprocess.run(["git", "-C", str(source), "diff", "--exit-code", "--quiet", "HEAD", "--"], check=True)
    print("Spotify source pins and immutable checkouts verified")


if __name__ == "__main__":
    main()
