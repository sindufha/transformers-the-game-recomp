> Private backup note: this is the original local-workspace handoff. Local paths/history remain historical; use README.md for clone/build instructions in this repository.

# Handoff proyek Transformers: The Game — Android static recompilation

Dokumen untuk AI/pengembang berikutnya. Dibuat pada **3 November 2026**, zona waktu Asia/Jakarta.

Tujuannya adalah melanjutkan proyek yang sudah berjalan, tanpa mengulang pekerjaan besar, menghilangkan data pengguna, mengembalikan bug lama, atau melebih-lebihkan hasil pengujian.

## 1. Baca bagian ini terlebih dahulu

- Versi terakhir yang **berhasil dibangun dan dipasang** adalah **0.5.35**, `versionCode=46`.
- Package Android: `io.github.transformers.recompiled.diagnostics`.
- APK terakhir: `transformers-recompiled/work/android-ui-0535/transformers-dxvk-universal-debug.apk`.
- Bukti pemasangan: `transformers-recompiled/work/ui0535/installation.json`.
- APK telah diverifikasi tanda tangan, alignment, dan hash APK yang terpasang.
- Proyek ini sudah melewati tahap analisis EXE, lifting, native linking, launcher, dan gameplay awal. **Jangan memulai port dari nol.**
- Gameplay awal Autobot dan Decepticon pernah memiliki bukti jalan pada HP. Itu bukan bukti seluruh campaign selesai atau setiap versi baru telah diuji gameplay.
- Perubahan terakhir terutama Java/UI. Native runtime terakhir dibangun untuk perbaikan descriptor sumber pada 0.5.24 dan dipakai ulang pada update UI berikutnya.
- Pengguna menyukai launcher sekarang. Jangan mendesain ulang launcher tanpa permintaan baru.
- Semua teks launcher harus **bahasa Inggris**. Footer harus **`Port By Sinn`**.
- Dynamic switching kontrol Menu/Robot/Car dinyatakan pengguna sudah bagus dan bekerja. Jangan menggantinya dengan tebakan berdasarkan tombol Transform.
- Pengguna menguji gameplay sendiri. Jangan otomatis membuka game, menjalankan debug/probe di HP, atau memasang aplikasi/APK fixture pengujian.
- Tugas terbaru dalam percakapan adalah menulis handoff ini. **Tidak ada permintaan baru untuk mengubah game atau memasang APK lagi pada tugas dokumentasi ini.**

### Status terakhir yang perlu dipahami dengan tepat

Pengguna melaporkan area scroll sidebar masih terlihat terpotong/berada di tengah setelah 0.5.34. Pada 0.5.35, padding dipindahkan dari ScrollView ke anak menu, viewport dibuat penuh, fading edge/overscroll dimatikan, dan scrollbar diberi track tipis. APK 0.5.35 sudah dipasang.

**Belum ada konfirmasi pengguna setelah pemasangan 0.5.35 bahwa tampilan scroll sudah sesuai.** Jangan menulis bahwa masalah visual ini telah terbukti selesai di perangkat. Yang terbukti: perubahan kode, build berhasil, instalasi berhasil, dan data/layout tersimpan dipertahankan.

## 2. Preferensi dan batas kerja pengguna

Instruksi gaya pengguna: jawaban singkat, langsung, mudah dipahami, tanpa basa-basi. Panjang dokumen ini memang diminta secara eksplisit; jawaban chat setelah menyerahkan dokumen tetap cukup singkat.

Preferensi yang bertahan dari percakapan:

1. Jika diminta mengimplementasikan dan memasang, kerjakan sampai APK konkret terpasang jika perangkat tersedia.
2. Jangan berhenti hanya pada rencana atau menanyakan ulang persetujuan yang sudah diberikan.
3. Jangan memasang APK lain untuk mencoba-coba fitur. Pengguna pernah secara eksplisit menolak cara pengujian itu.
4. Jangan menjalankan game/debug otomatis seperti yang pernah terjadi sebelumnya. Pengguna akan melakukan tes langsung.
5. Build, pemeriksaan paket, pemeriksaan sumber, dan tes host yang relevan berbeda dari menjalankan pengujian di HP. Tes host sudah dipakai untuk regresi kontrol dan layout; tetap pilih pemeriksaan yang sesuai perubahan.
6. Jangan mengubah binding, kamera, multi-touch, atau mekanik gameplay ketika tugasnya hanya desain.
7. Jangan menyalin seluruh data game ke penyimpanan aplikasi. Folder/EXE dipilih lalu dibaca langsung.
8. Jangan merusak save, `pcconfig.txt`, pilihan driver, URI sumber, atau custom layout saat update.
9. Seluruh antarmuka launcher harus berbahasa Inggris. Teks baru sidebar/editor juga memakai bahasa Inggris.
10. Logo identitas game adalah **Autobot HD**, bukan ikon gamepad generik.
11. Pengguna meminta toolbar editor dan panel slider kecil agar tidak menutupi game.
12. Jika perangkat terputus, sampaikan bahwa instalasi belum terjadi. Jangan menyebut APK terpasang hanya karena build selesai.

Larangan pengujian perangkat bukan alasan meninggalkan build yang belum diperiksa. Verifikasi kompilasi, provenance, tanda tangan, alignment, perubahan isi APK, dan keberhasilan instalasi tetap dilakukan tanpa menjalankan game.

## 3. Lokasi workspace dan konvensi path

Workspace yang diberikan pengguna:

```text
D:\Transformers-The-Game_Win_EN_ISO-Version\Transformers the game
```

Root source proyek recomp:

```text
D:\Transformers-The-Game_Win_EN_ISO-Version\Transformers the game\transformers-recompiled
```

Dalam dokumen ini:

- **G** = workspace game asli, yaitu direktori yang berisi `handoff.md` ini.
- **R** = `G/transformers-recompiled`.
- Path seperti `android/...`, `runtime/...`, `tools/...`, `work/...` pada penjelasan teknis biasanya relatif terhadap **R**.
- Perintah build harus dijalankan dari **R**, kecuali disebut lain.

Contoh masuk ke source root:

```powershell
Set-Location -LiteralPath 'D:\Transformers-The-Game_Win_EN_ISO-Version\Transformers the game\transformers-recompiled'
```

Tidak ada `.git` pada root proyek yang terlihat saat inventarisasi terakhir. Ada checkout dependency di bawah `external/`; jangan menganggap seluruh workspace adalah satu Git repository yang aman di-reset.

### Pemisahan file yang penting

```text
G/
  Transformers.exe                 Original PC x86 executable
  Transformers_Hi.DAT              Original game archive
  Transformers_Lo.DAT              Original game archive
  binkw32.dll                       Original Windows DLL; bukan Android library
  audio/                           Original loose audio
  fmv/                             Original movies
  Docs/                            Dokumentasi game asli, berbeda dari R/docs
  screenshot/                      Gambar dari pengguna/riwayat lokal
  Transformers the game.rar        Arsip asli lokal
  handoff.md                       Dokumen ini
  transformers-recompiled/         R: source port, tools, tests, work
```

Jangan memindahkan atau menghapus file asli untuk merapikan source. Jangan memasukkan R ke arsip yang diminta pengguna hanya berisi game asli.

Pernah ada permintaan ZIP game-only dalam riwayat. Namun saat handoff ini dibuat, `Transformers-game-only.zip` **tidak ditemukan** di G maupun satu direktori di atasnya. Jangan mengklaim arsip tersebut masih tersedia, dan jangan membuat ulang arsip besar tanpa tugas baru.

## 4. Identitas executable dan status lifting

| Item | Nilai terverifikasi |
| --- | --- |
| Nama | `Transformers.exe` |
| Ukuran | `3,702,784` byte |
| SHA-256 | `5aa3bd490f5b97f614a154fdca98cae845bb62c783c48e56042f19ac14aeefd0` |
| Format | PE32, x86, machine `0x014c` |
| Image base | `0x00400000` |
| Entry VA | `0x00666EAD` |
| On-disk imports | 189 menurut `work/preflight.json` |
| Frozen lift | `work/lift-5aa3bd490f5b-target/` |
| Snapshot EXE | `work/lift-5aa3bd490f5b-target/input.exe` |
| Lift status | `code_generated` |
| Function/dispatch entries | 14,470 |
| Generated C files | 50 |
| Translation diagnostics yang belum ditangani | 0 |
| Explicit interrupt boundaries | 16 |

Hash EXE asli dan snapshot lift dihitung ulang saat menulis handoff ini, dan keduanya cocok.

Jangan menerima EXE lain hanya karena nama filenya sama. Alamat guest pada decoder kontrol dan shim tertentu hanya valid untuk executable ini.

Pin dependency:

- Referensi pendekatan port: `https://github.com/codepdbh/nfsu2-recompiled`.
- pcrecomp repository: `https://github.com/sp00nznet/pcrecomp.git`.
- pcrecomp commit: `8c92919a99aaf9e129af5787776bca84863468cc`.
- Config: `config/toolchain.json` dan `config/code-regions.json`.
- Dependency Python utama yang dipin: `capstone==5.0.7`.

`lift-summary.json` masih memiliki field tahap seperti `runtime_status: not_implemented` dan `playable: false`. Itu bukan ringkasan kemampuan seluruh port sekarang. Metadata lifting tersebut berasal dari tahap lifting; runtime host kemudian dibangun terpisah.

Sebaliknya, 0 diagnostics dan ribuan fungsi juga bukan bukti bahwa semua jalur game sudah lengkap. Discovery masih heuristik dan batas unsupported harus tetap eksplisit.

## 5. Baseline APK terakhir

| Item | Nilai |
| --- | --- |
| Version name | `0.5.35` |
| Version code | `46` |
| Package | `io.github.transformers.recompiled.diagnostics` |
| Application label | `Transformers: The Game` |
| Minimum Android | Android 10 / API 29 |
| Target SDK | 29 |
| SDK untuk compile | Platform 36 |
| ABI | `arm64-v8a`, `armeabi-v7a`, `x86`, `x86_64` |
| APK | `work/android-ui-0535/transformers-dxvk-universal-debug.apk` |
| APK size | `173,584,461` byte |
| APK SHA-256 | `cd6058cfcb8e78b33d3eb360d6956ec1d17da54b80f2c86fcb2b4286af141140` |
| Install record | `work/ui0535/installation.json` |
| Build record | `work/android-ui-0535/apk-build-summary.json` |

Ini APK development/debug yang ditandatangani lokal, bukan bukti aplikasi release sudah di-hardening. Package masih memiliki nama `diagnostics` dan manifest `debuggable=true`; jangan mengganti identitas/tanda tangan hanya untuk merapikan nama.

Field `playable: false` atau `device_tested: false` dalam build summary adalah batas klaim tool packaging, bukan hasil tes gameplay terbaru. Bukti install berada pada laporan terpisah, dan bukti gameplay historis berada pada laporan/dokumentasi lain.

### Signature dan signing key

- Keystore lokal: `work/android/diagnostic-debug.jks`.
- Alias yang digunakan pipeline: `androiddebugkey`.
- Sertifikat SHA-256 publik yang tercatat:
  `0cf14d4ebd2455808f7cea26c70462fa6d4c0ee1512f99fa9da14afa2e961330`.
- File keystore masih ada ketika handoff dibuat.
- Pakai keystore yang sama untuk update `adb install -r`.
- Jangan mencetak, menyalin ke chat, mengunggah, atau memasukkan private signing key ke source release.
- Jangan uninstall aplikasi untuk menyiasati mismatch sertifikat karena itu menghapus data privat pengguna.

Dokumen ini tidak menyertakan private key atau isi rahasia keystore. Gunakan mekanisme signing yang sudah ada pada `tools/build_apk.py`.

## 6. Perangkat pengujian yang dikenal

Perangkat pada riwayat build/install:

| Item | Nilai |
| --- | --- |
| ADB serial | `3e752536` |
| Model | `M2012K11AG`, POCO F3 / alioth |
| Android | 13 |
| GPU | Adreno 650 |
| ABI yang benar-benar punya bukti eksekusi perangkat | ARM64 |

Serial tersebut adalah identitas sesi yang dikenal, bukan jaminan perangkat selalu tersambung. Selalu periksa `adb devices -l` sebelum instalasi. Pernah terjadi build selesai tetapi HP terputus; instalasi kemudian diulang setelah pengguna menyambungkan kembali.

Jangan salah memasang ke emulator atau perangkat lain jika serial ini tidak tersedia.

## 7. Peta arsitektur

Alur utama:

```text
Original x86 EXE + assets yang dipilih pengguna
  -> verified lift C + native core archive
  -> Win32/HLE compatibility runtime
  -> D3D9 guest bridge -> native DXVK -> Vulkan -> Android Surface
  -> DirectSound compatibility mixer -> AAudio
  -> virtual keyboard/mouse input dari Android controls

MainActivity (launcher process)
  -> URI pilihan folder/EXE + graphics/driver preferences
  -> GameActivity (:game process)
  -> SAF descriptors ditahan selama session
  -> nativeRun + existing session/surface lifecycle
```

Ini **static recompilation**, bukan menjalankan Windows EXE sebagai program native Android dan bukan memasang emulator Windows. EXE masih diperlukan sebagai input data/image yang identitasnya diverifikasi; eksekusi utama memakai kode hasil lifting yang telah dikompilasi ke ABI Android.

GameActivity menjalankan runtime pada proses Android terpisah `:game`. Foreground service, input, surface, dan sesi native harus dikelola sebagai satu sesi yang bisa bertahan saat Activity/surface berubah.

## 8. Peta file Android

Semua nama di tabel berikut berada di:

```text
android/src/io/github/transformers/recompiled/diagnostics/
```

| File | Tanggung jawab |
| --- | --- |
| `MainActivity.java` | Launcher English, banner, Play, Game Files, graphics, GPU driver, display/FPS preference |
| `GameActivity.java` | Surface, JNI, native worker, audio focus, native profile polling, FPS polling, pause owner UI |
| `GameSessionService.java` | Foreground notification dan resume session; explicit task removal mengakhiri proses game |
| `GameSources.java` | URI/index sumber, verifikasi EXE, mount descriptor-backed paths, session descriptor ownership |
| `AssetImporter.java` | Nama historis: sekarang memverifikasi sumber dan menyimpan indeks URI, bukan menyalin game |
| `SourcePaths.java` | Validasi path sumber; parent canonical di dalam root, final descriptor symlink diperbolehkan |
| `VerifiedFiles.java` | Hash/streaming copy atomik untuk metadata/cache/driver atau kebutuhan privat yang sah |
| `GameSettings.java` | Baca/tulis `pcconfig.txt`, prepare launch, canonical resolution, preserve fields lain |
| `DriverZipImporter.java` | Simpan ZIP driver, validasi ekstraksi dan pemilihan library |
| `DriverLibrary.java` | Catalog driver tersimpan dan validasi sebelum launch |
| `TouchControls.java` | Binding, layout default, hit testing, pointer ownership, held key/mouse aggregation |
| `CameraDrag.java` | Relative camera drag, pointer ID dan fractional movement |
| `TouchControlsArtwork.java` | Rendering desain tombol dan ikon, tidak menentukan binding |
| `TouchControlsView.java` | Canvas overlay, insets/scaling, editor drag/resize, profile drafts, storage, FPS geometry |
| `ControlLayout.java` | Model geometri tersimpan, parser, normalisasi, size clamp dan bounds |
| `TouchUiPanel.java` | Sidebar, editor panels, tabs, Save/Reset/Cancel, size selector, animasi |
| `FrameRateCounter.java` | Sampling counter frame native untuk FPS display |

Resources dan metadata penting:

- `android/AndroidManifest.xml`.
- `android/res/drawable-nodpi/ic_launcher.png`: Autobot HD.
- `android/res/drawable/ic_*.xml`: simbol launcher.
- `android/res/drawable/hud_*.xml`: simbol in-game controls.
- `android/launcher-icon-source.json`.
- `android/launcher-symbols-source.json`.
- `android/hud-symbols-source.json`.
- `android/MATERIAL-SYMBOLS-LICENSE.txt`.

Java menggunakan resource lookup melalui nama (`getIdentifier`) karena pipeline ini tidak mengandalkan Gradle-generated `R.java`. Nama resource yang dipakai kode harus benar-benar ada.

## 9. Peta native/runtime dan tools

| Area | File/direktori utama |
| --- | --- |
| JNI/session | `runtime/android_game.cpp` |
| Surface/presenter | `runtime/android_presenter.cpp` |
| Gate foreground/surface | `runtime/presentation_gate.h`, `session_clock.h`, `session_lifecycle.cpp` |
| Guest address/memory | `runtime/game_memory.c`, runtime pcrecomp yang disiapkan tool |
| Win32 startup/services | `runtime/startup.c`, `startup_user32.c`, `startup_services.c` |
| DirectInput/input bridge | `runtime/startup_input.c` |
| Audio | `runtime/startup_audio.cpp` dan file pendukungnya |
| Direct-source descriptor adapter | `runtime/source_files.c` |
| D3D9 bridge | `runtime/d3d9_bridge.cpp`, file `d3d9_*`, metadata generated bridge |
| Vulkan/driver loader | `runtime/android_vulkan.cpp`, `.h` |
| Native control profile | `runtime/control_profile.cpp`, `control_profile_state.h`, `control_profile.h` |
| x87 overlay | `runtime/transformers_x87.h`, `transformers_runtime.h` |
| DXVK Android integration | `ports/dxvk_android/` |
| Pinned upstream HLE sources | `external/pcrecomp/runtime/win32hle/` |

Tool utama:

- `tools/preflight.py`: inventaris original EXE/aset; bukan tes gameplay.
- `tools/lift.py`: target-aware lift, menolak menimpa output existing.
- `tools/build_native.py` / `build_arm64.py`: generated core archive per ABI.
- `tools/build_probe.py`: runtime preparation, validation/provenance, probe.
- `tools/build_game.py`: runtime C/C++ + DXVK + core archive + thin probe.
- `tools/build_apk.py`: compile resources/Java/DEX, kemas native/cache/driver, sign dan audit APK.
- `tools/build_dxvk.py`, `build_spirv_tools.py`, `build_adrenotools.py`: dependency build.
- `tools/compile_shader_capture.py`: kompilasi offline HLSL capture yang tepat.
- `tools/collect_device_shaders.py`, `service_device_shaders.py`: tooling perangkat historis; jangan otomatis dijalankan.

`build_probe.prepare_runtime` menyiapkan overlay/header/host source di output build dan memvalidasi dependency. Jangan mengganti checkout dependency atau mengedit generated C secara acak untuk memaksa link lolos.

## 10. Launcher yang sekarang diinginkan pengguna

Susunan launcher saat ini:

1. Banner dengan Autobot HD dan judul **`Transformers : The Game`**.
2. **Play Game** / readiness status.
3. **Game Files** langsung di bawah Play: **Select EXE** dan **Select folder**.
4. Graphics.
5. GPU Driver.
6. Display / FPS counter preference.
7. Footer **`Port By Sinn`**.

Launcher sudah memiliki simbol vektor untuk bagian menu dan tombol fungsi. Identitas banner memakai Autobot, bukan simbol gamepad. Pengguna mengoreksi hal ini secara spesifik.

Bahasa:

- Label, status, dialog, verification messages, dan error milik launcher sudah diterjemahkan English pada 0.5.29.
- MainActivity memakai English configuration context untuk widget bawaan di dalam launcher.
- Itu tidak mengganti bahasa HP atau system document picker.
- Nama file/ZIP milik pengguna tidak diterjemahkan.

`README.md`, `docs/STATUS.md`, dan `docs/ANDROID.md` masih mengandung ringkasan versi lama. Jangan menggunakan kalimat lama seperti “import copies all game data” sebagai kontrak aplikasi sekarang.

## 11. Akses data game langsung: desain saat ini

Kontrak pengguna: pilih folder game dan EXE, verifikasi, lalu baca dari sumber tanpa menggandakan seluruh game ke memori penyimpanan aplikasi.

Frozen asset manifest:

- `work/game-assets.json`.
- 482 file aset yang diharapkan.
- Total `4,346,085,350` byte, sekitar 4.35 GB desimal / 4.05 GiB.
- SHA-256 manifest pada APK terakhir:
  `36b043918fad4aaf726ec5fae327aaa6a3af1f2b971dc782882a9a8ba72cb83e`.
- EXE diverifikasi terpisah dengan hash input yang didukung.

Alur pemilihan sumber:

1. System SAF picker memberikan URI dan read permission.
2. Aplikasi meminta persistable URI permission untuk folder/EXE.
3. `AssetImporter` mencocokkan nama sumber tanpa membedakan kapital, menolak ambigu, lalu streaming-verifies ukuran/hash aset.
4. Indeks URI diterbitkan sebagai `files/direct-sources.json`.
5. URI EXE berada pada preferences `game_sources`, key `exe_uri`.
6. Jika EXE yang didukung ada dalam folder, importer dapat memilihnya otomatis.
7. File Windows DLL tidak diimpor sebagai library Android.

Alur launch:

1. `GameSources.Session.mount` membuka EXE + 482 aset sebagai read-only ParcelFileDescriptor.
2. Descriptor harus regular file, ukurannya cocok, dan bisa seek.
3. Descriptor ditahan sampai sesi native selesai, bukan sekadar sampai Activity pause.
4. Path privat di `files/game/` berisi symlink token menuju `/proc/self/fd/<fd>`.
5. Native mengenali token lalu memakai descriptor langsung; bukan membuka symlink tersebut kembali melalui filesystem permission checks.
6. Guest EXE tetap memiliki path virtual stabil `C:\Transformers.exe`; save/config menggunakan lokasi privat yang sudah ada.

Poin integritas yang jangan dilebih-lebihkan:

- Verifikasi hash penuh aset terjadi saat pemilihan/verifikasi sumber.
- Mount memeriksa metadata/ukuran/seekability, bukan meng-hash ulang 4.35 GB pada setiap launch.
- `available()` memvalidasi indeks dan akses root; itu bukan bukti setiap file masih ada pada saat tombol Play ditekan.
- Sumber hilang/izin dicabut harus gagal jelas saat pembukaan, bukan diam-diam fallback ke copy privat lama.
- EXE kembali diverifikasi pada boundary loader native.
- Tidak ada audit ulang seluruh data asli pada tugas penulisan handoff ini.

### Cleanup copy lama

Legacy private asset copies hanya dihapus bila path tepat, regular file, ukuran dan hash cocok dengan manifest. Copy yang dimodifikasi, save, dan konfigurasi harus dipertahankan. Cleanup selection ditunda jika proses game masih hidup.

Kata “tidak menyalin game” tidak berarti aplikasi tidak pernah menulis file: metadata URI, driver ZIP, shader cache, save, konfigurasi, dan layout UI tetap disimpan secara sah.

## 12. Bug EACCES penting: jangan dikembalikan

0.5.22 memakai descriptor link tetapi Java/native masih mencoba membuka ulang path symlink. Pada provider Android nyata, descriptor yang diberikan bisa dibaca, sementara membuka `/proc/self/fd/...` lewat path terkena `EACCES`.

Error aktual yang pernah terjadi:

```text
FileNotFoundException: .../files/game/Transformers.exe: open failed: EACCES
RandomAccessFile.<init>
GameSources$Session.add
```

Kegagalan itu terjadi sebelum `nativeRun` dan sebelum driver custom dimuat. Jangan mendiagnosisnya sebagai driver Vulkan rusak tanpa melihat urutan kejadian.

Perbaikan 0.5.24:

- Java menghapus pemeriksaan `new RandomAccessFile(target, "r")` pada link.
- `runtime/source_files.c` membaca target link hanya untuk memperoleh FD asli.
- New `open` menggunakan `fcntl(F_DUPFD/F_DUPFD_CLOEXEC)`.
- Karena duplikat FD berbagi kernel offset, adapter menyimpan offset virtual per new open dan membaca melalui `pread64`.
- `dup` atas handle yang sudah dibuka berbagi objek posisi sesuai semantik dup.
- `read`, `lseek`, `lseek64`, `close`, `dup`, `stat`, `stat64`, `access` dibungkus.
- `fopen/fopen64` memakai `funopen64` callbacks; internal read libc tidak otomatis terkena linker wrapping.
- Write/create/truncate/append pada source-token paths ditolak; unlink/rename sumber juga ditolak.
- File privat biasa tetap memakai libc dan tetap writable untuk save/settings.

Wrapper linker terkait di `tools/build_game.py` harus dipertahankan. Jangan hanya menambahkan fungsi `__wrap_*` tanpa `-Wl,--wrap=...` yang benar.

Bukti:

- `work/sources0524/native-source-test.log`: self-test production ARM64 mereproduksi penolakan EACCES melalui permission removal dan membuktikan FD adapter tetap bekerja.
- `work/sources0524/validation.json`: build/install dan batas pengujian.
- Test provider satu UID pernah lolos tetapi tidak cukup untuk menemukan kasus reopen permission ini.
- **Cross-UID fixture test tidak selesai**; instalasi fixture dibatalkan Android dengan `INSTALL_FAILED_USER_RESTRICTED`.
- Jangan mengutip keberadaan `tools/test_cross_uid_sources_android.py` sebagai bukti hasil cross-UID PASS.
- Pengguna kemudian melarang pengujian dengan APK tambahan; jangan menjalankan ulang fixture otomatis.

## 13. GPU driver bawaan dan driver custom

UI GPU Driver menyediakan Automatic, System driver, dan daftar ZIP pengguna.

Default otomatis:

- Bundled Turnip **26.2.5** untuk ARM64 Adreno 6xx.
- System Vulkan path pada perangkat yang tidak memenuhi pemilihan bundled path.
- Driver bersifat app-local melalui libadrenotools; tidak mengganti driver sistem Android.

Pinned provenance:

- DXVK commit: `c8dc91fabd00cac11d697ccf07426e798393cd40`.
- Turnip repository: `https://github.com/nihui/mesa-turnip-android-driver`.
- Release tag: `26.2.5`.
- Release commit: `d88dc83e06bcdd6dfa10413ef7e576c96b8b8830`.
- Turnip archive SHA-256: `b1701ef4a3da8a417beaa4108cc2d2edb8621e3f6c514ceadecb7f804a922181`.
- Bundled driver SHA-256: `56076b1026678fb154e3c09394e9cad7c7070fc1537df237d89a44ce1a3b757d`.
- Working directories: `work/turnip-26.2.5/`, `work/adrenotools/`.

Driver custom:

- Preferences file `gpu_driver`: keys `mode` dan `path`.
- Pilihan diterapkan pada sesi native baru; resume sesi existing tidak mengganti device driver.
- ZIP asli disimpan dan diekstrak di `files/drivers/library/<zip_sha256>/`.
- Struktur biasanya memuat `source.zip`, `package/`, dan `entry.properties`.
- Nama ZIP pengguna dipertahankan pada daftar; benturan nama diberi suffix.
- `meta.json.libraryName` memilih library, atau harus ada tepat satu `.so` valid.
- Batas compressed/extracted bytes 256 MiB dan 256 entries.
- Menolak traversal/absolute/duplicate paths, CRC/size rusak, ambiguous libraries, header ABI salah.
- Library harus ELF64 little-endian ET_DYN AArch64 dan memiliki marker Android Vulkan HMI.
- Hash library diperiksa ulang sebelum launch.
- Validasi struktural tidak membuktikan kompatibilitas semua GPU.
- Native hanya menerima custom path di canonical private driver-library root dan mempertahankan nama/direktori library yang diekstrak.
- Kegagalan pilihan custom tidak boleh disembunyikan melalui fallback diam-diam.

Referensi implementasi awal berasal dari proyek DMC3 lokal yang diberikan pengguna:

```text
D:/New folder/Devil may cry 3 - special edition/_re/v028-original/java/src/local/dmc3/runtimeprobe/
```

Jangan mencampur package, driver policy, atau build outputs DMC3 ke proyek Transformers.

Bukti historical driver load terdapat di `work/drivers0523/` dan `docs/GPU_DRIVERS.md`. Jangan mengasumsikan pilihan driver pengguna saat ini hanya dari default kode; update terakhir mempertahankan preferences, bukan mengubah pilihan.

## 14. Graphics, shaders, audio, dan batas runtime

### Graphics

D3D9 guest pointers/handles 32-bit dijembatani ke native DXVK COM objects. Host pointers tidak boleh dipotong menjadi DWORD dan dimasukkan ke memory guest.

Bridge memiliki accounting AddRef/Release, marshalling struktur pointer-sized, guest staging storage untuk lock/unlock, dan batas eksplisit untuk operasi yang belum diimplementasikan.

`tools/generate_d3d9_bridge.py` menghasilkan metadata/wrappers dari pinned DXVK headers. Jangan mengarang jumlah argumen stdcall atau slot vtable.

Dokumen `GRAPHICS_BRIDGE.md` berisi sejarah; sebagian gap lama, seperti BMP image encoding, sudah diperbaiki kemudian. Baca `MISSION_CRASH.md` dan source aktual sebelum menyimpulkan fitur belum ada.

### Shader cache

- Snapshot bundled saat ini: **343 shader cache entries**.
- Directory: `work/shader-cache/`.
- `D3DXCompileShader` membutuhkan exact cache request: source, macros, flags, entry, profile harus cocok.
- Kompilasi offline menggunakan Windows `System32/d3dx9_31.dll` yang dipercaya oleh helper, bukan DLL game yang dijalankan.
- Request baru yang tidak ada di cache tetap boundary fatal/captured, bukan diganti shader kosong.
- Cache di APK diverifikasi lalu dipasang ke area privat saat launch.
- Jangan mengurangi fitur graphics untuk menutupi cache miss tanpa permintaan dan bukti.

Kasus Decepticon Chapter 1 sebelumnya membutuhkan 14 shader baru, menaikkan jumlah dari 329 ke 343. Pengguna pernah mengonfirmasi chapter tersebut dapat dimainkan setelah perbaikan. Ini bukan jaminan semua level punya semua shader.

Tool collection perangkat dan temporary wait markers hanya untuk sesi debugging yang benar-benar diminta. Jika digunakan pada tugas masa depan, marker harus dibersihkan saat selesai; jangan meninggalkan runtime menunggu host tanpa batas.

### Audio

Gameplay audio melalui DirectSound compatibility mixer menuju AAudio. Original game tetap mengatur buffer, streaming, volume, dan sample selection. BGM saja tidak membuktikan SFX berfungsi.

Riwayat koreksi penting: loaded source buffers pernah salah dibatasi sebagai active voices, menyebabkan duplicate SFX gagal. Baca `docs/AUDIO.md` sebelum mengubah batas buffer/voice atau interface COM audio.

Saat runtime disuspend, AAudio mengeluarkan silence tanpa mengonsumsi ring PCM sesi. Jangan mereset seluruh audio hanya karena sidebar dibuka.

### Batas yang masih perlu dihormati

- FMV Bink decoding/audio belum tersedia sebagai fitur lengkap port.
- Seluruh campaign belum diuji tuntas.
- Cakupan D3D9/Win32 masih parsial; unsupported paths harus gagal eksplisit dengan konteks yang benar.
- Structured exception delivery/unwinding bukan implementasi penuh Windows SEH.
- Enam belas interrupt boundaries eksplisit tidak boleh dihapus massal supaya build terlihat bersih.
- Microbenchmark x87/COM bukan angka FPS seluruh game.

## 15. Grafik dan konfigurasi sebelum launch

`GameSettings.prepareLaunch` menulis kembali mode PC yang diterima **1280×720**, kemudian native bridge menerapkan render scale satu kali.

Opsi launcher:

| Scale | Label |
| --- | --- |
| 50 | 640×360 / 360p |
| 67 | 858×482 / 480p |
| 100 | 1280×720 / 720p |

Jangan menyimpan ukuran yang sudah diskalakan lalu menskalakannya lagi saat launch berikutnya.

Field konfigurasi penting:

- `ScreenWidth`, `ScreenHeight`.
- `TransformersUseHires`, `TransformersUseHiresPending` — keduanya harus konsisten.
- `BloomEnabled`, `ShadowsEnabled`, `SpeedBlurEnabled`.

Config path:

```text
files/game/documents/ACTIVISION/Transformers/pcconfig.txt
```

Penulisan memakai temporary file, sync, dan atomic move bila didukung, dengan preserving field/line-ending lain. Jangan mengganti seluruh config dengan template minimal yang menghapus preferensi/save-related fields.

## 16. Input dan binding yang harus dipertahankan

| Fungsi | Binding aktual |
| --- | --- |
| Menu directions | VK38 / VK40 / VK37 / VK39 |
| Menu Continue / label lama ENTER | VK13 |
| Menu Back | **VK8 / Backspace**, bukan Escape |
| Pause game | VK27 / Escape |
| Robot movement | W/S/A/D melalui joystick |
| Attack / internal id `melee` | E / VK69 |
| Block / vehicle Boost | Shift / VK160 |
| Jump / handbrake | Space / VK32 |
| Grab / horn | Q / VK81 |
| Light fire / id `guns` | Mouse button mask 2 |
| Rocket / id `rocket` | Mouse button mask 1 |
| Aim/lock / id `lock` | Mouse button mask 4 |
| Transform | Mouse wheel pulse 120 per touch |
| Car steering | A / D |
| Accelerator | W |
| Brake | S |

Pemisahan id, label, dan binding disengaja. Mengganti tulisan `MELEE` menjadi `ATTACK` pada renderer tidak boleh mengganti VK69. Continue tetap mengirim Enter walaupun tampilannya memakai ikon segitiga.

Invariants:

- Transform hanya satu wheel pulse untuk satu touch; bukan spam saat drag atau multi-finger menyentuh tombol sama.
- Kamera hanya movement, tidak menghasilkan fire/click tambahan.
- Pointer ID, bukan urutan index pointer, menentukan kepemilikan jari.
- Key ownership reference-counted: dua sumber menahan key yang sama tidak boleh saling melepas terlalu awal.
- Touch/hardware key ownership harus konsisten.
- ACTION_CANCEL, hilang jari, focus loss, dan pergantian profil harus melepas input yang relevan.
- `releaseTouches` berbeda dari `releaseAll`; jangan menyamakan jika hardware key masih harus ditahan.
- Custom editor tidak boleh mengirim key/mouse/wheel ke guest.
- FPS HUD tidak boleh menyerap gameplay hit atau menghasilkan key 0.

DirectInput camera pernah bermasalah karena game meminta absolute axes. `startup_input.c` mengakumulasi posisi absolute yang benar; jangan mengembalikan idle axes ke nol atau menghidupkan multiplier 8× untuk menutupi bug semantik.

## 17. Dynamic control profiles

Nilai native dan Java harus sama:

```text
0 HIDDEN
1 MENU
2 ROBOT
3 CAR
```

Native sampling berlangsung pada guest frame yang terserialisasi. Java polling membaca published/atomic profile, bukan mengintip guest memory tanpa guard di UI thread.

File utama: `runtime/control_profile_state.h` dan `runtime/control_profile.cpp`.

Alamat penting hanya untuk supported EXE:

| Data | Alamat / keterangan |
| --- | --- |
| Frontend flag | `0x71318c` |
| Scene pointer | `0x7183a8`; nama `frontend` juga dikenali |
| Pause menu active | byte `0x713677` |
| Save UI active | byte `0x71437a` |
| Loading engine flags | DWORD `0x6fb1d0`, bit `0x80` |
| Loading prompt enabled | DWORD `0x713464` |
| Loading prompt ready | DWORD `0x713468` |
| GUI root | `0x713648` |
| Paused / pending | `0x6fb0b0`, `0x6fb0b4` |
| Inhibit | `0x7622cc` |
| Actor pointer | `0x770614` |
| Actor form flags | actor + `0x628`; bit `0x40000` memilih Car |
| Actor animation | actor + `0x1b24` |

Prioritas decoder penting:

1. Frontend/menu scenes.
2. Visible pause/mission/start/completion/failure/save/loading prompts.
3. Loading aktif tetapi prompt belum siap -> HIDDEN.
4. Invalid scene / paused / pending / inhibit -> HIDDEN.
5. Valid actor -> Robot atau Car menurut form bit.

Validasi controller vtable, phase dan active flags membedakan prompt yang benar-benar interaktif dari loading/fade/cleanup. Actor pointer lama bisa masih ada saat prompt Enter; karena itu actor presence tidak cukup untuk memilih combat UI.

Dua regresi yang sudah diperbaiki:

- Saving files harus menampilkan menu controls walaupun pause/inhibit umum aktif.
- Loading “Enter to continue” harus menampilkan Menu ketika enabled+ready pada loop loading aktif, lalu kembali ke combat setelah loop berakhir walau ready flag masih tertinggal.

Jangan membuat state lokal `isCar = !isCar` ketika tombol Transform disentuh. Perubahan form tetap milik game asli, termasuk animasi transformasi dan scripted state.

## 18. Desain kontrol sekarang

0.5.32 memperkenalkan desain yang dipilih pengguna:

- Latar tombol dark translucent.
- Rounded filled symbols, bukan gambar wireframe tajam buatan awal.
- Ikon putih, amber pressed feedback, outline amber pada aksi utama.
- Joystick ring/thumb lebih sederhana.
- Label pendek: Attack, Aim, Fire, Grab, Continue.
- Robot, Car dan Menu tetap memiliki kelompok kontrol berbeda secara dinamis.

Sumber ikon:

- Google Material Symbols Rounded, 21 ikon HUD filled.
- Dua pedal vector paths authored lokal.
- Resource license Apache-2.0 sudah dibundel dalam APK.
- Symbol launcher dan HUD dipisah sehingga perubahan HUD tidak mengganti ikon launcher.

Autobot launcher image:

- Source page: `https://pngimg.com/image/45374`.
- Image URL: `https://pngimg.com/uploads/transformers/transformers_PNG8.png`.
- Resolusi 2048×2048.
- PNG 32×32 hasil ekstraksi original EXE pernah dipakai pada 0.5.25 tetapi pengguna menilainya buram; diganti HD pada 0.5.26.
- `tools/extract_launcher_icon.py` sekarang hanya menulis ekstraksi historis ke `work/original-exe-icon.png`, bukan menimpa resource HD.

Branding game adalah milik pemegang hak aslinya; jangan memasukkan gambar tersebut ke lisensi source code proyek seolah-olah authored sendiri.

## 19. Custom UI: model, interaksi, dan persistence

Entry point:

```text
Three dots -> sidebar -> Custom UI -> Menu / Robot / Car
```

Titik tiga tidak lagi menampilkan ekstra SHIFT/CTRL/Q/E/F/BKSP. Fungsinya sekarang callback `Output.menu()` untuk membuka sidebar.

Default positions pada logical viewport:

- More: `x=w/2-40`, `y=42`, radius 24.
- Exit: `x=w/2+34`, `y=42`, radius 26.
- Keduanya ada pada Menu/Robot/Car; HIDDEN tetap tanpa kontrol.

Editor:

- Menampilkan game surface terakhir yang dijeda.
- Memiliki tab Menu/Robot/Car dan draft independen untuk ketiganya.
- Memilih tombol lewat tap atau selector.
- Drag manual mengubah center posisi.
- Size slider 50–200% berlaku hanya pada tombol terpilih.
- Dua jari bisa memiliki drag masing-masing; satu tombol tidak dimiliki dua jari sekaligus.
- Drag empty space memindahkan tombol yang sedang dipilih, berguna ketika tombol berada di bawah tools.
- Hide/Show tools memungkinkan penempatan tanpa panel menghalangi.
- Save menyimpan semua draft profil dalam satu update preferences.
- Reset hanya mengosongkan draft profil aktif, dan permanen setelah Save.
- Cancel/Back membuang draft dan memulihkan saved layout.
- Save gagal tetap berada di editor; kode mencoba memulihkan nilai preference sebelumnya setelah commit gagal.

Storage:

```text
shared_prefs/touch_layouts.xml
  profile_1 -> Menu geometry string
  profile_2 -> Robot geometry string
  profile_3 -> Car geometry string
```

Format record `ControlLayout`:

```text
id,x_normalized,y_normalized,size_multiplier;
```

Contoh:

```text
melee,0.82,0.70,1.25;fps,0.50,0.20,1.0;
```

ID dibatasi `[a-z0-9]{1,40}`. Parser membatasi panjang string dan jumlah records, mengabaikan malformed/nonfinite data, menormalisasi center ke 0–1, dan size ke 0.5–2.0.

Normalisasi center memakai viewport logis yang sudah aman dari cutout/gesture insets. Ukuran berasal dari base control × multiplier. Semua bounds tombol dijaga tetap di layar; joystick menambahkan ruang bawah untuk label visible.

Viewport logical scaling saat ini:

```text
scale = max(0.1, min(displayDensity, min(usableWidth/840, usableHeight/400)))
```

Jangan mencampur pixel fisik, dp widget Android, dan logical control units saat menggeser atau menyimpan layout.

Saat native profile berubah selama editor terbuka, `actualProfile` diperbarui tetapi profil preview editor tidak ditimpa. Saat editor selesai, profil native terakhir dipulihkan.

## 20. FPS sebagai item Custom UI

Pada 0.5.34, FPS ditambahkan sebagai synthetic HUD control:

- ID: `fps`.
- Kind: `TouchControls.HUD=11`.
- Default center: `w/2,96`, half-width 46, half-height 14, radius basis 14.
- Ada pada model editor tiap profil.
- Selection label: `FPS counter`.
- Editor menampilkan sample **`FPS 60.0`**, bukan pengukuran FPS nyata.
- Normal gameplay memakai TextView FPS aktual, yang layout params/font size mengikuti geometri tersimpan.
- Counter tetap hanya tampil saat preference FPS diaktifkan.
- FPS editor marker tetap tersedia untuk ditata walaupun counter aktual dimatikan.
- Renderer tombol biasa melewati kind HUD.
- Gameplay `hit()` secara eksplisit melewati kind HUD; kamera atau kontrol di bawahnya tetap dapat menerima touch sebagaimana semestinya.

Perubahan posisi FPS tidak boleh hanya memindahkan sample editor tetapi melupakan TextView aktual. Jalur `setFpsCounter` / `updateFpsPosition` pada TouchControlsView menghubungkan keduanya.

## 21. Sidebar, viewport scroll, tools, dan animasi terbaru

Bagian ini memuat bug visual yang dilaporkan paling akhir dan alasan desain saat ini.

### Sidebar sebelum 0.5.35

Sidebar semula memakai padding safe-area pada layer/ScrollView. Setelah sheet background dibuat full-height pada 0.5.34, pengguna tetap melihat scroll area seperti terpotong di tengah.

### Perubahan 0.5.35

- `sidebarSheet` berupa FrameLayout berwarna opaque, lebar 280dp, tinggi MATCH_PARENT.
- Gravity sheet eksplisit TOP|RIGHT.
- ScrollView MATCH_PARENT di dalam sheet, gravity TOP|LEFT.
- ScrollView padding nol.
- Safe-area/content spacing diterapkan pada **anak LinearLayout menu**, bukan pada ScrollView.
- ScrollView `fillViewport=true`, `clipToPadding=false`.
- Vertical fading edge dimatikan dan fading length nol.
- Overscroll dimatikan.
- Scrollbar INSIDE_OVERLAY, fade scrollbar dimatikan.
- Track tipis dan thumb amber digunakan di sisi kanan.
- Thumb tetap merepresentasikan rasio scroll range yang sebenarnya; jangan memalsukan thumb full-height untuk menyamarkan viewport yang salah.

Jangan kembali menaruh top/bottom safe-area padding pada ScrollView jika ingin viewport/track benar-benar memenuhi tinggi sheet. Isi menu masih boleh memiliki padding agar teks tidak tertutup cutout.

### Tools compact sejak 0.5.34

- Toolbar atas: 430dp wide, centered, dua row sekitar 32dp + padding vertikal 5dp.
- Title kecil dan tombol Save/Reset/Cancel sekitar 58×32dp.
- Tab Menu/Robot/Car pada row kedua.
- Size panel: 300dp wide, dua row 32dp, selector + percentage + Hide pada satu row.
- Show tools: sekitar 94×32dp.
- Caption instruksi panjang di panel bawah sudah dibuang.

Ukuran compact ini merupakan permintaan eksplisit pengguna setelah versi sebelumnya terlalu besar. Jangan menaikkannya kembali ke panel full-width besar hanya untuk mengikuti default widget styling.

### Animasi dan pause ownership

- Sidebar membuka dengan slide dari kanan sekitar 240ms dan fade backdrop 200ms.
- Sidebar menutup dengan slide/fade sekitar 180ms.
- Visibility sidebar tetap aktif sampai close animation selesai, sehingga runtime tidak resume terlalu dini.
- Transition counter/closing flag menjaga callback lama dan interaksi ganda.
- Membuka editor mengaktifkan editor pause owner sebelum sidebar selesai menutup.
- Tools editor fade/slide dengan scale kecil sekitar 180ms; editor layer fade 200ms.
- Tab switch dan Show tools juga memakai animasi tools.

Animasi harus tetap dekoratif: jangan menunda pelepasan gameplay input, jangan mengeksekusi key event di completion callback, dan jangan melupakan owner pause ketika Activity mendapat focus lagi.

## 22. Lifecycle dan pause yang benar

Ada tiga hal berbeda:

1. Pause menu game melalui VK27.
2. Background/focus/surface pause dari Android.
3. Sidebar/editor UI pause.

Sidebar/editor memakai mekanisme kedua untuk membekukan sesi sementara, **tanpa mengirim Escape**. Jika game sebelumnya sudah berada di pause menu aslinya, menutup editor tidak boleh otomatis meng-unpause menu itu.

Native gate:

- `nativeSetForeground(false)` mengubah presentation gate dan mensuspend audio.
- Native graphics entry dan guest clock polls menunggu foreground + valid surface.
- Guest timing mengecualikan interval pause/background; host I/O time tetap real.
- Surface tetap dipertahankan saat editor dibuka, sehingga frame terakhir tetap terlihat.
- Detach/reattach surface memakai serialization dan tidak mengulang lifted entry.

Java guard foreground:

```text
loaded && resumed && hasWindowFocus && !touchUi.isOpen()
```

Implementasi aktual memeriksa loaded sebelum memanggil JNI, lalu menghitung active dari resumed/focus/UI owner. Audio-focus callback juga menghormati UI owner, bukan langsung menghidupkan game kembali.

Home/Recent Apps dan surface recreation mempertahankan sesi selama proses masih hidup. Explicit Exit atau task dismissal mengakhiri dedicated game process. Process death/reboot tidak memulihkan in-memory guest session; ini bukan save-state serialization.

Jangan mengembalikan bug lama `onStop()->finish()` atau `surfaceDestroyed()->finish()`.

## 23. Toolchain lokal dan kebiasaan Windows

| Tool | Path / versi |
| --- | --- |
| Shell | PowerShell |
| Python yang terlihat | `C:/Python314/` |
| JDK | `D:/AndroidTools/jdk17/jdk-17.0.20.1+1` |
| Android SDK | `D:/AndroidTools/android-sdk` |
| ADB | `D:/AndroidTools/android-sdk/platform-tools/adb.exe` |
| Build tools | `35.0.0` |
| Platform jar | `platforms/android-36/android.jar` |
| NDK | `27.1.12297006` |

Catatan praktis:

- Selalu baca/tulis Java/Markdown dengan UTF-8 eksplisit. PowerShell `Get-Content` dapat menampilkan mojibake walaupun file sebenarnya benar; jangan “memperbaiki encoding” dengan menulis ulang cp1252.
- Gunakan `Path.read_text(encoding='utf-8')` dan `write_text(..., encoding='utf-8')`.
- Hindari shell substitution/quoting yang mengeksekusi backticks atau `$()` tanpa sengaja.
- Gunakan `-LiteralPath` untuk operasi file PowerShell dengan spasi.
- Jangan memakai brace expansion Bash seperti `{kernel32.c,pe_loader.c}` di PowerShell.
- Jangan repurpose `$HOME` atau variable sistem untuk path task.
- Sebelum delete/move rekursif, pastikan resolved absolute target adalah staging task yang benar; jangan menghapus `work/` secara global.
- Build native memakai **satu compiler worker** untuk membatasi RAM.
- Jangan meluncurkan helper background dengan jendela terminal terlihat jika tidak diperlukan.

### Trap Java lambda yang pernah terjadi

Pipeline menggunakan `javac -source 8 -target 8 -bootclasspath android.jar`. Percobaan lambda pada UI baru gagal dengan:

```text
Unable to find method metafactory
cannot find symbol LambdaMetafactory.metafactory(...)
```

Perbaikannya: anonymous `View.OnClickListener`, `Runnable`, dan `OnApplyWindowInsetsListener`. Jangan menambahkan lambda baru atau mengubah seluruh toolchain hanya demi syntax pendek pada tugas UI kecil.

`tools/build_apk.py` sekarang melakukan `aapt2 compile --dir android/res`, lalu link compiled resources. License Material Symbols juga dikemas ke `assets/licenses/`.

## 24. Native artifacts yang harus dipakai ulang

| ABI | Core archive | Runtime output |
| --- | --- | --- |
| arm64-v8a | `work/build-arm64-release/libtransformers_lift.a` — O2 | `work/probe-arm64-target/` |
| armeabi-v7a | `work/build-armeabi-v7a-target/libtransformers_lift.a` — O0 | `work/probe-armeabi-v7a-target/` |
| x86 | `work/build-x86-target/libtransformers_lift.a` — O0 | `work/probe-x86-target/` |
| x86_64 | `work/build-x86_64-target/libtransformers_lift.a` — O0 | `work/probe-x86_64-target/` |

DXVK archives berada di `work/dxvk-<abi>/libdxvk_android.a`.

Runtime outputs memuat:

- `libtransformers_game.so`: core + runtime + DXVK.
- `transformers-probe`: thin executable yang memakai shared runtime itu.
- `game-build-summary.json`, `probe-build-summary.json` dan log compile/link.

APK mengemas thin probe sebagai `libtransformers_probe.so`; jangan menganggap itu core game kedua yang perlu dibangun raksasa lagi.

SHA-256 game libraries pada baseline APK 0.5.35:

| ABI | SHA-256 `libtransformers_game.so` |
| --- | --- |
| arm64-v8a | `e173c2b32131f5d612585beec9d19ce59807017748f5c83673c863eb644532c2` |
| armeabi-v7a | `034744cc8bcf08eb0293ecb8488dd2f8dfdc9df5b0078859ae7012f6099b73b0` |
| x86 | `170ce0dcedc4a1699e48c13754a854e6b7296262d22f1eb3ae4fd56925e81617` |
| x86_64 | `659c4cf92265be5c6ab6cd097b55ccdbcb20830b988ea513bdcf09383246dc5c` |

Jangan memaksa build dengan mengedit hash provenance agar cocok. Jika source closure atau compiler semantics berubah, rebuild layer yang benar.

## 25. Build APK untuk perubahan Java/UI/resource

Untuk UI saja, **jangan rebuild generated core atau DXVK**. Increment manifest version name/code, pilih output folder baru, lalu gunakan artifacts yang sudah tervalidasi.

Contoh reproduksi baseline 0.5.35 dari R:

```powershell
$env:JAVA_HOME='D:/AndroidTools/jdk17/jdk-17.0.20.1+1'

python tools/build_apk.py `
  --lift-dir work/lift-5aa3bd490f5b-target `
  --native-root work `
  --graphics-root work `
  --out work/android-ui-0535 `
  --sdk D:/AndroidTools/android-sdk `
  --jdk D:/AndroidTools/jdk17/jdk-17.0.20.1+1 `
  --asset-manifest work/game-assets.json `
  --shader-cache work/shader-cache `
  --keystore work/android/diagnostic-debug.jks `
  --adreno-tools work/adrenotools `
  --turnip-driver work/turnip-26.2.5
```

Untuk pekerjaan baru, ganti `--out` dengan staging task baru. Versi berikutnya setelah baseline ini akan membutuhkan versionCode lebih tinggi dari 46; jangan otomatis menimpa baseline hanya karena contoh memakai nama folder lama.

Pemeriksaan Java/DEX/resources saja dapat memakai `--prepare-only` dan output terpisah. Itu **tidak menghasilkan installable APK**; jangan menyatakan instalasi siap hanya dari prepare-only PASS.

Logs yang perlu dibaca jika gagal:

- `aapt2-compile.log`, `aapt2.log`.
- `javac.log`, `d8.log`.
- `apk-build-summary.json`.
- Log signing/alignment/badging yang dihasilkan script.

Jangan membangun dengan source yang sedang diedit paralel oleh proses lain. Bekukan perubahan sebelum packaging.

## 26. Jika runtime bridge berubah

Rebuild runtime untuk seluruh ABI yang akan dikemas, tetapi pakai core archive yang sama selama generated-code closure memang tidak berubah.

ARM64:

```powershell
python tools/build_game.py `
  --abi arm64-v8a `
  --lift-dir work/lift-5aa3bd490f5b-target `
  --core-archive work/build-arm64-release/libtransformers_lift.a `
  --dxvk-dir work/dxvk-arm64-v8a `
  --out work/probe-arm64-target `
  --jobs 1
```

ABI lain:

```powershell
python tools/build_game.py --abi armeabi-v7a --lift-dir work/lift-5aa3bd490f5b-target --core-archive work/build-armeabi-v7a-target/libtransformers_lift.a --dxvk-dir work/dxvk-armeabi-v7a --out work/probe-armeabi-v7a-target --jobs 1

python tools/build_game.py --abi x86 --lift-dir work/lift-5aa3bd490f5b-target --core-archive work/build-x86-target/libtransformers_lift.a --dxvk-dir work/dxvk-x86 --out work/probe-x86-target --jobs 1

python tools/build_game.py --abi x86_64 --lift-dir work/lift-5aa3bd490f5b-target --core-archive work/build-x86_64-target/libtransformers_lift.a --dxvk-dir work/dxvk-x86_64 --out work/probe-x86_64-target --jobs 1
```

Jalankan berurutan dengan satu worker. Periksa compilation results, `link.log`, `dynamic.log`, dependency hashes dan absence of TEXTREL/unbundled C++ dependency sebelum packaging.

Jika hanya shader cache bertambah, pertahankan native runtime kecuali bridge memang berubah. Jika DXVK backend berubah, rebuild backend dan runtime link yang terpengaruh; jangan diam-diam mengganti pinned revision.

## 27. Jika lifting/generated semantics berubah

Ini langkah mahal; lakukan hanya bila tugas dan bukti mengharuskannya.

```powershell
python tools/preflight.py
python tools/lift.py --out work/nama-lift-baru
```

Lifter menolak output yang sudah ada. Pertahankan frozen lift lama agar hasil bisa dibandingkan.

Contoh core build ARM64 jika memang dibutuhkan:

```powershell
python tools/build_native.py `
  --abi arm64-v8a `
  --lift-dir work/lift-5aa3bd490f5b-target `
  --out work/build-arm64-release `
  --opt 2 `
  --jobs 1
```

ABI lain baseline memakai `--opt 0`. Untuk eksperimen, gunakan output baru alih-alih menimpa archive terverifikasi.

Perubahan pada `transformers_x87.h`, `transformers_runtime.h`, generated ABI, dispatch layout, code-region profile, atau compiler flags bisa menginvalidasi core provenance. Jangan hanya relink archive lama.

Tidak boleh bulk-remove unsupported instruction/INT/SEH boundaries untuk mencapai 0 diagnostic atau startup yang tampak berhasil. Klasifikasikan apakah instruction nyata, data yang disalahartikan sebagai code, atau path unsupported yang benar-benar terjangkau.

## 28. Audit APK dan pemasangan tanpa menjalankan game

Gunakan laporan install versi terakhir sebagai contoh konkret:

```text
work/ui0535/install.py
work/ui0535/installation.json
```

Script ini khusus versi/path tertentu. **Jangan menjalankannya tanpa membaca dan menyesuaikan paths/version expectation.** File tersebut adalah artefak task, bukan installer generik.

Pemeriksaan yang dilakukan selama update UI:

1. Build summary signature/alignment PASS.
2. Perbandingan ZIP members dan hash setiap entry dengan baseline.
3. Perubahan yang diizinkan hanya manifest/DEX/resources sesuai task; native libraries, shader, driver dan aset lain harus sama untuk UI-only update.
4. Hash source launcher/kamera tertentu dicek untuk memastikan tidak ikut berubah.
5. Snapshot hash preferences, source index, `pcconfig.txt`, dan existing custom layout.
6. `adb install --no-incremental -r` memakai package/signing identity sama.
7. Verifikasi versionName/versionCode melalui metadata package.
8. Hash installed `base.apk` dibandingkan dengan build artifact.
9. Snapshot data/preferences setelah install harus cocok.
10. Tidak membuka aplikasi/game, tidak mengirim input, tidak memasang fixture lain.

Perintah instalasi langsung, hanya jika diminta/diotorisasi untuk task tersebut:

```powershell
$adb='D:/AndroidTools/android-sdk/platform-tools/adb.exe'
& $adb devices -l
& $adb -s 3e752536 install --no-incremental -r 'work/android-ui-0535/transformers-dxvk-universal-debug.apk'
```

Jangan melakukan `adb uninstall`, `pm clear`, atau mengganti package untuk mempermudah update. Jangan force-stop proses game yang sedang menyimpan tanpa konteks/otorisasi yang sesuai.

Jika ADB tidak menemukan perangkat, simpan APK dan laporkan instalasi pending. Jika install dialog dibatalkan Android/pengguna, jangan menganggapnya PASS dan jangan mencoba melewati proteksi sistem tanpa instruksi.

## 29. Data privat yang harus dipertahankan

App-private root biasanya:

```text
/data/user/0/io.github.transformers.recompiled.diagnostics/
```

Lokasi penting:

| Path relatif app-private | Isi |
| --- | --- |
| `shared_prefs/graphics.xml` | Render scale, texture/effects, FPS preference |
| `shared_prefs/game_sources.xml` | EXE URI |
| `files/direct-sources.json` | Folder tree URI dan indeks asset URIs |
| `shared_prefs/gpu_driver.xml` | Driver mode/path |
| `shared_prefs/touch_layouts.xml` | Saved Menu/Robot/Car geometry, termasuk FPS bila dikustomisasi |
| `files/game/documents/ACTIVISION/Transformers/pcconfig.txt` | Original-format game settings |
| `files/game/documents/ACTIVISION/Transformers/` | Save/config game |
| `files/drivers/library/` | ZIP driver pengguna dan extracted packages |
| `files/game/shader-cache/` | Cache yang dipasang launcher/runtime |
| `files/dxvk-runtime.log` | Log runtime historis; jangan diasumsikan selalu run terakhir |

Tidak boleh menghapus seluruh `files/game` karena ingin mengganti link akses sumber. Di dalamnya terdapat dokumen/save/config yang harus hidup lebih lama dari sesi.

Jangan membundel save pengguna ke APK atau source release. Jangan mengunggah user-specific URIs, logs pribadi, signing key, atau original game archives tanpa tugas publikasi yang jelas.

## 30. Pengujian yang sudah tersedia

Perintah host suite dari R:

```powershell
$env:JAVA_HOME='D:/AndroidTools/jdk17/jdk-17.0.20.1+1'
python -m unittest discover -s tests -v
```

Terakhir full suite dijalankan pada pekerjaan **0.5.34**:

```text
Ran 208 tests
OK (skipped=1)
```

Artinya **207 lulus, satu opsional dilewati**. Log: `work/ui0534/host-tests.log`.

Pada 0.5.35 hanya viewport/sidebar styling berubah. Paket dikompilasi dan dipasang dengan audit; full suite tidak dijalankan ulang. `work/ui0535/installation.json` secara tepat menyebutnya `previous_host_regressions`, bukan hasil tes baru.

Tes relevan:

- `tests/test_touch_controls.py`: 19 kasus production Java controller, key/mouse ownership, input, transform, cancel, automatic profiles, sidebar callback dan posisi default.
- `tests/test_control_layout.py`: tujuh kasus saved geometry, bounds, invalid values, independent profiles/reset, moved/resized hit bounds, sidebar release/cancel, FPS noninteractive geometry.
- `tests/test_camera_drag.py`: pointer/movement/cancel semantics.
- `tests/test_startup_input.py`: DirectInput state/axis semantics.
- Tes x87, runtime hotpaths, D3D9 bridge, shaders, GPU importer, settings, provenance dan lainnya berada di `tests/`.

Tes target saja bisa memakai discovery pattern, misalnya:

```powershell
python -m unittest discover -s tests -p 'test_control_layout.py' -v
python -m unittest discover -s tests -p 'test_touch_controls.py' -v
```

`TF_TEST_ANDROID_LINK=1` mengaktifkan fixture link opsional dan memerlukan dependency/environment yang sesuai. Jangan menyebut fixture skipped sebagai pass.

### Tool perangkat yang tidak boleh otomatis dijalankan

- `tools/test_direct_sources_android.py` memasang APK fixture.
- `tools/test_cross_uid_sources_android.py` memasang dua APK fixture.
- `tools/test_gpu_drivers_android.py` memasang dan membuka launcher fixture.
- Native probe CLI dapat mengeksekusi kode native, sebagian pilihan menjalankan GPU/audio dan sebagian memasuki game.

Keberadaan tool tersebut bukan otorisasi. Preferensi pengguna saat ini adalah tidak ada APK uji tambahan atau debug perangkat otomatis. Pengguna melakukan tes nyata sendiri.

## 31. Bukti gameplay dan batas klaim

Gunakan kategori bukti yang terpisah:

| Kategori | Apa yang dibuktikan |
| --- | --- |
| Source/static analysis | Struktur, alamat, kontrak yang dibaca dari kode; bukan keberhasilan runtime |
| Host unit/JVM/native fixture | Kasus yang diuji fixture; bukan seluruh game di Android |
| Native headless device probe historis | Boundary native tertentu; belum tentu surface/gameplay |
| APK build/sign/alignment | Paket dapat dibangun/ditandatangani; bukan tampilan UI benar |
| Install + installed hash | Paket tertentu benar-benar dipasang |
| User-operated gameplay report | Kondisi/scene yang benar-benar dicoba pengguna |

User reports yang penting:

- Gameplay awal sudah berjalan; jangan menyatakan proyek baru sekadar prototype mapper.
- Dynamic controls dinilai bekerja baik dan tidak ingin mekaniknya diubah pada desain ulang.
- Launcher sekarang disukai pengguna.
- Editor/sidebar butuh perbaikan compact/full-height berdasarkan pengalaman pengguna.
- Hasil visual scroll 0.5.35 belum dikonfirmasi setelah install.

Tidak ada gameplay/debug session yang dijalankan oleh assistant pada update UI terakhir. Jangan mengubah kalimat “installed and ready for your testing” menjadi “tested in gameplay” dalam handoff berikutnya.

## 32. Riwayat perubahan yang berguna untuk orientasi

| Versi | Perubahan utama | Bukti utama |
| --- | --- | --- |
| 0.5.6 | BMP encoder/readback untuk boundary mission-completion crash | `docs/MISSION_CRASH.md`, `work/crash056/` |
| 0.5.14 | Fullscreen, background/session resume, foreground/surface gate | `docs/ANDROID_LIFECYCLE.md`, `work/lifecycle0514/` |
| 0.5.16 | 14 shader Decepticon ditambahkan; total cache 343 | `docs/DECEPTICON_CRASH.md`, `work/decepticon0516/` |
| 0.5.17 | Back menu -> VK8 Backspace | `work/android-controls-0517/` |
| 0.5.19 | CPU x87 occupancy/COM registry hotpath optimizations | `docs/PERFORMANCE-0519.md`, `work/performance0519/` |
| 0.5.20 | Saving UI memilih Menu, bukan Hidden | `work/controls0520/` |
| 0.5.21 | Loading Enter prompt memilih Menu sebelum combat actor lama | `work/controls0521/` |
| 0.5.22 | Direct SAF sources, awalnya masih memiliki reopen/EACCES bug | `work/direct0522/` |
| 0.5.23 | Add GPU driver ZIP + pilihan automatic/system/custom | `work/drivers0523/` |
| 0.5.24 | Descriptor adapter fix; native runtimes terakhir dibangun ulang | `work/sources0524/` |
| 0.5.25 | Original EXE icon 32px; kemudian ditolak karena blur | `work/icon0525/` |
| 0.5.26 | Autobot PNG HD 2048px | `work/icon0526/` |
| 0.5.27 | Simbol dalam launcher dan judul banner lebih jelas | `work/launcher0527/` |
| 0.5.28 | Ganti gamepad banner dengan Autobot HD | `work/launcher0528/` |
| 0.5.29 | Full English launcher dan related app-owned messages | `work/english0529/` |
| 0.5.30 | Game Files dipindahkan langsung di bawah Play | `work/launcher0530/` |
| 0.5.31 | Footer `Port By Sinn` | `work/footer0531/` |
| 0.5.32 | Rounded in-game artwork; input mechanics tetap | `work/hud0532/` |
| 0.5.33 | Sidebar + per-profile drag/size editor, Save/Reset/Cancel | `work/ui0533/` |
| 0.5.34 | Compact tools, animations, FPS movable, full-height sheet | `work/ui0534/` |
| 0.5.35 | Full-height scroll viewport/track, padding dipindahkan ke menu child | `work/ui0535/` |

Simpan APK baseline yang relevan. Jangan membersihkan semua versi lama di `work` tanpa memahami bahwa sebagian merupakan bukti dan rollback artifacts.

## 33. Panduan diagnosis untuk AI berikutnya

### Jika UI tidak muncul pada prompt Enter/save

Mulai dari decoder profil native, bukan menggambar lebih banyak tombol setiap saat. Bandingkan kondisi saving/loading/menu flags untuk supported EXE. Pertahankan MENU priority atas generic pause/inhibit.

### Jika custom positions hilang

Periksa `touch_layouts` keys dan format records. Bedakan draft, Save, Reset, Cancel. Pastikan `layoutControls()` selalu menerapkan saved geometry sesudah membangun defaults, dan native profile polling tidak menimpa profil editor.

### Jika FPS preview pindah tetapi FPS asli tidak

Periksa `setFpsCounter`, `updateFpsPosition`, synthetic HUD control, scaled margins dan profile restoration. Jangan hanya mengganti sample Canvas.

### Jika editor memicu serangan/transform

Periksa jalur `onTouchEvent`: editing harus dialihkan ke editorTouch sebelum input gameplay. Hardware keys harus ditahan ketika sidebar/editor terbuka. Release held input sebelum membuka overlay.

### Jika game berjalan lagi ketika editor masih terbuka

Cari semua pemanggilan `nativeSetForeground` dan audio-focus callbacks. Active state harus mempertimbangkan UI pause owner, resumed, dan focus. Completion animasi sidebar tidak boleh melepaskan pause ketika editor sudah aktif.

### Jika sidebar masih terlihat terpotong

Jangan menyimpulkan hanya dari background penuh. Bedakan ukuran sheet, ukuran viewport ScrollView, clipping/padding, fading edges, scrollbar track, dan thumb. Pada baseline 0.5.35 viewport zero-padding dan content child menerima safe insets. Penilaian visual di perangkat tetap memerlukan laporan pengguna/otorisasi yang tepat.

### Jika EACCES muncul pada game data

Periksa descriptor mount dan wrapper path; jangan kembali membuka token `/proc/self/fd` sebagai file path. Bedakan permission failure sumber dari driver-load failure.

### Jika game crash di HLSL compile

Cari exact missing shader request dan frozen cache. Jangan mengganti shader atau mematikan fitur secara asal. Tool kompilasi offline ada; akses perangkat/capture baru harus sesuai instruksi pengguna.

### Jika native build provenance mismatch

Cari layer yang berubah: generated core, runtime bridge, backend, atau resource packaging. Jangan menyesuaikan hash metadata agar archive lama diterima.

### Jika install gagal

Periksa perangkat online, output install, storage, signature continuity, dan versionCode. Jangan menghapus data/uninstall sebagai jalan pintas. Error `device not found` tidak diperbaiki dengan rebuilding APK.

### Jika ingin menaikkan FPS

Bandingkan scene/resolution yang sama. Pisahkan presenting-thread CPU, asynchronous GPU work, thermal/pacing, dan frame-clock behavior. Helper microbenchmark tidak membuktikan target 30/60 FPS tercapai.

## 34. Dokumen pendalaman dan usia informasinya

Path berikut relatif terhadap G agar bisa dibuka langsung dari dokumen ini:

- [Custom UI terkini](docs/CUSTOM_UI.md).
- [Control artwork](docs/CONTROL_ARTWORK.md).
- [Launcher language](docs/LAUNCHER_LANGUAGE.md).
- [Launcher symbols/banner](docs/LAUNCHER_SYMBOLS.md).
- [Launcher HD icon](docs/LAUNCHER_ICON.md).
- [Direct sources dan EACCES](docs/DIRECT_SOURCES.md).
- [GPU driver import/selection](docs/GPU_DRIVERS.md).
- [Automatic native profiles](docs/AUTO_CONTROLS.md).
- [Touch controls historis](docs/TOUCH_CONTROLS.md).
- [Lifecycle/surface resume](docs/ANDROID_LIFECYCLE.md).
- [Game settings evidence](docs/GAME_SETTINGS.md).
- [Camera input semantics](docs/CAMERA_INPUT.md).
- [D3D9 bridge](docs/GRAPHICS_BRIDGE.md).
- [Audio](docs/AUDIO.md).
- [Decepticon cache boundary](docs/DECEPTICON_CRASH.md).
- [Mission-completion BMP fix](docs/MISSION_CRASH.md).
- [Offline optimization 0.5.19](docs/PERFORMANCE-0519.md).
- [Code/data boundary classification](docs/CODE_BOUNDARIES.md).
- [x87 implementation](docs/X87.md).
- [Startup findings ledger](docs/STARTUP_READINESS.md).
- [Long historical status](docs/STATUS.md).

README/status/older Android docs tidak sepenuhnya diperbarui hingga 0.5.35. Gunakan urutan kepercayaan berikut bila ada kontradiksi:

1. Permintaan dan konfirmasi pengguna terbaru.
2. Source aktual untuk perilaku yang diimplementasikan.
3. Build/install/test report spesifik artifact yang sedang dibahas.
4. Dokumen fitur terbaru dan bukti historis yang relevan.
5. Ringkasan README atau dokumen versi awal.

Jangan menghapus dokumentasi lama hanya karena usang; sering kali ia menyimpan alasan alamat, ABI atau bugfix yang tidak terlihat dari UI.

## 35. Langkah awal AI penerus

1. Baca bagian 1–3, preferensi pengguna, dan permintaan terbaru sebelum menyentuh kode.
2. Verifikasi manifest dan latest build/install record; jangan memilih APK hanya dari nama folder yang kelihatan paling besar.
3. Pilih layer tugas: launcher/resource, control renderer, editor interaction, native profile, source I/O, graphics/audio, atau lifter.
4. Buka source dan dokumen fitur yang relevan; jangan membaca seluruh `work` secara membabi buta atau mencetak file besar.
5. Untuk UI-only change, reuse native artifacts yang sekarang. Untuk mekanik baru, tambah regresi yang menguji behavior nyata pada layer yang sesuai.
6. Pertahankan save, settings, URI source, driver selection dan layouts. Jangan masukkan data pribadi ke artefak publik.
7. Jangan menjalankan game atau fixture APK otomatis. Jika pengguna akan menguji sendiri, build/install saja dan laporkan batas pengujian dengan jujur.
8. Jika diminta install, gunakan update `-r` dengan key/package yang sama dan verifikasi hasil konkret.
9. Tulis laporan singkat: apa yang berubah, versi yang terpasang, apa yang sudah diverifikasi, dan apa yang belum dicoba di gameplay.

Untuk handoff ini, pekerjaan yang dilakukan hanya pengumpulan konteks lokal, pengecekan identitas EXE/artefak/laporan, dan penulisan dokumentasi. Tidak ada perubahan runtime/UI, build baru, instalasi baru, atau game session baru yang diperlukan.
