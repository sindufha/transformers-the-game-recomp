"""Build pinned, app-local ARM64 Vulkan driver loading helpers with the NDK."""
import json
import os
from pathlib import Path
import subprocess
from build_probe import compile_one,find_compiler,sha256
from android_targets import validate_elf
from preflight import ROOT

source=ROOT/"external/libadrenotools"
linker=source/"lib/linkernsbypass"
pins={source:"8fae8ce254dfc1344527e05301e43f37dea2df80",linker:"aa3975893d83ef1bc84c321ec60c65fbf1287887"}
for path,pin in pins.items():
    if subprocess.check_output(["git","-C",str(path),"rev-parse","HEAD"],text=True).strip()!=pin:raise ValueError("Adreno Tools pin differs")
    if subprocess.check_output(["git","-C",str(path),"status","--porcelain","--untracked-files=no"],text=True).strip():raise ValueError("Modified Adreno Tools dependency")
out=ROOT/"work/adrenotools";out.mkdir(parents=True,exist_ok=True)
temporary=out/"tmp";temporary.mkdir(exist_ok=True);os.environ["TMP"]=os.environ["TEMP"]=str(temporary)
compiler=find_compiler(None);cxx=compiler.with_name("clang++.exe")
flags=["--target=aarch64-linux-android29","-O2","-g0","-fPIC"]
for include in (source,source/"include",linker):flags += ["-I",str(include)]
def compile_file(path):
    obj=out/(path.stem+".o");language=["-std=c++17"] if path.suffix==".cpp" else ["-std=c11"]
    visibility=["-fvisibility=hidden"] if path.parent.name=="hook" else []
    result=compile_one(cxx if path.suffix==".cpp" else compiler,flags+language+visibility+["-c",str(path),"-o",str(obj)],obj.with_suffix(".rsp"))
    if result["exit_code"]:raise RuntimeError("Adreno Tools compilation failed: "+str(path))
    return obj
objects={}
for path in [linker/"android_linker_ns.cpp",linker/"elf_soname_patcher.cpp",source/"src/bcenabler.cpp",source/"src/driver.cpp",source/"src/hook/hook_impl.cpp",source/"src/hook/main_hook.c"]:
    objects[path.stem]=compile_file(path)
records={}
archive=out/"liblinkernsbypass.a"
subprocess.run([str(compiler.with_name("llvm-ar.exe")),"rcs",str(archive),str(objects["android_linker_ns"]),str(objects["elf_soname_patcher"])],check=True)
for name,items in [("hook_impl",["hook_impl"]),("adrenotools",["bcenabler","driver"]),("main_hook",["main_hook"])]:
    path=out/("lib"+name+".so")
    command=flags+["-shared","-Wl,--no-undefined","-Wl,-z,max-page-size=16384","-Wl,-soname,"+path.name,"-Wl,--exclude-libs,ALL","-static-libstdc++",*[str(objects[n]) for n in items],"-landroid","-llog","-ldl","-o",str(path)]
    if name=="main_hook":command += ["-L",str(out),"-lhook_impl","-Wl,-z,global"]
    else:command += [str(archive)]
    result=compile_one(cxx,command,out/(name+"-link.rsp"))
    if result["exit_code"]:raise RuntimeError("Adreno Tools link failed: "+name)
    validate_elf(path.read_bytes()[:64],"arm64-v8a")
    dynamic=subprocess.check_output([str(compiler.with_name("llvm-readelf.exe")),"-d",str(path)],text=True)
    if "libc++_shared.so" in dynamic or "TEXTREL" in dynamic:raise ValueError("Unexpected Adreno Tools dependency or relocation")
    records[path.name]={"sha256":sha256(path),"bytes":path.stat().st_size}
(out/"LICENSE.txt").write_bytes((source/"LICENSE").read_bytes())
summary={"abi":"arm64-v8a","api":29,"build_tool_sha256":sha256(Path(__file__)),"pins":{str(k.relative_to(ROOT)):v for k,v in pins.items()},"libraries":records,"license_sha256":sha256(out/"LICENSE.txt"),"mapping_hooks_enabled":False,"executed":False}
(out/"build-summary.json").write_text(json.dumps(summary,indent=2)+"\n")
print("BUILT pinned Adreno Tools: "+", ".join(records))
