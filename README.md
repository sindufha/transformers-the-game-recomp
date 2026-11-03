# Transformers: The Game — private source and build backup

**Port By Sinn · baseline 0.5.35 / versionCode 46 · Android 10+**

This private repository contains the authored Android launcher/runtime, the full
frozen static recompilation, pinned dependency source bundles, and one active set
of verified native build artifacts for ARM64, ARMv7, x86 and x86_64. It replaces the
previous documentation-only repository state with a build-capable private backup.

The current launcher is English and includes direct game-folder access, optional
app-local GPU drivers, dynamic Menu/Robot/Car controls, a custom control editor,
movable FPS HUD, and an animated sidebar.

## What is included

- `android/`, `runtime/`, `ports/`, `config/`, build scripts and engineering docs.
- `work/lift-5aa3bd490f5b-target/gen/`: **all 50 generated C files**, headers and
  dispatch source; the function catalog contains **14,470 entries**.
- One current lifted-core archive, game runtime and thin launcher probe per ABI.
- Pinned DXVK and SPIRV-Tools archives needed for runtime/backend rebuilding.
- The 343-entry shader cache, app-local Adreno Tools helpers and Turnip 26.2.5.
- Self-contained Git bundles for pcrecomp, DXVK Native, libadrenotools and its
  linkernsbypass dependency, with exact revisions in `backup/dependencies.json`.
- `backup/files.sha256.json`: checksums for the selected backup contents.
- `handoff.md`: the detailed original workspace handoff and known limitations.

Large `.a` and `.so` files use **Git LFS**. Ordinary Git history contains the full
recompiled C source and source bundles. Dependency checkouts are restored locally
from those bundles; they are not duplicated as another tracked source tree.

## What is intentionally excluded

Original game EXE/DAT/audio/FMV assets, saves, personal source URIs, signing keys,
SDK/JDK/NDK installations, local tests and fixture APKs, individual `.o` caches,
temporary build files/logs, old APKs and redundant historical native outputs are
not backed up here. The original game EXE is required as a local verified build
input and must be supplied by you. Original data remains outside the APK.

An archive and a shared runtime are different build stages, not duplicate copies
of the same file: the archive supports relinking runtime changes, while the shared
runtime permits fast APK packaging without recompiling the lifted game. Exactly
one active set of each is retained for each supported ABI.

## Required software (Windows x64)

The verified toolchain uses command-line tools; Android Studio and MSVC are not
required.

| Software | Required version / purpose |
| --- | --- |
| Git for Windows | Clone repository and restore pinned dependency bundles |
| Git LFS | Download large native archives/libraries; pointer files cannot build |
| Python | **3.11 or newer**; original environment used Python 3.14 |
| Python packages | `python -m pip install -r requirements.txt` (Capstone 5.0.7) |
| JDK | **17**; verified with Temurin 17.0.20.1+1 |
| Android SDK command-line tools | `sdkmanager`, `aapt2`, `d8`, signing and alignment |
| Android platform | **android-36** for Java compilation |
| Android build tools | **35.0.0** |
| Android platform tools | `adb` for optional installation |
| Android NDK | **27.1.12297006** for rebuilding native/backend code |

Official downloads: [Git](https://git-scm.com/downloads/win),
[Git LFS](https://git-lfs.com/), [Python](https://www.python.org/downloads/windows/),
[Temurin JDK 17](https://adoptium.net/temurin/releases/?version=17),
[Android command-line tools](https://developer.android.com/studio#command-tools).

PowerShell examples below assume the SDK and JDK are installed at the original
locations. Change environment variables to your own paths.

```powershell
$env:JAVA_HOME='D:/AndroidTools/jdk17/jdk-17.0.20.1+1'
$env:ANDROID_HOME='D:/AndroidTools/android-sdk'
$ndk="$env:ANDROID_HOME/ndk/27.1.12297006"
```

After extracting Android command-line tools to `cmdline-tools/latest`:

```powershell
& "$env:ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager.bat" `
  --sdk_root="$env:ANDROID_HOME" `
  'platform-tools' 'platforms;android-36' 'build-tools;35.0.0' 'ndk;27.1.12297006'

& "$env:ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager.bat" --licenses
```

The fast packaging path reuses native binaries. The NDK is required when you want
to rebuild native code rather than only Java/resources or package the baseline.

## 1. Clone the private backup

Use your GitHub account with access to this private repository.

```powershell
git lfs install
git clone https://github.com/sindufha/transformers-the-game-recomp.git
Set-Location transformers-the-game-recomp
git lfs pull
python -m pip install -r requirements.txt
```

Run subsequent commands from the repository root. Do not copy only the files shown
by GitHub's web interface: the native `.a`/`.so` files must be hydrated by Git LFS.

## 2. Supply the supported original game and restore dependencies

Required `Transformers.exe` identity:

```text
Size:   3,702,784 bytes
SHA256: 5aa3bd490f5b97f614a154fdca98cae845bb62c783c48e56042f19ac14aeefd0
PE32 x86, image base 0x00400000, entry VA 0x00666EAD
```

Restore the build workspace using your own original EXE:

```powershell
python tools/restore_backup.py `
  --game-exe 'D:/My PC Games/Transformers/Transformers.exe' `
  --ndk "$ndk"
```

This checks the backup, validates/copies the EXE to the Git-ignored frozen-input
path, restores pinned dependency checkouts from local bundles, rebases path-only
SPIRV-Tools metadata, and prepares Android DXVK headers/source. It never executes
the Windows EXE, launches a game, installs an app, or runs ADB.

The generated C, binary archives and recorded hash checks are retained. The helper
does not bypass provenance validation. Existing modified dependency checkouts or
a different frozen EXE are rejected rather than overwritten.

Checksums can be inspected separately:

```powershell
python tools/restore_backup.py --verify-only
```

The archive location/source location fields in SPIRV-Tools metadata are relocatable;
all non-location metadata and binary hashes remain verified after restoration.

## 3. Build the APK using the included native runtime (fast path)

Preserve a consistent local signing key. **The original private signing key is not
in this repository.** To update an existing installed app without losing data,
securely restore/use its original key. A newly generated key can build a fresh
APK but cannot update an installation signed with a different certificate.

```powershell
New-Item -ItemType Directory -Force work/signing | Out-Null
# If updating an existing installation, put its original key at this path first.
$key='work/signing/local-debug.jks'

python tools/build_apk.py `
  --lift-dir work/lift-5aa3bd490f5b-target `
  --native-root work `
  --graphics-root work `
  --out work/android-build `
  --sdk "$env:ANDROID_HOME" `
  --jdk "$env:JAVA_HOME" `
  --asset-manifest work/game-assets.json `
  --shader-cache work/shader-cache `
  --keystore "$key" `
  --adreno-tools work/adrenotools `
  --turnip-driver work/turnip-26.2.5
```

When the supplied key path does not exist, the current development builder creates
a local debug key there. Do not commit it. The key directory must exist first.

Output:

```text
work/android-build/transformers-dxvk-universal-debug.apk
work/android-build/apk-build-summary.json
```

The builder compiles Android resources, Java and DEX, packages four ABIs and the
verified cache/driver resources, then verifies signing, ZIP alignment and native
content. It does not run the game. `--prepare-only` only compiles Java/resources;
it does not produce an installable APK.

For source/UI changes, increment `android/AndroidManifest.xml` version name/code.
The backed-up baseline is 0.5.35/code 46; a later update must use a higher code.
No native rebuild is needed for Java/resource-only changes.

## 4. Install on an Android phone

Android 10+ is required. ARM64 has physical-device gameplay evidence on a POCO F3,
Android 13 and Adreno 650; packaging other ABIs does not prove their gameplay.

For ADB installation, connect the phone by USB, enable/authorize USB debugging for
this computer, and select its actual serial from the device list:

```powershell
$adb="$env:ANDROID_HOME/platform-tools/adb.exe"
& $adb devices -l
$serial='YOUR_DEVICE_SERIAL'
& $adb -s "$serial" install --no-incremental -r `
  'work/android-build/transformers-dxvk-universal-debug.apk'
```

You can also transfer the APK and open it on the phone, granting the Android
installer permission when requested. No game data is embedded in the APK.

If an update reports a signing-certificate mismatch, use the original signing key.
Do not uninstall or clear app data as an automatic workaround: private saves,
settings, driver choices and custom layouts must be preserved.

After installation:

1. Open the launcher and use **Select folder** to choose the original PC game folder.
2. Allow persistent read access. The folder is verified by the frozen asset manifest.
3. Use **Select EXE** if the supported EXE was not selected automatically.
4. Choose graphics and GPU driver settings, then press **PLAY GAME**.
5. Three dots opens the sidebar. **Custom UI → Menu / Robot / Car** edits individual
   button positions/sizes and the FPS counter. Save commits; Cancel discards drafts.

The app reads original game assets directly through SAF descriptors; it does not
copy the 4.35 GB asset set into app storage. Saves/configuration remain app-private.
The phone must retain access to the chosen game folder.

## Optional: rebuild a native runtime after bridge/input/audio changes

The complete lifted archives are included, so runtime changes can relink against
them without recompiling every generated C unit. Use one compiler worker.

```powershell
python tools/build_game.py --abi arm64-v8a --ndk "$ndk" --lift-dir work/lift-5aa3bd490f5b-target --core-archive work/build-arm64-release/libtransformers_lift.a --dxvk-dir work/dxvk-arm64-v8a --out work/probe-arm64-target --jobs 1
python tools/build_game.py --abi armeabi-v7a --ndk "$ndk" --lift-dir work/lift-5aa3bd490f5b-target --core-archive work/build-armeabi-v7a-target/libtransformers_lift.a --dxvk-dir work/dxvk-armeabi-v7a --out work/probe-armeabi-v7a-target --jobs 1
python tools/build_game.py --abi x86 --ndk "$ndk" --lift-dir work/lift-5aa3bd490f5b-target --core-archive work/build-x86-target/libtransformers_lift.a --dxvk-dir work/dxvk-x86 --out work/probe-x86-target --jobs 1
python tools/build_game.py --abi x86_64 --ndk "$ndk" --lift-dir work/lift-5aa3bd490f5b-target --core-archive work/build-x86_64-target/libtransformers_lift.a --dxvk-dir work/dxvk-x86_64 --out work/probe-x86_64-target --jobs 1
```

Then rerun the APK packaging command. Preserve exact archive/overlay provenance;
do not edit hash records to make an incompatible core appear valid.

## Optional: rebuild all lifted C

Only necessary after changes to generated code, guest ABI, runtime headers or
compiler semantics. Baseline ARM64 uses O2; the other core archives use O0.

```powershell
python tools/build_native.py --abi arm64-v8a --ndk "$ndk" --lift-dir work/lift-5aa3bd490f5b-target --out work/build-arm64-release --opt 2 --jobs 1
python tools/build_native.py --abi armeabi-v7a --ndk "$ndk" --lift-dir work/lift-5aa3bd490f5b-target --out work/build-armeabi-v7a-target --opt 0 --jobs 1
python tools/build_native.py --abi x86 --ndk "$ndk" --lift-dir work/lift-5aa3bd490f5b-target --out work/build-x86-target --opt 0 --jobs 1
python tools/build_native.py --abi x86_64 --ndk "$ndk" --lift-dir work/lift-5aa3bd490f5b-target --out work/build-x86_64-target --opt 0 --jobs 1
```

Rebuild native runtimes for all packaged ABIs afterward. Individual object caches
are intentionally absent; first recompilation is expensive. The frozen generated
source avoids needing a fresh discovery/lift for ordinary rebuilding.

## Optional: rebuild DXVK, SPIRV-Tools or Adreno Tools

Pinned source bundles are already restored by the helper. Existing archives are
included for ordinary APK/runtime builds. To rebuild a backend, build each ABI's
SPIRV-Tools first, then DXVK, keeping the NDK make output path free of spaces:

```powershell
python tools/build_spirv_tools.py --abi arm64-v8a --ndk "$ndk" --jobs 1
python tools/build_dxvk.py --abi arm64-v8a --ndk "$ndk" --out work/dxvk-arm64-v8a --jobs 1
# Repeat with armeabi-v7a, x86 and x86_64 before universal runtime packaging.
python tools/build_adrenotools.py
```

`build_adrenotools.py` discovers the NDK under ANDROID_HOME; retain the pinned NDK
for reproducibility. Current scripts are Windows-oriented. Linux/macOS builds are
not claimed as validated by this backup.

## Validation, limitations and source rights

The original workspace's latest complete host run had 207 passes and one optional
skip (0.5.34). 0.5.35 was a sidebar viewport update with package/install verification.
Local test suites, fixture apps and device logs are intentionally excluded here;
references to them in historical engineering notes describe the original workspace.

The backup is verified by its content manifest, restoration from its dependency
bundles and a fresh APK package build from the staged backup. This is build evidence,
not a new gameplay test. No phone game/debug/test APK is run as part of backup.

The runtime has partial Win32/D3D9 coverage, exact offline shader-cache boundaries,
and no complete Bink FMV implementation. Initial Autobot/Decepticon gameplay has
historical evidence; the full campaign and every ABI are not fully validated.

Original game code/assets/branding belong to their rights holders. Generated
recompilation and game-derived shader material are not relicensed as original
project work. Keep third-party licenses with their code/binaries. This repository
is private backup material; it is not a claim of rights to redistribute the game.
The repository's prior changelog/documentation history is retained.
