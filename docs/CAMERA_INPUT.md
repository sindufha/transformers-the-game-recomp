# Camera input fix - 0.4.1

The game requests absolute DirectInput mouse axes. The previous bridge rejected
`SetProperty(DIPROP_AXISMODE, DIPROPAXISMODE_ABS)` and continued returning
relative deltas. This made camera motion cancel when the game subtracted the
previous sample; returning to zero on the next idle poll could undo a drag.

Evidence in the generated source for the supported EXE:

- `sub_005EBF60` initialization region at `0x005EBFF2` sets the Mouse2 format,
  then `0x005EC01D` calls device vtable slot 6 with property 2 and `dwData=0`.
  The zero value comes from the caller's zeroed EBP. The call's result is ignored.
- `sub_005EA580` at `0x005EA670` reads current and previous X samples and subtracts
  them at `0x005EA67E`. The same operation is applied to Y at `0x005EA6C4`.
- A diagnostic Android run confirmed that touch deltas reached `GetDeviceState`
  with the expected Mouse2 packet size (20 bytes, 11 objects). Input delivery
  existed; the axis mode semantics were wrong.

The bridge now accepts both `DIDF_ABSAXIS` and `DIDF_RELAXIS`, applies the format's
axis mode, and implements `DIPROP_AXISMODE` get/set. Absolute state and buffered
events expose accumulated axis positions. Repeated state reads retain the
position, while relative mode still clears deltas after each read. Mode changes
are rejected while acquired; unsupported mode values fail without changing state.

References: Microsoft [mouse axis interpretation](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee418272(v=vs.85)),
[DIDATAFORMAT](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee416606(v=vs.85))
and [SetProperty](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee417929(v=vs.85)).

Android drag now sends movement only. Heavy fire remains on its own button.
`CameraDrag` follows pointer IDs instead of pointer array order, hands off without
a jump when the active finger lifts, preserves fractional movement, and resets
on cancellation. Touch uses one mouse count per pixel; the exploratory 8x gain
was removed after identifying the axis mode bug.

`test_startup_input` and `test_camera_drag` pass 11 targeted tests, including the
real shim calls for the absolute mode request used by this game, idle and repeated
polls, repeated equal drags, absolute buffered events, format mode changes,
relative mode behavior, and multi-finger/cancel gesture sequences.

Runtime logs print the selected axis mode and the first 16 changed mouse states
per device. These bounded records support manual testing without screenshots or
automated screen input. On M2012K11AG / Android 13 / Adreno 650, the installed
final 0.4.1 APK logged `mouse axis mode=absolute (DIPROP_AXISMODE=0)` and changed
X/Y positions, with 334 exact shader cache hits and no fatal in the captured
runtime log. The user tested the camera manually and confirmed "Sudah normal"
when asked whether it pans normally and retains direction after finger release.
Evidence: `work/device/camera041-runtime.log` and `camera041-validation.json`.
No screenshots or automated screen input were used for this final verification.
