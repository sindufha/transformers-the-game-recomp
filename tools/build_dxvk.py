"""Build pinned DXVK Native D3D9 for Android using only installed NDK tools."""
from concurrent.futures import ThreadPoolExecutor, as_completed
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys

from android_targets import TARGETS, target_triple, validate_elf
from build_probe import find_compiler, response_argument, sha256
from preflight import ROOT

PORT = ROOT / "ports/dxvk_android"
spec = importlib.util.spec_from_file_location("dxvk_prepare", PORT / "prepare.py")
recipe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(recipe)


def run(command, log=None):
    result = subprocess.run([str(arg) for arg in command], capture_output=True, text=True, errors="replace")
    if log:
        log.write_text(result.stdout + result.stderr, encoding="utf-8")
    if result.returncode:
        raise ValueError(f"Command failed ({result.returncode}): {command[0]}\n" + (result.stdout + result.stderr)[-8000:])
    return result.stdout


def build_shaders(prepared, ndk, preparation):
    tools = ndk / "shader-tools/windows-x86_64"
    glslc, validator = tools / "glslc.exe", tools / "spirv-val.exe"
    if not glslc.exists() or not validator.exists():
        raise ValueError("NDK glslc and spirv-val are required")
    generated = prepared / "generated"
    generated.mkdir(exist_ok=True)
    shaders = []
    for folder in ("src/dxvk/shaders", "src/dxvk/hud/shaders", "src/d3d9/shaders"):
        shaders += [p for p in sorted((prepared / folder).iterdir()) if p.suffix in (".vert", ".frag", ".comp", ".geom")]
    records = []
    for shader in shaders:
        binary = generated / (shader.stem + ".spv")
        header = generated / (shader.stem + ".h")
        stamp = generated / (shader.stem + ".json")
        fingerprint = {"source_sha256": sha256(shader), "glslc_sha256": sha256(glslc), "target_env": "vulkan1.1"}
        if stamp.exists() and binary.exists() and header.exists():
            previous = json.loads(stamp.read_text(encoding="utf-8"))
            if all(previous.get(k) == v for k, v in fingerprint.items()) and previous.get("spirv_sha256") == sha256(binary) and previous.get("header_sha256") == sha256(header):
                records.append(previous)
                continue
        run([glslc, "--target-env=vulkan1.1", shader, "-o", binary])
        run([validator, "--target-env", "vulkan1.1", binary])
        data = binary.read_bytes()
        if len(data) % 4 or data[:4] != b"\x03\x02\x23\x07":
            raise ValueError("Invalid SPIR-V output")
        words = struct.unpack("<" + "I" * (len(data) // 4), data)
        header.write_text("#pragma once\n#include <stdint.h>\nconst uint32_t " + shader.stem + "[] = {\n" +
                          "\n".join("  " + ", ".join(f"0x{x:08x}" for x in words[i:i+8]) + "," for i in range(0, len(words), 8)) + "\n};\n", encoding="ascii")
        record = {**fingerprint, "name": shader.name, "spirv_sha256": sha256(binary), "header_sha256": sha256(header), "validated": True}
        stamp.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
        records.append(record)
    (generated / "shader-summary.json").write_text(json.dumps({"shaders": records, "count": len(records), "target_env": "vulkan1.1", "validator_sha256": sha256(validator)}, indent=2) + "\n", encoding="utf-8")
    print(f"VALIDATED {len(records)} Vulkan 1.1 built-in shaders", flush=True)
    return records


def source_files(prepared):
    patterns = ("src/util/*.cpp", "src/util/com/*.cpp", "src/util/config/*.cpp", "src/util/log/*.cpp", "src/util/sha1/*.c", "src/util/sha1/*.cpp", "src/util/sync/*.cpp", "src/dxso/*.cpp", "src/spirv/*.cpp", "src/vulkan/*.cpp", "src/dxvk/*.cpp", "src/dxvk/hud/*.cpp", "src/d3d9/*.cpp")
    sources = sorted(set(p for pattern in patterns for p in prepared.glob(pattern)))
    sources = [p for p in sources if p.name not in ("util_gdi.cpp", "dxvk_openvr.cpp", "dxvk_openxr.cpp")]
    sources += [prepared / f"src/util/platform/{name}.cpp" for name in ("util_env_linux", "util_luid_linux", "util_string_linux", "thread_native")]
    return sources + [PORT / "android_wsi.cpp", PORT / "spirv_specialize.cpp"]


def build(args):
    work = ROOT / "work"
    prepared = work / "dxvk-source"
    compiler = find_compiler(args.ndk)
    ndk = compiler.parents[5]
    spirv_dir=work/("spirv-"+args.abi)
    spirv_record=json.loads((spirv_dir/"build-summary.json").read_text(encoding="utf-8"))
    if spirv_record.get("status")!="spirv_tools_android_built" or spirv_record.get("abi")!=args.abi:
        raise ValueError("Build verified SPIRV-Tools for this ABI first")
    spirv_archives=[]
    for item in spirv_record["archives"]:
        path=Path(item["path"])
        if sha256(path)!=item["sha256"]:raise ValueError("SPIRV-Tools archive changed")
        spirv_archives.append(path)
    spirv_include=Path(spirv_record["source"])/"include"
    preparation = recipe.prepare((ROOT / args.source).resolve(), prepared, work)
    shaders = build_shaders(prepared, ndk, preparation)
    out = (args.out or work / ("dxvk-" + args.abi)).resolve()
    out.mkdir(parents=True, exist_ok=True)
    version = run([compiler, "--version"]).splitlines()[0]
    includes = [prepared, prepared / "include", prepared / "src/util", prepared / "include/native", prepared / "include/native/windows", prepared / "include/native/directx", prepared / "generated", PORT, spirv_include]
    common = [f"--target={target_triple(args.abi, args.api)}", "-fPIC", "-O2", "-g0", "-fno-strict-aliasing", "-ffunction-sections", "-fdata-sections", "-DDXVK_NATIVE", "-DDXVK_PLATFORM_LINUX", "-DDXVK_WSI_ANDROID", "-DVK_USE_PLATFORM_ANDROID_KHR", "-DNOMINMAX", "-Wno-deprecated-declarations", "-Wno-unused-private-field", "-Wno-microsoft-exception-spec", "-Wno-ignored-attributes"]
    if args.abi == "x86": common += ["-msse2", "-mfpmath=sse"]
    for include in includes: common += ["-I", str(include)]
    closure = hashlib.sha256(json.dumps({"preparation": preparation, "shaders": shaders, "wsi_header": sha256(PORT / "android_wsi.h"), "specializer_header":sha256(PORT/"spirv_specialize.h"), "spirv_tools":spirv_record, "builder": sha256(Path(__file__)), "compiler": version, "flags": common}, sort_keys=True).encode()).hexdigest()
    sources = source_files(prepared)
    objects = [out / (p.relative_to(prepared).as_posix().replace("/", "_") + ".o") if p.is_relative_to(prepared) else out / (p.name+".o") for p in sources]

    def compile_source(item):
        source, obj = item
        stamp = obj.with_suffix(".json")
        fingerprint = hashlib.sha256((closure + sha256(source)).encode()).hexdigest()
        if stamp.exists() and obj.exists():
            record = json.loads(stamp.read_text(encoding="utf-8"))
            if record.get("fingerprint") == fingerprint and record.get("object_sha256") == sha256(obj):
                return record
        cpp = source.suffix == ".cpp"
        flags = common + (["-x", "c++", "-std=c++17"] if cpp else ["-x", "c", "-std=c11"])
        response = obj.with_suffix(".rsp")
        response.write_text("\n".join(response_argument(str(x)) for x in flags + ["-c", str(source), "-o", str(obj)]) + "\n", encoding="utf-8")
        run([compiler, "@" + str(response)], obj.with_suffix(".log"))
        record = {"source": str(source), "fingerprint": fingerprint, "object_sha256": sha256(obj)}
        stamp.write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
        return record

    results = []
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = {pool.submit(compile_source, item): item[0] for item in zip(sources, objects)}
        for future in as_completed(futures):
            results.append(future.result())
            if len(results) % 10 == 0 or len(results) == len(sources):
                print(f"{args.abi}: compiled/cached {len(results)}/{len(sources)}", flush=True)
    archive = out / "libdxvk_android.a"
    response = out / "archive.rsp"
    response.write_text("\n".join(response_argument(str(obj)) for obj in objects) + "\n", encoding="utf-8")
    archive.unlink(missing_ok=True)
    mri='CREATE "'+archive.as_posix()+'"\n'+''.join('ADDMOD "'+p.as_posix()+'"\n' for p in objects)+''.join('ADDLIB "'+p.as_posix()+'"\n' for p in spirv_archives)+'SAVE\nEND\n'
    (out/"archive.mri").write_text(mri,encoding="utf-8")
    combined=subprocess.run([str(compiler.parent/"llvm-ar.exe"),"-M"],input=mri,capture_output=True,text=True)
    if combined.returncode:raise ValueError("Archive merge failed: "+combined.stderr)
    shared = out / "libdxvk_android.so"
    link = [f"--target={target_triple(args.abi, args.api)}", "-shared", "-static-libstdc++", "-Wl,--no-undefined", "-Wl,-z,max-page-size=16384", "-Wl,-soname,libdxvk_android.so", "-Wl,--whole-archive", str(archive), "-Wl,--no-whole-archive", "-lvulkan", "-landroid", "-llog", "-ldl", "-latomic", "-o", str(shared)]
    response = out / "link.rsp"
    response.write_text("\n".join(response_argument(str(x)) for x in link) + "\n", encoding="utf-8")
    cxx = compiler.parent / "clang++.exe"
    run([cxx, "@" + str(response)], out / "link.log")
    validate_elf(shared.read_bytes()[:64], args.abi)
    summary = {"status": "dxvk_native_android_built", "abi": args.abi, "api": args.api, "revision": recipe.PIN,
               "source": str((ROOT / args.source).resolve()), "source_preparation_sha256": sha256(prepared / "android-preparation.json"),
               "archive": str(archive), "archive_sha256": sha256(archive), "archive_bytes": archive.stat().st_size,
               "shared": str(shared), "shared_sha256": sha256(shared), "shared_bytes": shared.stat().st_size,
               "pic": True, "object_count": len(sources), "shader_count": len(shaders), "compiler": version,
               "wsi_sha256": sha256(PORT / "android_wsi.cpp"), "wsi_header_sha256": sha256(PORT / "android_wsi.h"),
               "spirv_tools_source_sha256":spirv_record["source_manifest_sha256"],
               "spirv_tools_archives":spirv_record["archives"],
               "specializer_sha256":sha256(PORT/"spirv_specialize.cpp"),
               "target": target_triple(args.abi, args.api), "runtime_tested": False, "gameplay_tested": False,
               "limitations": ["GPU/driver features remain device dependent", "No BC texture software fallback", "No guest COM bridge included in backend", "No gameplay verification from build/link"]}
    (out / "dxvk-build-summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")
    print(f"BUILT {archive}\nBUILT {shared}", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--abi", choices=TARGETS, default="arm64-v8a")
    parser.add_argument("--api", type=int, default=29)
    parser.add_argument("--ndk", type=Path)
    parser.add_argument("--source", type=Path, default=Path("external/dxvk-native"))
    parser.add_argument("--out", type=Path)
    parser.add_argument("--jobs", type=int, choices=range(1,9), default=4)
    args = parser.parse_args()
    try:
        build(args)
        return 0
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        print("ERROR: " + str(error), file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
