# Launcher symbols and banner — 0.5.27

The launcher uses Google Material Symbols Rounded vector drawables for its
section headings, play/file/driver/cancel buttons, resolution, file status and
graphics/display options. Labels remain alongside icons. Icons are tinted to
match existing foreground/accent colors and scale without raster blur.

The top banner now reads `Transformers : The Game`, with a bold 27sp title
that wraps naturally beside the HD Autobot logo (corrected in 0.5.28).
The PLAY section uses a play symbol; the generic controller icon is removed.
Existing launcher sections,
game controls, application icon, native runtime and settings behavior remain.

Sources and resource hashes: `android/launcher-symbols-source.json`.
The icons are from [Google Material Symbols](https://github.com/google/material-design-icons),
licensed under Apache-2.0. The license is included in the APK under
`assets/licenses/MATERIAL-SYMBOLS-LICENSE.txt`.

This resource/Java update reuses all existing native ABI artifacts. Packaging
compiles Java and vector resources and verifies signing/alignment. Installation
does not launch the game or install test fixture applications.
