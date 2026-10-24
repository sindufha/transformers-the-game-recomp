# Troubleshooting

## Installation or update is rejected

Use an APK compatible with the device's supported application ABIs. ARMv7 needs
32-bit app support, even on a device with a 64-bit CPU. All packages require
Android 10 or newer. An existing installation signed with another key cannot be
updated using this release's certificate.

## Executable or import verification fails

Compare your executable against the supported size and SHA-256 in the
[installation guide](INSTALLATION.md). Other EXE versions are not currently
supported. Import the complete game folder and keep its structure intact.
The app checks expected file sizes/hashes; it does not provide replacement files.

If an import was interrupted, choose the same folder again. Already verified
files can be reused by the importer. Make sure internal storage can hold the
private imported data plus the app's extracted native libraries.

## A chapter stops while loading

An uncollected shader request or unsupported API may stop an untested path.
v0.5.16 and later include the 14 additions that fixed the observed Decepticon
Chapter 1 shader-cache failure. This does not prove every later mission or
graphics combination is covered.

When reporting a failure, include the release version, device model, Android
version, GPU/driver choice, campaign/chapter, graphics settings, and the last
actions before the failure. Runtime logs are useful when available. Do not attach
the original EXE, game archives, private keys, or your entire imported game folder.

## Missing videos or uneven performance

Bink/FMV decoding is not implemented, so the original movie sequences are not
supported. Gameplay music and effects use the separate DirectSound/AAudio path.

Frame rate depends on scene complexity and device conditions. Lower resolution
alone may not remove CPU-side work. The prototype has no guaranteed 30 FPS floor
across all scenes and devices.

## A session does not resume

Home/Recent Apps can retain a live session; EXIT, task dismissal, force-stop,
reboot, or process termination ends it. A session notification can return to the
paused Activity. This feature is not a serialized save state.

## Controls

The large menu BACK button sends **Backspace**. PAUSE sends Escape, and ENTER
confirms menu/mission prompts. See the [control guide](CONTROLS.md) for all bindings.

Issues can be reported on the repository's [issue tracker](https://github.com/sindufha/transformers-the-game-recomp/issues).
