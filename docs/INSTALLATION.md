# Installation and updates

## Choose a package

Download an APK from the [v0.5.17 release](https://github.com/sindufha/transformers-the-game-recomp/releases/tag/v0.5.17).

| Package | Compatibility |
| --- | --- |
| ARM64 | Recommended for supported 64-bit devices. This is the physically tested ABI. |
| ARMv7 | Requires Android 10+ with 32-bit app support. No physical-device validation yet. |
| Universal | Includes all four built ABIs. Larger download; only ARM64 has been physically tested. |

The packages share application ID `io.github.transformers.recompiled.diagnostics`
and the same development signing certificate. Install one package at a time.
The ARM64 package includes the app-local Turnip preview; ARMv7 uses its system
Vulkan driver.

## First run

1. Install the downloaded APK using Android's installation prompt.
2. Open the launcher and select your supported `Transformers.exe`.
3. Import your complete PC game folder through the Android folder picker.
   Keep the original folder structure; the importer verifies 482 expected data files.
4. Allow space for approximately 4.35 GB of imported data plus the app's native
   libraries. Keeping another copy of the source folder on internal storage
   requires additional space.
5. Set resolution and graphics options, then start the game. Settings include
   High Res Texture, Bloom, Shadows, and Motion Blur.

Original game files are not included with the release. Only one executable build
is currently accepted:

```text
File:    Transformers.exe
Bytes:   3702784
SHA-256: 5aa3bd490f5b97f614a154fdca98cae845bb62c783c48e56042f19ac14aeefd0
```

Other executable versions are not automatically converted into this supported
build. No executable or game-data download is provided here.

## Updates

Close the game before installing an update. APKs signed with this project's
matching development certificate can update the existing installation in place.
Imported files and settings were preserved in the tested update flow; uninstalling
or clearing app storage removes the private imported copy.

If Android rejects an update, check the selected architecture and whether the
existing app was signed with a different key. See [troubleshooting](TROUBLESHOOTING.md).

## Resume a session

Home or Recent Apps pauses rendering, audio, and guest timers while keeping the
session in memory. Return through Recent Apps or the session notification.
EXIT or dismissing the app task ends the session. Process termination and reboot
cannot resume an in-memory session.

## Verify the download

Compare the APK hash against the release's `SHA256SUMS.txt`. For example:

```powershell
Get-FileHash .\downloaded.apk -Algorithm SHA256
```

`BUILD_INFO.json` lists package sizes, ABIs, certificate fingerprint, and tested
coverage. The private signing key is never part of a release.
