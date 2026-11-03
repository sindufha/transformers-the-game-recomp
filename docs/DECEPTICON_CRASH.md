# Decepticon Chapter 1 shader-cache boundary — 0.5.16

The retained Android crash is SIGABRT in `hle_fatal -> d3dx_operation ->
sub_005B1350` while the requested Decepticon chapter is loading. The exact
runtime boundary is:

```
D3DXCompileShader requires a compiled shader cache (request captured)
```

This is an explicit missing-shader abort. It is distinct from Vulkan device
loss, the earlier mission-completion BMP encoder boundary, and unsupported
Bink movie playback. The same log also demonstrates an earlier successful
surface detach/reattach and continued presentation in PID 9114.

Evidence: `work/decepticon0516/{runtime,crash,exit-info}.log`.

The exact captured request has SHA-256
`8ee98631fef9fcc252527941f121bbae41eebc404756b47ebfefd29105f8f4a5`:
20,971 bytes of original HLSL source, `vertexProgram`, `vs_3_0`, flags 0x200,
no macros. The existing trusted SDK helper compiles it using the installed
`System32/d3dx9_31.dll`, producing 1,256 bytes of shader bytecode and a 392-byte
constant table, with no compiler diagnostics. The exact source, flags and
profile are preserved; no replacement shader or graphics reduction is used.
The helper does not load the game executable or game DLLs.

The first request increased the cache from 329 to 330 entries. During the user
retry, thirteen additional exact requests were captured, compiled and installed;
the final bundled snapshot contains 343 entries (14 additions). The shader service
atomically uploaded the new request/bytecode/constants directory to the
app-private cache and checked all three hashes. The existing development wait
marker was temporarily enabled to collect additional shaders during the user's
next retry without restarting after each cache miss. The service is bounded
and must remove the marker before ending collection. If USB is disconnected,
marker cleanup must be completed once ADB is restored.

Five existing cache/compiler tests and five bytecode-bound tests pass. All 14
new outputs also pass the production shader token parser, request/output hashes
and constant-table header checks. The user confirmed Decepticon Chapter 1 is
playable. PID 17412 continues with hundreds of draws/frame, active nonzero SFX
and no fatal message in the retained retry/gameplay tails. Evidence:
`work/decepticon0516/runtime-{retry,gameplay}-tail.log` and
`new-shaders-verified.json`.

The new APK bundles that complete frozen snapshot; native runtime, original
assets, driver, English controls and settings are otherwise unchanged. This
verification covers the reported loading failure and observed gameplay, not
the entire campaign or every possible graphics-setting permutation. The
assistant does not launch/control the game or take screenshots.

Build/deployment/user-test status: `work/decepticon0516/validation.json`.
