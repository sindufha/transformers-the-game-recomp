"""Bounded, read-only PE inspection. No game execution or external packages."""

from __future__ import annotations

from collections import Counter
from dataclasses import dataclass
import hashlib
import math
from pathlib import Path
import struct


class PEError(ValueError):
    pass


@dataclass(frozen=True)
class Section:
    name: str
    rva: int
    virtual_size: int
    raw_offset: int
    raw_size: int
    flags: int
    entropy: float

    @property
    def executable(self) -> bool:
        return bool(self.flags & 0x20000000)

    @property
    def writable(self) -> bool:
        return bool(self.flags & 0x80000000)


class PEImage:
    MAX_BYTES = 128 * 1024 * 1024

    def __init__(self, data: bytes):
        self.data = data
        if len(data) > self.MAX_BYTES:
            raise PEError("PE exceeds the 128 MiB inspection limit")
        if self.read(0, 2) != b"MZ":
            raise PEError("Missing DOS MZ signature")
        pe = self.unpack("<I", 0x3C)[0]
        if self.read(pe, 4) != b"PE\0\0":
            raise PEError("Missing PE signature")
        self.machine, count, self.timestamp, _, _, opt_size, self.flags = self.unpack(
            "<HHIIIHH", pe + 4
        )
        if not 1 <= count <= 96:
            raise PEError("Invalid PE section count")
        opt = pe + 24
        self.read(opt, opt_size)
        self.magic = self.unpack("<H", opt)[0]
        if self.magic == 0x10B:
            min_opt, directory_offset, count_offset = 96, 96, 92
            self.pointer_size = 4
            self.image_base = self.unpack("<I", opt + 28)[0]
        elif self.magic == 0x20B:
            min_opt, directory_offset, count_offset = 112, 112, 108
            self.pointer_size = 8
            self.image_base = self.unpack("<Q", opt + 24)[0]
        else:
            raise PEError(f"Unsupported optional-header magic: {self.magic:#x}")
        if opt_size < min_opt:
            raise PEError("Truncated PE optional header")
        self.entry_rva = self.unpack("<I", opt + 16)[0]
        self.image_size = self.unpack("<I", opt + 56)[0]
        self.headers_size = self.unpack("<I", opt + 60)[0]
        if self.headers_size > len(data) or self.headers_size > self.image_size:
            raise PEError("Invalid SizeOfHeaders")
        self.subsystem = self.unpack("<H", opt + 68)[0]
        dir_count = self.unpack("<I", opt + count_offset)[0]
        if dir_count > (opt_size - directory_offset) // 8:
            raise PEError("Data directories extend beyond optional header")
        self.directories = [
            self.unpack("<II", opt + directory_offset + i * 8)
            for i in range(min(dir_count, 16))
        ]
        self.directories += [(0, 0)] * (16 - len(self.directories))
        self.sections = []
        self.layout_issues = []
        table = opt + opt_size
        self.read(table, count * 40)
        for i in range(count):
            name, vs, va, rs, ro, _, _, _, _, flags = self.unpack(
                "<8sIIIIIIHHI", table + i * 40
            )
            raw = self.read(ro, rs) if rs else b""
            entropy = -sum(
                (n / len(raw)) * math.log2(n / len(raw))
                for n in Counter(raw).values()
            ) if raw else 0.0
            if va + (vs or rs) > self.image_size:
                self.layout_issues.append(
                    f"Section {name.rstrip(bytes([0])).decode('ascii', 'replace')!r} extends beyond SizeOfImage"
                )
            self.sections.append(Section(
                name.rstrip(b"\0").decode("ascii", "replace"),
                va, vs, ro, rs, flags, entropy,
            ))
        spans = sorted(
            (s.rva, s.rva + max(s.virtual_size, s.raw_size))
            for s in self.sections if max(s.virtual_size, s.raw_size)
        )
        if any(right[0] < left[1] for left, right in zip(spans, spans[1:])):
            raise PEError("Overlapping section virtual ranges")

    @classmethod
    def from_path(cls, path: Path) -> PEImage:
        if path.stat().st_size > cls.MAX_BYTES:
            raise PEError("PE exceeds the 128 MiB inspection limit")
        return cls(path.read_bytes())

    def read(self, offset: int, size: int) -> bytes:
        if offset < 0 or size < 0 or offset + size > len(self.data):
            raise PEError(f"Read outside file at {offset:#x}, size {size:#x}")
        return self.data[offset:offset + size]

    def unpack(self, fmt: str, offset: int) -> tuple:
        return struct.unpack(fmt, self.read(offset, struct.calcsize(fmt)))

    def section_at(self, rva: int) -> Section | None:
        return next((s for s in self.sections
                     if s.rva <= rva < s.rva + max(s.virtual_size, s.raw_size)), None)

    def offset(self, rva: int, size: int = 1) -> int:
        if rva < 0 or size < 1:
            raise PEError("Invalid RVA or read size")
        if rva + size <= self.headers_size:
            self.read(rva, size)
            return rva
        section = self.section_at(rva)
        if section is None or rva + size > section.rva + section.raw_size:
            raise PEError(f"RVA {rva:#x} has no complete file-backed range")
        offset = section.raw_offset + rva - section.rva
        self.read(offset, size)
        return offset

    def cstring(self, rva: int) -> str:
        start = self.offset(rva)
        section = self.section_at(rva)
        bound = (section.raw_offset + section.raw_size) if section else self.headers_size
        end = self.data.find(b"\0", start, min(bound, start + 4096))
        if end < 0:
            raise PEError(f"Unterminated string at RVA {rva:#x}")
        try:
            return self.data[start:end].decode("ascii")
        except UnicodeDecodeError as exc:
            raise PEError(f"Non-ASCII import string at RVA {rva:#x}") from exc

    def imports(self) -> list[dict]:
        rva, size = self.directories[1]
        if not rva and not size:
            return []
        if not rva or size < 20:
            raise PEError("Invalid import directory")
        result = []
        for i in range(min(size // 20, 4096)):
            descriptor = self.unpack("<5I", self.offset(rva + i * 20, 20))
            if not any(descriptor):
                return result
            oft, _, _, name_rva, first_thunk = descriptor
            dll = self.cstring(name_rva)
            if not first_thunk:
                raise PEError("Import descriptor has no IAT")
            symbols = []
            for j in range(65536):
                thunk_rva = (oft or first_thunk) + j * self.pointer_size
                thunk = self.unpack(
                    "<I" if self.pointer_size == 4 else "<Q",
                    self.offset(thunk_rva, self.pointer_size),
                )[0]
                if not thunk:
                    break
                ordinal_mask = 1 << (self.pointer_size * 8 - 1)
                if thunk & ordinal_mask:
                    symbol = {"ordinal": thunk & 0xFFFF}
                else:
                    self.offset(thunk, 2)
                    symbol = {"name": self.cstring(thunk + 2)}
                symbol["iat_va"] = f"0x{self.image_base + first_thunk + j * self.pointer_size:08x}"
                symbols.append(symbol)
            else:
                raise PEError("Import thunk limit exceeded or missing terminator")
            result.append({"dll": dll, "symbols": symbols})
        raise PEError("Missing import descriptor terminator")

    def report(self) -> dict:
        entry = self.section_at(self.entry_rva)
        text = next((s for s in self.sections if s.name == ".text"), None)
        issues, signals = list(self.layout_issues), []
        if self.machine != 0x14C or self.magic != 0x10B:
            issues.append("This pipeline requires an x86 PE32 executable")
        if self.flags & 0x2000:
            issues.append("Input is a DLL, not a game executable")
        if entry is None or not entry.executable:
            issues.append("Entry point is outside an executable section")
        else:
            try:
                self.offset(self.entry_rva)
            except PEError:
                issues.append("Entry point is not file-backed")
        if text and text.entropy >= 7.6:
            signals.append("Main .text section has near-random byte entropy")
        if text and entry and text is not entry:
            signals.append("Entry point is outside the main .text section")
        if entry and entry.writable and entry.executable:
            signals.append("Entry section is both writable and executable")
        imports_error = None
        try:
            imports = self.imports()
        except PEError as exc:
            imports, imports_error = [], str(exc)
            issues.append(f"Import table cannot be inspected: {exc}")
        count = sum(len(item["symbols"]) for item in imports)
        if len(imports) >= 8 and count <= len(imports) * 2:
            signals.append("Import table has unusually few symbols per DLL")
        suspected = bool(text and text.entropy >= 7.6 and len(signals) >= 2)
        if suspected:
            issues.append("Suspected packed/encrypted input; obtain a validated unpacked image before lifting")
        if not text:
            issues.append("No .text section; code ranges need manual review")
        if not imports and not imports_error:
            issues.append("No import table; dynamic API resolution needs manual review")
        if self.directories[14][0]:
            issues.append("Managed CLR image is not supported by this pipeline")
        end = max([self.headers_size] + [s.raw_offset + s.raw_size for s in self.sections])
        return {
            "sha256": hashlib.sha256(self.data).hexdigest(),
            "file_size": len(self.data),
            "machine": f"0x{self.machine:04x}",
            "format": "PE32" if self.magic == 0x10B else "PE32+",
            "image_base": f"0x{self.image_base:08x}",
            "entry_va": f"0x{self.image_base + self.entry_rva:08x}",
            "entry_section": entry.name if entry else None,
            "image_size": self.image_size,
            "overlay_size": len(self.data) - end,
            "relocation_directory": {"rva": self.directories[5][0], "size": self.directories[5][1]},
            "tls_directory": {"rva": self.directories[9][0], "size": self.directories[9][1]},
            "sections": [{
                "name": s.name, "va": f"0x{self.image_base + s.rva:08x}",
                "virtual_size": s.virtual_size, "raw_offset": s.raw_offset,
                "raw_size": s.raw_size, "entropy": round(s.entropy, 6),
                "executable": s.executable, "writable": s.writable,
            } for s in self.sections],
            "import_count": count,
            "imports": imports,
            "packing_signals": signals,
            "suspected_packed": suspected,
            "blockers": issues,
            "status": "blocked" if issues else "candidate_for_lifting",
            "limitations": [
                "Entropy and section layout are heuristics, not identification of a protection vendor.",
                "Passing preflight does not prove complete code recovery or Android compatibility.",
                "Imports describe the on-disk image only; dynamically resolved APIs may be missing.",
                "No executable was launched, unpacked, decrypted, or modified.",
            ],
        }
