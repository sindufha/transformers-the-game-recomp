# HD launcher icon — 0.5.26

The Android application icon uses a 2048×2048 metallic Autobot PNG downloaded
from [PNGimg](https://pngimg.com/image/45374). The source URL, dimensions and
SHA-256 are recorded in `android/launcher-icon-source.json`. The PNG is copied
without visual alterations to `android/res/drawable-nodpi/ic_launcher.png`.
This replaces the blurry 32×32 icon extracted from the original EXE in 0.5.25.

Download the recorded image URL to reproduce the icon and verify its SHA-256.
`tools/extract_launcher_icon.py` retains the original 32px extraction only in
`work/original-exe-icon.png`, without replacing the HD resource.
The APK builder compiles `android/res` and the manifest references the icon.
Existing native libraries, source selection, saves and GPU drivers are retained.

This user-requested branding resource belongs to its original rights holders;
it is excluded from the project's source-code license. No additional game
assets or executable bytes are bundled by this change.
