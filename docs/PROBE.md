# Portable diagnostic probe

`tools/build_probe.py` produces an Android command-line ELF, not an APK.
It supports `--abi arm64-v8a`, `armeabi-v7a`, `x86`, and `x86_64`, minimum API 29.
It links generated C with the pinned `host_lite.c`, `guest_mem.c`, and
`pe_loader.c` runtime spine. The tool does not execute the ELF, launch the
original game, deploy to a device, or copy assets.

```powershell
python tools/build_probe.py --abi arm64-v8a --startup --lift-dir work/lift-5aa3bd490f5b-target --out work/startup/probe-arm64-target --core-archive work/build-arm64-target/libtransformers_lift.a
```

An incomplete lift, changed frozen executable, failed instruction marker,
missing dispatch entry, or changed pinned runtime fails before compilation.
The Android link uses `--no-undefined` and produces PIE with 16 KiB maximum
page alignment. `probe-build-summary.json` records source and artifact hashes,
the compiler, target, diagnostics, and explicit execution/playability flags.

The copied `recomp_types.h` stops on missing indirect-call/tail targets and
`STUB` invocations. It must be the first include directory for **every**
generated source; mixing ordinary and strict header objects is unsupported.
The source checkout stays unchanged. The copied host delegates register
save/load to a complete snapshot which includes SSE, MXCSR, arithmetic flags,
segment bases/selectors, x87 environment and integer shadow values. These are
structural safeguards; successful linking does not establish x86 equivalence.

`--startup` registers the selected Win32 API subset described in
[STARTUP.md](STARTUP.md). Without this flag all imports remain individually
named fatal boundaries. Unregistered imports, SEH/C++ landing-pad operations
and protected-mode software interrupts stop explicitly because the required
bridges/exception delivery are absent. There is no rendering, input or audio.

The executable accepts:

- `--provenance`: print the compiled input hash and entry address.
- `--self-test`: synthetic memory, SHA-256, callback-stack and thread-state
  tests. It does not enter any generated game function.
  A startup-enabled binary additionally tests selected startup APIs.
- `--map path`: hash-check the exact build input, map it and bind diagnostic
  startup APIs and diagnostic import boundaries, then return without entering
  guest code.
- `--run path`: explicitly enter lifted code for a supplied matching image.
  The first unsupported operation terminates. This is not a gameplay test.

`TF_TEST_ANDROID_LINK=1 python -m unittest discover -s tests -p test_probe.py -v`
links a synthetic PE fixture with the same runtime and validates the ELF.
Without a connected Android device this proves build/link integrity only;
the runtime self-test has not been observed executing on Android.

For separately built core archives, compile using
`prepare_runtime(lift_dir, output_dir)` from `tools/build_probe.py`, put its
`include_dir` first, and pass `--core-archive path/to/core.a`. Supply a sibling
`core.a.provenance.json` containing `input_sha256`, `target`,
`strict_header_sha256`, `archive_sha256`, and `runtime_overlay_sha256` (the
`transformers_x87.h` and `transformers_runtime.h` hash mapping). This prevents accidentally linking
the earlier permissive-dispatch objects as if they used the strict header.

ARMv7 builds add `-fmax-type-align=1` to avoid alignment-sensitive
LDRD/STRD/VLDR/VSTR instructions on generic guest-memory pointers. A compiler
assembly regression test covers integer/double reads and writes. This does
not solve potential guest/host VA collisions in a 32-bit process or prove
runtime compatibility on ARMv7 devices.
