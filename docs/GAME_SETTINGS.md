# Launcher settings evidence

Target: `work/lift-5aa3bd490f5b-target/input.exe`, SHA-256
`5aa3bd490f5b97f614a154fdca98cae845bb62c783c48e56042f19ac14aeefd0`.
Addresses below are original x86 virtual addresses at image base `0x00400000`.

## Evidence and limits

Read-only ADB inspection of our app's
`files/game/documents/ACTIVISION/Transformers/pcconfig.txt` observed the game's
plain-text, whitespace-separated settings. It contained `ScreenWidth 858`,
`ScreenHeight 482`, `FSAA 0`, and both texture flags set to `1`; bloom,
shadows and speed blur were also enabled. No device files were changed by
this investigation. No screenshots, input events or UI execution were used.

The mapping and semantics below come from original EXE bytes disassembled
with Capstone and corroborated against existing generated C in
`work/lift-5aa3bd490f5b-target/gen/recomp_0034.c`. This is static evidence,
not an observation of settings taking effect during gameplay. REA's
read-only doctor found Ghidra unavailable with the current JDK 17; no engine,
registration or environment configuration was changed. These ordinary shell
results are not REA Evidence records.

The extracted bytes, disassembly windows and hashes are retained locally in
`work/config-investigation/config-evidence.json`; the original observed
configuration is `work/config-investigation/pcconfig-observed.txt`.

## Proven configuration fields

| Setting | Config dispatch table | Parser / destination | Semantics |
| --- | --- | --- | --- |
| `ScreenWidth` | `0x006EBDF0` | `0x005E2A10` / `0x006EC2A4` | Integer width. |
| `ScreenHeight` | `0x006EBDF8` | `0x005E2A30` / `0x006EC2A8` | Integer height. |
| `BloomEnabled` | `0x006EBE50` | `0x005E2B90` / `0x006EC2D0` | Positive integer enables. |
| `ShadowsEnabled` | `0x006EBE68` | `0x005E2BF0` / `0x006EC2DC` | Positive integer enables. |
| `SpeedBlurEnabled` | `0x006EBE78` | `0x005E2C30` / `0x006EC2DE` | Positive integer enables. |
| `TransformersUseHires` | `0x006EBE80` | `0x005E2C50` / `0x006EC2DF` | Active high-resolution texture flag. |
| `TransformersUseHiresPending` | `0x006EBE88` | `0x005E2C70` / `0x006EC2E0` | Texture selection applied at next config load. |

The boolean parsers call the integer conversion at `0x0065E400`, compare the
result with zero, and store `setg` results. The launcher writes canonical
`0` or `1`. The original initializer `0x005E2760` defaults bloom, shadows,
speed blur and both texture flags to enabled (stores at `0x005E27D4`,
`0x005E27E6`, `0x005E27F2`, `0x005E27F8`, `0x005E27FE`).

After reading config, `0x005E2E4B`–`0x005E2E5E` copies pending texture state
into active state if they differ. The launcher sets **both** keys before
starting the game. The asset loader `0x005EECB0` checks active state at
`0x005EECF1`–`0x005EECF9`; only when enabled does it open its optional
second asset file via `0x006612A0` at `0x005EECFF` and store the handle at
`0x02BDAAD0`. Static evidence establishes the flag controls asset loading;
visual texture differences and performance remain for manual gameplay tests.

Resolution validation at `0x005E2C90`–`0x005E2CBB` accepts positive width
and height; nonpositive values revert to renderer defaults. However, this
does **not** establish that arbitrary config dimensions become the backbuffer.
Missing configuration uses launcher fallback `1280×720`.

### Resolution selection constraint

The game reads the configured dimensions at `0x005E5AE6`/`0x005E5AEB`,
then calls mode selector `0x005E3810` at `0x005E5AF9`. The selector chooses
the closest width from its enumerated mode list, then closest height among
that width, then closest refresh rate if requested. The selected index is
stored at renderer offset `+0x658`. At `0x005E52D6`–`0x005E52FC`,
the **selected mode's** dimensions are copied into the D3D9 presentation
parameters at renderer offsets `+0x138`/`+0x13C`. `CreateDevice` receives
this parameter block at `0x005E54F2`–`0x005E5510`.

Before selection, `0x005E5A2A`/`0x005E5A32` sets minimum enumerated
dimensions to `640×480`. If either `BloomMethod` or `ShadowMethod` is `2`,
`0x005E5A44`/`0x005E5A4C` increases that minimum to `800×600`.
`0x005E5A72`–`0x005E5A7D` rejects modes below either threshold. The
observed config has both methods set to `2`; merely disabling the boolean
effect flags does not change this filter. Exposing a smaller SurfaceView
mode alone therefore cannot guarantee that smaller dimensions survive.

The Android bridge forces `Windowed=TRUE` and preserves the dimensions it
receives, but that conversion occurs **after** original game mode selection;
it cannot recover a config dimension already coerced by that selection.
Consequently, desired output choices `640×360`, `854×480`, `960×540`,
`1280×720` are not all proven attainable by editing pcconfig with native
scale `100`. They require a mode-selection/filter change or use of the
existing bridge render scaling from a fixed, valid game base resolution.
The launcher therefore always reapplies a valid base `1280×720` before
every launch and uses the unchanged bridge's supported scales `50/67/100`.
The UI offers their actual outputs `640×360`, `858×482`, `1280×720`
(rounding to even pixels). Scale `67` is **not** exact `854×480`; the
native API does not currently accept scale `75`, so `960×540` is not offered.
No fixed SurfaceHolder size or native mode-filter patch is needed for this
approach. User gameplay testing remains necessary to verify visual output.

## Integration

```java
GameSettings current = GameSettings.load(gameRoot);
int scale = GameSettings.prepareLaunch(gameRoot, highRes, selectedScale,
                                        bloom, shadows, motionBlur);
// Pass the returned 50/67/100 bridge render scale to GameActivity/nativeRun.
```

The returned object exposes final fields `highResTextures`, `width`,
`height`, `bloom`, `shadows`, `motionBlur`. A four-argument `applyOptions`
overload edits only resolution and the two texture keys.

The native bridge already supports scaling. Applying that scale again to
persisted reduced game dimensions would progressively shrink the resolution
after restarts. The launcher reapplies a fixed valid base before every launch
when using bridge scaling. The generic config editor accepts other requested
dimensions but does not promise that the original game's mode selector will
use them unchanged.

The editor changes only selected values, preserves unrelated bytes, spacing,
comments and line endings, updates duplicate keys consistently, and replaces
the config via a temporary file. New config files receive `FileVersion 1`
and `FSAA 0`; existing FSAA, effect method/support fields, bindings and saves
are preserved. Invalid dimensions, oversized files and NUL-containing
encodings fail without replacing the original file.

Ten tests execute the same Java class with the local JDK, covering texture
toggle, pending state, resolution idempotence, effect toggles, preservation,
duplicates, invalid input, defaults, untouched saves, restoration of the valid
base after the game saves reduced dimensions, and rejection of unsupported
bridge scales without modifying the config. Run:

```text
python -m unittest discover -s tests -p test_game_settings.py -v
```

## Android read-path regression fixed in 0.5.1

The user's 0.5.0 session requested all effects and high-resolution textures OFF,
but the native game wrote their default ON values back. The original read-side
filesystem uppercases Windows paths. Case recovery for a host-absolute `Z:` path
requires directory enumeration; Android denies enumeration of ancestors such
as `/data`. The original loader consequently fails to open pcconfig, runs its
defaults initializer at `0x005E2DF8`, then saves defaults at `0x005E2DFF`.
The write-side CRT retaining the original case can still save to that path.

`prepare_documents` now mounts the existing private documents directory as
`D:` before returning `SHGetFolderPathA`. The reader only needs to resolve
case below that accessible directory. The physical config/save location is
unchanged. Runtime startup records the mapping with a `[settings] Documents`
log line; there is no default override or repeated patch of live game flags.

A headless Android probe reproduces the failure with the 0.5.0 runtime, and
reads all OFF values with `D:`. With the rebuilt production 0.5.1 ARM64 library
and its unmodified service setup, the same original constructor/loader reads
OFF, ON and mixed values correctly; pending high-resolution texture OFF also
replaces an active ON value. The probe reads the actual guest config fields,
not merely the launcher's preferences. It never enters the EXE entrypoint,
renderer, input or asset loader. All writes are confined to a disposable
`/data/local/tmp/tf-settings051` directory; the user's app config/saves are
untouched. Relevant evidence:

- `work/device/settings051-load.log`: original uppercase-path failure.
- `work/device/settings051-load-mapped.log`: mapped-root comparison.
- `work/device/settings051-production-off.log`, `settings051-production-on.log`,
  `settings051-production-mixed.log`, `settings051-production-pending-off.log`:
  rebuilt production runtime field observations.
- Five service tests pass, including mapped Documents paths and the existing
  traversal/symlink rejection checks; strict compilation covers all four ABIs.

Build the development-only probe with:

```text
python tools/build_config_probe.py --runtime-dir work/probe-arm64-target --out work/settings051/config-native-probe
```

Its `load` mode tests the production setup; `load-mapped` is an explicit
comparison mode that injects a mount and must not be used as proof of the
production fix. Manual gameplay/visual checks remain with the user.
