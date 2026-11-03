# Automatic touch profiles — 0.5.21

## Loading-screen ENTER prompt — 0.5.21

The user-authorized frame in `work/enter-prompt-0520/screen.png` shows the
loading background with ENTER to continue while ROBOT controls are displayed.
The matching passive state has an existing actor (`flags=0x40001000`, animation
0), pause/inhibit zero, and no active known mission prompt or saving byte.

Original routine `0x5166B0` supplies this separate loading-screen loop. It sets
engine flag `0x80` in DWORD `0x6FB1D0` at `0x51674A` and clears it at
`0x516DFF`. Its prompt drawing path tests enabled DWORD `0x713464` and ready
DWORD `0x713468` at `0x516B45`–`0x516B5B`, and resolves `XtoContinue` at
`0x516C2F`. The loading worker sets ready at `0x516416`. These are static
findings from the supported original PE and generated C; those three values
were not captured in the earlier passive snapshot before the user closed it.

The decoder now selects MENU when that loading loop is active, enabled and
ready. Earlier loading with no ready prompt hides stale combat controls.
Clearing the loop's engine flag restores ordinary actor classification even
if enabled/ready values remain set. No guest flags, frame rate or game input
are changed. Native regression fixtures cover prompt precedence, loading
without a prompt, unreadable fields, and robot/car restoration after exit.
Post-installation visual behavior remains a user-operated test.

## Saving-file menu regression — 0.5.20

The original save interface owns byte `0x71437A`, independently of the pause
menu and mission-panel visibility flags. Chapter completion sets it at
`0x498359`, enters phase 20, and waits for it to clear at `0x498367`.
The user-operated process PID 16617 had this byte set to 1, completion
vtable `0x687388` active in phase 20, generic pause 1/1, and both ordinary
menu flags zero. The previous decoder consequently published HIDDEN and Java
displayed zero controls. Passive snapshots are retained in
`work/controls059/saving{now,detail,confirm}-live-state.json`,
`saving-modal-fields.json`, and `saving-api-state.json`.

The decoder now reads this bounded byte and selects MENU before rejecting
generic pause/inhibit. It does not write game state or infer saving from the
completion phase alone. A cleared or unreadable saving byte does not enable
controls for unrelated pauses. Native regressions cover the captured phase20
state, the independent save interface, byte width, unreadable state, and
return to the normal robot/car profiles. Production self-tests include mapped
guest saving-byte transitions. Real-device visual behavior after installation
remains a user-operated test.

## Existing profile behavior

The overlay follows original game state. There are no manual GAME/MENU or
MOBIL/ROBOT selector buttons. Main menu, pause and mission-panel interfaces
receive navigation controls; gameplay receives the robot stick or vehicle
pads/pedals. Unmapped/unready state and blocked input hide the
overlay. A profile change releases all touch-owned input and camera ownership.
Repeated sampling of the same profile preserves held buttons and hardware keys.
During transformation, the actual form bit selects robot/car immediately;
transition flags and animation IDs no longer hide the controls. This removes
the roughly 1–1.5-second empty-control gap observed in 0.5.9.
The Start Mission confirmation now receives the menu layout with a visibly
labelled ENTER button. Its custom chapter-screen controller is independent
of both the ordinary pause-menu flag and the mission_panel open flag.
Mission completion, submission completion and failure confirmations also use
their own input-wait states. These select the same menu/ENTER layout rather
than being hidden by the generic paused flag.

## Evidence and source boundary

Target: frozen PE SHA-256
`5aa3bd490f5b97f614a154fdca98cae845bb62c783c48e56042f19ac14aeefd0`.
REA open_binary verified identity; focused Capstone reads of original PE bytes,
existing generated C, and passive process reads supply the findings below.
No Ghidra function analysis or assistant-driven game input is claimed.

| State | Read | Original evidence |
| --- | --- | --- |
| Frontend singleton | DWORD 0x71318C | InitFrontEnd binding 0x67F1C4 -> 0x414F00 -> 0x49AB70; predicate 0x473BE0; destruction/clear 0x493E60 |
| Scene name | scene=[0x7183A8], name=[scene] | Pause handler comparison 0x498B1C–0x498B24; live main-menu name is `FrontEnd` |
| Pause menu open | BYTE 0x713677 | Singleton 0x713630 +0x47; open stores 0x498BAB/0x498C3B; close 0x498BFA/0x498D7F |
| Mission panel open | root=[0x713648], panel=[root+0x20], BYTE panel+0x0C | Constructor 0x492463–0x49248B installs vtable 0x6872D0 and resolves `mission_panel`; method 0x4839A0 updates open byte |
| Start Mission / chapter confirmation | chapter=[root+0x4C], vtable 0x687358, BYTE chapter+0x2C active, DWORD chapter+0x28 == 9 | Phase table 0x497FE8 maps 9 to 0x497DDF; continue input checked 0x497E36–0x497E67; phase 8 pauses and enters 9 at 0x497DB0–0x497DC7; acceptance unpauses and moves to cleanup |
| Chapter completion | child=[root+0x3C], vtable 0x687388, active BYTE +0x2C, phase DWORD +0x28 == 22 | 0x4982AB opens ContinueWindow; phase 22 waits for accept at 0x4982C6–0x4982D8. Live blocked screen matched this gate |
| Mission failure | child=[root+0x40], vtable 0x6873C0, active BYTE +0x34, phase DWORD +0x30 == 7, prompt BYTE +0x35 | Both selector paths wait for input at 0x498628–0x498657 or 0x49893E–0x498948; phase 8 clears the prompt |
| Submission completion | child=[root+0x78], vtable 0x686FB0, active BYTE +0x2C, phase DWORD +0x28 == 12 | 0x494B4B opens SubMissionContinue; callback 0x48DB00 advances phase 12 to 13 |
| Paused/blocking | DWORDs 0x6FB0B0 / 0x6FB0B4 | Pending-to-current update 0x4035E0, frame call 0x510721; also used by blocking operations, so not by itself a visible-menu test |
| Input inhibited | DWORD 0x7622CC | Input update 0x510660–0x510690 passes paused-or-inhibit to 0x4230B0 |
| Controlled actor | DWORD 0x770614 | PlayerSpeed script binding 0x6B4C04 -> 0x43B4C0; controlled-player assignments 0x470626 / 0x472FE7 |
| Vehicle form | actor+0x628, bit 0x40000 | RobotForm binding 0x6B4C64 -> 0x412710, CarForm 0x6B4C70 -> 0x412770; both inspect bit 18 with opposite polarity |
| Transforming (not a hidden-UI gate) | actor+0x628 mask 0x1C0; animation actor+0x1B24 0x45/0x46 | Transforming script 0x412860; original form predicates reject transition animations, but the touch UI uses the real form bit throughout conversion |

Other generic visible widgets are not automatically menus: root+0x18 is
ammo_count and root+0x28 is dialog/narration. Their visibility alone must not
replace gameplay controls. The decoder checks the mission controller's exact
vtable before using its open byte. A missing scene or actor/configuration read
returns HIDDEN instead of accepting a persistent/stale player pointer alone.
ChapterTextScreen draws this confirmation through its private state machine;
its ordinary widget visibility flag can be false while it waits for Enter.
Only phase 9 is classified as interactive here. Phases 1–8 perform loading,
audio/fades; phase 10 cleans up. They are not promoted to menu controls.
The completion controllers follow the exact typed gates above. HubTextScreen
(root+0x38) has automatic phases without an input wait and is excluded.

## Runtime integration

`control_profile_state.h` is a bounded, freestanding decoder. `game_memory.c`
includes the unchanged pinned allocator and adds read guards: on 64-bit hosts,
reads use the allocator's reservation ranges under its memory lock. A flat
32-bit guest refreshes readable `/proc/self/maps` ranges once per snapshot.
Sampling runs under the serialized guest machine, never on the Android UI
thread. Original game bytes and state are not modified.

The generic D3D9 bridge has an optional successful-Present observer. Android
registers the title-specific sampler after verifying the exact PE. Sampling runs
on every successful Present; only an atomic profile crosses JNI. Java polls every
16 ms while the Activity is active and stops on teardown. No synthetic input,
screenshot recognition or local transform-button toggle drives classification.

`[controls] profile=HIDDEN/MENU/ROBOT/CAR` logs native transitions. `TFControls`
logs the applied Java profile number and control count. Zero-valued unused
native snapshot fields on a fast-return menu path are not a full memory dump.
Native `confirm=1/2/3` denotes chapter completion/failure/submission completion;
`confirm_phase` records the matched interactive phase.

## Verification

- Passive reads from the user-controlled 0.5.8 process matched main menu,
  robot, vehicle and pause. Robot flags were 0x40101000; car flags 0x400C1000;
  pause-menu byte changed from 0 to 1 with the car still present. Evidence:
  `work/controls059/{menu,robot,car,pause}-live-state.json`.
- Six native decoder tests cover these states, scene-name case, mission-panel
  identity, blocking/inhibit gates, continuously visible controls during
  transform flags/animations, unreadable and
  overflowing pointers. Sixteen Java replay tests plus four camera tests cover
  profile changes, hidden-state touch rejection, held-key release, stable polls,
  hardware ownership, all profile geometry and the existing pad controls.
- Six probe tests pass, one optional fixture skipped. On the ARM64 phone, the
  exact production library passes mapped-memory/profile self-test, original
  runtime self-test and D3DX image regression, without entering the game.
- Four native ABIs build. Only ARM64 was executed. APK/install hashes and live
  automatic UI verification are recorded in `work/controls059/validation.json`.
- The user confirmed automatic switching in 0.5.9; native and Java logs agree
  on MENU (8 controls), ROBOT (11), CAR (12), and the unwanted HIDDEN (0)
  periods during transformation. The 0.5.10 native regression verifies live
  transition values 0x400C1080 -> CAR and 0x40101040 -> ROBOT without hiding.
  Current evidence and deployment: `work/controls0510/validation.json`.
- 0.5.11 adds two decoder regressions and one Java Enter/layout regression:
  eight decoder, seventeen touch and four camera tests pass. The exact ARM64
  production library passes Start Mission native and runtime self-tests.
  The live blocked screen had chapter vtable 0x687358, active byte 1 and phase
  9 while both old menu flags were zero and generic pause was 1. Evidence:
  `work/controls0511/private-modal-fields.json`,
  `work/controls059/missionreopened-live-state.json`. The broad GUI-object scan
  was diagnostic only; no name/tree scan was added to the production frame path.
  Latest deployment and user test: `work/controls0511/validation.json`.
  Native `start=1 phase=9` and Java MENU/eight controls were subsequently
  observed on the phone, followed by ROBOT/phase10 and eleven controls with
  continued gameplay. Evidence: `work/controls0511/{runtime,ui}-verified.log`.

The assistant reads runtime/debug output; the user controls the game and tests
the visible behavior. Aircraft-specific layouts and unverified modal widgets
are not newly classified in this revision.

## 0.5.12 completion regression

The user-controlled blocked process had root+0x3C vtable 0x687388, phase 22,
active byte 1, and generic pause 1/1. Pause-menu and mission-panel flags were
zero, so 0.5.11 incorrectly published HIDDEN. The bounded confirmation checks
now take precedence over generic pause/inhibit. Evidence:
`work/controls0512/completion-modal-fields.json` and `completion-runtime.log`.
Twelve decoder, eighteen Java control and four camera tests pass, including
the captured completion state, typed/byte gates, adjacent automatic phases,
failure's distinct offsets, and HIDDEN-to-MENU ENTER press/release behavior.
Deployment and device validation are recorded in `work/controls0512/validation.json`.
