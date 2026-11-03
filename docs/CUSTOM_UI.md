# Sidebar and custom touch layouts — 0.5.33

The three-dot button now opens an in-game sidebar. It no longer expands extra
keyboard buttons. The sidebar contains Resume game and Custom UI entries for
Menu, Robot and Car. The default three-dot and Exit buttons sit near the top
center; the optional FPS display moves below them.

Choosing a profile closes the sidebar and opens its editor over the retained
game surface. The editor has Menu/Robot/Car tabs, Save, Reset, Cancel, a button
selector and a 50–200% size slider. Drag a button to move it; each finger owns
its own drag, and two fingers cannot fight over one button. Dragging empty
space moves the selected button, including controls beneath editor tools.
Hide tools/Show tools exposes the game area while positioning buttons.

Every profile has an independent draft. Switching tabs retains all drafts.
Save commits all three drafts together; Reset clears only the current draft
and takes effect permanently only after Save. Cancel/Back discards drafts.
The geometry strings are stored in private `touch_layouts` preferences under
`profile_1`, `profile_2` and `profile_3`. Each record stores a control ID,
normalized center and size multiplier, preserving positions across viewport
changes. Whole controls are clamped within the inset-safe viewport. The
joystick's visible label is included in its bottom constraint.

The sidebar/editor suspends the existing presentation/guest-clock gate through
`nativeSetForeground(false)` and suspends audio. It keeps the native session
and surface alive without sending Escape into the game. Focus changes,
resuming the Activity and audio-focus changes respect this UI pause owner.
Closing the sidebar or saving/canceling restores native foreground only while
the Activity is resumed and focused. Existing guest pause menus are preserved.
Held gameplay keys/mouse buttons are released before the overlay opens;
editor touches and hardware game keys cannot reach the guest. The most recent
automatic game-control profile is restored when editing ends.

`ControlLayout` handles saved geometry; `TouchControlsView` handles editor
drags and storage; `TouchUiPanel` draws Android sidebar/editor widgets.
Normal gameplay keyboard/mouse bindings, stick/camera behavior and automatic
profile detection are retained. Native binaries and the launcher are reused.

Validation: 19 production-controller JVM cases and six saved-layout cases
cover movement/weapons/multi-touch, sidebar cancellation/release, geometry
round trips, independent profiles/reset, invalid records, viewport clamping,
and touch/binding behavior after moving/resizing. Full host suite: 206 pass,
one optional test skipped. Evidence: `work/ui0533/`. Java/resources compile
against Android API 29+, and APK packaging verifies signatures/alignment.
No fixture APK or game/debug session is launched on the phone. Actual editor
touch interaction and pause/resume in gameplay remain for user validation.

## 0.5.34 refinements

The sidebar has an opaque rectangular sheet matching the full screen height.
Gesture/cutout insets apply to its scrolling content rather than shrinking the
background. It slides in from the right and slides out when dismissed. Pause
ownership stays active until the closing animation completes, including the
transition into the editor. Editor tools fade/slide in with a small scale
transition, including when switching profile tabs or showing tools again.

Editor tools are reduced to a centered 430dp-wide panel with two compact 32dp
rows. The size panel is 300dp wide with two 32dp rows, combining the selector,
percentage and Hide action on one row. Both panels have 5dp vertical padding;
the long instruction caption is removed. Sidebar menu buttons retain their
larger touch sizes.

FPS counter appears in the editor selector and can be dragged/resized with
the same profile-local Save/Reset/Cancel behavior. An editor-only FPS sample
marks its bounds even if the gameplay counter is disabled. The real counter
follows the saved geometry when enabled. Its HUD geometry is explicitly
excluded from gameplay hit testing, so it cannot emit keys or block camera
touches. Existing saved layout records remain compatible.

The added JVM regression covers FPS geometry round trips, bounds, reset and
noninteractive behavior during gameplay. Full host suite: 207 pass, one
optional test skipped. Build/install evidence: `work/ui0534/`.

## 0.5.35 sidebar scroll viewport

The ScrollView itself has no padding and explicitly fills the sheet from its
top edge to its bottom edge. Safe-area spacing moves into the scrolling menu
child, so it cannot shorten the scroll viewport or scrollbar track. Fading
edges and overscroll are disabled to remove the clipped/faded appearance. A
thin overlay track spans the available height; its thumb still reflects the
actual scroll range. Sidebar/editor animation and saved layouts are preserved.
Packaging/install evidence: `work/ui0535/`.
