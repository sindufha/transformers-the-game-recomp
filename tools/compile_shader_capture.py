"""Compile an exact captured HLSL request with the installed Windows SDK DLL.

Never loads the game's executable or DLLs. Outputs input, DX9 bytecode and CTAB
under the SHA256 of the complete request for reproducible offline cache use.
"""
from __future__ import annotations
import argparse
import ctypes as C
import hashlib
import json
import os
from pathlib import Path
import struct


def decode_request(data: bytes) -> dict:
    if data[:8] != b"TFHLSL01":
        raise ValueError("Invalid shader request magic")
    pos = 8
    def word():
        nonlocal pos
        if pos + 4 > len(data): raise ValueError("Truncated request")
        value = struct.unpack_from("<I", data, pos)[0]; pos += 4
        return value
    def block(nullable=False):
        nonlocal pos
        size = word()
        if nullable and size == 0xffffffff: return None
        if size > 4*1024*1024 or pos+size > len(data): raise ValueError("Invalid request field size")
        value=data[pos:pos+size]; pos+=size
        return value
    flags=word(); source=block(); entry=block(True); profile=block(True)
    count=word()
    if count > 256: raise ValueError("Too many macros")
    macros=[(block(True),block(True)) for _ in range(count)]
    if pos != len(data) or not source or not entry or not profile:
        raise ValueError("Invalid request fields or trailing data")
    if any(name is None or b"\0" in name or (value is not None and b"\0" in value) for name,value in macros):
        raise ValueError("Invalid macro strings")
    if b"\0" in entry or b"\0" in profile: raise ValueError("Invalid compiler strings")
    return dict(flags=flags,source=source,entry=entry,profile=profile,macros=macros)


def compile_request(data: bytes, output: Path) -> dict:
    request=decode_request(data)
    if os.name != "nt": raise RuntimeError("Windows SDK compiler is required")
    # The supported executable imports d3dx9_31.dll. Later compilers reject
    # writes to HLSL globals which this game's legacy source requires.
    dll_path=Path(os.environ["SystemRoot"])/"System32/d3dx9_31.dll"
    dll=C.WinDLL(str(dll_path))
    class Macro(C.Structure):
        _fields_=[("name",C.c_char_p),("value",C.c_char_p)]
    macros=(Macro*(len(request["macros"])+1))()
    for i,(name,value) in enumerate(request["macros"]): macros[i]=Macro(name,value)
    shader=C.c_void_p(); errors=C.c_void_p(); table=C.c_void_p()
    compile_fn=dll.D3DXCompileShader
    compile_fn.argtypes=[C.c_void_p,C.c_uint,C.POINTER(Macro),C.c_void_p,C.c_char_p,C.c_char_p,C.c_uint,C.POINTER(C.c_void_p),C.POINTER(C.c_void_p),C.POINTER(C.c_void_p)]
    compile_fn.restype=C.c_int32
    source=C.create_string_buffer(request["source"])
    def method(obj,slot,result,*types):
        address=C.cast(obj,C.POINTER(C.POINTER(C.c_void_p))).contents[slot]
        return C.WINFUNCTYPE(result,C.c_void_p,*types)(address)
    def blob(obj):
        if not obj.value:return b""
        size=method(obj,4,C.c_uint)(obj)
        pointer=method(obj,3,C.c_void_p)(obj)
        if size>16*1024*1024:raise ValueError("Compiler output too large")
        return C.string_at(pointer,size)
    status=compile_fn(source,len(request["source"]),macros,None,request["entry"],request["profile"],request["flags"],C.byref(shader),C.byref(errors),C.byref(table))
    try:
        diagnostics=blob(errors).decode("utf-8",errors="replace").rstrip("\0")
        if status<0: raise RuntimeError(f"HLSL compiler failed {status&0xffffffff:08x}: {diagnostics}")
        bytecode=blob(shader); ctab=blob(table)
        if not bytecode or not ctab:raise RuntimeError("Compiler did not return bytecode and constant table")
        digest=hashlib.sha256(data).hexdigest(); directory=output/digest
        directory.mkdir(parents=True,exist_ok=True)
        (directory/"request.bin").write_bytes(data)
        (directory/"shader.bin").write_bytes(bytecode)
        (directory/"constants.bin").write_bytes(ctab)
        summary=dict(request_sha256=digest,shader_sha256=hashlib.sha256(bytecode).hexdigest(),constant_sha256=hashlib.sha256(ctab).hexdigest(),compiler=str(dll_path),compiler_sha256=hashlib.sha256(dll_path.read_bytes()).hexdigest(),entry=request["entry"].decode(),profile=request["profile"].decode(),flags=request["flags"],shader_bytes=len(bytecode),constant_bytes=len(ctab),diagnostics=diagnostics)
        (directory/"summary.json").write_text(json.dumps(summary,indent=2)+"\n",encoding="utf-8")
        return summary
    finally:
        for obj in (shader,errors,table):
            if obj.value:method(obj,2,C.c_uint)(obj)


if __name__ == "__main__":
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("request",type=Path)
    parser.add_argument("--out",type=Path,default=Path("work/shader-cache"))
    args=parser.parse_args()
    print(json.dumps(compile_request(args.request.read_bytes(),args.out),indent=2))
