"""Build the NDK's shipped Apache-2.0 SPIRV-Tools optimizer, without downloads."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import re
from concurrent.futures import ThreadPoolExecutor
from build_probe import compile_one, response_argument
from android_targets import target_triple
from android_targets import TARGETS
from preflight import ROOT

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument("--abi",choices=TARGETS,default="arm64-v8a")
parser.add_argument("--ndk",type=Path,default=Path("D:/AndroidTools/android-sdk/ndk/27.1.12297006"))
parser.add_argument("--jobs",type=int,default=2)
parser.add_argument("--reuse-archives",action="store_true",help="Record an already completed build from this same NDK make directory")
parser.add_argument("--resume-ndk-objects",action="store_true",help="Reuse completed NDK optimizer objects and build/archive in the workspace (no temporary-drive writes)")
args=parser.parse_args()
ndk=args.ndk.resolve();source=ndk/"sources/third_party/shaderc/third_party/spirv-tools"
build=Path(os.environ["TEMP"])/("transformers-spirv-tools" if args.abi=="arm64-v8a" else "transformers-spirv-tools-"+args.abi)
build.mkdir(parents=True,exist_ok=True)
if " " in str(source) or " " in str(build):raise ValueError("NDK make requires paths without spaces")
def write_changed(path,text):
    if not path.exists() or path.read_text()!=text:path.write_text(text)
write_changed(build/"Android.mk","include "+source.as_posix()+"/Android.mk\n")
write_changed(build/"Application.mk","APP_ABI := "+args.abi+"\nAPP_PLATFORM := android-29\nAPP_STL := c++_static\nAPP_OPTIM := release\nAPP_CPPFLAGS := -fPIC\nAPP_MODULES := SPIRV-Tools-opt\n")
out=ROOT/("work/spirv-"+args.abi);out.mkdir(parents=True,exist_ok=True)
command=[str(ndk/"ndk-build.cmd"),"NDK_PROJECT_PATH=null","APP_BUILD_SCRIPT="+str(build/"Android.mk"),
    "NDK_APPLICATION_MK="+str(build/"Application.mk"),"NDK_OUT="+str(build/"obj"),"NDK_LIBS_OUT="+str(build/"libs"),"-j",str(max(1,min(args.jobs,8)))]
if args.resume_ndk_objects:
    # NDK make requires a space-free output path. Its generated headers and
    # completed optimizer objects are reusable when the temporary drive fills.
    # Compile the smaller core afresh so a partial object is never admitted.
    generated=out/"generated";generated.mkdir(exist_ok=True)
    native=build/"obj/local"/args.abi
    header_root=build.parent/"transformers-spirv-tools/obj/local/arm64-v8a"
    for root in (header_root,native):
        for item in root.iterdir():
            if item.is_file() and item.suffix in (".inc",".h"):shutil.copyfile(item,generated/item.name)
    makefile=(source/"Android.mk").read_text()
    compiler=ndk/"toolchains/llvm/prebuilt/windows-x86_64/bin/clang++.exe"
    archiver=compiler.with_name("llvm-ar.exe")
    flags=["--target="+target_triple(args.abi,29),"-std=c++11","-O2","-fPIC","-fno-exceptions","-fno-rtti","-ffunction-sections","-fdata-sections"]
    for folder in (source,source/"include",source/"source",source/"external/spirv-headers/include",generated):flags += ["-I",str(folder)]
    local_temp=out/"tmp";local_temp.mkdir(exist_ok=True)
    os.environ["TMP"]=os.environ["TEMP"]=str(local_temp)
    for module,variable in (("SPIRV-Tools-opt","SPVTOOLS_OPT_SRC_FILES"),("SPIRV-Tools","SPVTOOLS_SRC_FILES")):
        block=re.search(r"^"+variable+r"\s*:=\s*\\\n(.*?)(?:\n\s*\n)",makefile,re.M|re.S)
        if not block:raise ValueError("NDK source list missing: "+variable)
        files=re.findall(r"source/[\w/]+\.cpp",block[1]);objects=[];pending=[]
        for name in files:
            old=native/"objs"/module/Path(name).with_suffix(".o")
            if module=="SPIRV-Tools-opt" and old.exists() and old.read_bytes()[:4]==b"\x7fELF":
                subprocess.run([str(compiler.with_name("llvm-readobj.exe")),"--file-headers",str(old)],check=True,stdout=subprocess.DEVNULL)
                objects.append(old)
            else:
                target=out/"objects"/module/Path(name).with_suffix(".o");target.parent.mkdir(parents=True,exist_ok=True)
                objects.append(target);pending.append((name,target))
        def compile_source(item):
            name,target=item
            result=compile_one(compiler,flags+["-c",str(source/name),"-o",str(target)],target.with_suffix(".rsp"))
            if result["exit_code"]:raise RuntimeError("Optimizer compilation failed: "+name)
        with ThreadPoolExecutor(max_workers=max(1,min(args.jobs,8))) as pool:list(pool.map(compile_source,pending))
        response=out/(module+"-archive.rsp")
        response.write_text("\n".join(response_argument(str(p)) for p in objects)+"\n")
        subprocess.run([str(archiver),"rcs",str(out/("lib"+module+".a")),"@"+str(response)],check=True)
        print(f"RECOVERED {module}: {len(objects)} objects, {len(pending)} compiled in workspace",flush=True)
elif not args.reuse_archives:
    with (out/"build.log").open("w",encoding="utf-8") as log:
        result=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT)
    if result.returncode:raise RuntimeError("NDK optimizer build failed: "+str(out/"build.log"))
def digest(path):return hashlib.sha256(path.read_bytes()).hexdigest()
archives=[]
strip=ndk/"toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-strip.exe"
for name in ("libSPIRV-Tools-opt.a","libSPIRV-Tools.a"):
    target=out/name
    if not args.resume_ndk_objects:shutil.copyfile(build/"obj/local"/args.abi/name,target)
    subprocess.run([str(strip),"--strip-debug",str(target)],check=True)
    archives.append({"path":str(target),"sha256":digest(target),"bytes":target.stat().st_size})
license=source/"LICENSE"
if not license.is_file():license=ndk/"NOTICE"
shutil.copyfile(license,out/"LICENSE.txt")
sources={p.relative_to(source).as_posix():digest(p) for folder in ("source","include","external/spirv-headers/include") for p in sorted((source/folder).rglob("*")) if p.is_file()}
record={"status":"spirv_tools_android_built","abi":args.abi,"api":29,"ndk":str(ndk),"ndk_properties_sha256":digest(ndk/"source.properties"),
    "source":str(source),"source_manifest_sha256":hashlib.sha256(json.dumps(sources,sort_keys=True).encode()).hexdigest(),
    "archives":archives,"license_sha256":digest(out/"LICENSE.txt"),"executed":False}
(out/"build-summary.json").write_text(json.dumps(record,indent=2)+"\n",encoding="utf-8")
print("BUILT "+str(out),flush=True)
