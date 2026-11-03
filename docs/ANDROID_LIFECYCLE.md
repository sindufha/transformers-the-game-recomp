# Fullscreen and background resume — 0.5.14

Previously `onStop()` called `finish()`, `surfaceDestroyed()` also finished the
Activity, and `onDestroy()` always killed `:game`. Opening Recent Apps therefore
ended the native session. The replacement keeps the session and uses Android
[immersive system bars](https://developer.android.com/develop/ui/views/layout/immersive)
and the [Activity lifecycle](https://developer.android.com/guide/components/activities/activity-lifecycle).

## Android side

- Status/navigation bars are hidden at creation, resume and regained focus.
  API 30+ uses WindowInsetsController; Android 10 uses immersive sticky flags.
  Edge swipes can temporarily reveal system navigation. The surface extends to
  display cutout edges; touch controls keep the cutout/gesture safe insets.
- Pause/stop releases held touch/hardware input, mutes audio, stops UI polling
  and gates native rendering. It does not finish or restart the game.
- A same-process foreground service keeps a user-visible session notification
  with a resume action. It starts while the game Activity is visible, holds no
  wake lock, and uses START_NOT_STICKY. Its normal FOREGROUND_SERVICE permission
  is declared; no network/storage permission or boot receiver is added.
  See Android's [foreground service launch rules](https://developer.android.com/develop/background-work/services/fgs/launch).
- Returning from Recent Apps or the notification reuses the existing session.
  A recreated Activity attaches its new surface to the existing worker instead
  of calling the lifted entry a second time. Native status callbacks route to
  the current Activity; a completed session stops the foreground service.
- Explicit Exit or dismissing the app task ends the dedicated game process.
  Force-stop, system/user process termination and device reboot cannot restore
  the in-memory guest session. This is resume, not a serialized save state.

## Native ownership

The optional D3D9 operation guard serializes graphics/object-registry calls
with Android surface changes. Background render threads wait on a condition
variable, releasing the operation mutex while waiting. Lifecycle JNI callbacks
do not enter the guest machine or modify its state flags.

Following the [SurfaceHolder callback contract](https://developer.android.com/reference/android/view/SurfaceHolder.Callback),
before SurfaceDestroyed returns, the host drains DXVK's command/submission
threads and invalidates its implicit/exposed swap-chain presenters, then releases
the ANativeWindow. `android_presenter.cpp` uses public C++ methods of the exact
pinned native DXVK backend. Upstream source and the backend archive are unchanged.
The bridge holds exclusive lifecycle ownership while doing this; guest devices,
backbuffers, textures, shaders and game memory are retained.

On reattachment the existing Android WSI creates a new HWND generation. The
next original Present supplies this override and DXVK recreates presentation
against the new surface. It does not call a guest Reset or restart the PE.
The existing render-size policy is applied to every replacement window.

Guest GetTickCount, timeGetTime and QueryPerformanceCounter exclude intervals
without foreground or a surface. These shims register before the original HLE
modules because its resolver uses the first registered name. QPC remains in
microseconds with the original 1 MHz frequency. Host I/O/wait timers stay real.
Guest clock polls also wait on the lifecycle gate, so a game's frame limiter
cannot spin on a frozen timestamp in the background. Nested native calls can
unwind after focus loss without retaining an outer lock while waiting.
The AAudio callback outputs silence without consuming the PCM ring while the
session is suspended, so audio queues are retained too.

## Verification

- Three host clock regressions cover duplicate focus/surface events, long
  background intervals, 64-bit timing and resumed elapsed time.
- Nineteen touch, four camera and seven package/provenance tests pass.
- The production ARM64 library passes `--lifecycle-self-test`: foreground and
  surface gating, six detach/resume cycles, in-flight operation serialization
  and frozen clock. Existing controls, BMP image and HLE runtime probes pass.
  These probes do not run game code or access a screen/GPU.
- Four native ABIs are built and packaged. Java compiles against SDK 36 with
  minimum/target 29. Only ARM64 has native execution evidence.
- Deployment, hashes and the user-operated Recent Apps/fullscreen check are
  recorded in `work/lifecycle0514/validation.json`. The assistant does not launch
  the game, send screen input, or capture device screenshots.
