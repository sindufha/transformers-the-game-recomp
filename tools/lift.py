"""Preflight, freeze input, invoke pinned pcrecomp, and reject incomplete lifts."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys

from pe_image import PEError, PEImage
from preflight import ROOT, inspect_game, write_report

FAILURE_MARKERS = ("ERROR lifting", "/* FAILED */", "UNIMPLEMENTED:", "RECOMP_TODO")


def scan_generated(directory: Path) -> list[dict]:
    failures = []
    for source in sorted(directory.glob("*.c")):
        function = None
        with source.open(encoding="utf-8") as stream:
            for number, line in enumerate(stream, 1):
                match = re.match(r"void\s+(\w+)\(void\)", line) or re.search(r"ERROR lifting (\w+)", line)
                if match:
                    function = match.group(1)
                if any(marker in line for marker in FAILURE_MARKERS):
                    failures.append({"file": source.name, "line": number,
                                     "function": function, "diagnostic": line.strip()})
    return failures


def git_output(checkout: Path, *args: str) -> str:
    return subprocess.check_output(
        ["git", "-C", str(checkout), *args], text=True, stderr=subprocess.STDOUT
    ).strip()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--exe", type=Path, default=ROOT.parent / "Transformers.exe")
    parser.add_argument("--game-dir", type=Path, default=ROOT.parent)
    parser.add_argument("--pcrecomp", type=Path, default=ROOT / "external" / "pcrecomp")
    parser.add_argument("--out", type=Path, help="New output directory; existing directories are refused")
    parser.add_argument("--generic", action="store_true", help="Use unextended upstream pipeline")
    args = parser.parse_args()
    try:
        report = inspect_game(args.exe.resolve(), args.game_dir.resolve())
        write_report(report, ROOT / "work")
        if report["blockers"]:
            for blocker in report["blockers"]:
                print(f"BLOCKED: {blocker}", file=sys.stderr)
            return 2
        checkout = args.pcrecomp.resolve()
        pin = json.loads((ROOT / "config" / "toolchain.json").read_text(encoding="utf-8"))["pcrecomp_commit"]
        if git_output(checkout, "rev-parse", "HEAD") != pin:
            raise ValueError(f"pcrecomp must be checked out at {pin}")
        if git_output(checkout, "status", "--porcelain", "--untracked-files=no"):
            raise ValueError("Tracked pcrecomp files are modified; use the pinned clean checkout")
        if importlib.util.find_spec("capstone") is None:
            raise ValueError("Missing capstone; install requirements.txt")
        profile_path = ROOT / "config" / "code-regions.json"
        profile = json.loads(profile_path.read_text(encoding="utf-8")) if profile_path.exists() else {}
        use_target = not args.generic and profile.get("input_sha256") == report["sha256"]
        suffix = "-target" if use_target else ""
        output = (args.out or ROOT / "work" / f"lift-{report['sha256'][:12]}{suffix}").resolve()
        # Freeze the bytes used for analysis: the user's executable may change during work.
        blob = args.exe.read_bytes()
        if hashlib.sha256(blob).hexdigest() != report["sha256"]:
            raise ValueError("Executable changed during inspection; rerun the command")
        output.mkdir(parents=True, exist_ok=False)
        snapshot = output / "input.exe"
        snapshot.write_bytes(blob)
        (output / "input-preflight.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
        command = [
            sys.executable, "-u", "-m", "tools", str(snapshot), "--all",
            "--output", str(output / "gen"),
            "--functions-json", str(output / "functions.json"),
            "--pe-json", str(output / "pe.json"),
            "--stubs", str(output / "import_stubs.c"),
        ]
        if use_target:
            from catalog_cache import profile_digest
            cache = ROOT / "work" / f"discovery-{report['sha256'][:12]}-{profile_digest(profile)[:12]}.json.gz"
            command = [
                sys.executable, "-u", str(ROOT / "tools" / "run_lift.py"), str(snapshot),
                "--pcrecomp", str(checkout), "--profile", str(profile_path), "--cache", str(cache),
                "--output", str(output / "gen"), "--functions-json", str(output / "functions.json"),
                "--pe-json", str(output / "pe.json"), "--stubs", str(output / "import_stubs.c"),
            ]
        print(f"Input: {report['sha256']}", flush=True)
        print(f"Output: {output}", flush=True)
        with (output / "lift.log").open("w", encoding="utf-8") as log:
            completed = subprocess.run(command, cwd=checkout, stdout=log, stderr=subprocess.STDOUT)
        failures = scan_generated(output / "gen")
        catalog_path = output / "functions.json"
        catalog = json.loads(catalog_path.read_text(encoding="utf-8")) if catalog_path.exists() else []
        if not isinstance(catalog, list):
            raise ValueError("Unexpected functions.json schema from pinned pcrecomp")
        generated = list((output / "gen").glob("recomp_*.c"))
        addresses = {item["address_int"] for item in catalog}
        entry_present = int(report["entry_va"], 16) in addresses
        complete = completed.returncode == 0 and not failures and bool(catalog) and len(generated) >= 2 and entry_present
        summary = {
            "schema_version": 1, "status": "code_generated" if complete else "lift_incomplete",
            "input_sha256": report["sha256"], "pcrecomp_commit": pin,
            "lifter_exit_code": completed.returncode, "function_count": len(catalog),
            "entry_in_catalog": entry_present,
            "generated_c_files": len(generated), "diagnostic_count": len(failures),
            "diagnostics": failures,
            "runtime_status": "not_implemented", "playable": False,
            "target_profile": str(profile_path) if use_target else None,
            "explicit_interrupt_boundaries": len(profile.get("trap_sites", [])) if use_target else 0,
            "limitations": [
                "Function discovery is heuristic; passing this check does not prove all functions were recovered.",
                "Win32, Direct3D, audio, input, guest memory, exceptions and threads still require a host runtime.",
                "Generated import stubs are diagnostics, not implementations of Windows APIs.",
                "No game executable was launched by this command.",
            ],
        }
        (output / "lift-summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        print(f"{summary['status'].upper()}: {len(catalog)} functions, {len(failures)} diagnostics")
        print(f"Details: {output / 'lift-summary.json'}")
        return 0 if complete else 3
    except (OSError, ValueError, PEError, subprocess.CalledProcessError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
