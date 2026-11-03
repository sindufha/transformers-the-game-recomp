"""Audit the local game and write a reproducible input-readiness report."""

from __future__ import annotations

import argparse
from collections import Counter
import json
from pathlib import Path
import sys

from pe_image import PEError, PEImage

ROOT = Path(__file__).resolve().parents[1]


def inspect_game(exe: Path, game_dir: Path) -> dict:
    report = {"schema_version": 1, "executable": str(exe.resolve()), **PEImage.from_path(exe).report()}
    assets = []
    for name in ("Transformers_Hi.DAT", "Transformers_Lo.DAT", "binkw32.dll"):
        path = game_dir / name
        item = {"path": name, "present": path.is_file()}
        if path.is_file():
            with path.open("rb") as source:
                item.update(size=path.stat().st_size, header_hex=source.read(32).hex())
        assets.append(item)
    report["assets"] = assets
    report["asset_directories"] = []
    for name in ("audio", "fmv"):
        directory = game_dir / name
        files = [p for p in directory.rglob("*") if p.is_file()] if directory.is_dir() else []
        report["asset_directories"].append({
            "path": name, "present": directory.is_dir(), "file_count": len(files),
            "total_bytes": sum(p.stat().st_size for p in files),
            "extensions": dict(sorted(Counter(p.suffix.lower() for p in files).items())),
        })
    report["asset_limitations"] = "DAT/FMVs are inventoried only; their formats and contents were not decoded."
    return report


def write_report(report: dict, output: Path) -> None:
    output.mkdir(parents=True, exist_ok=True)
    (output / "preflight.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    lines = [
        "# Transformers input audit", "", f"Status: **{report['status']}**", "",
        f"Executable: `{report['executable']}`", f"SHA-256: `{report['sha256']}`",
        f"Format: {report['format']}, machine {report['machine']}",
        f"Entry: `{report['entry_va']}` in `{report['entry_section']}`",
        f"On-disk imports: {report['import_count']}", "",
        "## Blockers", "",
    ]
    lines += [f"- {item}" for item in report["blockers"]] or ["None detected by preflight."]
    lines += ["", "## Packing signals (heuristics)", ""]
    lines += [f"- {item}" for item in report["packing_signals"]] or ["None detected."]
    lines += ["", "## Sections", "", "| Section | VA | Raw bytes | Entropy | W | X |", "|---|---|---:|---:|---|---|"]
    for s in report["sections"]:
        lines.append(f"| {s['name'].strip() or '(unnamed)'} | {s['va']} | {s['raw_size']} | {s['entropy']:.6f} | {s['writable']} | {s['executable']} |")
    lines += ["", "## On-disk imports", ""]
    for dll in report["imports"]:
        names = [v.get("name", "#" + str(v.get("ordinal", "?"))) for v in dll["symbols"]]
        lines.append(f"- `{dll['dll']}`: " + ", ".join(f"`{v}`" for v in names))
    lines += ["", "## Limitations", ""] + [f"- {v}" for v in report["limitations"]]
    lines += [f"- {report['asset_limitations']}", ""]
    (output / "preflight.md").write_text("\n".join(lines), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, default=ROOT.parent / "Transformers.exe")
    parser.add_argument("--game-dir", type=Path, default=ROOT.parent)
    parser.add_argument("--out", type=Path, default=ROOT / "work")
    args = parser.parse_args()
    try:
        report = inspect_game(args.exe, args.game_dir)
        write_report(report, args.out)
    except (OSError, PEError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    print(f"{report['status'].upper()}: {args.exe.name}")
    print(f"Report: {(args.out / 'preflight.md').resolve()}")
    for blocker in report["blockers"]:
        print(f"- {blocker}")
    return 2 if report["blockers"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
