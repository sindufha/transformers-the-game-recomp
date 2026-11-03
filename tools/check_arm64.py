"""Check generated C with Android Clang without linking or running a game."""

from concurrent.futures import ThreadPoolExecutor
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys

from preflight import ROOT
from build_probe import prepare_runtime


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lift-dir", type=Path, required=True)
    parser.add_argument("--ndk", type=Path)
    parser.add_argument("--api", type=int, default=29)
    parser.add_argument("--jobs", type=int, default=2, choices=range(1, 5))
    args = parser.parse_args()
    try:
        lift_dir = args.lift_dir.resolve()
        manifest = json.loads((lift_dir / "lift-summary.json").read_text(encoding="utf-8"))
        if args.ndk:
            ndk = args.ndk.resolve()
        else:
            sdk = os.environ.get("ANDROID_HOME") or os.environ.get("ANDROID_SDK_ROOT")
            if not sdk:
                raise ValueError("Set ANDROID_HOME or pass --ndk")
            candidates = [p for p in (Path(sdk) / "ndk").iterdir() if p.is_dir()]
            ndk = max(candidates, key=lambda p: tuple(int(v) for v in p.name.split(".")))
        host_tag = "windows-x86_64" if os.name == "nt" else ("darwin-x86_64" if sys.platform == "darwin" else "linux-x86_64")
        compiler = ndk / "toolchains" / "llvm" / "prebuilt" / host_tag / "bin" / ("clang.exe" if os.name == "nt" else "clang")
        if not compiler.is_file():
            raise ValueError(f"Android Clang not found: {compiler}")
        version = subprocess.check_output([str(compiler), "--version"], text=True).splitlines()[0]
        sources = sorted((lift_dir / "gen").glob("recomp_*.c"))
        if not sources:
            raise ValueError("No generated C sources")
        logs = lift_dir / "arm64-syntax"
        logs.mkdir(exist_ok=True)
        prepared = prepare_runtime(lift_dir, lift_dir / "arm64-syntax-runtime")

        def check(source: Path) -> dict:
            command = [
                str(compiler), f"--target=aarch64-linux-android{args.api}",
                "-I", str(prepared["include_dir"]), "-I", str(ROOT / "runtime"),
                "-include", str(lift_dir / "gen" / "recomp_funcs.h"),
                "-std=c11", "-fsyntax-only", "-fno-strict-aliasing", "-fwrapv", "-ffp-contract=off",
                str(source),
            ]
            log_path = logs / (source.stem + ".log")
            with log_path.open("w", encoding="utf-8") as log:
                result = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
            print(f"{'PASS' if result.returncode == 0 else 'FAIL'} {source.name}", flush=True)
            return {"source": source.name, "exit_code": result.returncode, "log": str(log_path)}

        with ThreadPoolExecutor(max_workers=args.jobs) as pool:
            results = list(pool.map(check, sources))
        passed = all(item["exit_code"] == 0 for item in results)
        summary = {
            "status": "syntax_passed" if passed else "syntax_failed",
            "input_sha256": manifest["input_sha256"], "compiler": version,
            "target": f"aarch64-linux-android{args.api}", "sources": results,
            "lift_status": manifest["status"], "lift_diagnostic_count": manifest["diagnostic_count"],
            "linked": False, "executed": False, "playable": False,
            "limitations": "Syntax checks can pass despite unsupported-instruction comments, incorrect semantics, missing functions or missing runtime APIs.",
        }
        (lift_dir / "arm64-syntax-summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        print(f"{summary['status']}: {len(results)} source files; no linking or execution")
        return 0 if passed else 1
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
