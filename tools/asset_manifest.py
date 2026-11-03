"""Inventory only game data; never copy assets or execute the game."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re

from pe_image import PEImage
from preflight import ROOT


def asset_path(name: str) -> PurePosixPath:
    parts = name.split("/")
    if (not name or "\\" in name or any(p in ("", ".", "..") or ":" in p for p in parts)
            or any(ord(c) < 32 for c in name)):
        raise ValueError(f"Invalid asset path: {name!r}")
    path = PurePosixPath(name)
    allowed = name in ("Transformers_Hi.DAT", "Transformers_Lo.DAT")
    allowed |= len(parts) > 1 and parts[0] == "audio" and path.suffix.lower() == ".wav"
    allowed |= len(parts) > 1 and parts[0] == "fmv" and path.suffix.lower() == ".bik"
    if not allowed:
        raise ValueError(f"Not an admitted game data file: {name!r}")
    return path


def validate_manifest(data: dict, input_sha: str) -> dict:
    if data.get("version") != 1 or data.get("input_sha256") != input_sha:
        raise ValueError("Asset manifest does not match this executable/build")
    files = data.get("files", [])
    if not isinstance(files, list) or not 2 <= len(files) <= 20000:
        raise ValueError("Invalid asset manifest count")
    seen, total = set(), 0
    for item in files:
        name = str(asset_path(item["path"]))
        if name.casefold() in seen:
            raise ValueError(f"Duplicate asset path: {name}")
        seen.add(name.casefold())
        size = item["bytes"]
        if type(size) is not int or not 0 <= size <= 16 * 1024**3:
            raise ValueError("Invalid asset byte count")
        if not re.fullmatch(r"[0-9a-f]{64}", item["sha256"]):
            raise ValueError("Invalid asset digest")
        total += size
    if not {"transformers_hi.dat", "transformers_lo.dat"} <= seen:
        raise ValueError("Both DAT files are required")
    if total != data.get("total_bytes"):
        raise ValueError("Asset manifest total differs from file sizes")
    return data


def create_manifest(game: Path) -> dict:
    game = game.resolve()
    image = PEImage.from_path(game / "Transformers.exe")
    if image.report()["blockers"]:
        raise ValueError("Executable failed preflight")
    files = [game / name for name in ("Transformers_Hi.DAT", "Transformers_Lo.DAT")]
    files += sorted(p for p in (game / "audio").rglob("*") if p.is_file() and p.suffix.lower() == ".wav")
    files += sorted(p for p in (game / "fmv").rglob("*") if p.is_file() and p.suffix.lower() == ".bik")
    items = []
    for path in files:
        path.resolve().relative_to(game)  # Reject links escaping the selected game directory.
        name = path.relative_to(game).as_posix()
        asset_path(name)
        before = path.stat()
        digest = hashlib.sha256()
        count = 0
        with path.open("rb") as stream:
            while block := stream.read(1024 * 1024):
                digest.update(block)
                count += len(block)
        after = path.stat()
        if count != before.st_size or (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
            raise ValueError(f"Asset changed during hashing: {name}")
        items.append({"path": name, "bytes": count, "sha256": digest.hexdigest()})
    data = {"version": 1, "input_sha256": hashlib.sha256(image.data).hexdigest(),
            "total_bytes": sum(item["bytes"] for item in items), "files": items}
    return validate_manifest(data, data["input_sha256"])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--game-dir", type=Path, default=ROOT.parent)
    parser.add_argument("--out", type=Path, default=ROOT / "work/game-assets.json")
    args = parser.parse_args()
    data = create_manifest(args.game_dir)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(data, indent=2) + "\n", encoding="utf-8")
    print(f"Inventoried {len(data['files'])} files, {data['total_bytes']} bytes; no data copied")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
