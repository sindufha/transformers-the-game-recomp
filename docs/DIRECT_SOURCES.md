# Direct game-folder access — 0.5.22

The launcher now selects the original game folder through Android's document
picker and retains its read permission. Selection verifies all 482 expected
assets by size/SHA-256, saves only a small URI index, and accepts the supported
EXE from that folder when present. The separate EXE picker also verifies and
retains its URI instead of copying its bytes. No Windows DLL is imported.

At launch, the game process opens read-only ParcelFileDescriptors for the
selected EXE/assets and retains them until the native session returns. Private
game paths contain symbolic links to those process-local descriptors. These links are lookup tokens only: native open duplicates the granted FD,
and read uses pread64 with an independent virtual offset per open. dup handles
share their offset; lseek/lseek64, stat/stat64 and read-only stdio callbacks
operate on descriptors directly. No source path is reopened: Android can grant
a provider FD while rejecting /proc/self/fd reopening with EACCES. Archives,
audio, movies and EXE are not copied. Files must be regular and seekable; Java
checks descriptor metadata and seekability before entering native code.
A source removed or permission revoked produces an error, never a fallback to
an old imported private copy. Stale descriptor links from a terminated process
are replaced on the next mount. Activity/surface recreation retains the live
session and its descriptors.

The native C: mount keeps the EXE's guest path stable even when its real host
path belongs to an external documents provider. Documents/save/configuration
remain in the existing private D: location. The source-file adapter rejects
write/truncate/append, unlink, and rename operations on descriptor-backed
originals. Normal private save and settings writes remain available.

Only after successful source verification and index publication does selection
remove size/hash-matching old private asset copies at the exact manifest paths.
It never removes the selected source, save files, configuration, or modified
local copies. Cleanup is deferred while the separate game process is alive;
mount also removes verified private duplicates before replacing them with links.
Missing source selections are required again after this update; private imports
alone do not make the launcher ready. The APK itself still includes generated
native code, the existing shader cache and drivers, not original game assets.

## Evidence

- `tools/test_direct_sources_android.py --serial <serial>` builds a disposable
  synthetic DocumentsProvider/instrumentation APK. It executes the production
  Java source-selection/mount code on Android: source hashes, no data/EXE
  copies, descriptor links, independent offsets/seek, session cleanup,
  unavailable sources, ambiguous names, cancellation, verified old-copy
  removal, and save/original preservation. The fixture app is uninstalled.
  Result: `work/tests/direct-saf/result.log`.
- Production ARM64 `--source-files-self-test` exercises the actual native
  adapter's read/seek behavior, write/delete rejection and private writes.
  It uses synthetic files only and never starts a game or a screen surface.
- Existing host suite: 187 pass, one optional Android-link test skipped.
- Four native ABIs rebuilt using the existing generated-code archives.
  Source APK: `work/android-direct-0522/transformers-dxvk-universal-debug.apk`.
  Package/install evidence: `work/direct0522/validation.json`.

The device tests use a synthetic local provider, not the user's selected game
folder. Actual folder selection and gameplay after installation remain a user
test. Android URI permission persistence is requested from the system picker;
there is no broad storage permission or system-driver change.

## 0.5.24 regression

The production ARM64 self-test now retains an open descriptor then removes file
permissions, proving raw pathname reopening fails EACCES while adapter reads,
independent open offsets, shared dup offsets, 64-bit seeking and stdio succeed.
Write/delete rejection, unchanged original bytes and private save writes are
checked too. Evidence: `work/sources0524/native-source-test.log`.

`tools/test_cross_uid_sources_android.py` additionally builds two disposable
fixture apps with different Android UIDs, grants a SAF tree from the provider
to the client, and exercises production Java mount plus native source adapter.
The native check requires raw reopening to fail EACCES before testing direct
reads of the synthetic EXE/assets. Both apps are uninstalled afterward.
