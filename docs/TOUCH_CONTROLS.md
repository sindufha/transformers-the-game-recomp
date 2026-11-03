# Touch controls

In 0.5.17 the large BACK button above ENTER sends Backspace (VK 8 / DirectInput
scan 0x0E). Pause still uses Escape. The menu replay checks Backspace press and
release, rejects an Escape event, and preserves an independently held ENTER.

Since 0.5.15, all control labels, overlay hints and the accessibility description
are in English. Examples include Pause, Back, Exit, Move, Lock, Pick Up, Jump,
Left/Right, Brake, H-Brake and Horn. Layout geometry and original bindings are
unchanged. The existing nineteen touch replay tests pass.

0.5.13 follows the positions in the user's three reference images, with the
existing transparent blue outline style, icons, rounded pads and colored pedals:

- Menu: separate four-way direction pads at lower left; Back above the larger
  ENTER button at right; Exit at upper right. Q/E/Backspace remain under `...`.
- Robot: Pause at upper left, Transform and Lock beneath it, circular movement
  stick at lower left. The right cluster puts Guns low/left of Rocket, a larger
  Melee button to their right, Pick Up (Q) and Jump above, and Block beside Melee.
- Car: Pause/Transform at left, wide Boost above independent Left/Right pads;
  Rocket/Guns at upper right and Brake/Gas below. Horn and handbrake are retained.

The native game state still selects menu/robot/car automatically. Controls scale
with the available view and respect Android gesture insets. Side clusters leave
the center available for camera drag and a clear bottom status strip. The user's
new positions replace the earlier strict radar/health exclusion and two-column
combat bank. The artwork in the supplied sketches is not copied.

Nineteen Java replay tests and four camera tests pass. Bounds/non-overlap and
relative placement are checked at four landscape sizes, including wide and tall
views. `work/controls0513/layout-preview.png` is a local geometry preview exported
from the actual Java control model, not an Android screen capture. Deployment
verification is in `work/controls0513/validation.json`. No screen input or device
screenshot is used; the user tests the installed APK.

The reference is NFSU2's
[RacingControlsView.java](https://github.com/codepdbh/nfsu2-recompiled/blob/1aac7a9dfb953b305a1ecd795ab53f21daefe598/android/app/src/main/java/com/nfsu2/androidevolved/RacingControlsView.java),
checked at upstream main `1aac7a9dfb953b305a1ecd795ab53f21daefe598`.
Its single Canvas overlay, circular hit regions, independent finger ownership,
and release-on-lost-touch behavior informed this implementation. The Transformers
layout and two-axis control model are implemented separately in this project.
The 0.5.8 driving arrangement also follows the independent steering buttons
and pedals in the upstream [classic racing layout](https://github.com/codepdbh/nfsu2-recompiled/blob/main/android/app/src/main/java/com/nfsu2/androidevolved/RacingControlsView.java#L115-L124),
checked again for this revision. Transformers uses its own W/S/A/D bindings
and reserves its existing HUD areas.

## Profiles and bindings

The overlay starts hidden until the native game publishes a valid profile.
Original frontend/pause/mission-panel state shows navigation controls.
Start Mission and mission completion/failure confirmation states also show
the menu layout with ENTER, even while gameplay is paused. Actor
form selects the robot stick or vehicle pads. Loading/blocked input temporarily
hide controls. Transformation selects the matching layout without an empty gap.
Profile changes clear touch holds;
unchanged polls keep movement/fire held. The original Transform action remains
the single wheel-up binding. See [AUTO_CONTROLS.md](AUTO_CONTROLS.md).

| Control | Input |
| --- | --- |
| Circular stick, GAME | W/S/A/D, including diagonal combinations |
| Four direction pads, MENU | Up/Down/Left/Right arrows |
| Kiri / Kanan pads, MOBIL | Hold A / D independently |
| Gas pedal, MOBIL | Hold W to accelerate |
| Rem pedal, MOBIL | Hold S to brake/reverse |
| Drag empty space to the right of the stick, GAME | Existing camera mouse deltas |
| Guns | Right mouse button, mask 2 (Weapon B) |
| Rocket | Left mouse button, mask 1 (Weapon A) |
| Melee, fist icon | E, VK 69 (Attack A / close-range attack) |
| Block, shield icon | Left Shift, VK 160; block/dodge in robot form |
| Boost, driving profile | Same Left Shift binding, vehicle boost |
| Kunci | Middle mouse button, mask 4 |
| Lompat | Space |
| Rem Tgn, driving profile | Space, vehicle handbrake |
| Angkat Q / Klakson | Q |
| Transform | One mouse-wheel +120 pulse per new contact; the game toggles form |
| Pause | Escape |
| BACK, above ENTER | Backspace, VK 8 / scan 0x0E |
| ENTER | Enter |
| BKSP, inside extras | Backspace |
| Extra keys | Shift, Ctrl, Q, E, F, Backspace |
| Keluar, inside extras | Close the game Activity |

The stick is visually analog, with a clamped circular thumb and a neutral zone.
It drives eight-direction keyboard input through the existing runtime; it does
not add a proportional DirectInput gamepad axis. Direction hysteresis prevents
chattering near the neutral zone. Actions retain the existing keyboard/mouse
bridge, and their effect follows the game's own bindings.

Driving pads/pedals hold their key continuously until released or dragged
outside their touch bounds. Gas, steering, weapons, Boost and camera fingers
have independent ownership. Rectangular hit areas match the pad/pedal shape.
The 0.5.7 time-proportioned steering code and its animation timer were removed;
there is no analog driving state. Release, cancelled/lost pointers, profile
changes and focus loss release touch-owned keys while preserving hardware holds.
Actual steering feel remains a user ergonomics test.

The supplied PC manual `../Docs/Change/manual.pdf`, pages 3-5, documents Weapon A
as left mouse, Weapon B as right mouse, Attack A as E, and Convert to
Vehicle/Robot Form as Mouse Wheel Up in the robot, car, and aircraft tables.
Page 7 also documents E for swinging melee objects and the airborne ground smash.
The Guns/Rocket names retain this port's previously tested light/heavy mouse
bindings; the manual calls them Weapon B/A rather than naming the projectile.
Page 3 binds Left Shift to dodge/block; page 4 binds it to vehicle boost,
Space to handbrake, W to accelerate and S to reverse/brake.
Weapon appearance can differ by character. Transform always sends the original
wheel-up binding: no local robot/car flag can drift out of sync with the game.

## Input ownership and checks

One overlay handles the complete multi-touch event stream. A finger remains
assigned to its original control or camera zone, so aiming across a fire button
does not press it. Movement, camera and both weapon buttons may be held together.
Shared keys/buttons have reference counts. A second finger cannot steal the
stick; lost pointers, cancelled gestures and focus loss release held input.
Touch release preserves separately held hardware keyboard keys. Transform
button sends one wheel-up pulse per contact, including when a weapon is held.
Dragging out of and back into it does not retrigger; a second finger while it is
held does not duplicate the pulse. Cancellation releases the contact without
emitting a second pulse.

`test_touch_controls.py` runs seventeen Java replay tests against the controller
shipped in the APK. They cover eight-way movement, neutral-zone hysteresis,
menu mappings, distinct Guns/Rocket/E bindings, simultaneous movement/camera/weapons/melee, shared-key ownership,
hardware keys, pointer loss, cancellation, one-shot transformation, profile
switching, HUD clearance, and non-overlapping in-bounds layouts at four landscape sizes.
The current cases cover simultaneous pad/pedal/weapon/camera holds, pedal
drag-out/re-entry, pointer loss, hardware/shared-key ownership, profile switching,
Shift while aiming/firing, and the cleared camera lane. Geometry is
checked with both driving and walking profiles, including rectangle/circle
overlap and HUD clearance. The offline car-pad layout from production Java
coordinates was inspected in `work/controls058/layout/car-pad.png`;
it is not a device screenshot or pixel render of Android Canvas.
The four existing CameraDrag tests also pass. These are replay/model tests;
manual ergonomics and visual checks on the device are recorded separately.

These control changes are Java only and do not change the four native game
binaries. The user tests touch ergonomics personally while the assistant reads
debug output only; no screenshots or screen inputs were used for this revision.
Start Mission's private confirmation phase also selects the menu layout in
0.5.11, including ENTER; its loading and cleanup phases remain hidden.
APK 0.5.11 deployment, automatic profile tests and preserved preferences/config
are recorded in `work/controls0511/validation.json`.
