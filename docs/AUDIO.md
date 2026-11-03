# Original game audio bridge — 0.5.5

`runtime/startup_audio.cpp` implements the original game's DirectSound playback
interfaces using Android AAudio. Original lifted code still loads WAV/packed-DAT
samples, selects music/dialogue, fills streaming buffers and applies its volume
controls. No replacement soundtrack or PCM assets are bundled in the APK.
FMV Bink decoding/audio remains unavailable; this revision covers gameplay audio.

## SFX correction in 0.5.5

The user heard BGM and Optimus narration in 0.5.4 but no gameplay effects.
The retained log `work/device/audio054-sfx-report.log` shows healthy AAudio
callbacks and zero xruns while playback often remains at one active buffer.
Those aggregate counters alone cannot identify which sample is playing.

The old backend incorrectly limited **all loaded buffers** to 256, including
preloaded source samples. The original game has a separate 64-slot playback
pool; at `0x0054F578` it calls DuplicateSoundBuffer for an SFX voice, returning
failure before Play when duplication fails. Hundreds of loaded sources can
therefore block every new effect without stopping already-created streams.
This is a confirmed implementation mismatch and the leading explanation for
the reported symptom; full audible gameplay verification remains a user test.

Creation/duplication now grow with available memory. An independent active
list keeps the mixer from scanning preloaded assets; Stop, final Release and
one-shot completion remove voices from that list. Repeated Play does not add
the same voice twice. A separate SFX bus is summed before the existing final
clip, with no change to the original volume, pitch or spatial calculations.
Logs now include total buffers, active SFX, Play calls, duplicate successes,
creation/duplication failures and SFX-only nonzero samples/peak. Here SFX means
buffers without the streaming-notification flag, matching this game's paths.

The native audio self-test now preloads 384 sources, duplicates and plays 64
effects concurrently, checks nonzero shared PCM, and checks repeated Play,
Stop, Release and natural one-shot cleanup. This extended regression passed
on the connected phone using the 0.5.6 production library: zero create/duplicate
failures and zero xruns in that short muted test. Evidence:
`work/crash056/audio-self-test.log`. The 0.5.4 results below are historical.

## Original contract and evidence

Input identity: SHA-256
`5aa3bd490f5b97f614a154fdca98cae845bb62c783c48e56042f19ac14aeefd0`.
Static findings use existing generated C corroborated with original PE data.
REA `open_binary` independently identified that exact PE; no Ghidra analysis or
game entrypoint execution is claimed for these findings.

- Init `0x0054EE50` calls DSOUND ordinal 11 through IAT `0x0067C024` at
  `0x0054EE86`. It creates a primary buffer with descriptor size 36 and flags
  `0x11`, sets stereo 44.1 kHz/s16, then requests the 3D listener interface.
- SFX creation uses flags `0xB8`: software, 3D, volume and frequency. The game
  duplicates sample storage into independently controlled voices. Its initial
  3D setup uses head-relative positions, min/max distances of `1e9`, and a
  zero listener rolloff so its own volume code controls attenuation.
- Streaming `0x00552700` requests Buffer8 and **IDirectSoundNotify**, creates a
  four-second PCM ring and sets notifications at 0, quarter, half and three
  quarters, sharing an event. `0x00552930` reads the play cursor and refills
  through Lock/Unlock. Treating the Notify IID as IKsPropertySet would be wrong.
- The original reader `0x005538B0` requires RIFF/WAVE PCM tag 1. A bounded
  header scan of all 362 loose WAV assets found 288 mono 22.05 kHz/s16 and
  74 stereo 48 kHz/s16. Compressed audio decoding is not needed for this path.

Interface orders, counts and GUIDs follow [Microsoft's DirectSound SDK header](https://raw.githubusercontent.com/microsoft/win32metadata/main/generation/WinSDK/RecompiledIdlHeaders/um/dsound.h):
Device8 12 slots, Buffer8 24, Listener3D 18, Buffer3D 21 and Notify 4. Buffer
Initialize has three stdcall arguments including `this`; Spatial slots 16/17
are SetMaxDistance/SetMinDistance. Guest vtables contain HLE tokens, never
native host pointers. Core reference counting retains canonical IUnknown
identity and shares PCM storage across duplicates while preserving independent
volume, frequency, position and playback state.

## Output and threading

The software mixer supports PCM8/16 mono/stereo, linear resampling, volume,
pan, looping, one-shot playback and changed frequency. Software 3D handles
listener/source positions, orientation, head-relative/disabled modes,
distance/rolloff, cones and Doppler. Deferred settings commit together and
immediate property changes survive a later deferred commit. This is stereo
software spatialization; advanced vendor DSP/EAX and HRTF algorithms are not
implemented.

The adapter validates [WAVEFORMATEX](https://learn.microsoft.com/en-us/windows/win32/api/mmeapi/ns-mmeapi-waveformatex),
keeps guest-writeable Lock storage separate from the host's published PCM,
and publishes only the exact [Unlock regions](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee418073(v=vs.85)).
Circular locks return both regions. Duplicate buffers share that published
storage. The play cursor subtracts only queued frames belonging to that
voice, so a newly started loop does not falsely report the end of its ring.

A normal mixer worker fills a bounded preallocated SPSC float ring. The
AAudio high-priority callback only consumes it, applies focus gain and fills
missing frames with silence. It performs no allocation, logging, mutex waits
or guest-memory access, following [AAudio callback requirements](https://developer.android.com/ndk/reference/group/audio).
The worker advances cursors after output underruns. Notification events enter
the guest machine only after releasing the mixer lock, allowing the original
streaming thread to wake and refill its buffer without a lock-order inversion.

AAudio uses shared stereo float output at the device's native rate. The worker
lead and AAudio buffer request are each two bursts. Normal output errors are
reported and retried from the game thread. Shutdown releases the guest machine
while joining workers, then reacquires it while freeing guest allocations.
Process-exit cleanup is registered before playback. GameActivity requests
game audio focus, ducks/mutes on focus changes and sends volume buttons to
the media stream; microphone/recording permission is neither requested nor used.

`[audio]` logs report buffer formats, active voices, nonzero mixed samples,
peak/clipping, callbacks, xruns and queued frames alongside the existing FPS log.
These counters establish processing, not an audible end-to-end observation.

## Verification and limits

- 12 native PCM mixer fixtures pass: silence, unsigned8, signed16, stereo,
  resampling, gains, clipping, loop/one-shot/seek cursors and underrun catchup.
- 11 native contract fixtures pass: PCM header validation, malformed fields,
  wrapped locks, flag overrides/overflow and notification boundaries/wraps.
- Six host probe-boundary tests pass; one optional synthetic Android-link test
  is skipped. Native runtime self-tests pass on the connected ARM64 phone.
- The new `transformers-probe --audio-self-test` executes synthetic guest32 COM
  calls against the exact production library and a real **muted** AAudio stream.
  It verifies Device8/Buffer8/3D interfaces, primary format, shared duplicates,
  independent pitch/volume, initial loop cursor, quarter-ring event delivery,
  wrapped publication and Stop. The device reported 48 kHz stereo float,
  192-frame bursts, 384-frame worker lead, 269 callbacks, 96,374 nonzero mixed
  samples before mute, and **zero xruns during this short test**. It never
  enters the game executable or operates a display/input surface.
- All four native ABIs build. The APK's ARM64 library matches the tested one;
  shader cache and Turnip files are unchanged. Existing app preferences/config
  are retained during the update. Test files under `/data/local/tmp/tf-audio054`
  are removed afterward.

Audibility, full gameplay streaming and route/focus changes still require the
user's test. No recording, screenshot, automated touch/key input or app launch
was performed by the assistant. Other ABIs were compiled, not run on a device.

Evidence: `work/audio054/{host-tests,self-test,audio-self-test}.log`,
`work/audio054/native-test-result.json`, `work/device/audio054-validation.json`.
Artifact: `work/android-audio-054/transformers-dxvk-universal-debug.apk`.
