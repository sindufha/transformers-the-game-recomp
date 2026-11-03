# Startup and virtual window model

This is a static finding ledger for input SHA-256
`5aa3bd490f5b97f614a154fdca98cae845bb62c783c48e56042f19ac14aeefd0`.
The initial findings below came from static analysis. Generated-code line
references describe the frozen target lift. A subsequent Android startup
observation is recorded separately below; none of the synthetic unit tests
executes proprietary code.

## Proven early calls

| Guest address | Static observation | Port implication |
| --- | --- | --- |
| `0x00666EAD` | CRT entry calls SEH prolog `0x0066ABDC`, then `GetVersionExA` through IAT `0x0067C088`. | The normal prolog writes the simulated `FS:[0]` chain. It does not call the fatal `recomp_eh_push` helper. Exception delivery and unwinding remain unsupported. |
| `0x00667033` | CRT entry directly calls `0x005E6B50`. | Candidate WinMain is established by entry analysis and subsequently appears as the calling function in the Android missing-SPI failure log. |
| `0x005E6B75`, `0x005E6B82`, `0x005E6B8F` | WinMain reads StickyKeys, ToggleKeys and FilterKeys using `SystemParametersInfoA`. | Startup requires initialized 8/8/24-byte guest structures rather than a generic successful no-op. |
| `0x005E6B93` | WinMain calls `0x005E5B70(0)`. That routine makes SPI SET calls for those accessibility structures. | These requests must stay in virtual guest state and must not change Android accessibility settings. |
| `0x005E6BA6`, `0x005E6BB0` | WinMain gets then disables the screensaver using SPI actions `0x10`/`0x11`. | The implementation stores one virtual BOOL; it does not alter host power management. |
| `0x005E6BBB` | WinMain calls `CoInitializeEx(NULL, 2)`. | Apartment bookkeeping must distinguish first, nested, conflicting-model and separate-thread calls. |
| `0x005E62CD`, `0x005E62EC` | Early setup calls `CoInitialize` then `CoCreateInstance`. If creation fails, the explicit fallback sets its result to 1. | Returning `REGDB_E_CLASSNOTREG` is the faithful result for an unavailable COM server. No fake WMI object is created. This fallback is static evidence, not proof the overall startup succeeds. |
| `0x005E5C7E` | Locale setup opens a registry key with `RegOpenKeyExA`; failure branches to `GetUserDefaultLangID` at `0x005E5CB6`. | The new services module answers missing read-only keys with `ERROR_FILE_NOT_FOUND`. The game has an explicit missing-key fallback. |
| `0x005E6F09` | Window setup registers a 48-byte `WNDCLASSEXA`, WndProc `0x005E6C20`, with zero class/window extra bytes. | The bounded virtual class model covers this structure shape. Custom icon load failure is not checked before class registration. |
| `0x005E6FA4` | Window setup calls `CreateWindowExA`. | Window callbacks and a real guest `HWND` identity are required before DXVK can attach presentation to the host surface. |
| `0x005E6536`, `0x005E654D`, `0x005E6554` | The message loop calls `PeekMessageA`, `TranslateMessage`, `DispatchMessageA`. | Queued input must retain order and correct MSG32 layout; `PM_NOREMOVE` must not consume messages. |
| `0x006644C4` | A thunk tail-calls IAT `0x0067C2E8`, `Direct3DCreate9`. | This remains the graphics bridge boundary, independent of USER32 bookkeeping. |
| `0x0053121C`, `0x0053123E` | A helper acquires a CryptoAPI provider and requests random bytes. The provider string at `0x0069385C` is `Microsoft Base Cryptographic Provider v1.0`, type 1, first flags `0xF0000020`. | The new services module accepts this ephemeral context and uses libc `getrandom`; persistent key containers remain unsupported. Reachability/timing of this helper is not established here. |

The saved REA entry analysis is Evidence
`ev_fa63de7b96d1b6da4990fc24ea30075d09d24445b9541b829e4b5a2e37ed361e`;
the WinMain analysis is
`ev_c72321380ee0f8bdb153bdeba8cf55b7128043be8b4a268e2128f25318805ebb`.
Both are preserved in `work/native-analysis.json`.

## Observed Android entry boundary

`work/device/base-startup.log` records an Android probe mapping the verified
image at `0x00400000`, binding all 189 imports to either implementations or
named fatal boundaries, then entering `0x00666EAD`. It stopped at:

```text
FATAL: unimplemented import SystemParametersInfoA (...) called from guest 0x005E6B50
```

This establishes that this build reached the WinMain function before failing
on its first SPI call. It does not establish that the new window/services
implementations pass startup, that DXVK presents frames, or that gameplay is
available. The full log also reports an unanswered dynamic lookup for
`IsProcessorFeaturePresent`; that lookup did not itself stop this run.

## Implemented module

`runtime/startup_user32.c` registers a bounded virtual window/event layer with
no SDL dependency. It supports 32 classes, 32 windows, a 256-message queue,
`WM_NCCREATE`/`WM_CREATE`/`WM_DESTROY`/`WM_NCDESTROY` callbacks, show/move/activation,
basic window metrics, and per-guest-thread COM initialization counts. The
module has 28 named registrations; registration count is not proof of complete
Windows API compatibility.

SPI GET/SET accessibility requests validate `uiParam` and `cbSize` and roundtrip
their complete structures. Unsupported SPI actions return FALSE with error
120; invalid supported request shapes return FALSE with error 87. The
screensaver flag is local to the guest. No host setting is changed.

`CoInitialize`/`CoInitializeEx` return `S_OK`, `S_FALSE`, `RPC_E_CHANGED_MODE` or
`E_INVALIDARG` according to stored guest-thread state. Successful nested calls
need matching `CoUninitialize` calls. `CoCreateInstance` clears its output and
returns `CO_E_NOTINITIALIZED` or `REGDB_E_CLASSNOTREG`; there are no class servers
or proxy-marshalling facilities in this module.

The Android host can use:

```c
tf_user32_register();
tf_user32_set_display(width, height); /* under guest machine lock */
tf_user32_set_pump(pump_events);      /* called by PeekMessageA */
tf_user32_key_event(virtual_key, down); /* safe from Android UI thread */
```

Key events enter a mutex-protected queue. The guest drains them while pumping
messages, preserving key-up events if its main queue is full. The host must
check a FALSE enqueue result. The event model does not yet translate Android
scan codes, text input, DirectInput devices or XInput controllers.

Current limitations are explicit: one guest pump thread, no cross-thread
SendMessage delivery, no custom icon/resource cursor decoding, no Android
message-box adapter, no host cursor movement, no non-client desktop drawing,
no exact Windows keyboard-layout translation and no complete Win32 focus/window
manager behavior. `LoadIconA` and unimplemented custom `LoadCursorA` return
failure; MessageBox logs its text and returns failure rather than inventing a
button click. `timeGetTime` uses the existing monotonic millisecond clock.

## Validation boundary

`tests/test_startup_user32.py` executes only this module with a synthetic guest
and fake WndProc, not game code. Cases check SPI roundtrip and validation,
COM nesting/mode conflict/thread isolation, window creation failure cleanup,
callback order, key/quit queue behavior, invalid HWNDs and stdcall argument
purging. The module also passes strict Clang syntax checks for Android API 29
on ARM64, ARMv7, x86 and x86_64. These tests do not establish on-device rendering
or a playable game.

API contracts were checked against Microsoft documentation for
[SystemParametersInfoA](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-systemparametersinfoa),
[CoInitializeEx](https://learn.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-coinitializeex),
and [CreateWindowExA](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-createwindowexa).

## Private filesystem, registry and random services

`runtime/startup_services.c` is independent of the window layer and registers
16 names, including two named BSTR functions and their ordinal aliases.
The host calls `tf_services_register(game_dir)` before import binding. The
directory must be the existing private app game directory. NULL permits
registering the non-filesystem services without configuring save folders;
invalid folder setup returns -1.

The registry is deliberately empty and read-only. Valid predefined roots can
be opened with an empty subkey and return their original identity. Missing
subkeys and values return error 2, invalid handles return 6, unsupported write
access returns 5, and invalid reserved/options arguments return 87. No locale,
installation path or license value is fabricated.

`SHGetFolderPathA` supports CSIDL_PERSONAL for the current virtual user, using
`game_dir/documents/` mounted at `D:\`. The game uppercases read-side paths;
this mount avoids case lookup through protected Android ancestors such as
`/data`, which caused pcconfig reads to fail on host-absolute `Z:` paths.
Physical config/save locations remain unchanged. Unsupported
CSIDLs/tokens/flags fail. `SHCreateDirectoryExA` accepts absolute guest paths
only inside that subtree. It walks components using directory descriptors,
`openat`, `mkdirat` and `O_NOFOLLOW`; prefix collisions, `..` and symlink escapes
fail. Creation uses mode 0700. This module does not expose Android public
Documents or invoke a host shell; `ShellExecuteA` returns association failure.

CryptoAPI contexts use audited unused HLE handle type 12. The supported subset
is `PROV_RSA_FULL`, the default or Microsoft Base provider, NULL/empty container
and `CRYPT_VERIFYCONTEXT`, optionally combined with MACHINE_KEYSET/SILENT. It
has no persisted keys, hashing or encryption functions. `CryptGenRandom` loops
over real libc `getrandom` calls, handles interrupted/partial reads and reports
failure rather than substituting weak or zero-filled bytes. Released contexts
cannot generate more bytes.

`CoCreateGuid` uses the same secure random source with UUID v4/variant bits;
`UuidToStringA` preserves the GUID field byte order, and `RpcStringFreeA` releases
its guest heap allocation and clears the caller's pointer. SysAllocString and
SysFreeString implement the UTF-16 length-prefixed BSTR layout for the game's
OLEAUT32 ordinals 2 and 6.

Five tests in `tests/test_startup_services.py` pass. They execute the actual new
C shims with synthetic guest memory and injected filesystem/random results,
checking registry fallback codes, typed crypto lifetimes, EINTR/partial reads,
OS random failure, UUID text/ownership, BSTR bytes, directory recursion and
escape/symlink rejection. Four Android ABI syntax checks use the actual NDK
headers. The injected filesystem/random tests do not establish kernel behavior
on Android; `tf_services_selftest()` is provided for a real-device random and
UUID formatter check.

Service contracts were checked against Microsoft documentation for
[RegOpenKeyExA](https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regopenkeyexa),
[CryptAcquireContextA](https://learn.microsoft.com/en-us/windows/win32/api/wincrypt/nf-wincrypt-cryptacquirecontexta),
[SHGetFolderPathA](https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shgetfolderpatha),
[SHCreateDirectoryExA](https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shcreatedirectoryexa)
and [UuidToStringA](https://learn.microsoft.com/en-us/windows/win32/api/rpcdce/nf-rpcdce-uuidtostringa).
