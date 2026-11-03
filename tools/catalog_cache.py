"""Portable discovery cache: metadata and byte ranges, never pickled objects."""

import gzip
import hashlib
import json
from pathlib import Path


def save_catalog(path: Path, functions: dict, identity: dict) -> None:
    records = []
    for f in sorted(functions.values(), key=lambda f: f.address):
        record = {
            "address": f.address, "end": f.end, "name": f.name, "size": f.size,
            "calls_to": sorted(f.calls_to), "called_from": sorted(f.called_from),
            "jump_targets": sorted(f.jump_targets), "is_thunk": f.is_thunk,
            "entry_kind": f.entry_kind, "blocks": [],
        }
        for b in sorted(f.blocks.values(), key=lambda b: b.start):
            record["blocks"].append({
                "start": b.start, "end": b.end, "successors": list(b.successors),
                "is_exit": b.is_exit,
                "instructions": [[i.address, i.size] for i in b.instructions],
            })
        records.append(record)
    with gzip.open(path, "wt", encoding="utf-8") as output:
        json.dump({"schema_version": 1, "identity": identity, "functions": records}, output,
                  separators=(",", ":"))


def load_catalog(path: Path, disassembler, identity: dict) -> dict:
    # These are locally generated caches, still fully checked against immutable
    # input bytes and instruction lengths. They contain no executable Python.
    from disasm32 import BasicBlock, Function, Instruction
    with gzip.open(path, "rt", encoding="utf-8") as source:
        document = json.load(source)
    if document.get("schema_version") != 1 or document.get("identity") != identity:
        raise ValueError("Discovery cache does not match input, tool revision and code regions")
    functions = {}
    for r in document["functions"]:
        f = Function(address=r["address"], end=r["end"], name=r["name"], size=r["size"],
                     calls_to=set(r["calls_to"]), called_from=set(r["called_from"]),
                     jump_targets=set(r["jump_targets"]), is_thunk=r["is_thunk"],
                     entry_kind=r["entry_kind"])
        if not disassembler.is_code_address(f.address):
            raise ValueError("Cached function starts outside an admitted code region")
        for br in r["blocks"]:
            b = BasicBlock(start=br["start"], end=br["end"],
                           successors=br["successors"], is_exit=br["is_exit"])
            for va, size in br["instructions"]:
                if not 1 <= size <= 15 or not disassembler.is_code_address(va):
                    raise ValueError("Invalid cached instruction range")
                data = disassembler.read_bytes(va, size)
                decoded = list(disassembler.md.disasm(data, va, count=1))
                if len(decoded) != 1 or decoded[0].size != size:
                    raise ValueError(f"Cached instruction decode mismatch at {va:#x}")
                i = decoded[0]
                b.instructions.append(Instruction(i.address, i.size, i.mnemonic, i.op_str,
                                                  bytes(i.bytes), list(i.operands)))
            f.blocks[b.start] = b
        functions[f.address] = f
    return functions


def profile_digest(profile: dict) -> str:
    return hashlib.sha256(json.dumps(profile, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
