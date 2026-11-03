# Reviewed code/data boundaries and software interrupts

Reviewed on 22 October 2026 against the frozen input
`work/lift-5aa3bd490f5b/input.exe`, SHA-256
`5aa3bd490f5b97f614a154fdca98cae845bb62c783c48e56042f19ac14aeefd0`.
All addresses below are preferred-image virtual addresses, and range ends are
exclusive. This review reads bytes and decodes instructions with Capstone 5.0.7;
it does not run the game. The machine-readable decision is
`config/code-regions.json`.

## DirectInput arrays

**Exclude `[0x006644d0, 0x00665fc0)` from instruction discovery and traversal.
Keep its bytes mapped as guest data.** It contains three contiguous arrays of
16-byte `DIOBJECTDATAFORMAT` records. Each has a matching 24-byte
`DIDATAFORMAT` descriptor in another section:

| Array start | Array end | Count | Descriptor | Data packet size | Identification |
|---|---|---:|---|---:|---|
| `0x006644d0` | `0x006654d0` | 256 | `0x006ab514` | `0x100` | Keyboard |
| `0x006654d0` | `0x00665f10` | 164 | `0x006ab71c` | `0x110` | Joystick2 |
| `0x00665f10` | `0x00665fc0` | 11 | `0x006ab924` | `0x14` | Mouse2 |

Observed descriptor DWORDs, in order:

```text
0x006ab514: 00000018 00000010 00000002 00000100 00000100 006644d0
0x006ab71c: 00000018 00000010 00000001 00000110 000000a4 006654d0
0x006ab924: 00000018 00000010 00000002 00000014 0000000b 00665f10
```

The fields match the documented sequence `dwSize`, `dwObjSize`, `dwFlags`,
`dwDataSize`, `dwNumObjs`, and `rgodf`. In this PE32 image they give the exact
array bounds above. Each object record contains a GUID pointer, packet offset,
type, and flags. These layouts and their relationship to `SetDataFormat` are
documented by Microsoft in
[DIDATAFORMAT](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee416606(v=vs.85))
and [DIOBJECTDATAFORMAT](https://learn.microsoft.com/en-us/previous-versions/windows/desktop/ee416633(v=vs.85)).

For every keyboard record `i` from 0 through 255, the four observed DWORDs are
`{0x006ab294, i, 0x8000000c | (i << 8), 0}`. The pointed-to bytes at
`0x006ab294` decode to GUID `55728220-d33c-11cf-bfc7-444553540000`, identified
as `GUID_Key` by the
[mingw-w64 DirectInput header](https://github.com/mingw-w64/mingw-w64/blob/master/mingw-w64-headers/include/dinput.h).
The keyboard record pattern, joystick aspect fields, and mouse count also agree
with the independently maintained
[Wine DirectInput data format implementation](https://github.com/wine-mirror/wine/blob/master/dlls/dinput/data_formats.c).
These names are an inference from the matched structures; no original linker
symbols were recovered.

The immediately preceding instruction at `0x006644ca` is the six-byte IAT
thunk `ff2524c06700`. The instruction at `0x00665fc0` is the six-byte IAT
thunk `ff251cc06700`. Both remain code. Full byte hashes, each descriptor's
expected bytes, and both boundary anchors are retained in the JSON profile.
The complete 6,896-byte excluded region has SHA-256
`d7222c0908ce4572f4843633a026fcb5cc39f3c3dad5b3e46edf21b09a6a8adb`.

The previous catalog's `sub_006644D0` has a reported size of 4,201 bytes and
1,504 decoded instructions, all starting inside the first two arrays. Its
`lcall` at `0x00664e79`, `hlt` at `0x006654e0`, and `aam` at `0x00665500`
are consequences of reading object records as instructions. The saved Ghidra
result in `work/native-analysis.json` also reports no procedure at
`0x006644d0`; this corroborates the byte evidence but is not the reason for
the exclusion.

There is one raw pointer to `0x006644d0` in the file: the `rgodf` field at
`0x006ab528`. The pinned disassembler's `find_data_code_pointers` scans raw
pointer values in non-code sections. Its `probes_as_function_body` accepts a
full 4,096-byte clean decode without a terminator; these repetitive records
satisfy that heuristic. This explains how data can enter the catalog even
though its owner is a data-format descriptor. Re-run discovery with the
exclusion; do not merely delete generated C diagnostic lines.

## Real software interrupt sites

**Preserve all sixteen `CD 06` instructions as code.** They are selected by an
observed 64-way computed jump, rather than by speculative pointer scanning:

```text
0x00673de7: 83 ec 2c                sub esp, 0x2c
0x00673dea: 83 e0 3f                and eax, 0x3f
0x00673ded: ff 24 85 9e 45 6f 00    jmp dword ptr [eax*4 + 0x006f459e]
```

All 64 DWORDs in `[0x006f459e, 0x006f469e)` point to code entries in
`[0x00673df4, 0x00674286)`. The table SHA-256 is
`e3d17a78755a6a11b5fc6eac31df38888cf927ccaacfc61cea15accfdaab9b89`.
Every entry listed below begins with `83 c4 2c cd 06`: restore the temporary
44-byte stack allocation, then request software interrupt 6.

| Table index | Entry address | Interrupt address |
|---:|---|---|
| 1 | `0x00673dfa` | `0x00673dfd` |
| 3 | `0x00673e05` | `0x00673e08` |
| 9 | `0x00673e3e` | `0x00673e41` |
| 11 | `0x00673e59` | `0x00673e5c` |
| 17 | `0x00673ece` | `0x00673ed1` |
| 19 | `0x00673eed` | `0x00673ef0` |
| 25 | `0x00673f72` | `0x00673f75` |
| 27 | `0x00673f91` | `0x00673f94` |
| 33 | `0x00674016` | `0x00674019` |
| 35 | `0x00674035` | `0x00674038` |
| 41 | `0x006740ba` | `0x006740bd` |
| 43 | `0x006740d9` | `0x006740dc` |
| 49 | `0x0067415e` | `0x00674161` |
| 51 | `0x0067417d` | `0x00674180` |
| 57 | `0x00674202` | `0x00674205` |
| 59 | `0x00674221` | `0x00674224` |

The real FDIV helper at `0x00673cd0` loads two 80-bit operands, classifies
their exponent and significand bits, adjusts x87 control state, and can scale
the operands using `0.9375` and `1.0625` constants at `0x006f4550` and
`0x006f4554`. Nearby wrappers at `0x00674286` and `0x00674299` spill x87
operands and call that helper. These observations strongly suggest a CRT
Pentium FDIV workaround family. They do not identify its exact source version
or prove which selector values the game passes during normal execution.
Microsoft's historical compiler behavior is described in its
[LNK2001 floating division support note](https://ftp.zx.net.nz/pub/archive/ftp.microsoft.com/MISC/KB/en-us/128/895.HTM)
(a mirror of Microsoft KB128895).

The pattern of forbidden selectors is consistent with unused opcode forms,
but this is an inference. It does not justify replacing an interrupt with an
arithmetic operation, an early return, or a no-op.

`INT 6` is a **software interrupt request**, not the `UD2` instruction and not
automatically the same event as a processor-generated invalid-opcode fault.
Intel specifies that protected-mode `INT n` checks the selected IDT gate's
privilege and can fault with general protection when access is disallowed.
See the `INT n` operation and exception definitions in
[Intel SDM Volume 2A](https://cdrdv2-public.intel.com/868140/253666-089-sdm-vol-2a.pdf)
and the privilege discussion in
[Intel SDM Volume 3A](https://www.intel.com/content/dam/www/public/us/en/documents/manuals/64-ia-32-architectures-software-developer-vol-3a-part-1-manual.pdf).
The executable bytes alone do not establish Windows IDT state or the final
Windows exception code.

The supported initial translation is a nonreturning host trap hook retaining
the vector, instruction address, next address, guest registers/stack, flags,
and x87 state. If Windows interrupt/SEH delivery is later implemented, that
hook can route to its verified behavior. Until then, terminate with a precise
unsupported guest interrupt diagnostic. The instruction after a trap must not
run by ordinary fallthrough. This is an explicit runtime limitation even if
the generated C compiles successfully.

## Validation boundary

- Reject the profile for any different complete input SHA-256; also validate
  the recorded range hashes, descriptor anchors, and code anchors.
- Preserve the exact data bytes and neighboring IAT thunks. Discovery and
  recursive traversal must not create an instruction inside an excluded range.
- Retain all sixteen table-selected trap entries. An unsupported `INT` at any
  other address must still fail translation; this review is not a generic
  allowlist for arbitrary interrupt vectors.
- Exercise a synthetic trap body with a sentinel instruction after the trap.
  Check vector/address/context and prove the sentinel is never executed.
- Re-run full discovery and diagnostic scanning. Reduced function counts are
  expected after rejecting data, but neither counts nor syntax checks prove
  whole-program recovery or gameplay correctness.
- Device startup, normal runtime reachability of these selectors, exact
  Windows exception mapping, and x87 precision remain unverified by this
  static review.
