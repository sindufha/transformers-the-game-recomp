# Vulkan bring-up evidence

On M2012K11AG / Adreno 650 the actual game drew its Transformers loading logo,
then reported `VK_ERROR_DEVICE_LOST`. This is not a playable-game result.
`work/device/game-after-network.log` and `game-latest.png` retain evidence.

The development APK can include Khronos's Android validation layer with
`--validation-layer work/validation/libVkLayer_khronos_validation.so`
and one matching `--abis` target. Normal universal builds omit this library.
The layer is version `vulkan-sdk-1.3.290.0`, downloaded from the official
KhronosGroup release. Archive SHA-256:
`8eb74be5403f72caac9502c2d5eb5a9353dfcb10695f198374d6e481445036c1`.
The package includes its license. See
[Android's validation-layer guide](https://developer.android.com/ndk/guides/graphics/validation-layer).

Before candidate fixes, the real-device log contained:

- `VUID-VkSwapchainCreateInfoKHR-compositeAlpha-01280`: unsupported opaque alpha.
- `VUID-vkCmdDraw-magFilter-04553`: the zero-swizzled R32 float dummy image
  lacks linear-filter support on this GPU.
- `VUID-VkRenderPassBeginInfo-renderPass-00904` and
  `VUID-vkCmdDraw-renderPass-02684`: load/store render-pass variants had
  dependencies incompatible with the pipeline/framebuffer default pass.

`work/device/vulkan-validation-summary.json` records the observed errors.
The bounded Android preparation recipe now selects a supported alpha mode,
uses a filterable RGBA8 sampled dummy with the same zero swizzle, and supplies
identical conservative dependencies across render-pass variants. These changes
must be checked on device; compiling them alone is not verification.

GPU debug settings were scoped to this diagnostic package. Their original
values are recorded in `work/device/gpu-debug-settings-baseline.json` and must
be restored when the validation run finishes. The normal build must not rely
on enabled external validation settings.

The first candidate removed the original four VUIDs. Later loading exposed
`VUID-vkCmdDraw-viewType-07752`: legacy SM1 shaders referenced 2D, 3D and cube
samplers at the same binding despite pipeline specialization selecting one.
The Android port now links the NDK-shipped SPIRV-Tools optimizer to freeze the
actual pipeline values and eliminate inactive branches before vkCreateShaderModule.
There is no forced texture dimension. The native Android fixture passed all
three selected types, each retaining exactly one descriptor of the right
dimension, with valid Vulkan 1.1 SPIR-V output. Evidence:
`work/spirv-selftest/result.json`. Game rendering must still be verified.

The strengthened fixture uses DXSO's constant bitfield extraction and switch,
not merely a direct comparison. NDK 27's optimizer did not fold constant
`OpBitFieldUExtract`; the Android adapter now folds valid scalar 32-bit extracts
before dead-branch elimination. All three typed fixture variants passed on the
phone. The fresh game run loaded the validation layer, reported no VUIDs, and
rendered the full licensing screen. It still encountered `VK_ERROR_DEVICE_LOST`
later. Evidence: `work/device/vulkan-validation-bitfield-fresh.log`,
`bitfield-fresh-runtime.log`, and `bitfield-fresh-screen.png`.

An explicitly created private `gpu-debug-draws` marker enables per-draw event
queries and bounded shader captures under `gpu-debug-shaders`. With this mode,
1,757 draws completed; the following fullscreen triangle-strip draw timed out.
Its captured PS 1.1 is a texture-copy shader (SHA-256
`dd3a10fbc32d8f316e77f3723ce66a44697bdfc923f1c1a70a2d5081bf8c8449`).
The captured fragment modules passed Vulkan 1.1 SPIR-V validation. This isolates
the hang; it does not establish whether the remaining cause is the driver,
resource synchronization, or another runtime issue.
Evidence: `work/device/draw-trace-runtime.log` and `gpu-shaders/`.

## App-local Turnip result

The standard SPIRV-Tools performance passes removed inactive private-register
traffic and unused PointCoord input, but the stock driver still hung on the
fullscreen texture copy. Turnip then passed the same native triangle/readback
test and all 180 presents, followed by the guest32 COM smoke test.
`work/device/turnip-smoke.log` records Turnip Adreno 650, Vulkan 1.3.296.

The game subsequently reached the Autobot campaign, The Suburbs, and rendered
the actual 3D world. Forward movement, jump and pause were observed. Screenshots
and logs are named `turnip-enter-level`, `turnip-move-w`, `turnip-space` and
`turnip-pause` under `work/device/`. This establishes basic first-level gameplay,
not complete campaign coverage or a performance guarantee.

The APK loader helpers are built from BSD-2-Clause
[libadrenotools](https://github.com/bylaws/libadrenotools), pinned at
`8fae8ce254dfc1344527e05301e43f37dea2df80`, and its pinned liblinkernsbypass
submodule `aa3975893d83ef1bc84c321ec60c65fbf1287887`.
`tools/build_adrenotools.py` records library hashes, verifies ELF architecture,
rejects undefined symbols/unbundled C++ dependencies, and copies the license.
Only the custom driver feature is enabled; GPU memory import/turbo hooks are
not requested.

The driver is the community-built
[K11MCH1 Turnip 24.3.0 R9v2 release](https://github.com/K11MCH1/AdrenoToolsDrivers/releases/tag/v24.3.0_r9),
based on Mesa commit `4497ea2`, not an official Mesa binary release.
Archive SHA-256: `8a781186ed96d7f9b1620ae4caa508eb121b78af4b3777f0907dada34ce6714a`.
Driver SHA-256: `3a1d3425a87cfd0ebc9cc87a03a387f4814f9e6f6084457f16756400c44e385e`.
Provenance and the upstream Mesa licensing document are retained under
`work/turnip/` and included in APK notices when this driver is bundled.

Normal packaging adds `--adreno-tools work/adrenotools --turnip-driver work/turnip`.
The ARM64 runtime probes the system adapter before selecting the packaged
driver for Qualcomm Adreno 6xx. Other adapters and ABIs use the system loader.
The actual resulting device properties are logged, so requesting a custom
loader is not mistaken for proof that it loaded. Bundled selection on the
tested 650 is recorded in `work/device/bundled-turnip-startup.log` with the
private development-driver marker absent.

Global GPU debug settings and the synchronization-validation property have
been restored to their original values (`work/device/gpu-settings-restored.json`).
Turnip is loaded only inside the separate game process; no system driver is
installed or replaced.
