"""Link an Android ARM64 diagnostic CLI; never execute a game or deploy a device."""
from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

from pe_image import PEImage
from preflight import ROOT
from lift import scan_generated
from android_targets import TARGETS, target_triple, validate_elf

PIN = "8c92919a99aaf9e129af5787776bca84863468cc"
PINNED_HASHES = {
    "runtime/win32hle/kernel32.c": "8f486f625ff595ff7d3ba4b8ffa4e1c92905ed7b2c5944d546325faffad8f959",
    "runtime/win32hle/kernel32_crt.c": "7e43e9cfdf9fe96f52bff23e5cb8cfd60cf6154eaad66291f48a0164088e0294",
    "runtime/win32hle/kernel32_ext.c": "d365f98b75a9034f19ab8f554caaeb4ffa54e8c4ee7c22e56f06d6c3f03d352a",
    "runtime/win32hle/module.c": "dc2e5b874a9d5957b07739590dd3ebd1a6caf71c9320a22fac22a1990af25ea8",
    "runtime/win32hle/hle_path.c": "1ab1e3ee0b7f64a07b53c1d781767f43153d2ffe0f579a8219799e80814bb043",
    "runtime/win32hle/host_lite.c": "d19256f00c251061aceff394bb24164774bba0b8a6fd62cc22933554c1cb73cc",
    "runtime/recomp32/recomp_types.h": "1ab5b031e600cbfbc97649e3958b45014de99e812addb165728fdf2c90508cf9",
    "runtime/win32hle/guest_mem.c": "50b2ed727184dcab7267d0c7c02b18f58f1aac4116a6d83d46fc62ad11aedbe3",
    "runtime/win32hle/pe_loader.c": "2fc940272028ab3576ef68bb647ac3124c68345f7efd56adeccc950fb59bf679",
    "runtime/win32hle/win32hle.h": "c4a8b9624cf46b302993e3109551fb0ef99d477ebb14ca4b6e0d7c9d409dfbbb",
    "runtime/win32hle/pe_loader.h": "63be48620966ddb9102b9e77f1ba3e8844dc16b164b9da15552edd5e10cc375b",
    "runtime/win32hle/pe_format.h": "81b7a434e2c107ccaea6e1befd6b957f63a997672de2007e44ca034b494963eb",
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def replace_once(text: str, old: str, new: str) -> str:
    if text.count(old) != 1:
        raise ValueError(f"Pinned runtime splice no longer matches: {old[:72]!r}")
    return text.replace(old, new, 1)


def prepare_runtime(lift_dir: Path, out: Path) -> dict:
    """Create a strict copied header and full-state host; original checkout is read-only.

    Use include_dir FIRST when compiling every generated C translation unit.
    This API does not compile or validate an incomplete lift.
    """
    lift_dir, out = lift_dir.resolve(), out.resolve()
    checkout = ROOT / "external" / "pcrecomp"
    for name, expected in PINNED_HASHES.items():
        # Git's Windows checkout can use CRLF. This check pins source content
        # independently from that setting; artifact hashes below stay raw.
        normalized = (checkout / name).read_bytes().replace(b"\r\n",b"\n")
        if hashlib.sha256(normalized).hexdigest() != expected:
            raise ValueError(f"Pinned runtime changed: {name}")
    include = out / "include"
    include.mkdir(parents=True, exist_ok=True)
    header = (checkout / "runtime/recomp32/recomp_types.h").read_text(encoding="utf-8")
    header = replace_once(header, "#include <math.h>", "#include <math.h>\n"
        "__attribute__((noreturn)) void tf_probe_missing_dispatch(const char *, uint32_t, uint32_t);\n"
        "__attribute__((noreturn)) void tf_probe_unimplemented(const char *);")
    header = replace_once(header,
        'fprintf(stderr, "ICALL: unresolved VA 0x%08X from 0x%08X\\n", _va, g_cur_func); \\\n        eax = 0;',
        'tf_probe_missing_dispatch("ICALL", _va, g_cur_func);')
    header = replace_once(header,
        'else { fprintf(stderr, "ITAIL: unresolved VA 0x%08X from 0x%08X\\n", _va, g_cur_func); }',
        'else { tf_probe_missing_dispatch("ITAIL", _va, g_cur_func); }')
    stub_at = header.index("#define STUB(name)")
    stub_end = header.index("\n\n#endif", stub_at)
    header = header[:stub_at] + '#define STUB(name) tf_probe_unimplemented(name)' + header[stub_end:]
    (include / "recomp_types.h").write_text(header, encoding="utf-8")
    for name in ("win32hle.h", "pe_loader.h", "pe_format.h"):
        shutil.copyfile(checkout / "runtime/win32hle" / name, include / name)

    host = (checkout / "runtime/win32hle/host_lite.c").read_text(encoding="utf-8")
    host = replace_once(host, '#include "win32hle.h"',
                        '#include "win32hle.h"\n#include "probe_support.h"\n#include "dispatch_cache.h"')
    lookup = host.index("recomp_func_t recomp_lookup(uint32_t va) {")
    lookup_end = host.index("\nrecomp_func_t recomp_lookup_manual", lookup)
    host = host[:lookup] + (
        "recomp_func_t recomp_lookup(uint32_t va) {\n"
        "    static TF_DispatchCache cache;\n"
        "    return tf_dispatch_lookup(va, recomp_dispatch_table, recomp_dispatch_count, &cache);\n"
        "}\n"
    ) + host[lookup_end:]
    start = host.index("typedef struct {\n    uint32_t eax, ecx, edx")
    end = host.index("/* A guest thread's stack", start)
    host = host[:start] + (
        "typedef tf_probe_state regs_t;\n"
        "static void regs_save(regs_t *r) { tf_probe_state_save(r); }\n"
        "static void regs_load(const regs_t *r) { tf_probe_state_load(r); }\n\n"
    ) + host[end:]
    host = replace_once(host, "g_ms.r.fpu_cw = 0x027F;",
        "g_ms.r.fpu_cw = 0x037F;\n            g_ms.r.mxcsr = 0x1F80;\n            g_ms.r.x87.tags = 0xFFFF;\n            g_ms.r.x87.limitations = TF_X87_LIMITS;")
    # Import lookup mutates current-import metadata. Snapshot first, so a
    # callback roundtrip restores the caller's full state.
    host = replace_once(host,
        "    recomp_func_t f = recomp_lookup(va);",
        "    regs_t saved; regs_save(&saved);\n    recomp_func_t f = recomp_lookup(va);")
    host = replace_once(host, "    regs_t saved; regs_save(&saved);\n    for (int i = nargs",
        "    for (int i = nargs")
    host_path = out / "host_lite.c"
    host_path.write_text(host, encoding="utf-8")
    return {"include_dir": include, "host_source": host_path,
            "runtime_dir": ROOT / "runtime", "pcrecomp_dir": checkout,
            "generated_dir": lift_dir / "gen"}


def validate_lift(lift_dir: Path) -> tuple[dict, PEImage]:
    manifest = json.loads((lift_dir / "lift-summary.json").read_text(encoding="utf-8"))
    if manifest.get("status") != "code_generated" or manifest.get("diagnostic_count") != 0:
        raise ValueError("Incomplete lift: inspect lift-summary.json before linking")
    if manifest.get("pcrecomp_commit") != PIN:
        raise ValueError("Lift does not use the pinned pcrecomp revision")
    snapshot = lift_dir / "input.exe"
    if sha256(snapshot) != manifest["input_sha256"]:
        raise ValueError("Frozen input hash does not match lift provenance")
    image = PEImage(snapshot.read_bytes())
    report = image.report()
    if report["blockers"] or image.machine != 0x14c or image.magic != 0x10b:
        raise ValueError("Frozen input failed PE32 preflight")
    if scan_generated(lift_dir / "gen"):
        raise ValueError("Generated sources contain diagnostic/failed instructions")
    dispatch = lift_dir / "gen/recomp_dispatch.c"
    entry = image.image_base + image.entry_rva
    if f"0x{entry:08X}u" not in dispatch.read_text(encoding="utf-8"):
        raise ValueError("Entry missing from generated dispatch")
    return manifest, image


def find_compiler(ndk: Path | None) -> Path:
    if ndk is None:
        sdk = os.environ.get("ANDROID_HOME") or os.environ.get("ANDROID_SDK_ROOT")
        if not sdk:
            raise ValueError("Pass --ndk or set ANDROID_HOME")
        candidates = [p for p in (Path(sdk) / "ndk").iterdir() if p.is_dir()]
        ndk = max(candidates, key=lambda p: tuple(int(v) for v in p.name.split(".")))
    tag = "windows-x86_64" if os.name == "nt" else ("darwin-x86_64" if sys.platform == "darwin" else "linux-x86_64")
    compiler = ndk / "toolchains/llvm/prebuilt" / tag / "bin" / ("clang.exe" if os.name == "nt" else "clang")
    if not compiler.is_file():
        raise ValueError(f"Android Clang missing: {compiler}")
    return compiler.resolve()


def validate_archive(core_archive: Path, input_hash: str, target: str,
                     header_hash: str, overlay_hashes: dict) -> dict:
    provenance = json.loads(core_archive.with_name(core_archive.name + ".provenance.json").read_text(encoding="utf-8"))
    if (provenance.get("input_sha256") != input_hash or
        provenance.get("strict_header_sha256") != header_hash or
        provenance.get("runtime_overlay_sha256") != overlay_hashes or
        provenance.get("target") != target or
        provenance.get("archive_sha256") != sha256(core_archive)):
        raise ValueError("Prebuilt archive provenance does not match this strict build")
    return provenance


def response_argument(arg: str) -> str:
    # LLVM response files use C-like quoting on every supported host. Forward
    # slashes avoid Windows backslashes being interpreted as escapes.
    return '"' + arg.replace("\\", "/").replace('"', '\\"') + '"'


def compile_one(compiler: Path, command: list[str], rsp: Path) -> dict:
    rsp.write_text("\n".join(map(response_argument, command)) + "\n", encoding="utf-8")
    log = rsp.with_suffix(".log")
    with log.open("w", encoding="utf-8") as stream:
        result = subprocess.run([str(compiler), "@" + str(rsp)], stdout=stream,
                                stderr=subprocess.STDOUT, timeout=600)
    return {"exit_code": result.returncode, "response_file": str(rsp), "log": str(log)}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lift-dir", type=Path, required=True)
    parser.add_argument("--ndk", type=Path)
    parser.add_argument("--out", "--output", dest="out", type=Path, required=True)
    parser.add_argument("--api", type=int, default=29)
    parser.add_argument("--jobs", type=int, choices=range(1, 5), default=2)
    parser.add_argument("--abi", choices=TARGETS, default="arm64-v8a")
    parser.add_argument("--startup", action="store_true", help="Link selected POSIX Win32 startup APIs")
    parser.add_argument("--core-archive", type=Path,
                        help="Prebuilt strict-header archive; requires sibling archive.provenance.json")
    args = parser.parse_args()
    summary = {"status": "failed", "linked": False, "executed": False, "playable": False}
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    summary_path = out / "probe-build-summary.json"
    try:
        lift_dir = args.lift_dir.resolve()
        manifest, image = validate_lift(lift_dir)
        prepared = prepare_runtime(lift_dir, out)
        compiler = find_compiler(args.ndk)
        target = target_triple(args.abi, args.api)
        entry = image.image_base + image.entry_rva
        include = prepared["include_dir"]
        (include / "probe_build.h").write_text(
            f'#define TF_PROBE_INPUT_SHA256 "{manifest["input_sha256"]}"\n'
            f'#define TF_PROBE_INPUT_SIZE {len(image.data)}ull\n'
            f'#define TF_PROBE_ENTRY 0x{entry:08X}u\n', encoding="utf-8")
        with (include / "probe_build.h").open("a",encoding="utf-8") as build_header:
            build_header.write(f"#define TF_PROBE_STARTUP {int(args.startup)}\n")
        common = [f"--target={target}", "-std=c11", "-O0", "-g0", "-fno-strict-aliasing",
                  "-fwrapv", "-ffp-contract=off", "-fno-omit-frame-pointer"]
        if args.abi == "armeabi-v7a":
            common += ["-fmax-type-align=1"]
        for directory in (include, ROOT / "runtime", lift_dir / "gen"):
            common += ["-I", str(directory)]
        header_hash = sha256(include / "recomp_types.h")
        overlay_hashes = {name:sha256(ROOT / "runtime" / name)
            for name in ("transformers_x87.h", "transformers_runtime.h")}
        core_sources = sorted((lift_dir / "gen").glob("recomp_*.c"))
        if len(core_sources) < 2:
            raise ValueError("Generated source set is missing")
        sources = [ROOT / "runtime/probe.c", ROOT / "runtime/probe_support.c", prepared["host_source"],
                   prepared["pcrecomp_dir"] / "runtime/win32hle/guest_mem.c",
                   prepared["pcrecomp_dir"] / "runtime/win32hle/pe_loader.c"]
        if args.startup:
            sources += [ROOT / "runtime/startup.c"] + [prepared["pcrecomp_dir"] / "runtime/win32hle" / name
                for name in ("kernel32.c","kernel32_crt.c","kernel32_ext.c","module.c","hle_path.c")]
        core_archive = args.core_archive.resolve() if args.core_archive else None
        if core_archive:
            summary["core_archive_provenance"] = validate_archive(core_archive,
                manifest["input_sha256"], target, header_hash, overlay_hashes)
        else:
            sources += core_sources
        objects = [out / f"{i:02d}-{p.stem}.o" for i, p in enumerate(sources)]
        summary.update({"input_sha256": manifest["input_sha256"], "entry": f"0x{entry:08X}", "abi":args.abi,
            "target": target, "startup_apis":args.startup, "strict_header_sha256": header_hash,
            "runtime_overlay_sha256": overlay_hashes,
            "runtime_support_header_sha256":sha256(ROOT / "runtime/probe_support.h"),
            "compiler": subprocess.check_output([str(compiler), "--version"], text=True).splitlines()[0],
            "sources": [{"path":str(p),"sha256":sha256(p)} for p in sources]})
        if args.startup:
            summary["startup_source_sha256"]={name:sha256(ROOT / "runtime" / name)
                for name in ("startup.c","startup.h","startup_state.h")}

        def compile_item(item: tuple[int, Path]) -> dict:
            i, source = item
            command = common.copy()
            if source in core_sources:
                command += ["-include", str(lift_dir / "gen/recomp_funcs.h")]
            command += ["-c", str(source), "-o", str(objects[i])]
            result = compile_one(compiler, command, out / f"compile-{i:02d}.rsp")
            result["source"] = str(source)
            print(f'{"PASS" if result["exit_code"] == 0 else "FAIL"} {source.name}', flush=True)
            return result
        with ThreadPoolExecutor(max_workers=args.jobs) as pool:
            results = list(pool.map(compile_item, enumerate(sources)))
        summary["compilation"] = results
        if any(item["exit_code"] for item in results):
            raise ValueError("Compilation failed: inspect per-source logs")
        executable = out / "transformers-probe"
        executable.unlink(missing_ok=True)
        link = common + list(map(str, objects)) + ([str(core_archive)] if core_archive else []) + [
            "-pie", "-Wl,--no-undefined", "-Wl,-z,max-page-size=16384", "-lm", "-o", str(executable)]
        summary["link"] = compile_one(compiler, link, out / "link.rsp")
        if summary["link"]["exit_code"]:
            raise ValueError("Link failed: inspect link.log")
        elf = executable.read_bytes()
        validate_elf(elf,args.abi)
        summary.update({"status":"linked_diagnostic_probe", "linked":True,
            "artifact":str(executable), "artifact_sha256":sha256(executable), "artifact_bytes":len(elf),
            "limitations":["CLI executable, not APK; no Android lifecycle/surface/input bridge.",
                ("Selected Win32 startup APIs registered; remaining APIs, INT and SEH stop explicitly."
                 if args.startup else "No Win32 API modules registered. APIs, missing dispatch, INT and SEH stop explicitly."),
                "Source translation and successful linking do not prove x86 semantic equivalence.",
                "No game or device was executed by this command."]})
        print(f"Linked {executable} ({len(elf)} bytes); not executed; playable=false")
        return 0
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as exc:
        summary["error"] = str(exc)
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    finally:
        summary_path.write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    raise SystemExit(main())
