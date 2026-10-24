# Touch controls

The overlay automatically follows menus, robot form, and vehicle form. Robot
movement uses a circular stick; vehicles use separate steering pads and pedals.
Empty space on the right is available for camera drag.

## Menus

| Button | Original input |
| --- | --- |
| UP / DOWN / LEFT / RIGHT | Arrow keys |
| ENTER | Enter |
| BACK, above ENTER | **Backspace — VK 8 / DirectInput scan 0x0E** |
| EXIT | End the game session |

## Shared gameplay actions

| Control | Original input |
| --- | --- |
| PAUSE | Escape |
| TRANSFORM | One mouse-wheel-up pulse; the game toggles form |
| GUNS | Right mouse button |
| ROCKET | Left mouse button |
| Drag empty space | Relative mouse movement for the camera |
| `...` | Extra keyboard controls, including BKSP/Backspace |

## Robot

| Control | Original input |
| --- | --- |
| MOVE stick | W / A / S / D, including diagonals |
| MELEE | E |
| BLOCK | Left Shift |
| LOCK | Middle mouse button |
| JUMP | Space |
| PICK UP | Q |

## Vehicle

| Control | Original input |
| --- | --- |
| LEFT / RIGHT | A / D |
| GAS | W |
| BRAKE | S |
| BOOST | Left Shift |
| H-BRAKE | Space |
| HORN | Q |

Movement, camera drag, and weapon buttons support simultaneous touches. Layout
changes and focus loss release touch-owned input to avoid stuck keys. Mission
confirmation screens use the menu controls. Dedicated aircraft behavior still
needs separate validation.

Opening Home or Recent Apps pauses the native session. Return through Recent
Apps or the session notification to continue. EXIT or dismissing the app task
ends the session. An edge swipe can temporarily reveal Android's system bars.
