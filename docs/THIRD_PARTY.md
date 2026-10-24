# Third-party components

Release APKs contain third-party notices under `assets/licenses/`. The release
also provides `THIRD_PARTY_NOTICES.txt`, reproducing the bundled license/notice
texts, and `BUILD_INFO.json` with package and component information.

| Component | Use | Notice |
| --- | --- | --- |
| [pcrecomp](https://github.com/sp00nznet/pcrecomp) | Recompilation/runtime foundation | MIT |
| [DXVK Native](https://github.com/Joshua-Ashton/dxvk-native) | Native Direct3D 9/Vulkan backend | zlib/libpng |
| [libadrenotools](https://github.com/bylaws/libadrenotools) | ARM64 app-local driver loading | BSD 2-Clause |
| [Mesa/Turnip](https://www.mesa3d.org/) | Bundled ARM64 driver preview | See the bundled Mesa license/copyright text |
| [SPIRV-Tools](https://github.com/KhronosGroup/SPIRV-Tools) | Shader processing | See the bundled NDK shader-component notices |

The ARM64 package includes the Turnip 26.2.5 preview from
[mesa-turnip-android-driver](https://github.com/nihui/mesa-turnip-android-driver/releases/tag/26.2.5).
The ARMv7 package uses its system Vulkan driver. The universal package includes
the same ARM64 driver components for compatible ARM64 execution.

[NFSU2 Recompiled](https://github.com/codepdbh/nfsu2-recompiled) inspired the
workflow, Android control approach, and project presentation. No NFSU2 game files,
screenshots, or branding images are included here. The banner and control diagram
are original documentation artwork; the diagram is not a gameplay screenshot.

Original Transformers game files remain separate and must be supplied by the
user. This project is independent and is not an official game release. Source
code is not currently distributed by this repository.
