# Transformers: The Game — Recompiled

[![Android](https://img.shields.io/badge/Android-10%2B-3DDC84)](#requirements)
[![Graphics](https://img.shields.io/badge/Graphics-DXVK%20%2F%20Vulkan-orange)](#features)
[![Audio](https://img.shields.io/badge/Audio-AAudio-blue)](#features)
[![Status](https://img.shields.io/badge/Status-Playable%20Prototype-yellow)](#current-results)

An experimental Android port of the PC version of **Transformers: The Game**,
using static x86 recompilation, a project-specific Windows API compatibility
layer, and native DXVK rendering through Vulkan.

**Current development build: v0.5.17 · October 24, 2026**

This is a **documentation-only repository**. It contains no original game files,
source code, APKs, generated game code, extracted shaders, or signing keys.
The running prototype requires a separately prepared development APK and your
own compatible PC game files.

[Project status](docs/STATUS.md) · [Touch controls](docs/CONTROLS.md)

## Current results

| Area | Result |
| --- | --- |
| Autobot campaign | The Suburbs reached playable 3D gameplay. |
| Decepticon campaign | Chapter 1 loading and gameplay confirmed by the tester. |
| Rendering | Direct3D 9 calls reach native DXVK and Vulkan. |
| Audio | Original gameplay music, dialogue, and sound effects use DirectSound/AAudio. |
| Controls | Automatic menu, robot, and vehicle layouts with English labels. |
| Fullscreen and resume | Immersive mode implemented; native surface recovery and continued rendering observed. |

Device testing has used a **POCO F3 / M2012K11AG, Android 13, Adreno 650**.
The project remains a prototype; the complete campaigns have not been validated.

## Features

- Circular robot movement stick, camera drag, separate Guns/Rocket buttons,
  Melee, Block, Lock, Jump, Pick Up, and a single Transform action.
- Independent vehicle steering pads, Gas/Brake pedals, Boost, and Handbrake.
- Menu direction pads, ENTER, and a large BACK button mapped to **Backspace**.
- Controls follow the game's menu and character state, including mission prompts.
- Launcher resolution and graphics settings, including High Res Texture, Bloom,
  Shadows, and Motion Blur.
- Background session retention with a return-to-game notification. Rendering,
  guest timers, and audio pause while the app is inactive.
- A local shader cache expanded to **343 exact requests**, including 14 additions
  collected while fixing Decepticon Chapter 1 loading.

## Requirements

- Android 10 or newer and a compatible Vulkan-capable device.
- A compatible development APK; no APK release is distributed in this repository.
- Your own supported PC executable and game data, approximately 4.35 GB.

With a development build, select the executable, import the game folder, choose
graphics settings, and start the game. The launcher verifies the supported files
before using its private imported copy.

## Known limits

Bink/FMV playback is unavailable. Additional levels or graphics configurations
may need shader requests that have not yet been collected; Android-side HLSL
compilation is not implemented. Only ARM64 has been tested on a physical device,
although four Android ABIs have been built. Performance varies by scene and
device; a constant frame rate is not guaranteed.

## Credits

Inspired by [NFSU2 Recompiled](https://github.com/codepdbh/nfsu2-recompiled),
particularly its recompilation workflow and Android control approach.
Development also uses [pcrecomp](https://github.com/sp00nznet/pcrecomp) and
[DXVK Native](https://github.com/Joshua-Ashton/dxvk-native).
This is an independent project, not an official Transformers release.
