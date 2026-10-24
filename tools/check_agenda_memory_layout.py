"""Inspect the linked P4 image; never reads device memory or credentials."""
import argparse
from pathlib import Path
from elftools.elf.elffile import ELFFile


def inspect(path):
    with Path(path).open("rb") as file:
        elf = ELFFile(file)
        sections = {index: section.name for index, section in enumerate(elf.iter_sections())}
        symbols = [(symbol.name, symbol["st_size"], sections.get(symbol["st_shndx"], ""))
                   for symbol in elf.get_section_by_name(".symtab").iter_symbols() if symbol["st_size"]]
        sizes = {section.name: section["sh_size"] for section in elf.iter_sections()}
        return symbols, sizes


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("elf", type=Path)
    parser.add_argument("--baseline", type=Path)
    args = parser.parse_args()
    symbols, sizes = inspect(args.elf)
    required = ("work_mem_int.", "s_ui", "s_projection", "s_snapshot_slots", "s_icon_slots")
    for name in required:
        matches = [s for s in symbols if s[0].startswith(name)] if name.endswith(".") else [s for s in symbols if s[0] == name]
        assert matches and all(section == ".ext_ram.bss" for _, _, section in matches), name
        for label, size, section in matches:
            print(f"{label}: {size} bytes in {section}")
    assert any(label.startswith("work_mem_int.") and size == 65536 for label, size, _ in symbols)
    assert all(label not in ("s_product_body", "s_update_stream_chunk") for label, _, _ in symbols)
    vault = [s for s in symbols if s[0] == "s_credentials"]
    assert vault and all(section.startswith(".dram") for _, _, section in vault)
    internal = sum(size for section, size in sizes.items() if section.startswith(".dram") and (section.endswith(".bss") or section.endswith(".data")))
    print(f"Internal static data: {internal} bytes; external BSS: {sizes.get('.ext_ram.bss', 0)} bytes")
    if args.baseline:
        _, prior = inspect(args.baseline)
        old = sum(size for section, size in prior.items() if section.startswith(".dram") and (section.endswith(".bss") or section.endswith(".data")))
        print(f"Internal static reduction: {old - internal} bytes")
        assert old - internal >= 120 * 1024
    print("Memory placement: PASS (TLS/credential allocations require runtime evidence)")


if __name__ == "__main__":
    main()
