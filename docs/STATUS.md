> Current private backup baseline: 0.5.35 (versionCode 46). Read [README.md](../README.md) and [CUSTOM_UI.md](CUSTOM_UI.md) for the current build and UI state. Entries below are historical.

# Status pengembangan - 3 November 2026

## Perbaikan akses descriptor Android 0.5.24

Error EACCES terjadi karena descriptor SAF dibuka ulang lewat symlink. Mount
Java kini mempertahankan descriptor tanpa membuka ulang path. Adapter native
menduplikasi descriptor dan memakai pread dengan posisi terpisah setiap open;
dup berbagi posisi, stdio memakai callback, stat membaca metadata descriptor.
Data game/EXE tidak disalin; sumber, save, grafis dan pilihan driver dipertahankan.

Uji ARM64 di HP mereproduksi penolakan EACCES pada path dan berhasil membaca
melalui adapter produksi, termasuk seek 64-bit, stdio, write/delete denial dan
file save privat. 200 tes host lulus, satu opsional dilewati. Empat ABI dibangun
ulang dengan archive lifted core yang sama. Bukti: `work/sources0524/`; APK
`work/android-sources-0524/`. Gameplay setelah update belum diuji.

## Pilihan dan impor ZIP driver GPU 0.5.23

Launcher menambahkan pilihan otomatis, driver sistem dan library driver ZIP
pengguna, dengan tombol Add GPU driver (.zip). Nama ZIP asli dipertahankan;
ekstraksi dan metadata diterbitkan atomik. Driver ARM64 diperiksa struktur
ELF/HMI, CRC, batas ZIP dan hash sebelum diteruskan ke native. Pilihan disimpan
untuk sesi game baru. Driver bawaan, shader, data game dan pengaturan lama
dipertahankan. Rincian: `GPU_DRIVERS.md`.

Ke-13 tes importer lulus, termasuk ZIP Turnip asli. Uji headless native di
Adreno 650 berhasil memilih driver sistem, otomatis dan custom; path custom
di luar library ditolak. Hasil uji launcher Android/paket/pemasangan dicatat
di `work/drivers0523/validation.json`. Uji gameplay driver pengguna tetap
dilakukan pengguna. APK: `work/android-drivers-0523/`.

## Baca folder game langsung 0.5.22

Launcher menyimpan izin dan URI folder/EXE, bukan menyalin seluruh game.
Selection memverifikasi ukuran/hash; sesi native membaca file sumber melalui
descriptor yang dipertahankan selama sesi. Save dan konfigurasi tetap di lokasi
privat yang sama. Salinan aset privat lama hanya dibersihkan sesudah sumber
terverifikasi dan hash salinannya cocok; save tidak disentuh.

Uji DocumentsProvider Android dengan fixture sintetis lulus untuk pembacaan
langsung, seek, tanpa salinan data/EXE, sumber hilang, pembatalan, cleanup dan
preservasi save. Self-test adapter native ARM64 lulus. 187 tes host lulus,
satu opsional dilewati. Uji folder asli/gameplay dilakukan pengguna.
Rincian: `DIRECT_SOURCES.md`. Paket: `work/android-direct-0522/`.
Pemeriksaan paket/pemasangan: `work/direct0522/validation.json`.

## ENTER pada layar transisi/loading 0.5.21

Loop loading asli `0x5166B0` kini dikenali melalui bit engine `0x80` pada
`0x6FB1D0`. Saat flag enabled `0x713464` dan ready `0x713468` aktif, profil
MENU menyediakan ENTER. Loading sebelum prompt siap tetap HIDDEN, dan keluar
dari loop mengembalikan profil robot/mobil walaupun flag ready masih tersisa.
Ini memperbaiki frame pengguna yang menampilkan ENTER to continue bersama
kontrol combat dan pointer aktor lama. Rincian bukti: `AUTO_CONTROLS.md`.

187 tes host lulus, satu tes opsional dilewati. Self-test library produksi
ARM64 di HP lulus, termasuk prompt loading dan transisi kembali ke combat.
Keempat ABI memakai core 0.5.19, cache shader dan driver yang sama.
APK: `work/android-controls-0521/transformers-dxvk-universal-debug.apk`.
Hasil pemeriksaan paket/pemasangan: `work/controls0521/validation.json`.
Uji visual game setelah pemasangan dilakukan pengguna.

## Menu kontrol pada layar saving 0.5.20

Status saving asli di byte `0x71437A` kini memilih profil MENU sebelum
pemeriksaan pause/inhibit umum. Kondisi HP pengguna: ChapterCompleteInitial
aktif pada fase 20, saving 1, pause 1/1 dan kedua flag menu biasa nol.
Sebelumnya kondisi ini memilih HIDDEN dan Java menampilkan nol tombol.
Kontrol panah, BACK dan ENTER kini tersedia selama antarmuka saving aktif;
byte yang sudah nol atau tidak terbaca tidak mengaktifkan menu saving.

Regresi mereproduksi kegagalan sebelum perbaikan. Setelah perbaikan, 184 tes
host lulus dan satu tes opsional dilewati. Library produksi ARM64 lulus
self-test profil kontrol di HP, termasuk transisi saving MENU/HIDDEN dan
fase completion 20. Keempat ABI dibangun ulang dengan core hasil recompilation
0.5.19; shader, driver, aset dan Java kontrol dipertahankan.

APK: `work/android-controls-0520/transformers-dxvk-universal-debug.apk`.
Bukti build, paket dan pemasangan: `work/controls0520/validation.json`.
Uji visual layar saving setelah pemasangan dilakukan pengguna.

## BACK menu mengirim Backspace 0.5.17

Tombol BACK besar di atas ENTER sekarang mengirim VK8 (Backspace), bukan VK27.
Bridge DirectInput yang ada memetakan VK8 ke scan 0x0E. Tes menu memeriksa
keydown/keyup Backspace, tidak mengirim Escape, dan tidak melepas ENTER yang
sedang ditahan. Seluruh 19 tes kontrol lulus. Posisi dan label tombol tetap.
APK universal 0.5.17 versionCode 28 sudah dipasang; hash cocok dan pengaturan
grafis/game dipertahankan. Native, driver dan 343 shader sama dengan 0.5.16.
Artefak: `work/android-controls-0517/transformers-dxvk-universal-debug.apk`.
Bukti pemasangan: `work/controls0517/validation.json`.

## Shader Decepticon Chapter 1 0.5.16

Crash loading berasal dari permintaan D3DXCompileShader yang belum ada dalam
cache. Permintaan shader asli dikompilasi dengan compiler D3DX9_31; pengumpulan
pada uji pengguna menambahkan 14 shader sehingga cache bertambah 329 menjadi
343. Pengguna mengonfirmasi Decepticon Chapter 1 bisa dimainkan; log PID 17412
mencatat rendering gameplay dan SFX aktif tanpa fatal pada tail yang disimpan.
Semua bytecode baru lulus parser shader produksi dan hash sesuai input/output.
Lima tes cache/compiler serta lima tes batas bytecode juga lulus.

APK 0.5.16 membundel snapshot 343 shader. Native, aset game, driver, kontrol
Inggris dan grafis tetap sama. Status pemasangan dan penghapusan marker uji
dicatat di `work/decepticon0516/validation.json`.
APK universal 0.5.16 versionCode 27 sudah dipasang; hash cocok dan konfigurasi
grafis/game dipertahankan. Marker shader-cache-wait sudah dihapus dan layanan
pengumpulan dihentikan setelah 14 shader terpasang. Log penuh uji Decepticon
tidak memuat fatal; pengguna telah mengonfirmasi Chapter 1 bisa dimainkan.
Rincian: [DECEPTICON_CRASH.md](DECEPTICON_CRASH.md).

## Label kontrol bahasa Inggris 0.5.15

Seluruh label kontrol menu/robot/mobil, tombol tambahan, petunjuk overlay dan
deskripsi aksesibilitas sekarang memakai bahasa Inggris. Posisi, ukuran,
binding, renderer dan lifecycle tetap sama. 19 tes kontrol yang ada lulus.
APK universal 0.5.15 versionCode 26 sudah dipasang; hash cocok dan pengaturan
dipertahankan. Native library, aset, shader dan driver identik dengan 0.5.14.
Artefak: `work/android-controls-0515/transformers-dxvk-universal-debug.apk`.
Bukti build/pemasangan: `work/controls0515/validation.json`.

## Fullscreen dan resume background 0.5.14

GameActivity sekarang menyembunyikan status/navigation bar dengan immersive
mode. Membuka Recent Apps tidak lagi memanggil finish atau mematikan proses.
Sesi native dipertahankan melalui foreground service dengan notifikasi kembali
ke game; input, rendering, waktu guest dan audio dijeda selama background.
Surface DXVK dilepas setelah command/present selesai, kemudian dipasang ulang
tanpa menghapus device/resource game atau memanggil entry game kembali.

33 tes host lulus (clock/kontrol/kamera/package), serta empat probe native ARM64
(lifecycle, kontrol, BMP dan runtime). Uji lifecycle mencakup enam siklus
detach/resume dan serialisasi operasi aktif. Uji gameplay Recent Apps/fullscreen
tetap dilakukan pengguna; status pemasangan dan hasil uji ada di
`work/lifecycle0514/validation.json`.
APK universal 0.5.14 versionCode 25 sudah dipasang. Hash APK/library dan source
terverifikasi; aset, shader, driver serta pengaturan grafis/game dipertahankan.
Uji Recent Apps dan fullscreen pengguna masih menunggu.
Rincian: [ANDROID_LIFECYCLE.md](ANDROID_LIFECYCLE.md).

## Posisi kontrol sesuai referensi pengguna 0.5.13

Menu memakai empat pad arah di kiri, Kembali/ENTER di kanan dan Keluar di atas.
Robot memakai analog kiri bawah, Transform/Kunci kiri tengah dan susunan
serangan kanan sesuai gambar pengguna. Block dipertahankan di samping Melee.
Mobil memakai Boost lebar di atas pad Kiri/Kanan, senjata kanan atas serta
pedal Rem/Gas kanan bawah. Klakson, rem tangan dan tombol tambahan tetap ada.
Desain transparan berikon dipertahankan dengan ukuran tombol disesuaikan.

19 tes kontrol dan empat tes kamera lulus, termasuk batas layar, tombol tidak
bertumpuk pada empat ukuran landscape, susunan referensi dan pad menu dengan
Enter. Runtime/native, aset, shader dan driver sama dengan 0.5.12.
APK universal 0.5.13 versionCode 24 sudah dipasang; hash cocok dan konfigurasi
grafis/game dipertahankan. Uji gameplay dilakukan pengguna.
Artefak: `work/android-controls-0513/transformers-dxvk-universal-debug.apk`.
Bukti pemasangan: `work/controls0513/validation.json`.

## Tombol konfirmasi selesai misi 0.5.12

Layar selesai misi yang sedang terbuka terbaca sebagai ChapterCompleteInitial,
vtable 0x687388, aktif 1, fase 22. Flag pause umum 1/1 sementara flag menu biasa
nol, sehingga overlay 0.5.11 keliru menjadi HIDDEN. Deteksi kini mengenali fase
Continue tersebut sebelum menyembunyikan input yang diblokir. Layar submisi
selesai (fase 12) dan misi gagal (fase 7 dengan prompt aktif, layout field
berbeda) juga memakai gerbang konfirmasi asli. Fase loading/fade dan cleanup
tidak dianggap menu.

12 tes decoder, 18 tes kontrol dan empat tes kamera lulus. Library ARM64 baru
lulus uji native profil dan runtime pada HP tanpa menjalankan atau mengontrol
game. APK universal 0.5.12 versionCode 23 sudah dipasang; hash cocok dan
preferences/config tetap sama. Aset, shader dan driver identik dengan 0.5.11.
Bukti: `work/controls0512/validation.json`. Uji tampilan konfirmasi selesai
misi setelah pemasangan masih menunggu pengguna.
Rincian: [AUTO_CONTROLS.md](AUTO_CONTROLS.md).

## Tombol ENTER pada Start Mission 0.5.11

Halaman Start Mission menggunakan controller ChapterTextScreen tersendiri:
root UI +0x4C, vtable 0x687358, byte aktif +0x2C dan fase 9 di +0x28.
Saat halaman itu menunggu Enter, flag pause menu dan mission_panel tetap nol,
sehingga deteksi lama keliru menyembunyikan tombol. Status tersebut kini
memilih layout menu dengan tombol berlabel ENTER (VK13). Loading/fade fase
1–8 dan cleanup fase 10 tetap tidak diperlakukan sebagai menu konfirmasi.

Delapan tes decoder, 17 tes kontrol dan empat tes kamera lulus, serta uji
native profil dan runtime pada HP lulus. APK universal 0.5.11 versionCode 22
sudah dipasang; hash cocok dan pengaturan dipertahankan. Aset, shader dan
driver tidak berubah. Pada uji pengguna, log native mencatat MENU dengan
start=1 fase9, Java menerapkan delapan kontrol menu, lalu kembali ke ROBOT
fase10 dengan sebelas kontrol dan gameplay berlanjut. Tidak ada kontrol layar
atau screenshot perangkat oleh agen.
Rincian: [AUTO_CONTROLS.md](AUTO_CONTROLS.md).
Artefak: `work/android-controls-0511/transformers-dxvk-universal-debug.apk`.
Bukti: `work/controls0511/validation.json`.

## Layout otomatis tanpa celah transformasi 0.5.10

Status menu utama, robot, mobil dan pause telah dicocokkan dengan pembacaan
pasif pada HP. Versi 0.5.9 menghubungkan status tersebut ke UI otomatis dan
menghapus pemilih mode manual; pengguna mengonfirmasi pergantian berfungsi.
Log native dan Java menunjukkan profil menu/robot/mobil sesuai, tetapi kontrol
sempat disembunyikan 1–1,5 detik selama animasi transformasi.

0.5.10 mengikuti bit bentuk karakter selama animasi tanpa layout kosong.
Sampling berjalan setiap Present berhasil dan polling UI dipercepat dari 75
ke 16 ms. Profil yang sama mempertahankan tombol yang ditahan. Menu/pause tetap
berdasarkan status GUI asli; loading/blocked input masih menyembunyikan kontrol.

Enam tes decoder, 16 replay kontrol dan empat tes kamera lulus. Uji native
profil serta runtime pada ARM64 lulus; empat ABI dibangun. APK 0.5.10
versionCode 21 sudah dipasang dan hash cocok. Aset, shader, driver dan pengaturan
tetap sama. Respons sentuhan terbaru diuji pengguna sendiri.
Rincian: [AUTO_CONTROLS.md](AUTO_CONTROLS.md).
Artefak: `work/android-controls-0510/transformers-dxvk-universal-debug.apk`.
Bukti: `work/controls0510/validation.json`, `work/controls059/runtime-live.log`,
`work/controls059/ui-live.log`.

## Pad dan pedal mobil 0.5.8

Profil MOBIL kini memakai tombol Kiri/Kanan di kiri dan pedal Gas/Rem di kanan,
tanpa analog atau timer steering. Tombol menahan A/D/W/S secara terpisah.
Boost, rem tangan, senjata, klakson dan Transform tetap tersedia; analog dan
Block dipertahankan di profil robot. Area HUD dan kamera tetap dilapangkan.

15 tes kontrol dan empat tes kamera lulus, termasuk input bersamaan,
pembatalan sentuhan, pedal keluar/masuk, shared keys, pergantian profil serta
batas persegi/pedal terhadap HUD. APK universal 0.5.8 versionCode 19 sudah
dipasang; versi dan hash cocok, preferences/pcconfig tidak berubah. Semua
native library, aset, shader dan driver identik dengan 0.5.7. Pengujian di
game dilakukan pengguna, tanpa kontrol layar atau screenshot HP oleh agen.
Artefak: `work/android-controls-058/transformers-dxvk-universal-debug.apk`.
Bukti: `work/controls058/validation.json`.

## Layout dan kontrol mobil 0.5.7

Tombol MOBIL/ROBOT memilih profil analog; profil mobil memperlebar area lurus
dan memberi belokan bertahap pada sebagian defleksi, sambil mempertahankan
gas/rem. Block memakai Left Shift sesuai manual, dengan label Boost dalam
profil mobil. Tombol serangan dipindah ke dua kolom di tepi kanan; Kunci
berada di kiri agar area kamera lebih lega. Transform tetap mengikuti game.

15 tes replay kontrol dan empat tes kamera lulus. APK 0.5.7 versionCode 18
sudah dipasang dan hash diverifikasi. Native library, aset, shader dan driver
identik dengan 0.5.6; pengaturan tetap sama. Ergonomi di HP diuji pengguna
sendiri, tanpa kontrol layar atau screenshot perangkat oleh agen.
Rincian: [TOUCH_CONTROLS.md](TOUCH_CONTROLS.md).
Artefak: `work/android-controls-057/transformers-dxvk-universal-debug.apk`.
Bukti: `work/controls057/validation.json`.

## Crash akhir misi pertama 0.5.6

Log 0.5.5 menunjukkan SIGABRT karena bridge belum menyediakan
D3DXSaveSurfaceToFileInMemory. Jalur asli meminta BMP dari surface A8R8G8B8
system-memory saat transisi. Encoder BMP kini mengembalikan ID3DXBuffer
berisi gambar valid; loader tekstur juga menerima BMP tersebut.

Enam tes BMP, sebelas tes bridge, lima tes shader dan enam tes probe lulus
(satu tes opsional dilewati). Pada HP, uji COM encoder memakai library produksi,
uji runtime dan uji audio 384 sampel/64 suara lulus tanpa membuka game atau
mengendalikan layar. Keempat ABI dibangun. APK 0.5.6 versionCode 17 sudah
dipasang, hash cocok dan preferences/pcconfig tetap sama. Penyelesaian misi
asli tetap menunggu tes pengguna. Rincian: [MISSION_CRASH.md](MISSION_CRASH.md).
Artefak: `work/android-crash-056/transformers-dxvk-universal-debug.apk`.
Bukti: `work/crash056/validation.json`.

## Perbaikan SFX 0.5.5

Pengguna mendengar BGM/narasi pada 0.5.4 tetapi tidak mendengar SFX. Backend
membatasi seluruh buffer, termasuk sampel yang baru dimuat, menjadi 256.
Kode asli memerlukan duplicate buffer untuk memutar efek; duplicate gagal
sebelum Play bila batas tersebut penuh. Batas objek buatan itu dihapus;
mixer kini hanya memproses daftar suara aktif dan tetap berbagi PCM antar
duplicate. Log baru memisahkan jumlah buffer, SFX aktif, Play, duplicate,
kegagalan alokasi dan sampel/peak SFX. Grafis dan konfigurasi tidak diubah.

23 tes audio host dan enam tes probe lulus (satu opsional dilewati).
Self-test perangkat baru mencakup 384 sampel dan 64 suara SFX bersamaan.
APK universal empat ABI sudah dibangun, tanda tangan/alignment/CRC dan
kesesuaian library diperiksa. Aset, cache shader dan driver sama dengan 0.5.4.
APK 0.5.5 sudah dipasang ulang melalui ADB dengan data aplikasi dipertahankan;
versi dan hash APK terpasang cocok. Self-test perangkat dan audibilitas SFX
gameplay belum diverifikasi. Rincian: [AUDIO.md](AUDIO.md).
Artefak: `work/android-audio-055/transformers-dxvk-universal-debug.apk`.
Bukti build/tes: `work/audio055/validation.json`.

## Audio asli gameplay 0.5.4

Bridge DirectSound8 sekarang memakai mixer PCM dan keluaran Android AAudio.
Musik/dialog streaming mendapatkan notifikasi posisi buffer agar loader asli
bisa terus mengisi ring; SFX mendukung storage bersama pada duplicate buffer,
volume, pan, pitch dan audio 3D software. Audio focus dan tombol volume Android
memakai stream media. Tidak ada penggantian soundtrack atau impor ulang aset.

Sebanyak 23 tes audio host lulus. Uji native pada HP menjalankan stream AAudio
nyata yang dibisukan: callback berjalan, PCM menghasilkan sampel nonzero sebelum
mute, notifikasi quarter-ring berfungsi dan tidak ada xrun pada uji singkat.
Keempat ABI dibangun; uji suara/gameplay tetap dilakukan pengguna sendiri.
Audio/video FMV Bink belum didukung. Rincian: [AUDIO.md](AUDIO.md).
Artefak: `work/android-audio-054/transformers-dxvk-universal-debug.apk`.
APK universal 0.5.4 (versionCode 15) sudah dipasang melalui ADB; hash APK
terpasang cocok dan konfigurasi/preferences tetap sama. Game tidak dibuka
oleh agen. Bukti: `work/device/audio054-validation.json`.

## Optimasi CPU tanpa penurunan grafis 0.5.3

Runtime menggunakan cache pencarian fungsi dan menolak VA import di luar tabel
tanpa binary search. Setter konstanta shader memakai ulang buffer konversi;
pencarian nama konstanta tidak lagi mengalokasikan string. Spam peringatan
DynoScene dibatasi dengan sampel dan ringkasan jumlah. State grafis, driver,
shader, draw dan simulasi tidak diturunkan. Log `[perf]` baru mencatat FPS,
waktu CPU thread game, waktu Present, serta jumlah panggilan/draw tiap dua detik.

Microbenchmark lookup pada HP menunjukkan 7,99x untuk alamat fungsi berulang
dan 18,20x untuk penolakan alamat import sebelum lookup shim. Angka tersebut
bukan kenaikan FPS gameplay. Tes native headless lulus; 25 tes host lulus
dan satu tes opsional dilewati. Keempat ABI selesai dibangun. Pengujian gameplay
versi ini tetap dilakukan pengguna sendiri. Rincian: [PERFORMANCE.md](PERFORMANCE.md).
APK universal 0.5.3 (versionCode 14) sudah terpasang; hash instalasi cocok
dengan artefak, termasuk library ARM64 yang diuji. Hash preferences grafis
dan pcconfig sebelum/sesudah instalasi identik. Game tidak dibuka oleh agen.

## Cache shader grafis rendah 0.5.2

Perbaikan dua shader awal belum cukup: permintaan pixel berikutnya memakai
source berbeda dari vertex. Pengumpulan request asli kemudian dijalankan
selama pengguna membuka game dan masuk level sendiri. Sebanyak 47 request
tambahan dikompilasi dengan SDK D3DX9_31 dan dipasang secara atomik dengan
verifikasi hash. Pengguna mengonfirmasi bisa bermain dan menutup game.
Log sesi tidak mengandung FATAL, mencatat aktivitas DynoScene, dan proses
game hanya membuka Transformers_Lo.DAT (tanpa arsip Hi).

APK 0.5.2 (versionCode 13) menyertakan 329 request shader lengkap, termasuk
hasil pengumpulan ini. Kelima tes cache shader lulus; file cache dalam APK
diverifikasi ukuran/hash dan keempat native game binary identik dengan 0.5.1.
Pengumpulan sementara dihentikan dan marker shader-cache-wait dihapus sebelum
instalasi. Tidak ada kontrol layar, screenshot atau pembukaan game oleh agen.
Ini memverifikasi sesi/level yang diuji pengguna, belum semua variasi atau level.
Artefak: `work/android-shaders-052/transformers-dxvk-universal-debug.apk`.
Bukti sesi/paket/instalasi: `work/device/shaders052-validation.json`.

## Perbaikan pembacaan pengaturan 0.5.1

Bug 0.5.0 terisolasi pada pembacaan file: loader mengubah path Windows menjadi
huruf besar, sedangkan Android melarang enumerasi direktori induk `/data` untuk
memperbaiki perbedaan case. File konfigurasi gagal dibuka; game memakai dan
menulis nilai bawaan ON. Folder documents kini dipetakan ke drive virtual D:
agar pencarian case dimulai dari direktori privat yang bisa diakses. Lokasi
fisik config dan save tetap sama, tanpa override paksa flag game.

Probe native tanpa UI mereproduksi kegagalan lama, kemudian menguji constructor
dan loader asli dengan library produksi baru. Resolusi 1280x720 dan lima flag
terbaca sesuai nilai OFF, ON, campuran, serta transisi pending OFF. Lima tes
service lulus, termasuk pemeriksaan kompilasi ketat untuk empat ABI.
Rincian dan batas bukti: [GAME_SETTINGS.md](GAME_SETTINGS.md).

APK universal 0.5.1 (versionCode 12) sudah dipasang melalui pembaruan ADB;
hash APK terpasang identik dengan artefak. Game tidak dibuka oleh agen,
tanpa screenshot atau input layar. Pengguna melakukan uji gameplay sendiri.
Folder uji sementara pada HP telah dihapus. Artefak:
`work/android-settings-051/transformers-dxvk-universal-debug.apk`.
Bukti: `work/device/settings051-validation.json`.

Uji pengguna berikutnya mengalami SIGABRT saat startup: runtime meminta shader
HLSL yang belum ada pada cache, lalu berhenti pada batas
`D3DXCompileShader requires a compiled shader cache (request captured)`.
Permintaan vertex persis dan pasangan pixel yang dideklarasikan dalam source
dikompilasi dengan SDK D3DX9_31, lalu dipasang secara atomik ke cache privat;
hash ketiga file masing-masing cache diverifikasi. Agen tidak membuka game
atau mengirim input. Startup sesudah penambahan cache belum diuji pengguna.
Bukti: `work/device/settings051-shader-repair.json`.

## Launcher dan kontrol 0.5.0

Launcher kini menampilkan kesiapan EXE/data, tombol Mulai Game, resolusi
internal 640x360 / 858x482 / 1280x720, serta sakelar High Res Texture,
Bloom, Bayangan dan Motion Blur. Menu diagnostik/uji coba dihapus.
Pengaturan diterapkan ke pcconfig asli game sebelum mulai; kedua flag tekstur
aktif/pending diperbarui bersama. Basis 1280x720 dipulihkan setiap mulai,
lalu bridge menerapkan skala pilihan agar resolusi tidak mengecil berulang.
Rincian bukti statis dan batas pengujian: [GAME_SETTINGS.md](GAME_SETTINGS.md).

Kontrol memisahkan GUNS (mouse kanan) dan ROCKET (mouse kiri); MELEE berikon
tinju mengirim E. Satu tombol TRANSFORM mengirim wheel-up pada setiap sentuhan
baru, mengikuti toggle mobil/robot milik game. Analog dan drag kamera tetap
memakai pelacakan multitouch. Rincian: [TOUCH_CONTROLS.md](TOUCH_CONTROLS.md).

Sebanyak 35 tes terkait lulus: 10 pengaturan, 11 kontrol, 4 kamera dan 10 impor.
APK universal empat ABI versi 0.5.0 (versionCode 11) dibangun, diverifikasi dan
dipasang sebagai pembaruan melalui ADB. Hash APK terpasang sama dengan artefak;
binary native identik dengan 0.4.6. Pengujian visual/gameplay revisi ini
dilakukan pengguna sendiri; agen tidak mengambil screenshot, mengirim input,
atau membuka game. Artefak:
`work/android-launcher-050/transformers-dxvk-universal-debug.apk`.
Bukti instalasi dan pemeriksaan: `work/device/launcher050-validation.json`.

Pemeriksaan sesi pengguna 11:26 menemukan masalah integrasi: log launcher
meminta highRes/bloom/shadows/motionBlur=false, tetapi pcconfig setelah startup
menyimpan kelima flag (termasuk texture pending) sebagai 1. Proses game juga
masih membuka Transformers_Hi.DAT. Resolusi terbukti diterapkan: CreateDevice
dan viewport 858x482, skala 67%. Tes host sebelumnya belum membuktikan bahwa
opsi boolean dipakai runtime; penyebab pengembalian nilai belum ditentukan.
Bukti: `work/device/launcher050-settings-observation.json` dan log terkait.

## Kontrol sentuh 0.4.6

Toolbar keyboard diganti overlay transparan dengan analog bulat kiri, area drag
kamera, tombol tempur bulat kanan, dan Mobil/Robot di atas analog. Profil MENU
memakai panah/Enter; profil GAME memakai WASD. Profil diganti manual lewat tombol
GAME/MENU. Tombol `...` menyediakan keyboard tambahan serta Keluar. Letak
kontrol menyisakan ruang untuk health bar, panel senjata dan radar game.

Sepuluh tes replay multi-touch/layout dan empat tes kamera lulus. APK universal
empat ABI sudah dibangun, ditandatangani dan dipasang sebagai versi 0.4.6.
Binary native keempat ABI identik dengan 0.4.4. Log saat uji kandidat kontrol
0.4.5 tidak menunjukkan fatal. Pengguna memilih menguji kontrol sendiri;
agen hanya membaca log. Kenyamanan layout final belum dikonfirmasi pengguna.
Rincian referensi, pemetaan dan batas analog: [TOUCH_CONTROLS.md](TOUCH_CONTROLS.md).

## Perbaikan tampilan 0.4.4 dan optimasi 0.4.3

Runtime Android dan bridge D3D9 sebelumnya dikompilasi `-O0`, sedangkan DXVK
dan lifted core ARM64 memakai `-O2`. Runtime/bridge kini dibangun dengan `-O2`
untuk keempat ABI; `-fno-strict-aliasing`, `-fwrapv`, dan `-ffp-contract=off`
tetap aktif. APK 0.4.2 memberi 12,9 FPS pada adegan awal dan sampel berubah
hingga 19,45 FPS saat pengguna bermain; sudut kamera/adegan ikut berubah, jadi
belum ada perbandingan setara.

Build 0.4.3 menambahkan skala backbuffer 50% (640x360), 67% (854x480),
atau 100% (1280x720), dipilih dari launcher. Pada skala 50%, bridge sudah
mengembalikan dimensi 640x360 ke game, tetapi SetViewport memperkecilnya lagi
menjadi 320x180. Akibatnya gambar hanya mengisi pojok kiri atas. Mengabaikan
resize SurfaceView saja tidak menyelesaikan bug tersebut.

Build 0.4.4 meneruskan viewport tanpa penskalaan tambahan dan menggunakan
ukuran swapchain fisik sebagai tujuan presentasi Android. APK ARM64 sudah
terpasang. Screenshot intro `work/device/renderfix044-check.png` menunjukkan
gambar memenuhi area SurfaceView. Log `work/device/renderfix044-runtime.log`
memastikan backbuffer 640x360, viewport 640x360 dengan status sukses, dan
output swapchain 1280x526; tidak ada fatal pada log yang diambil. Sebelas tes
bridge lulus. APK universal empat ABI sudah dibangun, diverifikasi tanda tangan
dan CRC-nya; binary ARM64 identik dengan APK yang diuji di HP. Artefak:
`work/android-renderfix-044/transformers-dxvk-universal-debug.apk`, SHA-256
`cc0476e0dcdc90823c5017a84d20c3352f4dfd38589b7d39d578be7fa1973112`.
Bukti terstruktur: `work/device/renderfix044-validation.json`.

Dua jendela pembacaan SurfaceFlinger setelah perbaikan menunjukkan 57,55 dan
57,54 FPS dengan frame terus diperbarui. Adegan saat sampling tidak dicatat;
angka ini tidak membuktikan gameplay atau FPS pada adegan berat.

Driver Turnip memakai rilis Mesa Android 26.2.5 resmi; hash arsip rilis dan
binary dicatat di `work/turnip-26.2.5/provenance.json`. Target 30 FPS belum
terverifikasi.

Referensi NFSU2 juga menerjemahkan game menjadi C ARM64 dan memakai DXVK
Native/Vulkan. Repo itu melaporkan 35–51+ FPS pada Snapdragon 8 Gen 3 di
1170x540; angka tersebut berasal dari perangkat dan game berbeda, jadi bukan
target langsung untuk Transformers atau Adreno 650.

## Perbaikan kamera 0.4.1

Bridge kini menerima `DIPROP_AXISMODE_ABS` yang diminta game, mempertahankan
koordinat mouse absolut antar-poll, dan tetap mendukung mode relatif. Drag
layar dipisahkan dari Heavy serta memakai pointer ID dan sisa gerakan pecahan.
Sebelas tes input/gesture lolos dan keempat ABI sudah dibangun ulang.
Rincian penyebab dan bukti source: [CAMERA_INPUT.md](CAMERA_INPUT.md).

APK universal 0.4.1 sudah dipasang dan hash instalasi diverifikasi:
`e1d684726fefe1cd8ebcc7cbf466548b28c18a9d9b172e369fe6f1da18e58328`
(159.878.015 byte). Pengguna telah menguji dan mengonfirmasi kamera normal.
Log mencatat mode absolut aktif dan posisi X/Y berubah; tidak ada fatal pada
log yang diambil (`work/device/camera041-runtime.log`, 334 cache shader hit).
Sesuai permintaan pengguna, pengujian ini tidak memakai input layar otomatis
atau screenshot; agen hanya membaca log runtime.
Laporan: `work/device/camera041-validation.json`.

## Hasil gameplay preview 0.4.0

**Level pertama sudah mencapai gameplay 3D pada HP. Ini masih preview.**

M2012K11AG / Android 13 / Adreno 650 menjalankan campaign Autobot, The Suburbs.
Bumblebee, lingkungan, NPC, kendaraan dan HUD tampil. Gerak maju, lompat serta
menu pause telah diuji melalui kontrol sentuh. Bukti sebelum/sesudah:
`work/device/turnip-enter-level.png`, `turnip-move-w.png`, `turnip-space.png`
dan `turnip-pause.png`. Ini belum membuktikan seluruh mission/campaign selesai.

Revisi universal terbaru juga menguji tombol Car, mengemudi dengan W, dan
Robot kembali ke bentuk robot. Bukti: `work/device/preview04-actions-car.png`,
`preview04-actions-drive.png`, `preview04-actions-robot.png`. Fire dan Heavy
berhasil menembak serta menghancurkan lingkungan (`preview04-actions-fire.png`,
`preview04-actions-heavy.png`); tidak ada fatal atau cache miss pada jalur ini.
Tombol Lock mengirim tombol mouse tengah; penguncian pada musuh belum diuji.

Driver Adreno bawaan mengalami `VK_ERROR_DEVICE_LOST` pada draw fullscreen.
Turnip khusus aplikasi melewati titik tersebut dan masuk level. APK dapat
membundel Turnip untuk ARM64 Adreno 6xx; GPU/ABI lain menggunakan loader sistem.
Pemilihan otomatis sudah diamati pada Adreno 650 tanpa marker driver private.
Driver sistem Android tidak dipasang ulang. Rincian: [GPU_VALIDATION.md](GPU_VALIDATION.md).

## Build dan data

Minimum Android 10 / API 29. Target APK universal: arm64-v8a, armeabi-v7a,
x86, x86_64. Hanya ARM64 yang sudah diuji pada perangkat. Keempat runtime
harus lolos link tanpa undefined symbols dan memakai source closure identik.
ARM64 menggunakan lifted core O2; runtime/bridge Android kini O2 untuk semua ABI.

Input EXE yang didukung: 3.702.784 byte, SHA-256
`5aa3bd490f5b97f614a154fdca98cae845bb62c783c48e56042f19ac14aeefd0`.
14.470 dispatch, 50 source C, nol diagnostik translasi yang belum ditangani.
Ini bukan bukti kesetaraan semantik penuh.

482 file data / 4.346.085.350 byte yang diimpor pengguna tetap dipertahankan.
APK berisi hasil recompilation dan cache shader; DAT/WAV/BIK tetap diimpor.
280 permintaan HLSL sudah dikompilasi dengan SDK D3DX9_31 menggunakan source,
macro, entry, profile dan flags yang persis sama. Cache tambahan tetap mungkin
diperlukan untuk level, efek atau pengaturan lain.

## Pengujian

- **91 tes lolos**, termasuk Android link opsional, 248 pembanding x87 hardware,
  DirectInput, DDS/BC, CTAB, cache shader, importer Java dan provenance loader.
  Laporan: `work/tests-dxvk-current.log`.
- Fixture optimizer berjalan pada HP: pemilihan sampler 2D, cube dan 3D
  masing-masing menyisakan satu descriptor dengan dimensi yang tepat.
- Turnip: triangle, exact pixel readback dan 180 Present lolos; jalur COM guest32
  CreateDevice/Clear/Present juga lolos (`work/device/turnip-smoke.log`).
- Uji runtime/thread/TLS/USER32/COM/random/input lolos pada HP
  (`work/device/threads-selftest.log`).
- Pada pengujian awal resolusi 2320x954, jendela pengukuran SurfaceFlinger
  mencatat sekitar 10,8 present/detik. Buffer presentasi 1280x526 pada revisi
  berikutnya mencatat sekitar 12,9 present/detik (127 sampel); menu tidak lagi
  terpotong. Pengukuran ini mencakup intro/dunia awal, bukan jaminan FPS.
  Bukti: `work/device/preview04-final-present-timing.json`,
  `preview04-final-world.png`, `preview04-final-move.png`,
  `preview04-final-jump-air.png`, `preview04-final-pause.png`.
- Pada artefak final setelah tembakan/kerusakan lingkungan, 127 sampel terbaru
  mencatat sekitar 7,4 present/detik. Jadi performa saat ini masih rendah dan
  berubah menurut adegan; penurunan resolusi belum membuat permainan lancar.
  Bukti: `work/device/preview04-actions-present-timing.json`.

Build/packaging mencatat `executed=false` dan `playable=false` karena perintah
build sendiri tidak menjalankan artefak. Bukti perangkat disimpan terpisah.

## Batas aktif

Audio DirectSound dan decoder Bink belum diimplementasikan; factory melaporkan
kegagalan dan game melewati jalur yang tidak tersedia. Tidak ada audio/frame
video palsu. Winsock belum menyediakan transport. Shader di luar cache tetap
menghentikan runtime secara eksplisit pada mode normal. CTAB struct/array-element,
effect compiler, beberapa API Windows dan exception Windows masih terbatas.
Enam belas INT 6 tetap fatal. Menutup atau meninggalkan activity mengakhiri
proses game; sesi tidak dapat dilanjutkan dari background.

Pengaturan validasi GPU Android sudah dikembalikan ke baseline. Bukti:
`work/device/gpu-settings-restored.json`. Paket 0.2/0.3 lama tidak mewakili preview
Turnip terbaru. APK universal terbaru ditulis ke
`work/android-dxvk/transformers-dxvk-universal-debug.apk`.

Artefak final 0.4.0: 159.918.975 byte, SHA-256
`d5dc6db9e2197e467c8e38f2a9ab78fc1a712c67dbf593abb3ff03ebcb4be8e4`.
Hash base.apk yang terpasang di HP cocok. Tanda tangan dan zip alignment
lolos; self-test native artefak final keluar dengan kode 0.
Uji renderer final pada 1280x526 lolos exact pixel readback, 180 Present dan
COM guest32 CreateDevice/Clear/Present (`preview04-actions-smoke.log`).
Laporan perangkat terikat hash: `work/device/preview04-device-validation.json`.
