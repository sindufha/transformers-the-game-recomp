# Offline CPU optimization: 0.5.19

No game session, device input, GPU benchmark, or network access is required for
this revision's analysis and validation. Graphics options, draw calls, shader
cache, Vulkan driver, simulation, and floating-point arithmetic are retained.

## Existing evidence

The last 20 stored windows in
`work/decepticon0516/runtime-verified.log` have medians of 24.175 FPS,
41.365 ms/frame, and 39.310 ms of presenting-thread CPU time per frame.
This supports prioritizing CPU overhead. The 0.110 ms Present API duration is
asynchronous host-call timing; it does not measure GPU execution time.
These windows are previous gameplay, not an after-change FPS measurement.

## Changes

- x87 helpers maintain the occupied-register count from the slot's old and new
  empty/nonempty state. The previous implementation scanned all eight tags after
  each mark, push, pop, or free. Physical stack rotation, arithmetic, value/tag
  classification, fault handling, and environment save/load are unchanged.
  Whole-tag writes still recompute the count. The generated source contains
  155,997 push/pop sites and 9,865 explicit mark sites; these are static counts,
  not a runtime profile.
- The D3D9 COM registry has a 256-slot exact-handle pointer cache in front of
  its existing ordered map. Registry nodes remain authoritative. Collisions
  fall back to that map; retirement invalidates before erasure. Every cache hit
  still checks the current reference count and callers still check interface
  kind. It does not cache graphics state, omit setters, or skip draws.

## Offline validation

The occupancy fixture covers all 524,288 tag-word/TOP combinations, every
logical destination, repeated frees, NaN/Inf/zero classification, and eligible
push/pop round trips. Existing hardware x87 environment differential tests
continue to pass. COM tests cover exact identity, collisions, invalidation,
address reuse, and 10,000 live objects.

An alternating Windows x64 synthetic benchmark of two pushes, a marked
arithmetic result, and two pops measured 35.671 ns before versus 14.058 ns after
(median of seven samples, 2,000,000 iterations each). That is 2.54x for this
helper workload, not a prediction of Android FPS. In the analogous ARM64
fixture the optimized helper bodies inline into the workload, eliminating its
four outlined helper calls.

That inlining also has a footprint cost: production ARM64 `.text` grows from
74,680,328 to 90,361,560 bytes (21.0%). The faster helper benchmark therefore
does not establish a whole-game speedup or rule out instruction-cache effects.
The packaged universal APK is 171,549,844 bytes. Keep the 0.5.18 APK available
for an actual user-controlled comparison.

All four generated-code archives must be rebuilt because the x87 overlay is
part of their validated source closure. Builds use one compiler worker to
limit memory use. APK checks retain the original signing certificate and
verify native provenance and unchanged shader/driver/asset bytes.

Actual Android frame rate after these changes remains for user testing; no
sustained 30 FPS result is claimed from host microbenchmarks.

Evidence: `work/performance0519/`, `tests/test_runtime_hotpaths.py`,
`tests/test_x87.py`. APK: `work/android-performance-0519/transformers-dxvk-universal-debug.apk`.
