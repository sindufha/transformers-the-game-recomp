"""Compile and archive the verified lift using Android Clang directly."""

from concurrent.futures import ThreadPoolExecutor
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

from build_probe import compile_one, find_compiler, prepare_runtime, response_argument, sha256, validate_lift
from preflight import ROOT
from android_targets import TARGETS, target_triple


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lift-dir", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--ndk", type=Path)
    parser.add_argument("--api", type=int, default=29)
    parser.add_argument("--jobs", type=int, choices=range(1, 5), default=2)
    parser.add_argument("--opt", choices=("0", "1", "2"), default="0")
    parser.add_argument("--abi", choices=TARGETS, default="arm64-v8a")
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    summary = {"status": "failed", "linked_executable": False, "executed": False, "playable": False}
    try:
        lift_dir = args.lift_dir.resolve()
        manifest, _ = validate_lift(lift_dir)
        runtime = prepare_runtime(lift_dir, out)
        compiler = find_compiler(args.ndk)
        version = subprocess.check_output([str(compiler), "--version"], text=True).splitlines()[0]
        target = target_triple(args.abi, args.api)
        includes = [runtime["include_dir"], ROOT / "runtime", lift_dir / "gen"]
        flags = [f"--target={target}", "-std=c11", f"-O{args.opt}", "-g0", "-fno-strict-aliasing",
                 "-fwrapv", "-ffp-contract=off", "-fno-omit-frame-pointer"]
        if args.abi == "armeabi-v7a":
            flags += ["-fmax-type-align=1"]
        for include in includes:
            flags += ["-I", str(include)]
        flags += ["-include", str(lift_dir / "gen/recomp_funcs.h")]
        header_hash = sha256(runtime["include_dir"] / "recomp_types.h")
        overlays = {name: sha256(ROOT / "runtime" / name)
                    for name in ("transformers_x87.h", "transformers_runtime.h")}
        dependencies = {"strict_header": header_hash, "runtime_overlays": overlays,
                        "declarations": sha256(lift_dir / "gen/recomp_funcs.h"), "flags": flags,
                        "compiler": version}
        sources = sorted((lift_dir / "gen").glob("recomp_*.c"))
        objects = [out / (source.stem + ".o") for source in sources]
        summary.update(input_sha256=manifest["input_sha256"], target=target, abi=args.abi,
                       strict_header_sha256=header_hash, runtime_overlay_sha256=overlays,
                       compiler=version, optimization=args.opt)

        def compile_source(item):
            source, obj = item
            fingerprint = hashlib.sha256(json.dumps({**dependencies, "source": sha256(source)},
                                                    sort_keys=True).encode()).hexdigest()
            record_path = obj.with_suffix(".json")
            if obj.exists() and record_path.exists():
                record = json.loads(record_path.read_text(encoding="utf-8"))
                if record.get("fingerprint") == fingerprint and record.get("object_sha256") == sha256(obj):
                    print(f"CACHED {source.name}", flush=True)
                    return record
            result = compile_one(compiler, flags + ["-c", str(source), "-o", str(obj)],
                                 out / (source.stem + ".rsp"))
            result.update(source=str(source), fingerprint=fingerprint)
            if result["exit_code"] == 0:
                result["object_sha256"] = sha256(obj)
                record_path.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
            print(f"{'PASS' if result['exit_code'] == 0 else 'FAIL'} {source.name}", flush=True)
            return result

        with ThreadPoolExecutor(max_workers=args.jobs) as pool:
            results = list(pool.map(compile_source, zip(sources, objects)))
        summary["compilation"] = results
        if any(r["exit_code"] for r in results):
            raise ValueError("ARM64 compilation failed; inspect per-source logs")
        archive = out / "libtransformers_lift.a"
        response = out / "archive.rsp"
        response.write_text("\n".join(response_argument(str(obj)) for obj in objects) + "\n", encoding="utf-8")
        ar = compiler.parent / ("llvm-ar.exe" if compiler.suffix == ".exe" else "llvm-ar")
        # Remove an earlier archive so renamed chunks cannot leave stale members.
        archive.unlink(missing_ok=True)
        subprocess.run([str(ar), "rcs", str(archive), "@" + str(response)], check=True, timeout=300)
        summary.update(status="native_archive_built", archive=str(archive), archive_sha256=sha256(archive),
                       archive_bytes=archive.stat().st_size, object_count=len(objects))
        provenance = {key: summary[key] for key in ("input_sha256", "target", "strict_header_sha256",
                                                   "runtime_overlay_sha256", "archive_sha256", "compiler", "optimization")}
        archive.with_name(archive.name + ".provenance.json").write_text(json.dumps(provenance, indent=2) + "\n", encoding="utf-8")
        print(f"BUILT {archive}: {archive.stat().st_size} bytes; runtime symbols unresolved until executable link")
        return 0
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as exc:
        summary["error"] = str(exc)
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    finally:
        (out / "arm64-build-summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
        (out / "native-build-summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    raise SystemExit(main())
