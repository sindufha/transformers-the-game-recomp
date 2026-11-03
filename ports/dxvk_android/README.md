# DXVK Native Android backend

The backend uses official DXVK Native tag `native-1.9.2b`, pinned commit
`c8dc91fabd00cac11d697ccf07426e798393cd40` from
<https://github.com/Joshua-Ashton/dxvk-native>. It supplies D3D9's native host
implementation and its DXSO shader translator; the project's separate runtime
bridge is responsible for guest pointers and COM calls.

The project adapter in `android_wsi.cpp` creates an Android Vulkan surface from
an `ANativeWindow`. `prepare.py` makes bounded replacements in a generated copy
of the source, leaving the external checkout clean. It adapts CPU intrinsics,
optional Vulkan features, landscape suboptimal presents, cleanup, and logging.
The requirements were compared with the NFSU2 reference port; its WSI, pipeline
cache, renderer, and game-specific code were not copied into this backend.

Run from the project root with Python and the installed Android NDK:

```powershell
git clone --branch native-1.9.2b --depth 1 https://github.com/Joshua-Ashton/dxvk-native.git external/dxvk-native
python tools/build_spirv_tools.py --abi arm64-v8a
python tools/build_spirv_tools.py --abi armeabi-v7a
python tools/build_spirv_tools.py --abi x86
python tools/build_spirv_tools.py --abi x86_64
python tools/build_dxvk.py --abi arm64-v8a
python tools/build_dxvk.py --abi armeabi-v7a
python tools/build_dxvk.py --abi x86
python tools/build_dxvk.py --abi x86_64
```

API 29 (Android 10) is the minimum. No WSL, Gradle, downloaded shader tools, or
network access is needed after cloning. The builder uses NDK `glslc` to compile
all 54 embedded shaders for Vulkan 1.1 and validates them with `spirv-val`.

The NDK-shipped SPIRV-Tools optimizer is linked statically. Fragment shader
modules freeze the actual Vulkan pipeline specialization values, propagate
constants, and remove dead sampler branches before driver compilation. This
avoids legacy DXSO's statically aliased 2D/3D/cube views on mobile GPUs while
retaining the selected texture type. Source/archive hashes and the NDK notice
are retained; no optimizer binary is downloaded. `tools/test_spirv_specialize.py`
executes all three dimension variants on the connected Android device and
validates their output using the NDK's host `spirv-val`.

Outputs are ignored local artifacts:

- `work/dxvk-source/`: prepared upstream source and public headers.
- `work/dxvk-<abi>/libdxvk_android.a`: position-independent archive for the app.
- `work/dxvk-<abi>/libdxvk_android.so`: independently linked backend with
  `--no-undefined`, a static C++ runtime, and 16 KiB ELF segment alignment.
- `work/dxvk-<abi>/dxvk-build-summary.json`: revision, hashes, target, and limits.

Consumers of the static archive need the NDK C++ runtime and `vulkan`, `android`,
`log`, `dl`, and `atomic`. Native headers live under
`work/dxvk-source/include/native/windows` and `directx`.

The exported WSI API is declared in `android_wsi.h`. Set a retained window before
creating a D3D9 device. Use the token returned by `dxvkAndroidGetWindowHandle()`
for the native device/presentation HWND. It is distinct from a guest HWND and
from the `ANativeWindow` pointer. Stop rendering before detaching/replacing a
window. Device/swapchain recreation remains the caller's responsibility.

Compilation and linkage do not establish GPU compatibility or game playability.
Vulkan 1.1 alone does not guarantee every feature used by this DXVK revision.
The backend has no software BC texture fallback, no custom Vulkan driver loader,
and no persistent pipeline-cache adapter. Missing clip-distance and non-solid
fill features reduce those capabilities on affected GPUs.

## Third-party source notices

- DXVK Native uses the zlib license, retained in `DXVK-LICENSE` and the prepared
  tree's `LICENSE`. The original authors are Philip Rebohle and Joshua Ashton.
- The bundled native DirectX headers include Wine-derived material licensed
  under LGPL 2.1 or later. Their existing copyright/license notices are kept in
  both the clean checkout and the generated copy; they are not relicensed.
- `include/vulkan_android.h` is Khronos Vulkan-Headers `v1.2.189`, with its
  Apache-2.0 notice retained. It was sourced through the pinned NFSU2 reference
  checkout, whose `ports/dxvk_android/README.md` identifies that upstream version.

The altered source and reproducible recipe are available locally. This backend
does not include game executable/data files.

## Native device query

`python tools/build_dxvk_query.py --abi arm64-v8a` links a small standalone
`work/dxvk-arm64-v8a/dxvk-query` executable against the same archive. It calls
`Direct3DCreate9`, enumerates adapters, queries D3D9 capabilities, and checks
texture formats. It does not load a guest EXE or create a Vulkan logical device,
surface, or swapchain. It can run from `/data/local/tmp` using `adb shell`.

The connected Android 13 / Adreno 650 device passed this query: one adapter,
Vulkan 1.1.128, VS/PS 3.0, 16384-square textures, 16 streams, and uncompressed
ARGB/XRGB textures. DXT1, DXT3, and DXT5 were rejected, consistently with that
driver's missing BC feature. The observed output and exit code are recorded in
`work/device/dxvk-query.log` and `dxvk-query-summary.json`. This establishes
native adapter initialization, while a windowed clear/present and game execution
are separate integration checks.
