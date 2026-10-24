# Changelog

## v0.5.17 — Android playable preview

- Fixed the large menu BACK button to send Backspace (VK 8 / DirectInput 0x0E).
- Preserved independent ENTER holds when BACK is pressed or released.
- Packaged ARM64, ARMv7, and universal download options with the same runtime
  content and development signing certificate.
- Added release checksums, build information, third-party notices, installation
  instructions, and a control layout diagram.

The update passed 19 touch replay tests. Public APK variants are signature,
alignment, ZIP, architecture, and runtime-content checked. Only ARM64 has
physical-device gameplay coverage.

## v0.5.16 — Decepticon Chapter 1 loading

- Captured and compiled 14 missing vertex/pixel shader requests.
- Expanded the bundled shader cache from 329 to 343 requests.
- Decepticon Chapter 1 reached playable gameplay in the tester's retry.
- Preserved the original shader inputs, flags, and profiles.

## v0.5.15 — English controls

- Translated control labels, overlay hints, and accessibility descriptions into English.

## v0.5.14 — Fullscreen and background sessions

- Added immersive status/navigation-bar handling and restoration on resume.
- Stopped closing the game simply because the Activity stopped or its surface disappeared.
- Retained the native session with a return notification and presentation recovery.
- Paused rendering, audio, and guest time while inactive.

## Earlier milestones

- Automatic menu/robot/vehicle profiles and mission-confirmation controls.
- Robot stick, vehicle steering pads/pedals, separate weapons, and Block.
- Native Direct3D 9/Vulkan rendering and launcher graphics settings.
- Gameplay music/dialogue/SFX through DirectSound/AAudio.
- BMP save/readback handling for the observed mission-completion transition.

This history summarizes implemented milestones. It does not establish full
campaign or all-device compatibility.
