"""Link the verified lifted core, D3D9 bridge, DXVK and Android JNI surface."""
from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import re
import os
from pathlib import Path
import subprocess
import sys

from android_targets import TARGETS, target_triple, validate_elf
from build_probe import compile_one, find_compiler, prepare_runtime, sha256, validate_archive, validate_lift
from preflight import ROOT

DXVK_PIN="c8dc91fabd00cac11d697ccf07426e798393cd40"


def runtime_dependencies(sources: list[Path]) -> dict:
    """Hash quoted project includes, excluding unrelated work-in-progress files."""
    roots=(ROOT/"runtime",ROOT/"ports/dxvk_android");seen=set();pending=list(sources);result={}
    while pending:
        source=pending.pop()
        if source in seen:continue
        seen.add(source)
        for name in re.findall(r'^\s*#include\s*"([^"\n]+)"',source.read_text(encoding="utf-8"),re.M):
            target=next((p.resolve() for p in (source.parent/name,*[r/name for r in roots]) if p.is_file()),None)
            if target is None or not any(target.is_relative_to(r) for r in roots):continue
            result[target.relative_to(ROOT).as_posix()]=sha256(target);pending.append(target)
    return result


def validate_backend(directory: Path,target: str) -> dict:
    summary=json.loads((directory/"dxvk-build-summary.json").read_text(encoding="utf-8"))
    if (summary.get("status")!="dxvk_native_android_built" or summary.get("revision")!=DXVK_PIN or
        summary.get("target")!=target or not summary.get("pic") or
        summary.get("archive_sha256")!=sha256(directory/"libdxvk_android.a")):
        raise ValueError("DXVK backend provenance differs from the pinned target")
    return summary


def main() -> int:
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lift-dir",type=Path,required=True)
    parser.add_argument("--core-archive",type=Path,required=True)
    parser.add_argument("--dxvk-dir",type=Path,required=True)
    parser.add_argument("--out",type=Path,required=True)
    parser.add_argument("--abi",choices=TARGETS,default="arm64-v8a")
    parser.add_argument("--ndk",type=Path)
    parser.add_argument("--jobs",type=int,choices=range(1,5),default=2)
    args=parser.parse_args(); out=args.out.resolve(); out.mkdir(parents=True,exist_ok=True)
    temporary=out/"tmp";temporary.mkdir(exist_ok=True)
    os.environ["TMP"]=os.environ["TEMP"]=str(temporary)
    summary={"status":"failed","linked":False,"executed":False,"playable":False,"device_tested":False}
    try:
        lift=args.lift_dir.resolve(); manifest,image=validate_lift(lift)
        prepared=prepare_runtime(lift,out); compiler=find_compiler(args.ndk)
        target=target_triple(args.abi,29); include=prepared["include_dir"]
        (include/"probe_build.h").write_text(
            f'#define TF_PROBE_INPUT_SHA256 "{manifest["input_sha256"]}"\n'
            f'#define TF_PROBE_INPUT_SIZE {len(image.data)}ull\n'
            f'#define TF_PROBE_ENTRY 0x{image.image_base+image.entry_rva:08X}u\n'
            '#define TF_PROBE_STARTUP 1\n#define TF_PROBE_PLATFORM 1\n',encoding="utf-8")
        overlay={name:sha256(ROOT/"runtime"/name) for name in ("transformers_x87.h","transformers_runtime.h")}
        archive=args.core_archive.resolve()
        provenance=validate_archive(archive,manifest["input_sha256"],target,sha256(include/"recomp_types.h"),overlay)
        dxvk=args.dxvk_dir.resolve()
        dxvk_summary=validate_backend(dxvk,target)
        backend=dxvk/"libdxvk_android.a"
        sources=[ROOT/"runtime/probe_support.c",prepared["host_source"],
            *[prepared["pcrecomp_dir"]/"runtime/win32hle"/name for name in
              ("pe_loader.c","kernel32.c","kernel32_crt.c","kernel32_ext.c","module.c","hle_path.c")],
            ROOT/"runtime/game_memory.c",ROOT/"runtime/source_files.c",
            ROOT/"runtime/startup.c",ROOT/"runtime/startup_user32.c",
            ROOT/"runtime/android_game.cpp",ROOT/"runtime/android_presenter.cpp",ROOT/"runtime/session_lifecycle.cpp",ROOT/"runtime/android_vulkan.cpp",ROOT/"runtime/dxvk_smoke.cpp",ROOT/"runtime/d3d9_bridge.cpp"]
        # Additional bridge wrappers use the same C shim ABI and are maintained
        # separately from the host-pointer marshalling implementation.
        for name in ("d3d9_hle.cpp","d3d9_shims.c","d3d9_bridge_shims.cpp","d3d9_bridge_hle.cpp","startup_services.c","startup_input.c","startup_audio.cpp","control_profile.cpp"):
            if (ROOT/"runtime"/name).is_file(): sources.append(ROOT/"runtime"/name)
        flags=[f"--target={target}","-O2","-g0","-fPIC","-fno-strict-aliasing","-fwrapv","-ffp-contract=off","-DTF_ANDROID_AUDIO=1"]
        if args.abi=="armeabi-v7a":flags += ["-fmax-type-align=1"]
        for directory in (include,ROOT/"runtime",lift/"gen",ROOT/"ports/dxvk_android",
                          ROOT/"work/dxvk-source/include/native",ROOT/"work/dxvk-source/include/native/windows",
                          ROOT/"work/dxvk-source/include/native/directx"):
            flags += ["-I",str(directory)]
        objects=[out/f"{i:02d}-{source.stem}.o" for i,source in enumerate(sources)]
        source_hashes={str(p):sha256(p) for p in sources}
        headers=runtime_dependencies(sources)
        def compile_item(item):
            i,source=item
            language=["-std=c++17","-DDXVK_NATIVE","-DNOMINMAX"] if source.suffix==".cpp" else ["-std=c11"]
            if source.name=="android_presenter.cpp":
                language += ["-DDXVK_PLATFORM_LINUX","-DDXVK_WSI_ANDROID","-DVK_USE_PLATFORM_ANDROID_KHR",
                    "-Wno-deprecated-declarations","-I",str(ROOT/"work/dxvk-source/include")]
            result=compile_one(compiler,flags+language+["-c",str(source),"-o",str(objects[i])],out/f"compile-{i:02d}.rsp")
            result["source"]=str(source)
            print(("PASS " if result["exit_code"]==0 else "FAIL ")+source.name,flush=True)
            return result
        with ThreadPoolExecutor(max_workers=args.jobs) as pool:
            results=list(pool.map(compile_item,enumerate(sources)))
        if source_hashes!={str(p):sha256(p) for p in sources} or headers!=runtime_dependencies(sources):
            raise ValueError("Runtime sources changed during compilation; repeat the build from stable sources")
        summary.update(input_sha256=manifest["input_sha256"],target=target,abi=args.abi,
            core_archive_provenance=provenance,dxvk_archive_sha256=sha256(backend),dxvk_revision=dxvk_summary["revision"],
            dxvk_source_preparation_sha256=dxvk_summary.get("source_preparation_sha256"),
            spirv_tools_source_sha256=dxvk_summary.get("spirv_tools_source_sha256"),
            runtime_source_sha256={p.name:sha256(p) for p in sources if p.parent==ROOT/"runtime"},
            sources=[{"path":str(source),"sha256":sha256(source)} for source in sources],compilation=results)
        if any(result["exit_code"] for result in results):raise ValueError("Runtime compilation failed; inspect compile logs")
        library=out/"libtransformers_game.so"; library.unlink(missing_ok=True)
        # Existing core objects use Android's PIE relocation model. Bsymbolic
        # binds their globals locally; the ELF link must contain no TEXTREL.
        command=flags+["-shared","-Wl,--no-undefined","-Wl,-Bsymbolic","-Wl,--exclude-libs,ALL","-Wl,--wrap=vkGetInstanceProcAddr",
            "-Wl,--wrap=open","-Wl,--wrap=open64","-Wl,--wrap=fopen","-Wl,--wrap=fopen64","-Wl,--wrap=unlink","-Wl,--wrap=rename",
            *[f"-Wl,--wrap={name}" for name in ("read","close","lseek","lseek64","dup","stat","stat64","access")],
            "-Wl,-soname,libtransformers_game.so",
            "-Wl,-z,max-page-size=16384",*map(str,objects),str(archive),str(backend),
            "-static-libstdc++","-lvulkan","-landroid","-laaudio","-llog","-ldl","-latomic","-lm","-o",str(library)]
        cxx=compiler.with_name("clang++.exe" if compiler.suffix==".exe" else "clang++")
        summary["link"]=compile_one(cxx,command,out/"link.rsp")
        if summary["link"]["exit_code"]:raise ValueError("Game runtime link failed; inspect link.log")
        validate_elf(library.read_bytes()[:64],args.abi)
        readelf=compiler.with_name("llvm-readelf.exe" if compiler.suffix==".exe" else "llvm-readelf")
        dynamic=subprocess.check_output([str(readelf),"-d",str(library)],text=True)
        (out/"dynamic.log").write_text(dynamic,encoding="utf-8")
        if "TEXTREL" in dynamic or "libc++_shared.so" in dynamic:raise ValueError("Unexpected relocation or unbundled C++ dependency")
        summary.update(status="linked_dxvk_game_runtime",linked=True,artifact=str(library),
            artifact_sha256=sha256(library),artifact_bytes=library.stat().st_size,
            runtime_dependency_sha256=headers,
            limitations=["Built, not executed on Android.","D3D9 bridge coverage is partial; unsupported APIs fail explicitly.",
                "Native triangle draw/readback test is separate from game startup/gameplay."])
        # Share the lifted core with the graphics process instead of embedding
        # hundreds of MB of identical machine code twice in a universal APK.
        probe=out/"transformers-probe"
        probe_link=flags+["-std=c11",str(ROOT/"runtime/probe.c"),"-pie","-Wl,--no-undefined",
            "-Wl,-z,max-page-size=16384","-L",str(out),"-ltransformers_game","-lm","-o",str(probe)]
        thin=compile_one(compiler,probe_link,out/"probe-link.rsp")
        if thin["exit_code"]:raise ValueError("Thin diagnostic probe link failed")
        validate_elf(probe.read_bytes()[:64],args.abi)
        probe_dynamic=subprocess.check_output([str(readelf),"-d",str(probe)],text=True)
        (out/"probe-dynamic.log").write_text(probe_dynamic,encoding="utf-8")
        if "[libtransformers_game.so]" not in probe_dynamic:raise ValueError("Probe lacks its shared runtime dependency")
        probe_summary={"status":"linked_diagnostic_probe","linked":True,"executed":False,"playable":False,
            "input_sha256":manifest["input_sha256"],"abi":args.abi,"target":target,"startup_apis":True,
            "startup_source_sha256":{name:sha256(ROOT/"runtime"/name) for name in ("startup.c","startup.h","startup_state.h")},
            "artifact":str(probe),"artifact_sha256":sha256(probe),"artifact_bytes":probe.stat().st_size,
            "requires_game_runtime":summary["artifact_sha256"],"link":thin}
        (out/"probe-build-summary.json").write_text(json.dumps(probe_summary,indent=2)+"\n",encoding="utf-8")
        print(f"BUILT {library} ({library.stat().st_size} bytes); DXVK linked; playable unverified")
        return 0
    except (OSError,ValueError,KeyError,subprocess.SubprocessError) as error:
        summary["error"]=str(error); print("ERROR: "+str(error),file=sys.stderr);return 1
    finally:(out/"game-build-summary.json").write_text(json.dumps(summary,indent=2)+"\n",encoding="utf-8")


if __name__=="__main__":raise SystemExit(main())
