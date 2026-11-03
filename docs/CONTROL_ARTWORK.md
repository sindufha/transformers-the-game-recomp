# Rounded control artwork — 0.5.32

Only the in-game touch overlay presentation changes. Dark translucent buttons
use rounded filled vector symbols, white labels and amber pressed states.
Attack, Continue and the accelerator have an amber resting outline. The
joystick has a simpler ring and thumb cap. Short display labels include Attack,
Aim, Fire, Grab and Continue; their original input codes remain unchanged.

`TouchControlsArtwork.java` draws the existing `TouchControls` state. Its
resource cache loads once when the view is created. It does not send input,
modify control state, alter layout, or choose dynamic profiles.

`TouchControls.java`, `CameraDrag.java`, `GameActivity.java`, the launcher and
session service are byte-for-byte unchanged. The view's touch event handler,
profile switching, cancellation, coordinate scaling and inset handling are
also unchanged. Button positions, hit regions, keyboard/mouse bindings,
multi-touch, camera dragging and Robot/Vehicle/Menu switching are preserved.
Native binaries for every ABI are reused without modification.

There are 21 Google Material Symbols Rounded filled resources, with sources
and hashes in `android/hud-symbols-source.json`. Their Apache-2.0 license is
already packaged under `assets/licenses/MATERIAL-SYMBOLS-LICENSE.txt`.
The two rounded pedal symbols are authored vector paths.

Evidence is in `work/hud0532/`: the mechanics source audit, a local artwork
preview using the production layout positions, the APK build log and the
installation record. The preview is not an Android gameplay capture. No
fixture APKs are installed and no game/debug session is started; gameplay
validation is left to the user as requested.
