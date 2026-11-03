# Mission completion crash — 0.5.6

## Observed failure

The user reported a crash when finishing the first mission. The retained
0.5.5 runtime log ends with:

```
[win32hle] FATAL: D3D9 guest bridge: D3DXSaveSurfaceToFileInMemory encoder is unavailable
```

Android's crash buffer records SIGABRT at 2026-10-23 16:39:58 +0700 in the
game worker, followed by hle_fatal, d3dx_operation, sub_005E4260 and
sub_005E35B0. The corresponding earlier 0.5.4 crash has the same boundary.
This is an explicit unsupported-function abort, rather than evidence of
Vulkan device loss or an audio failure. Audio immediately beforehand has
active SFX and nonzero SFX samples, with creation/duplication failures 0/0.

Evidence: `work/crash055-mission1/{crash,runtime,logcat}.log`.
The selected PE was independently identified through REA open_binary as
SHA-256 `5aa3bd490f5b97f614a154fdca98cae845bb62c783c48e56042f19ac14aeefd0`.

## Original call and correction

Existing generated code for that input shows sub_005E4260 creates an
A8R8G8B8 system-memory surface, copies a render target into it, then calls
0x006644A0 at 0x005E436E with format 0, null palette and full-surface rect.
The input PE import parser independently maps its IAT 0x0067C304 to
D3DXSaveSurfaceToFileInMemory. Format 0 is BMP in the
[DirectX image-format enumeration](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dximage-fileformat).
The precise use of the resulting image beyond this transition is not claimed.

The bridge now encodes a valid 24-bit BMP from supported 32-bit surfaces and
returns a guest ID3DXBuffer with the real byte count and guest data address.
It supports full images/subrectangles, channel order conversion, bottom-up
rows and zero-filled DWORD padding. Temporary locks and readback resources
are released on errors; unsupported formats and invalid rectangles return
HRESULT failures. The native fallback handles non-lockable render targets
and resolves multisampling before system-memory readback. That fallback
compiled but was not exercised by the synthetic test.

The existing texture loader can read the resulting BMP back into BGRA, while
retaining DDS handling. BMP decoding is limited to bounded 24/32-bit BI_RGB;
PNG/JPEG/other image encoders, arbitrary resizing and mip generation are not
added. See the [save-surface contract](https://learn.microsoft.com/en-us/windows/win32/direct3d9/d3dxsavesurfacetofileinmemory)
and [bitmap layout](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/ns-wingdi-bitmapinfoheader).

## Verification

- Six native host BMP fixtures pass, including decoding the output with
  Pillow independently, exact colors/orientation, crops, alternate channel
  order, padded rows, top-down BMP input, truncation and rejected bounds.
- Eleven existing D3D9 bridge tests, five shader tests and six probe tests
  pass; one optional synthetic link fixture is skipped.
- The production ARM64 library passes --d3dx-image-self-test on the phone:
  synthetic system-memory COM surface -> encoder -> guest buffer -> parse
  and decode, exact pixels, cropped output, buffer pointer/size/Release,
  balanced locks and invalid inputs. No window, GPU, game entry or screenshot
  is used by that test.
- The same library passes the native runtime self-test and the muted real
  AAudio regression with 384 preloaded sources plus 64 SFX voices, zero
  creation/duplicate failures and zero xruns in that short test.
- Four ABIs build. Final installation and hashes are recorded in
  `work/crash056/validation.json` once deployment completes.

Completing the actual mission is still the user's test. The assistant does
not launch the app, send game input or capture the device screen.
