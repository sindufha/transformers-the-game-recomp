"""Build a universal Android 10 diagnostic APK using the installed SDK tools."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import uuid
import zipfile

from android_targets import TARGETS, validate_elf
from build_probe import sha256
from preflight import ROOT
from asset_manifest import validate_manifest
from build_game import DXVK_PIN


def run(command: list[str], log: Path) -> None:
    with log.open("w", encoding="utf-8") as stream:
        result = subprocess.run(list(map(str, command)), stdout=stream, stderr=subprocess.STDOUT, timeout=300)
    if result.returncode:
        raise ValueError(f"Command failed ({result.returncode}); see {log}")


def native_artifacts(base: Path, input_sha: str, abis=TARGETS) -> dict:
    artifacts = {}
    capabilities = None
    for abi in abis:
        directory = base / ("probe-arm64-target" if abi == "arm64-v8a" else f"probe-{abi}-target")
        manifest = json.loads((directory / "probe-build-summary.json").read_text(encoding="utf-8"))
        path = directory / "transformers-probe"
        if (manifest.get("status") != "linked_diagnostic_probe" or not manifest.get("linked") or
            manifest.get("input_sha256") != input_sha or manifest.get("artifact_sha256") != sha256(path) or
            not manifest.get("target", "").endswith("android29") and not manifest.get("target", "").endswith("androideabi29")):
            raise ValueError(f"Unverified or mismatched native artifact: {abi}")
        validate_elf(path.read_bytes()[:64], abi)
        current=(manifest.get("startup_apis",False),manifest.get("startup_source_sha256"))
        if capabilities is not None and current != capabilities:
            raise ValueError("Native runtime capabilities differ between ABIs")
        capabilities=current
        artifacts[abi] = {"path": path, "sha256": manifest["artifact_sha256"], "bytes": path.stat().st_size,
                          "startup_apis":manifest.get("startup_apis",False),
                          "startup_source_sha256":manifest.get("startup_source_sha256"),
                          "requires_game_runtime":manifest.get("requires_game_runtime")}
    return artifacts


def adreno_artifacts(directory: Path) -> dict:
    metadata=json.loads((directory/"build-summary.json").read_text())
    expected={"libadrenotools.so","libhook_impl.so","libmain_hook.so"}
    if (metadata.get("abi")!="arm64-v8a" or metadata.get("api")!=29 or
        set(metadata.get("libraries",{}))!=expected or
        metadata.get("build_tool_sha256")!=sha256(ROOT/"tools/build_adrenotools.py") or
        metadata.get("license_sha256")!=sha256(directory/"LICENSE.txt")):
        raise ValueError("Adreno Tools provenance differs")
    result={}
    for name,record in metadata["libraries"].items():
        data=(directory/name).read_bytes();validate_elf(data[:64],"arm64-v8a")
        if hashlib.sha256(data).hexdigest()!=record["sha256"]:raise ValueError("Adreno Tools library changed")
        result["lib/arm64-v8a/"+name]=data
    return result


def game_artifacts(base: Path,input_sha: str,abis=TARGETS) -> dict:
    artifacts={}; closure=None
    for abi in abis:
        directory=base/("probe-arm64-target" if abi=="arm64-v8a" else f"probe-{abi}-target")
        manifest=json.loads((directory/"game-build-summary.json").read_text(encoding="utf-8"))
        path=directory/"libtransformers_game.so"
        if (manifest.get("status")!="linked_dxvk_game_runtime" or not manifest.get("linked") or
            manifest.get("input_sha256")!=input_sha or manifest.get("abi")!=abi or
            manifest.get("dxvk_revision")!=DXVK_PIN or manifest.get("artifact_sha256")!=sha256(path)):
            raise ValueError(f"Unverified DXVK game runtime: {abi}")
        validate_elf(path.read_bytes()[:64],abi)
        current=(manifest.get("runtime_source_sha256"),manifest.get("runtime_dependency_sha256"),
                 manifest.get("dxvk_source_preparation_sha256"),manifest.get("spirv_tools_source_sha256"))
        if not all(current):raise ValueError("Game runtime source closure missing")
        if closure is not None and current!=closure:raise ValueError("Game runtime sources differ between ABIs")
        closure=current
        artifacts[abi]={"path":path,"sha256":manifest["artifact_sha256"],"bytes":path.stat().st_size,
                        "dxvk_revision":DXVK_PIN,"spirv_tools_source_sha256":manifest["spirv_tools_source_sha256"]}
    return artifacts


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--lift-dir", type=Path, required=True)
    parser.add_argument("--native-root", type=Path, default=ROOT / "work")
    parser.add_argument("--out", type=Path, default=ROOT / "work/android")
    parser.add_argument("--sdk", type=Path)
    parser.add_argument("--jdk", type=Path)
    parser.add_argument("--prepare-only", action="store_true")
    parser.add_argument("--asset-manifest", type=Path, default=ROOT / "work/game-assets.json")
    parser.add_argument("--graphics-root",type=Path,help="Four verified JNI/DXVK game runtime builds")
    parser.add_argument("--keystore",type=Path,help="Reuse a local debug keystore for in-place APK updates")
    parser.add_argument("--abis",nargs="+",choices=TARGETS,default=list(TARGETS),help="Defaults to universal; select one ABI for device iteration")
    parser.add_argument("--shader-cache",type=Path,help="Exact captured HLSL requests compiled with the supported SDK")
    parser.add_argument("--validation-layer",type=Path,help="Optional development-only Vulkan layer; requires exactly one matching ABI")
    parser.add_argument("--adreno-tools",type=Path,help="Verified ARM64 app-local Vulkan driver loader helpers")
    parser.add_argument("--turnip-driver",type=Path,help="Pinned Turnip directory; bundled for ARM64 Adreno 6xx")
    args = parser.parse_args()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)
    summary = {"status": "failed", "playable": False, "device_tested": False,
               "minimum_android": "10", "min_sdk": 29, "target_sdk": 29}
    try:
        manifest = json.loads((args.lift_dir / "lift-summary.json").read_text(encoding="utf-8"))
        if manifest["status"] != "code_generated" or manifest["diagnostic_count"]:
            raise ValueError("The APK requires a lift without translation diagnostics")
        snapshot = args.lift_dir / "input.exe"
        if sha256(snapshot) != manifest["input_sha256"]:
            raise ValueError("Lift snapshot changed")
        if len(args.abis)!=len(set(args.abis)):raise ValueError("Duplicate ABI selection")
        graphics=game_artifacts(args.graphics_root,manifest["input_sha256"],args.abis) if args.graphics_root else {}
        validation_data=None
        adreno_files={}
        if args.adreno_tools:
            if "arm64-v8a" not in args.abis or not graphics:raise ValueError("Adreno Tools requires ARM64 graphics")
            adreno_files=adreno_artifacts(args.adreno_tools)
        turnip_data=None
        turnip_provenance=None
        if args.turnip_driver:
            if not adreno_files:raise ValueError("Bundled Turnip requires Adreno Tools")
            turnip_data=(args.turnip_driver/"vulkan.ad07xx.so").read_bytes();validate_elf(turnip_data[:64],"arm64-v8a")
            turnip_provenance=json.loads((args.turnip_driver/"provenance.json").read_text(encoding="utf-8"))
            if (hashlib.sha256(turnip_data).hexdigest()!=turnip_provenance.get("driver_sha256") or
                len(turnip_data)!=turnip_provenance.get("driver_bytes")):
                raise ValueError("Turnip driver differs from its recorded release provenance")
            release_archive=args.turnip_driver/turnip_provenance.get("release_asset","")
            if release_archive.is_file() and hashlib.sha256(release_archive.read_bytes()).hexdigest()!=turnip_provenance.get("archive_sha256"):
                raise ValueError("Turnip release archive checksum mismatch")
            for name in ("MESA-LICENSE.rst","provenance.json"):
                if not (args.turnip_driver/name).is_file():raise ValueError("Turnip attribution missing")
            adreno_files["lib/arm64-v8a/libvulkan_turnip.so"]=turnip_data
        if args.validation_layer:
            if len(args.abis)!=1 or not graphics:raise ValueError("Validation layer requires one graphics ABI")
            validation_data=args.validation_layer.read_bytes();validate_elf(validation_data[:64],args.abis[0])
            if not args.validation_layer.with_name("LICENSE.txt").is_file():raise ValueError("Validation layer license missing")
        shader_files={};shader_index=[]
        if args.shader_cache:
            if not graphics:raise ValueError("Shader cache requires the graphics runtime")
            for directory in sorted(args.shader_cache.iterdir()):
                if not directory.is_dir():continue
                metadata=json.loads((directory/"summary.json").read_text(encoding="utf-8"))
                if directory.name!=sha256(directory/"request.bin") or metadata["request_sha256"]!=directory.name or metadata["shader_sha256"]!=sha256(directory/"shader.bin") or metadata.get("constant_sha256")!=sha256(directory/"constants.bin"):
                    raise ValueError("Shader cache provenance mismatch")
                for name in ("request.bin","shader.bin","constants.bin"):
                    data=(directory/name).read_bytes();relative=directory.name+"/"+name
                    if not data or len(data)>8*1024*1024:raise ValueError("Invalid shader cache size")
                    shader_files[relative]=data
                    shader_index.append({"path":relative,"bytes":len(data),"sha256":hashlib.sha256(data).hexdigest()})
        asset_bytes = args.asset_manifest.read_bytes()
        asset_data = validate_manifest(json.loads(asset_bytes), manifest["input_sha256"])
        summary["assets"] = {"files":len(asset_data["files"]), "bytes":asset_data["total_bytes"],
            "manifest_sha256":hashlib.sha256(asset_bytes).hexdigest(),"bundled":False,"importer":True}
        sdk = args.sdk or Path(os.environ.get("ANDROID_HOME", os.environ.get("ANDROID_SDK_ROOT", "")))
        jdk = args.jdk or Path(os.environ.get("JAVA_HOME", ""))
        executable = ".exe" if os.name == "nt" else ""
        java, javac, keytool = [jdk / "bin" / (name + executable) for name in ("java", "javac", "keytool")]
        build_tools = sdk / "build-tools/35.0.0"
        android_jar = sdk / "platforms/android-36/android.jar"
        aapt2, aapt, zipalign = [build_tools / (name + executable) for name in ("aapt2", "aapt", "zipalign")]
        for tool in (java, javac, keytool, android_jar, aapt2, aapt, zipalign):
            if not tool.is_file():
                raise ValueError(f"Build dependency missing: {tool}")
        # Compile into a fresh directory so removed/renumbered anonymous Java
        # classes from a previous build cannot leak into this APK's DEX.
        classes, generated, dex = out / "classes" / uuid.uuid4().hex, out / "generated", out / "dex"
        for directory in (classes, generated, dex): directory.mkdir(parents=True, exist_ok=True)
        package = generated / "io/github/transformers/recompiled/diagnostics"
        package.mkdir(parents=True, exist_ok=True)
        (package / "BuildInfo.java").write_text(
            'package io.github.transformers.recompiled.diagnostics;\n'
            'final class BuildInfo {\n'
            f' static final String INPUT_SHA256="{manifest["input_sha256"]}";\n'
            f' static final long INPUT_BYTES={snapshot.stat().st_size}L;\n'
            f' static final boolean DXVK_AVAILABLE={str(bool(graphics)).lower()};\n'
            '}\n', encoding="utf-8")
        base = out / "base.apk"
        resources = out / "resources.zip"
        run([str(aapt2), "compile", "--dir", str(ROOT / "android/res"),
             "-o", str(resources)], out / "aapt2-compile.log")
        run([str(aapt2), "link", "--manifest", str(ROOT / "android/AndroidManifest.xml"),
             "-I", str(android_jar), "--min-sdk-version", "29", "--target-sdk-version", "29",
             "-o", str(base), str(resources)], out / "aapt2.log")
        sources = sorted((ROOT / "android/src").rglob("*.java")) + [package / "BuildInfo.java"]
        run([str(javac), "-encoding", "UTF-8", "-source", "8", "-target", "8", "-bootclasspath",
             str(android_jar), "-d", str(classes), *map(str, sources)], out / "javac.log")
        class_jar = out / "classes.jar"
        with zipfile.ZipFile(class_jar, "w", zipfile.ZIP_DEFLATED) as archive:
            for path in sorted(classes.rglob("*.class")):
                archive.write(path, path.relative_to(classes).as_posix())
        run([str(java), "-cp", str(build_tools / "lib/d8.jar"), "com.android.tools.r8.D8",
             "--min-api", "29", "--lib", str(android_jar), "--output", str(dex), str(class_jar)], out / "d8.log")
        summary["input_sha256"] = manifest["input_sha256"]
        if args.prepare_only:
            summary["status"] = "java_and_manifest_compiled"
            print("PASS: Java, DEX and Android 10 manifest prepared; no installable APK produced")
            return 0
        artifacts = native_artifacts(args.native_root, manifest["input_sha256"],args.abis)
        for abi,item in artifacts.items():
            if item.get("requires_game_runtime") and (abi not in graphics or
                graphics[abi]["sha256"]!=item["requires_game_runtime"]):
                raise ValueError("Thin native probe requires its matching packaged game runtime")
        unsigned = out / "unsigned.apk"
        shutil.copyfile(base, unsigned)
        with zipfile.ZipFile(unsigned, "a", zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
            archive.write(dex / "classes.dex", "classes.dex")
            archive.writestr("assets/game-assets.json", asset_bytes)
            archive.write(ROOT/"android/MATERIAL-SYMBOLS-LICENSE.txt","assets/licenses/MATERIAL-SYMBOLS-LICENSE.txt")
            if shader_files:
                archive.writestr("assets/shader-cache/index.json",json.dumps(shader_index).encode())
                for name,data in shader_files.items():archive.writestr("assets/shader-cache/"+name,data)
            for abi, artifact in artifacts.items():
                archive.write(artifact["path"], f"lib/{abi}/libtransformers_probe.so")
            for abi, artifact in graphics.items():
                archive.write(artifact["path"],f"lib/{abi}/libtransformers_game.so")
            if validation_data:
                archive.writestr(f"lib/{args.abis[0]}/libVkLayer_khronos_validation.so",validation_data)
                archive.write(args.validation_layer.with_name("LICENSE.txt"),"assets/licenses/VULKAN-VALIDATION-LICENSE.txt")
            for name,data in adreno_files.items():archive.writestr(name,data)
            if adreno_files:
                archive.write(args.adreno_tools/"LICENSE.txt","assets/licenses/ADRENOTOOLS-LICENSE.txt")
                archive.write(args.adreno_tools/"build-summary.json","assets/licenses/ADRENOTOOLS-BUILD.json")
            if turnip_data:
                archive.write(args.turnip_driver/"MESA-LICENSE.rst","assets/licenses/MESA-LICENSE.rst")
                archive.write(args.turnip_driver/"provenance.json","assets/licenses/TURNIP-PROVENANCE.json")
            if graphics:
                archive.write(ROOT/"ports/dxvk_android/DXVK-LICENSE","assets/licenses/DXVK-LICENSE.txt")
                archive.write(ROOT/"ports/dxvk_android/README.md","assets/licenses/DXVK-ANDROID-NOTICE.md")
                archive.write(ROOT/"external/pcrecomp/LICENSE","assets/licenses/PCRECOMP-LICENSE.txt")
                notices=[ROOT/f"work/spirv-{abi}/LICENSE.txt" for abi in args.abis]
                if len({sha256(path) for path in notices})!=1:raise ValueError("SPIRV-Tools licenses differ between ABIs")
                archive.write(notices[0],"assets/licenses/NDK-SHADERC-NOTICE.txt")
        aligned = out / "aligned.apk"
        run([str(zipalign), "-f", "-P", "16", "4", str(unsigned), str(aligned)], out / "zipalign.log")
        key = args.keystore.resolve() if args.keystore else out / "diagnostic-debug.jks"
        if not key.exists():
            run([str(keytool), "-genkeypair", "-keystore", str(key), "-storepass", "android",
                 "-keypass", "android", "-alias", "androiddebugkey", "-keyalg", "RSA", "-keysize", "2048",
                 "-validity", "10000", "-dname", "CN=Android Debug,O=Transformers Local Port,C=ID", "-noprompt"], out / "keytool.log")
        variant="universal" if set(args.abis)==set(TARGETS) else "-".join(args.abis)
        output=out/("transformers-"+("dxvk" if graphics else "diagnostics")+"-"+variant+"-debug.apk")
        signer = [str(java), "-jar", str(build_tools / "lib/apksigner.jar")]
        run(signer + ["sign", "--ks", str(key), "--ks-key-alias", "androiddebugkey",
                      "--ks-pass", "pass:android", "--key-pass", "pass:android", "--min-sdk-version", "29",
                      "--out", str(output), str(aligned)], out / "sign.log")
        run(signer + ["verify", "--verbose", str(output)], out / "verify.log")
        run([str(zipalign), "-c", "-P", "16", "4", str(output)], out / "alignment-check.log")
        run([str(aapt), "dump", "badging", str(output)], out / "badging.log")
        badging = (out / "badging.log").read_text(encoding="utf-8")
        if "sdkVersion:'29'" not in badging or any(abi not in badging for abi in args.abis):
            raise ValueError("Packaged Android minimum/ABI metadata did not match")
        with zipfile.ZipFile(output) as archive:
            expected = {f"lib/{abi}/libtransformers_probe.so" for abi in args.abis}
            expected |= {f"lib/{abi}/libtransformers_game.so" for abi in graphics}
            if validation_data:expected.add(f"lib/{args.abis[0]}/libVkLayer_khronos_validation.so")
            expected.update(adreno_files)
            actual = {name for name in archive.namelist() if name.startswith("lib/")}
            if expected != actual or not archive.read("classes.dex").startswith(b"dex\n"):
                raise ValueError("APK native libraries or DEX do not match expected contents")
            if archive.read("assets/game-assets.json") != asset_bytes:
                raise ValueError("Packaged asset manifest changed")
            for name,data in shader_files.items():
                if archive.read("assets/shader-cache/"+name)!=data:raise ValueError("Packaged shader changed")
            if validation_data and archive.read(f"lib/{args.abis[0]}/libVkLayer_khronos_validation.so")!=validation_data:raise ValueError("Packaged validation layer changed")
            for name,data in adreno_files.items():
                if archive.read(name)!=data:raise ValueError("Packaged Adreno Tools library changed")
            for group,filename in ((artifacts,"libtransformers_probe.so"),(graphics,"libtransformers_game.so")):
                for abi, artifact in group.items():
                    with archive.open(f"lib/{abi}/{filename}") as stream:
                        digest = hashlib.sha256()
                        while block := stream.read(1024 * 1024): digest.update(block)
                    if digest.hexdigest() != artifact["sha256"]:
                        raise ValueError(f"Packaged native content changed: {abi}/{filename}")
        summary.update(status="signed_universal_diagnostic_apk" if variant=="universal" else "signed_diagnostic_apk", artifact=str(output),
            artifact_sha256=sha256(output), artifact_bytes=output.stat().st_size,
            abis=list(args.abis), signature_verified=True, alignment_verified=True,
            startup_apis=all(v["startup_apis"] for v in artifacts.values()),
            dxvk=bool(graphics),graphics={abi:{k:v for k,v in item.items() if k!="path"} for abi,item in graphics.items()},
            shader_cache_entries=len(shader_files)//3,
            vulkan_validation_layer_sha256=hashlib.sha256(validation_data).hexdigest() if validation_data else None,
            adrenotools={name:hashlib.sha256(data).hexdigest() for name,data in adreno_files.items()},
            bundled_turnip=bool(turnip_data),
            turnip_driver={k:v for k,v in (turnip_provenance or {}).items() if k in ("repository","release_tag","release_commit","release_url","archive_sha256","driver_sha256","driver_bytes","mesa_version")},
            native={abi: {"sha256": v["sha256"], "bytes": v["bytes"],
                          "startup_apis":v["startup_apis"],"startup_source_sha256":v["startup_source_sha256"]}
                    for abi, v in artifacts.items()},
            limitations=["Development launcher; graphics bridge is partial and gameplay unverified.",
                         "Build output has not been executed by this packaging command.",
                         "Includes locally generated game-derived native code; no DAT/audio/FMV assets bundled."])
        print(f"BUILT {output} ({output.stat().st_size} bytes); {len(args.abis)} ABI(s), Android 10+, development")
        return 0
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as exc:
        summary["error"] = str(exc)
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    finally:
        (out / "apk-build-summary.json").write_text(json.dumps(summary, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    raise SystemExit(main())
