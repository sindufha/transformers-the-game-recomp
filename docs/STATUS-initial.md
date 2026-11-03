# Status aktual — 22 Oktober 2026

**Tahap: analisis dan generation C. Belum ada APK atau game yang bisa dimainkan.**

## Hasil yang diverifikasi

| Pemeriksaan | Hasil | Bukti lokal |
|---|---|---|
| Input terbaru | PE32 x86, 3.702.784 byte, entry `0x00666ead` | `work/preflight.json` |
| Identitas input | SHA-256 `5aa3bd490f5b97f614a154fdca98cae845bb62c783c48e56042f19ac14aeefd0` | preflight dan snapshot `work/lift-5aa3bd490f5b/input.exe` |
| Import pada disk | 189 simbol, 16 DLL | `work/preflight.json` |
| Katalog dan generation C | 14.471 entri dispatch, entry tercakup, 15 chunk C + dispatch | `work/lift-5aa3bd490f5b/functions.json`, `gen/` |
| Kualitas translasi | **Belum lengkap**, 23 diagnostik pada 19 entri | `work/lift-5aa3bd490f5b/lift-summary.json` |
| Sintaks untuk Android ARM64 | **16/16 file lolos**, Clang 18.0.2, API 29, NDK 27.1.12297006 | `work/lift-5aa3bd490f5b/arm64-syntax-summary.json` |
| Unit test tooling | 9 tes lolos | `python -m unittest discover -s tests -v` |
| Gate CMake | Benar-benar menolak lift yang belum lengkap sebelum inisialisasi compiler | `work/cmake-gate-check.log` |
| Smoke pipeline | Fixture PE sintetis diterjemahkan tanpa diagnostik; object ELF AArch64 berhasil dibuat | `work/smoke/lift/`, `tiny-arm64.o` |
| Runtime game, rendering, gameplay | **Belum dijalankan atau diverifikasi** | Belum ada runtime Android |
| Perangkat Android | Tidak ada device tersambung saat `adb devices` | Tidak ada hasil uji device |

C yang dihasilkan berukuran sekitar 182 MB. Jumlah entri dispatch mencakup alias
dan hasil penemuan heuristik; angka itu bukan bukti jumlah fungsi asli atau
kelengkapan recovery. Pemeriksaan sintaks tidak melakukan linking atau
menjalankan instruksi game.
Archive game belum dibangun karena gate translasi masih menahan build.

## Diagnostik yang menahan build

| Jenis | Jumlah | Temuan |
|---|---:|---|
| `lcall`, `hlt`, `aam` | 3 | Semua berasal dari kandidat `sub_006644D0`. Byte menunjukkan pola berulang menyerupai tabel; Ghidra tidak mengenali prosedur pada alamat tersebut. Batas kode/data perlu dikoreksi berdasarkan bukti. |
| `int 6` | 16 | Berada di beberapa routine x87 dekat `0x00673dfa`; perilaku trap dan jalur yang menggunakannya belum dipetakan. |
| `fnstenv` | 2 | Penyimpanan environment x87 belum didukung lifter. |
| `fldenv` | 2 | Pemulihan environment x87 belum didukung lifter. |

Tidak ada diagnostik yang dihapus atau dianggap sukses. Upstream mengembalikan
exit code 0 meskipun menghasilkan `UNIMPLEMENTED:`; driver menandai hasil
`lift_incomplete` dan CMake menolak membangun archive game.

## Bukti analisis native

- Input terbaru dibuka oleh REA dengan SHA-256 yang sama seperti snapshot lift.
- CRT entry `0x00666ead`: Evidence
  `ev_fa63de7b96d1b6da4990fc24ea30075d09d24445b9541b829e4b5a2e37ed361e`.
  Pseudocode menunjukkan setup versi Windows, heap, I/O, environment, lalu
  memanggil `0x005e6b50` dengan empat argumen yang sesuai pola WinMain.
- Kandidat WinMain `0x005e6b50`: Evidence
  `ev_c72321380ee0f8bdb153bdeba8cf55b7128043be8b4a268e2128f25318805ebb`.
  Memanggil `SystemParametersInfoA`, `CoInitializeEx`, dan mengatur affinity
  thread. Label WinMain merupakan inferensi dari alur CRT dan signature panggil.
- `0x006644d0`: `analyze_function` mengembalikan “No procedure exists at the
  requested address”. Ini hanya hasil database analisis Ghidra, bukan bukti
  bahwa alamat tersebut mustahil dieksekusi.

Pseudocode adalah hasil decompiler, bukan source asli. Hasil lengkap disimpan
lokal di `work/native-analysis.json`. Game tidak dieksekusi dalam investigasi.

## Riwayat identitas input

Pada awal investigasi, file bernama sama berukuran 5.174.017 byte dengan hash
`193933d44ec5820784e3ff2de403e8b761f51be2b8cc0192c3daa090d7fd3c8b`.
Entry berada di `stxt371`, `.text` berentropi 7,999, hanya ada 21 import, dan
Ghidra menunjukkan entry menulis ulang instruksinya. Temuan itu mendukung
dugaan packing pada **input awal saja**; vendor proteksi tidak diidentifikasi.

Input di direktori berubah saat investigasi menjadi identitas terbaru di atas.
Penyebab perubahan tidak diketahui. Tool proyek membaca executable dan membuat
snapshot di `work/`; tidak ada langkah proyek yang mengubah file game asli.
Hasil REA awal tersimpan di `work/historical-rea.json`; tidak dipakai sebagai
bukti untuk executable terbaru.

## Pekerjaan port yang masih terbuka

1. Koreksi recovery kode/data dan implementasikan semantik instruksi tersisa
   dengan pengujian terhadap perilaku x86.
2. Sediakan guest memory 32-bit, IAT binding, SEH/C++ exception, indirect-call
   dispatch, serta CPU/thread state lengkap termasuk SSE, MXCSR dan flags.
3. Integrasikan Win32 yang benar-benar dipanggil game. Import pada disk belum
   mencakup semua metode COM atau API yang di-resolve secara dinamis.
4. Implementasikan bridge Direct3D 9/D3DX, DirectInput 8, XInput, DirectSound,
   Bink, dan path/storage Android. `import_stubs.c` hanya daftar diagnostik.
5. Bangun aplikasi Android, sambungkan surface/lifecycle dan kontrol sentuh,
   lalu verifikasi startup → menu → loading → gameplay → save/load pada device.

`pcrecomp/runtime/win32hle` menyediakan sebagian fondasi POSIX, tetapi tidak
menyediakan D3D9, DirectInput, XInput, atau SEH/C++ exception. State thread di
host_lite dan implementasi CPUID non-MSVC juga perlu pekerjaan sebelum dapat
dipakai untuk game ini. Proyek NFSU2 menggunakan bridge dan penyesuaian khusus
alamat game; mengganti nama executable pada proyek tersebut tidak cukup.

Inventaris aset: dua DAT, 362 WAV dan 118 BIK ditemukan. Isi DAT dan codec
media belum dianalisis. Aset tidak disalin ke proyek.

## Sumber implementasi

- [Referensi NFSU2](https://github.com/codepdbh/nfsu2-recompiled)
- [Translator pcrecomp yang dipin](https://github.com/sp00nznet/pcrecomp/blob/8c92919a99aaf9e129af5787776bca84863468cc/tools/lift/translator.py)
- [Runtime Win32 HLE](https://github.com/sp00nznet/pcrecomp/blob/8c92919a99aaf9e129af5787776bca84863468cc/runtime/win32hle/README.md)
- [Register/thread state](https://github.com/sp00nznet/pcrecomp/blob/8c92919a99aaf9e129af5787776bca84863468cc/runtime/win32hle/host_lite.c)
- [CPU model dan dispatch](https://github.com/sp00nznet/pcrecomp/blob/8c92919a99aaf9e129af5787776bca84863468cc/runtime/recomp32/recomp_types.h)
