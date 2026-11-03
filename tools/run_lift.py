"""Target-aware pcrecomp integration, keeping the pinned dependency unchanged."""

from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import sys

from catalog_cache import load_catalog, profile_digest, save_catalog
from preflight import ROOT


def load_profile(path: Path | None, blob: bytes, image_base: int, sections) -> dict:
    if path is None:
        return {}
    profile = json.loads(path.read_text(encoding="utf-8"))
    if profile["input_sha256"] != hashlib.sha256(blob).hexdigest():
        raise ValueError("Reviewed code regions belong to a different executable")
    # The complete binary hash already binds all descriptor and table bytes.
    # An optional per-range hash makes the reviewed byte range independently
    # auditable without deriving any instructions from it.
    def read_va(va: int, size: int) -> bytes:
        section = next((s for s in sections if image_base + s.virtual_address <= va
                        and va + size <= image_base + s.virtual_address + s.raw_size), None)
        if section is None:
            raise ValueError(f"Profile anchor is not file-backed: {va:#x}")
        start = section.raw_offset + va - image_base - section.virtual_address
        return blob[start:start + size]

    for anchor in profile.get("code_anchors", []):
        expected = bytes.fromhex(anchor["expected_bytes"])
        if read_va(int(anchor["va"], 0), len(expected)) != expected:
            raise ValueError("Reviewed code anchor mismatch")
    for trap in profile.get("trap_sites", []):
        expected = bytes.fromhex(trap["expected_bytes"])
        if read_va(int(trap["va"], 0), len(expected)) != expected:
            raise ValueError("Reviewed interrupt bytes mismatch")
        if read_va(int(trap["jump_table_va"], 0) + 4 * trap["jump_table_index"], 4) != int(trap["entry_va"], 0).to_bytes(4, "little"):
            raise ValueError("Reviewed interrupt dispatch table mismatch")
    for item in profile.get("excluded_code_ranges", []):
        lo, hi = int(item["start"], 0), int(item["end"], 0)
        if lo >= hi:
            raise ValueError("Excluded code range must be non-empty and end-exclusive")
        section = next((s for s in sections if image_base + s.virtual_address <= lo
                        and hi <= image_base + s.virtual_address + s.raw_size), None)
        if section is None:
            raise ValueError("Reviewed data range is not file-backed in one section")
        offset = section.raw_offset + lo - image_base - section.virtual_address
        if item.get("sha256") and hashlib.sha256(blob[offset:offset + hi - lo]).hexdigest() != item["sha256"]:
            raise ValueError("Reviewed data bytes do not match their digest")
        for array in item.get("arrays", []):
            start, end = int(array["start"], 0), int(array["end"], 0)
            if not lo <= start < end <= hi or end - start != array["record_size"] * array["record_count"]:
                raise ValueError("Reviewed array bounds are inconsistent")
            expected = bytes.fromhex(array["descriptor_expected_bytes"])
            if read_va(int(array["descriptor_va"], 0), len(expected)) != expected:
                raise ValueError("Reviewed array descriptor mismatch")
            if hashlib.sha256(read_va(start, end - start)).hexdigest() != array["sha256"]:
                raise ValueError("Reviewed array digest mismatch")
    return profile


def reviewed_disassembler(base, profile: dict):
    ranges = [(int(r["start"], 0), int(r["end"], 0))
              for r in profile.get("excluded_code_ranges", [])]

    class ReviewedDisassembler(base):
        def is_code_address(self, va):
            return super().is_code_address(va) and not any(lo <= va < hi for lo, hi in ranges)

        def code_section_bounds(self, va):
            bounds = super().code_section_bounds(va)
            if bounds is None:
                return None
            start, end = bounds
            for lo, hi in ranges:
                if lo <= va < hi:
                    return None
                if hi <= va:
                    start = max(start, hi)
                elif va < lo:
                    end = min(end, lo)
            return start, end

        def disassemble_at(self, va, max_bytes=4096):
            bounds = self.code_section_bounds(va)
            if bounds is None:
                return
            yield from super().disassemble_at(va, min(max_bytes, bounds[1] - va))

    return ReviewedDisassembler


def target_lifter(base, profile: dict, functions: dict):
    from x87_extension import make_x87_lifter
    x87_base = make_x87_lifter(base)
    traps = {int(item["va"], 0): item for item in profile.get("trap_sites", [])}
    counts = Counter()

    class TargetLifter(x87_base):
        def __init__(self, **kwargs):
            super().__init__(**kwargs, lifted=set(functions), precise_carry=True, precise_sbb=True)

        def _lift_instruction(self, insn):
            if insn.mnemonic == "int" and insn.address in traps:
                entry = traps[insn.address]
                vector = int(entry["vector"])
                if bytes(insn.bytes) != bytes((0xCD, vector)):
                    raise ValueError("Reviewed software interrupt byte mismatch")
                counts["explicit_guest_interrupt"] += 1
                # This is an INT instruction, not UD2. The host deliberately
                # stops here until Windows gate/exception semantics are supplied.
                return [f"transformers_guest_interrupt({vector}u, 0x{insn.address:08X}u, "
                        f"0x{insn.address + insn.size:08X}u); /* reviewed INT boundary */"]
            return super()._lift_instruction(insn)

    return TargetLifter, counts


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pe_file", type=Path)
    parser.add_argument("--pcrecomp", type=Path, default=ROOT / "external" / "pcrecomp")
    parser.add_argument("--profile", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--functions-json", type=Path, required=True)
    parser.add_argument("--pe-json", type=Path, required=True)
    parser.add_argument("--stubs", type=Path, required=True)
    parser.add_argument("--cache", type=Path)
    parser.add_argument("--discover-only", action="store_true")
    parser.add_argument("--split", type=int, default=300)
    args = parser.parse_args()
    checkout = args.pcrecomp.resolve()
    for child in ("pe", "disasm", "lift"):
        sys.path.insert(0, str(checkout / "tools" / child))
    import translator
    from disasm32 import Disassembler
    from pe_analyze import export_json

    info, blob, iat = translator.load_pe(str(args.pe_file))
    profile = load_profile(args.profile, blob, info.image_base, info.sections)
    pin = json.loads((ROOT / "config" / "toolchain.json").read_text(encoding="utf-8"))["pcrecomp_commit"]
    identity = {"input_sha256": hashlib.sha256(blob).hexdigest(), "pcrecomp_commit": pin,
                "code_profile_sha256": profile_digest(profile), "discovery_revision": 1}
    reviewed = reviewed_disassembler(Disassembler, profile)
    disassembler = reviewed(blob, info.image_base, info.sections)
    export_json(info, str(args.pe_json))
    if args.cache and args.cache.exists():
        print(f"[*] Loading validated discovery cache: {args.cache}", flush=True)
        functions = load_catalog(args.cache, disassembler, identity)
    else:
        seeds = [info.image_base + info.entry_point_rva]
        seeds += [info.image_base + e.rva for e in getattr(info, "exports", []) if e.rva]
        functions = disassembler.find_functions(info.code_start, info.code_end, iat, seeds=seeds)
        if args.cache:
            save_catalog(args.cache, functions, identity)
            print(f"[*] Saved discovery cache: {args.cache}", flush=True)
    for f in functions.values():
        for b in f.blocks.values():
            for i in b.instructions:
                if any(int(r["start"], 0) < i.address + i.size and i.address < int(r["end"], 0)
                       for r in profile.get("excluded_code_ranges", [])):
                    raise ValueError(f"Data decoded as code at {i.address:#x}")
    translator.generate_function_list(functions, str(args.functions_json))
    translator.generate_import_stubs(iat, str(args.stubs))
    if args.discover_only:
        return 0
    lifter, counts = target_lifter(translator.Lifter, profile, functions)
    translator.Lifter = lifter
    translator.generate_recomp_files(functions, iat, str(args.output), split_size=args.split)
    for source in args.output.glob("recomp_*.c"):
        text = source.read_text(encoding="utf-8")
        text = text.replace('#include "recomp_types.h"', '#include "recomp_types.h"\n'
                            '#include "transformers_x87.h"\n#include "transformers_runtime.h"', 1)
        source.write_text(text, encoding="utf-8")
    (args.output.parent / "target-lift.json").write_text(json.dumps({
        **identity, "schema_version": 1, "function_count": len(functions),
        "excluded_data_ranges": profile.get("excluded_code_ranges", []),
        "handled_boundaries": dict(counts),
        "limitations": ["INT boundaries terminate explicitly; Windows exception delivery is not implemented.",
                        "The upstream binary64 x87 arithmetic model remains an approximation."],
    }, indent=2) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
