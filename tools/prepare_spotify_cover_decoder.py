"""Prepare private fixed-pool TJpgDec from the project's pinned LVGL checkout."""
import argparse
from pathlib import Path


def prepare(source, output):
    output.mkdir(parents=True, exist_ok=True)
    header = (source / "tjpgd.h").read_text()
    required = '#include "../../lv_conf_internal.h"'
    if header.count(required) != 1:
        raise ValueError("Unexpected pinned JPEG header")
    header = header.replace(required, "#define LV_USE_TJPGD 1").replace('"tjpgdcnf.h"', '"np2_tjpgdcnf.h"')
    code = (source / "tjpgd.c").read_text().replace('"tjpgd.h"', '"np2_tjpgd.h"')
    for name, text in (("np2_tjpgd.h", header), ("np2_tjpgd.c", code),
                       ("np2_tjpgdcnf.h", (source / "tjpgdcnf.h").read_text())):
        path = output / name
        if not path.exists() or path.read_text() != text:
            path.write_text(text, newline="\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    prepare(args.source, args.output)
