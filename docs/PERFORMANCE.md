# Performance iteration 0.5.3

Scope: reduce CPU overhead without reducing resolution, graphics options,
draws, simulation work, floating-point precision or shader quality. The game
was closed for this revision; the assistant did not launch it or control UI.

## Changes

- The prepared host runtime caches successful exact-VA dispatch lookups in
  1,024 slots. Collision misses use the original sorted-table search. Out-of-range
  addresses return immediately, avoiding a failed search of 14,470 generated
  entries before each imported Win32/COM method resolves. Import trampolines
  are not cached: their pending-shim metadata is still updated every time.
  The cache shares the existing guest-machine lock contract; the dispatch
  table is immutable. The pinned upstream checkout and lifted core are unchanged.
- D3DX constant setters reuse conversion storage owned by their constant table
  instead of allocating/freeing a vector on every upload. Name lookup uses
  a non-owning string view rather than allocating a string. The conversion,
  register layout, zero padding and upload calls are unchanged. DXVK's
  `SetShaderConstants` copies into its state synchronously (including recording
  into state blocks); it does not retain the temporary input pointer.
- Repeating `NUWARNING` messages from DynoScene retain four samples per second
  and a suppressed-message count. Other guest diagnostics and native fatal
  messages remain unthrottled. This reduces unbuffered file writes from a noisy
  development executable; scene logic and draws still execute normally.
- Successful Present calls report FPS, average wall frame time, presenting-thread
  CPU time, Present duration, and COM/draw API counts every two seconds. There
  are no GPU queries, extra GPU waits or frame limit changes. The initial
  loading interval is excluded. `worker_cpu_ms` excludes DXVK worker threads;
  `present_ms` is host API duration and is not a GPU timer.

The source helper fixtures verify exact dispatch membership, table holes,
out-of-range/import addresses, empty tables, random hit/miss sequences and
collision-heavy table sweeps. They also verify warning budgets across clock
wrap and frame-window/reset accounting.

## Device component benchmark

The headless `tools/benchmark_dispatch.c` loads either the 0.5.2 baseline library
or rebuilt production ARM64 library and calls only `recomp_lookup`. It never
calls a returned function, the game entrypoint, renderer or input. Every one
of the 14,470 table addresses resolves in both libraries. Each timing sample
performs 2,000,000 lookups; the median of three alternating baseline/candidate
runs on the same Adreno 650 phone is recorded below.

| Lookup operation | Baseline | Candidate | Component speedup |
| --- | ---: | ---: | ---: |
| Repeating 64 valid game-function addresses | 38.871 ns | 4.864 ns | 7.99x |
| Out-of-range import VA rejection before shim lookup | 37.287 ns | 2.049 ns | 18.20x |

These are lookup microbenchmarks, **not gameplay FPS improvements**. GPU time,
game logic, translation overhead outside this lookup, and driver/compiler
stalls can still dominate a frame. Actual gameplay FPS remains for the user's
test and the new `[perf]` output. No sustained 30 FPS claim is made.

The rebuilt ARM64 runtime also passes its native headless self-tests covering
memory, SHA-256, callbacks, complete thread-state preservation, file I/O/64-bit
seek, suspended-thread lifecycle/TLS, USER32/COM, random/UUID and input helpers.
The first self-test attempt used adb's unwritable root cwd and failed its file
creation check; rerunning in the disposable test directory passed. No production
code change was needed for that test-environment issue. The test directory was
removed after collecting evidence.

Host results: 25 tests pass, one optional Android synthetic-link test is skipped.
The four native ABI runtimes build with one compile worker to limit host memory
use. The APK retains the exact 0.5.2 shader-cache files and Turnip driver.
Installation verifies that graphics preferences and pcconfig hashes are unchanged.

Evidence:

- `work/performance053/dispatch-benchmark.json`
- `work/performance053/native-selftest.log`
- `work/device/performance053-validation.json`
- `work/android-performance-053/apk-build-summary.json`

Artifact: `work/android-performance-053/transformers-dxvk-universal-debug.apk`.
