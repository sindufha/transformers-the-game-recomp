# GPU driver library — 0.5.23

The launcher's GPU DRIVER card offers:

- Automatic: retains the bundled Turnip 26.2.5 selection for ARM64 Adreno 6xx
  and the existing system-loader fallback on other devices.
- System driver: explicitly selects Android's system Vulkan loader.
- Imported ZIPs: Add GPU driver (.zip) copies the selected driver archive into
  the app-private driver library, extracts it, and retains its original name
  in the choices. It does not copy game data.

Selection persists in `gpu_driver` preferences and applies to a new native
game session. Returning to a surviving background session does not reload its
driver or recreate its device. Imported drivers require ARM64/Adreno; unsupported
process architectures disable ZIP import/custom choices. Failed imports leave
the current choice and existing library intact.

## Import and loading

The implementation follows the supplied DMC3 `v028-original` references:
LauncherActivity's Add GPU driver button/choices, DriverZipImporter,
DriverLibrary, and GameActivity's selected native driver directory/name.
The ZIP importer was adapted directly; the library and launcher integration
use this project's package, preferences and existing libadrenotools loader.

Packages are content-addressed by the ZIP's SHA-256. ZIP bytes, extraction and
metadata publish atomically in `files/drivers/library/<sha256>/`. Reimporting
the same ZIP reuses its entry. Different ZIPs with the same display name retain
separate entries with a numeric suffix. `meta.json`'s `libraryName` selects its
relative library; otherwise exactly one validated `.so` must be present.

The importer bounds compressed/extracted bytes to 256 MiB and entry count to
256. Absolute/traversal/duplicate paths, damaged CRC/size, missing metadata
targets and ambiguous libraries fail. Validation checks the ELF64 little-endian
shared-library/ARM64 header and Android Vulkan HMI marker. The selected library
is hashed again before launch; missing or changed files fail explicitly.
Those checks establish package structure, not compatibility with every GPU.

GameActivity sends mode and the validated path over JNI. The native loader
accepts custom paths only beneath the canonical private driver-library root,
preserves the extracted library's directory/name, and loads through the existing
libadrenotools hooks. It logs the requested selection; Vulkan device properties
establish the driver that actually loaded. Libraries remain loaded until the
separate game process exits. No system GPU driver or global Android settings
are changed. There is no silent fallback after an explicit custom selection
fails to load.

## Verification

- Thirteen host tests execute the production importer/catalog: retained archive
  bytes/name, nested metadata, multiple drivers, duplicate imports/names,
  traversal, CRC damage, wrong ABI/type/HMI, size/entry limits and changed files.
  The actual bundled Turnip release ZIP also imports and validates.
- Disposable Android instrumentation exercises the production launcher's choices,
  picker-result handling, valid/invalid ZIP import and persisted selection.
  Its fixture app is removed after the test; no original game code is launched.
- The production ARM64 diagnostic `--gpu-driver-test` creates a headless Vulkan
  instance and enumerates devices. On Adreno 650, system selects the system
  Adreno driver; automatic and custom select Turnip. A custom path outside the
  driver library is rejected. These are driver-load checks, not gameplay tests.

Evidence: `work/drivers0523/`, `work/tests/gpu-driver-ui/result.log`.
APK: `work/android-drivers-0523/transformers-dxvk-universal-debug.apk`.
