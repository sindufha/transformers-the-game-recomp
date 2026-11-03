# D3D9 guest bridge

The bridge in `runtime/d3d9_bridge.cpp` translates the recompiled game's
32-bit Direct3D 9 calls into native DXVK COM calls. It has no game addresses,
resolution hooks, object-retention patches, or copied NFS-specific behavior.

`tools/generate_d3d9_bridge.py` reads DXVK native revision
`c8dc91fabd00cac11d697ccf07426e798393cd40` and emits 297 base D3D9
method wrappers, including their exact x86 stdcall argument counts. The
generated metadata records the upstream header digest and every slot.
Generated files are checked for reproducibility in tests. The native Windows
and DirectX headers retain their upstream notices and licenses.

## Host integration

Call under the existing guest machine lock:

1. Configure the Android window and DXVK window system.
2. `tf_d3d9_configure(base, 0x100000000ull, allocator, release, native_window)`.
   The allocation callbacks return/release guest VAs. `base` is zero for the
   32-bit flat-memory runtime. The address limit is `uint64_t` on every ABI.
   `native_window()` returns `dxvkAndroidGetWindowHandle()`, an opaque native
   WSI token, rather than a guest HWND or the raw Android window pointer.
3. `tf_d3d9_register()` before binding the game's IAT.
4. Optional `tf_d3d9_smoke(width,height)` exercises actual guest32 import
   dispatch and COM vtables through native CreateDevice, Clear, and Present.
5. Enter the recompiled game. Missing paths remain fatal and name the API.

The existing shim registry assigns callable guest VAs to every method.
No changes to the recompiled indirect-call dispatcher are required.
The bridge expects the machine lock to serialize object maps and callbacks.

## Implemented paths

- Direct3D creation, capability checks, device creation/reset, clear/present,
  render state, transforms, streams, indexed/non-indexed draws, declarations,
  shader-model 2/3 bytecode, shader constants, queries, and state blocks.
- Native resource object inputs and outputs use typed guest handles. AddRef,
  Release, supported QueryInterface and surface GetContainer use explicit
  owned-reference accounting. Internally retained native objects can keep a
  dormant identity wrapper until acquired again or shutdown.
- Guest lock results for 2D/cube textures, surfaces, vertex and index buffers
  use allocated guest staging memory. Unlock copies changed rows back and
  releases the staging allocation. Native mapped pointers never enter guest
  memory.
- Pointer-sized presentation/creation/lock structures are marshalled. Native
  `D3DADAPTER_IDENTIFIER9` tail padding is also isolated: the guest receives
  exactly its 1100-byte structure even when native sizeof is 1104.
- D3DXMatrixMultiply (including either-input aliasing), D3DXMatrixOrthoLH,
  and guest-backed ID3DXBuffer creation, access, and COM references.
- D3DX legacy DDS uploads for 2D/cube textures with existing mip chains,
  common uncompressed formats, and DXT1 through DXT5. If the native device
  rejects compressed texture creation, the D3DX path decodes BC blocks on
  the CPU into A8R8G8B8. It preserves compressed support results for direct
  guest CreateTexture/CheckDeviceFormat calls; it does not falsely report
  native BC support. D3DX_FROM_FILE forbids the fallback format substitution.

Android presentation uses a host-managed window. The adapter retains the
guest HWND in device creation metadata and never truncates native handles.
Failure to acquire an Android surface returns D3DERR_DEVICELOST.

## Explicit gaps

D3DXCreateEffectCompiler, image encoding, non-DDS image
decoding, DDS resizing/color key/palette conversion, and DDS mip generation
are not implemented. DDS default loads retain the supplied mip chain.
Shader models 1, 2 and 3 have bounded token validation. Legacy VS/PS 1.1
creation is confirmed in the actual game's Android startup.

`D3DXCompileShader` accepts only exact cached requests: source bytes, entry,
profile, flags and macros must match. `tools/compile_shader_capture.py` uses
the trusted installed Windows System32 D3DX9_31 compiler, matching this
executable's import version. No game DLL is executed. Unknown requests are
captured and remain explicit runtime failures. This is an offline cache,
not a general Android HLSL compiler. `--shader-cache work/shader-cache` bundles
verified requests/bytecode/CTAB into the APK; the launcher installs them with
streaming SHA-256 verification. No external compiler runs on the phone.

The guest32 constant-table adapter provides descriptions, top-level lookups,
defaults and scalar/vector/matrix register upload. Nested structs and array
element handles remain explicit boundaries. Bink decoding and audio are not
implemented by this graphics adapter.
Volume pointer/lock methods, native GDI surface DCs, software-device callback
registration, shared OS handles, private IUnknown data, unknown COM
interfaces, native monitor handles, and GetNPatchMode's x87 float return
stop explicitly rather than returning fake success. Other DLL startup,
input, audio and exception support are separate subsystems.

The bridge's span checks enforce the 32-bit address-space boundary; they do
not prove that every guest page in a span is committed. The PE/guest memory
runtime remains responsible for actual mappings and fault handling.
This adapter is a rendering bring-up path, not a claim that gameplay works.

## Verification

The two C++ translation units compiled for arm64-v8a, armeabi-v7a, x86 and
x86_64 at API 29 with NDK 27.1.12297006. ARMv7 compilation uses the same
`-fmax-type-align=1` guest-access policy as the existing runtime.
The x86 headers emit expected upstream pragma-pack warnings.

`tests/test_d3d9_bridge.py` checks generated ABI output and executes a
freestanding native fixture for DDS truncation/mip/cube bounds, BC color and
alpha interpolation, odd-size output guards, aliasing matrix multiplication,
and orthographic projection. These tests execute synthetic helper code only.
The native GPU smoke additionally requires a real Android surface; successful
compilation alone does not establish device rendering or game compatibility.

Primary ABI references: pinned DXVK native `include/native/directx/d3d9.h`
and `d3d9types.h`; ID3DXBuffer IID/method order is also defined in
[Wine's d3dx9core.h](https://github.com/wine-mirror/wine/blob/master/include/d3dx9core.h).
