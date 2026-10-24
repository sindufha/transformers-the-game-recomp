# Project status

**Development snapshot: v0.5.17 — October 24, 2026**

[Download the Android preview](https://github.com/sindufha/transformers-the-game-recomp/releases/tag/v0.5.17).
ARM64, ARMv7, and universal packages are provided with checksums, build
information, and third-party notices. All retain the verified build's runtime
content and certificate; only ARM64 has physical-device gameplay evidence.

| Component | Current result |
| --- | --- |
| Recompilation | A native Android runtime has been built from the supported PC x86 input. |
| Android packaging | Android 10+ packages built for arm64-v8a, armeabi-v7a, x86, and x86_64. |
| Physical-device testing | ARM64 on POCO F3 / M2012K11AG, Android 13, Adreno 650. |
| Campaign testing | Autobot gameplay in The Suburbs and Decepticon Chapter 1 confirmed playable. |
| Graphics | Native DXVK D3D9-to-Vulkan rendering; resolution and graphics options exposed in the launcher. |
| Audio | Gameplay music, narration, and sound effects through DirectSound/AAudio. |
| Touch input | Robot stick, vehicle pads/pedals, camera drag, combat controls, and automatic profiles. |
| Mission prompts | ENTER controls restored for Start Mission and completion/failure confirmations. |
| Fullscreen | Immersive system-bar handling implemented, including restoration on resume. |
| Background resume | Session retained; native surface detach/reattach and subsequent rendering observed in the same process. |

## Recent results

- **v0.5.17:** The large menu BACK button sends Backspace (VK 8, DirectInput
  scan 0x0E). Its press/release behavior and independent ENTER hold pass the
  touch replay tests.
- **v0.5.16:** Fixed the observed Decepticon Chapter 1 loading abort caused by
  missing shader-cache entries. Fourteen exact vertex/pixel requests were
  compiled, expanding the local cache from 329 to 343. The tester then confirmed
  the chapter could be played.
- **v0.5.15:** English control labels and overlay hints.
- **v0.5.14:** Fullscreen and background-session lifecycle handling, with paused
  rendering/audio/timers and presentation recovery.

The latest control update passed 19 touch replay tests. The shader update passed
five cache/compiler tests, five token-bound tests, and validation of all 14 new
bytecode outputs. Native lifecycle, runtime, control-state, and BMP image probes
have also passed on ARM64. APK signature, alignment, packaged hashes, and retained
settings were checked during deployment.

## Remaining work

- Validate more missions, both campaigns, and additional devices.
- Implement Bink/FMV playback and an Android-side HLSL compilation path.
- Investigate remaining unsupported APIs and unobserved shader combinations.
- Improve frame pacing and performance across demanding scenes.
- Validate dedicated aircraft controls separately from the current vehicle layout.

Resume retains a live in-memory session. Force-stop, reboot, or process termination
ends that session; it is not a persistent save-state system.

The project currently accepts one supported executable build:

```text
SHA-256: 5aa3bd490f5b97f614a154fdca98cae845bb62c783c48e56042f19ac14aeefd0
```

These are development results, not a claim that every mission or configuration
works. Original files and detailed local investigation artifacts are not published.
