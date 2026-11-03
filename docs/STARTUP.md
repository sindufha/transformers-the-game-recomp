# Bridge API startup

`runtime/startup.c` mendaftarkan subset yang dipilih dari runtime pcrecomp
pada revisi `8c92919a99aaf9e129af5787776bca84863468cc`. Source upstream tetap
utuh; hash source dan overlay dicatat di setiap `probe-build-summary.json`.
Aktifkan melalui `tools/build_probe.py --startup`.

Untuk input EXE yang didukung, **83 dari 99 import bernama KERNEL32** mempunyai
registrasi. Ini angka cakupan nama, bukan bukti 83 API telah lolos uji game.
Modul selain KERNEL32 belum dihubungkan. Import lainnya tetap fatal.

Subset upstream meliputi heap, TLS, event/mutex, critical section, beberapa
query locale/ANSI, modul/resource, file/directory dan waktu. Modelnya terbatas:
POSIX file I/O, satu heap guest, locale bawaan dan penjadwalan guest serial.
Tidak menjanjikan seluruh perilaku Windows untuk semua parameter.

Overlay proyek menyediakan:

- `GetStdHandle` / `SetStdHandle`: nilai handle disimpan dan dapat diganti.
- `SetUnhandledExceptionFilter`: menyimpan pointer dan mengembalikan pointer
  sebelumnya; belum memanggil filter saat exception.
- `SetHandleCount`: perilaku API usang mengembalikan jumlah yang diminta.
- API affinity: model satu CPU guest dengan mask 1; handle/mask yang tidak
  didukung ditolak. Tidak mengubah affinity host.
- `SetFilePointerEx`: argumen stdcall 64-bit, posisi hasil 64-bit, validasi
  method, handle dan offset negatif. Model memakai `lseek64` dan rentang
  offset bertanda POSIX, bukan seluruh semantik unsigned Windows.

Import KERNEL32 yang belum didaftarkan:

`CreateThread`, `DebugBreak`, `ExpandEnvironmentStringsA`, `GetSystemDirectoryA`,
`GetTimeZoneInformation`, `HeapDestroy`, `IsBadCodePtr`, `IsBadReadPtr`,
`IsBadWritePtr`, `RaiseException`, `ResumeThread`, `RtlUnwind`,
`SetThreadPriority`, `UnhandledExceptionFilter`, `VirtualProtect`, `VirtualQuery`.

`--self-test` pada probe startup menguji alokasi/realloc/free, TLS, event,
standard handle, previous exception filter, file read/write dan seek melewati
4 GiB tanpa menulis di posisi tersebut. Self-test ini berhasil dikompilasi
dan di-link untuk empat ABI, **belum diamati berjalan di Android**.
Tes host menjalankan helper state handle/filter dan memeriksa assembly ARMv7
agar akses memori guest tidak memakai opcode yang menuntut alignment 8-byte.

CRT entry statis `0x00666EAD` memasang frame melalui helper `0x0066ABDC`,
memanggil API versi/modul, menyiapkan CRT, lalu memiliki call ke kandidat
WinMain `0x005E6B50`. Bukti REA entry:
`ev_fa63de7b96d1b6da4990fc24ea30075d09d24445b9541b829e4b5a2e37ed361e`.
Ini hubungan statis; urutan yang benar-benar tercapai belum direkam runtime.
