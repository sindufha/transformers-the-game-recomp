# Launcher Transformers Android 10+

Build terbaru 0.5.17: `work/android-controls-0517/transformers-dxvk-universal-debug.apk`.
BACK besar di menu mengirim Backspace. Cache shader mencakup 14 shader tambahan dari uji Decepticon Chapter 1. Label kontrol memakai bahasa Inggris. Posisi menu/robot/mobil mengikuti tiga gambar pengguna; menu memakai pad arah dan ENTER. Deteksi Start Mission/selesai misi serta kontinuitas kontrol selama transformasi dipertahankan.
Fullscreen immersive dan resume sesi melalui Recent Apps tersedia; rendering/audio dijeda selama background. Lihat [ANDROID_LIFECYCLE.md](ANDROID_LIFECYCLE.md). APK sebelumnya disimpan sebagai cadangan; salinan lama/perantara dibersihkan.
Minimum SDK 29, empat ABI: `arm64-v8a`, `armeabi-v7a`, `x86`, `x86_64`.
APK menggunakan tanda tangan debug lokal, tanpa permission jaringan atau
permission penyimpanan umum. Level pertama sudah mencapai gameplay 3D;
audio gameplay memakai bridge DirectSound/AAudio; FMV Bink belum tersedia.

Fungsi aplikasi:

1. **Pilih EXE** menyalin input melalui document picker dan
   memeriksa ukuran serta SHA-256. EXE dengan identitas berbeda ditolak.
2. **Impor folder game** meminta folder utama game melalui SAF, memeriksa
   482 file data (4.346.085.350 byte, sekitar 4,05 GiB), lalu menyalin file yang
   cocok ke penyimpanan privat. Semua file diperiksa ukuran dan SHA-256-nya.
   Batal/gagal menyisakan file yang sudah valid; pemilihan folder yang sama
   memeriksa kembali file tersebut dan melanjutkan yang kurang. Ruang kosong
   diperiksa sebelum penyalinan. File asli tidak diubah.
3. **Grafis** menyediakan resolusi internal 640x360, 858x482 atau 1280x720,
   High Res Texture, Bloom, Bayangan dan Motion Blur. Pilihan diterapkan
   sebelum proses game dimulai. Lihat [GAME_SETTINGS.md](GAME_SETTINGS.md).
4. **Mulai Game** menjalankan kode hasil recompilation dengan DXVK.
   Menu runtime/startup/pemetaan dan salin hasil uji sudah dihapus dari launcher.

Folder sumber dipilih pengguna menggunakan
[ACTION_OPEN_DOCUMENT_TREE](https://developer.android.com/training/data-storage/shared/documents-files#grant-access-directory).
Importer hanya membaca DAT yang ditentukan serta WAV/BIK dalam manifest;
tidak mengimpor DLL Windows. Nama file sumber dicocokkan tanpa membedakan
huruf besar/kecil, lalu disimpan dengan nama dan hierarki yang diharapkan.
Penyalinan streaming memakai memori tetap; file baru diterbitkan dengan rename
atomik setelah hash cocok. Tidak ada permission penyimpanan umum atau jaringan.

Runtime game dimasukkan dalam APK di `lib/<abi>/libtransformers_game.so`
dan diekstrak oleh installer. Proses `:game` memuat library native;
EXE yang diimpor dipetakan sebagai data untuk kode hasil recompilation.
Ini mengikuti batas executable Android
10. [Android 10 execution behavior](https://developer.android.com/about/versions/10/behavior-changes-10#execute-permission)

Input yang didukung berukuran 3.702.784 byte dengan SHA-256
`5aa3bd490f5b97f614a154fdca98cae845bb62c783c48e56042f19ac14aeefd0`.
EXE dan data impor disimpan di `files/game/`. Child process menggunakan folder
ini sebagai current directory, sehingga file I/O guest dapat membuka jalur
relatif. Versi sebelumnya yang menyimpan `files/input.exe` dimigrasikan saat
probe dipanggil. Data tidak dimasukkan dalam APK; hanya manifest nama/ukuran/hash.

APK menggunakan bridge startup pada [STARTUP.md](STARTUP.md).
Rendering DXVK dan bridge audio gameplay tersedia; FMV Bink belum tersedia.
Saat activity ditutup/dibuat ulang, impor dibatalkan; pilih folder lagi untuk
melanjutkan. Streaming/digest/atomic replacement diuji dengan JVM host.
SAF, proses native, impor data dan rendering DXVK sudah dijalankan pada
M2012K11AG / Android 13 / Adreno 650. ABI selain ARM64 belum dijalankan.

APK diperiksa dengan AAPT2, D8, zipalign dan apksigner. Self-test runtime,
thread/TLS, rendering GPU dan startup sampai pemuatan shader telah diamati
di HP. Campaign Autobot / The Suburbs sudah mencapai dunia 3D; gerak maju,
lompat dan pause telah diuji. Native libraries memakai alignment page 16 KiB, tetapi
kompatibilitas runtime 16 KiB belum diuji.

Laporan build DXVK ada di `work/android-dxvk/apk-build-summary.json`,
`verify.log`, `alignment-check.log` dan `badging.log`.

## Build DXVK dan shader

Backend native dipin pada DXVK native-1.9.2b. Jalankan `tools/build_dxvk.py`
dan `tools/build_game.py` untuk setiap ABI sebelum packaging. Tambahkan
`--graphics-root work/graphics --native-root work/graphics` ke build APK.
Gunakan kembali `--keystore work/android/diagnostic-debug.jks` agar pembaruan
mempertahankan EXE/data yang telah diimpor.

`--shader-cache work/shader-cache` menyertakan hasil kompilasi shader yang
sudah diamati. Activity menyalin cache dengan pemeriksaan ukuran/SHA-256
sebelum menjalankan game. Cache memuat source HLSL dan bytecode turunan game;
DAT/WAV/BIK tetap diimpor dari salinan pengguna. Compiler SDK Windows hanya
dipakai saat pengembangan dan tidak dibutuhkan pada HP untuk cache yang cocok.

Proses `:game` memuat `libtransformers_game.so`, memetakan EXE sebagai data,
mengikat impor Win32, lalu menjalankan kode hasil recompilation. EXE tidak
dijalankan langsung oleh Android. Surface Android diteruskan ke native DXVK.

## Preview Turnip

Tambahkan `--adreno-tools work/adrenotools --turnip-driver work/turnip` saat
packaging untuk menyertakan loader dan driver ARM64 yang dipin. Loader GPU
memilih Turnip bawaan APK pada Adreno 6xx; adapter/ABI lain memakai driver
sistem. Hanya Adreno 650 yang telah diuji. Lihat [GPU_VALIDATION.md](GPU_VALIDATION.md)
untuk hash, sumber, lisensi dan hasil perangkat.

Analog bulat memakai panah pada profil MENU dan WASD pada profil GAME.
GUNS mengirim tombol mouse kanan, ROCKET tombol kiri, MELEE mengirim E,
Lock tombol tengah, dan TRANSFORM memakai satu pulsa wheel +120 per sentuhan
baru untuk toggle mobil/robot bawaan game. Kontrol tambahan ada pada `...`.
Drag layar hanya mengirim gerakan kamera, dengan pelacakan pointer multitouch;
tembakan berat memakai tombol ROCKET. Bridge menghormati mode mouse absolut
yang diminta game. Lihat [CAMERA_INPUT.md](CAMERA_INPUT.md). Back Android mengirim Esc ke game. Meninggalkan activity
mengakhiri sesi native; impor dan cache tetap disimpan. Bridge audio asli game
dibahas di [AUDIO.md](AUDIO.md); FMV Bink belum tersedia.
Belum seluruh level atau variasi pengaturan grafis diuji.

Selama pengembangan saja, marker private `files/game/shader-cache-wait` mengizinkan
runtime menunggu maksimum 60 detik saat permintaan shader baru ditangkap.
`tools/service_device_shaders.py --serial <serial>` mengompilasi input persis
dengan SDK host, memeriksa hash upload dan menerbitkan direktori cache secara
atomik. Hapus marker sebelum uji mandiri; mode normal berhenti pada cache miss.
Perangkat tidak membutuhkan komputer untuk shader yang sudah dibundel.

Buffer presentasi dibatasi ke lebar 1280 pada layar yang lebih besar, dengan
rasio surface dipertahankan. Desktop virtual DXVK menggunakan ukuran yang
sama supaya menu tidak terpotong. Pada HP uji buffer berukuran 1280x526;
game memilih backbuffer 1280x720 dari daftar mode dan melakukan presentasi
ke surface Android. Marker private `gpu-native-resolution` menonaktifkan batas.

Tool build runtime mengarahkan TMP/TEMP ke workspace agar proses compile/link
tidak bergantung pada ruang kosong drive sistem Windows.
