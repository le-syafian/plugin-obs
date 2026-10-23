# Scene Color & Program Countdown

Plugin OBS Studio 32 untuk macOS dan Windows (diuji di OBS 32.2.2, macOS Apple Silicon; build Windows x64 dibuat oleh GitHub Actions dan belum diuji langsung di Windows) dengan empat fitur:

1. **Warna scene di panel Scenes**: klik kanan scene → **Scene Color / Warna Scene** → pilih warna preset, *Custom Color…*, atau *Remove Color*.
   Warna disimpan di *private settings* scene, jadi ikut tersimpan di scene collection.
2. **Tanda ON AIR / PREVIEW di panel Scenes**: scene yang tayang di Program dapat bingkai merah dan **titik merah berkedip**; scene di Preview (Studio Mode) dapat bingkai hijau dan **simbol mata**. Arahkan kursor ke scene untuk melihat artinya.
   - Mengikuti output yang sebenarnya, jadi saat transisi berjalan scene asal dan tujuan sama-sama ON AIR.
   - Kalau scene yang sama ada di Program dan Preview, keduanya tampil (bingkai merah + bingkai dalam hijau, mata + titik).
3. **Countdown di bawah Program**: bar yang menampilkan nama video, progress, waktu berjalan/durasi, dan **sisa waktu** video yang sedang tayang di Program.
   - Mode biasa: bar ada di bawah area preview (yang memang adalah Program). Bar tetap tampil walaupun preview dimatikan.
   - Studio Mode: bar ada tepat di bawah tampilan Program.
   - Kuning saat sisa ≤ 10 detik, merah (berkedip) saat ≤ 5 detik. Di dua zona ini sisa waktu tampil sampai persepuluh detik.
4. **Putar video di Preview (Studio Mode)**: bar di bawah Preview dengan tombol Play/Pause, putar ulang dari awal, dan Stop, plus countdown. Klik atau geser progress bar untuk seek.
   - Di OBS, video di scene Preview biasanya belum jalan sampai on air; tombol Play memutarnya dari awal supaya bisa dicek dulu.
   - Kalau video yang sama sedang ON AIR (misalnya scene Preview = scene Program), kontrolnya dikunci dan muncul titik merah berkedip, supaya output live tidak ikut terganggu.

Pengaturan ada di **Tools → Scene Color & Countdown…**: ambang peringatan/kritis, kedip, tampilan waktu berjalan, kontrol Preview, warna ON AIR/PREVIEW, gaya tanda (bingkai + simbol, atau seluruh baris diwarnai), dan gaya warna scene (seluruh baris atau strip di kiri).

## Cara pasang

### Sebelum mulai

- Butuh **OBS Studio 32.2 atau lebih baru** (diuji di 32.2.2). Cek versinya di **Help → About** (Windows) atau **OBS → About OBS** (macOS). OBS yang lebih lama akan menolak plugin ini.
- Unduh file plugin dari halaman [**Releases**](https://github.com/le-syafian/plugin-obs/releases/latest):

  | Sistem | File |
  |---|---|
  | Windows 10/11 (64-bit) | `scene-color-countdown-<versi>-windows-x64.zip` |
  | macOS 13 atau lebih baru (Apple Silicon dan Intel) | `scene-color-countdown-<versi>-macos-universal.zip` |

  `…-windows-x64-pdb.zip` hanya berisi simbol debug untuk membaca crash log, jadi tidak perlu dipasang. Build terbaru yang belum dirilis ada sebagai artifact di tab **Actions**.

### Windows

1. **Tutup OBS.**
2. Klik kanan zip yang sudah diunduh → **Extract All…** → **Extract**. Hasil ekstraknya berisi dua folder: `obs-plugins` dan `data`.
3. Buka folder instalasi OBS, biasanya `C:\Program Files\obs-studio`. Kalau tidak ada di sana, klik kanan shortcut OBS → **Open file location**, lalu naik dua tingkat dari folder `bin\64bit`.
4. Salin folder `obs-plugins` dan `data` ke folder `obs-studio` itu. Yang disalin adalah kedua folder tersebut, bukan folder pembungkusnya (`scene-color-countdown-<versi>-windows-x64`). Windows akan meminta izin administrator; klik **Continue**. Saat update, pilih **Replace** kalau ditanya soal file yang sudah ada.
   Hasil akhirnya harus seperti ini:
   ```
   C:\Program Files\obs-studio\obs-plugins\64bit\scene-color-countdown.dll
   C:\Program Files\obs-studio\data\obs-plugins\scene-color-countdown\locale\en-US.ini
   C:\Program Files\obs-studio\data\obs-plugins\scene-color-countdown\locale\id-ID.ini
   ```
5. Buka OBS, lalu [cek hasil pemasangan](#cek-hasil-pemasangan).

**Update ke versi baru:** ulangi langkah di atas; file lama akan ditimpa.
**Menghapus:** tutup OBS, lalu hapus `obs-plugins\64bit\scene-color-countdown.dll` dan folder `data\obs-plugins\scene-color-countdown`.

### macOS

1. **Tutup OBS.**
2. Klik dua kali zip yang sudah diunduh sampai muncul `scene-color-countdown.plugin`. Safari biasanya sudah mengekstraknya otomatis.
3. Di Finder, pilih **Go → Go to Folder…** (⇧⌘G), ketik `~/Library/Application Support/obs-studio/`, lalu tekan Enter. Kalau di sana belum ada folder `plugins`, buat folder baru dengan nama itu.
4. Pindahkan `scene-color-countdown.plugin` ke dalam folder `plugins`.
5. macOS menandai file dari internet dan bisa memblokirnya saat OBS memuat plugin. Buka **Terminal**, lalu jalankan:
   ```bash
   xattr -dr com.apple.quarantine ~/Library/Application\ Support/obs-studio/plugins/scene-color-countdown.plugin
   ```
6. Buka OBS, lalu [cek hasil pemasangan](#cek-hasil-pemasangan).

**Update ke versi baru:** hapus `scene-color-countdown.plugin` yang lama, lalu ulangi langkah di atas.
**Menghapus:** tutup OBS, lalu hapus `scene-color-countdown.plugin` dari folder `plugins`.
Untuk membangun sendiri dari kode sumber, lihat [Build macOS](#build-macos-tanpa-xcode); `make install` memasang ke folder yang sama.

### Cek hasil pemasangan

- Menu **Tools** punya **Scene Color & Countdown…**, dan klik kanan sebuah scene di panel Scenes memunculkan submenu **Scene Color**.
- Di log OBS (**Help → Log Files → View Current Log**), cari `scene-color-countdown`. Baris `[scene-color-countdown] Loaded version 1.2.0` berarti plugin sudah termuat.

### Kalau plugin tidak muncul

| Yang terlihat | Penyebab dan solusi |
|---|---|
| Tidak ada baris `scene-color-countdown` sama sekali di log | File tidak berada di lokasi yang benar. Di Windows, cocokkan dengan hasil akhir di langkah 4; biasanya folder pembungkus ikut tersalin. Di macOS, `scene-color-countdown.plugin` harus berada langsung di dalam folder `plugins`. |
| `compiled with newer libobs 32.2` | OBS terlalu lama. Update OBS ke 32.2 atau lebih baru. |
| Windows: `LoadLibrary failed … (126)` atau `(127)` | Ada file yang kurang, atau OBS terlalu lama untuk versi Qt yang dipakai plugin. Update OBS, lalu pasang ulang. |
| macOS: "cannot be opened because the developer cannot be verified" | Jalankan perintah `xattr` di langkah 5, atau buka **System Settings → Privacy & Security** lalu pilih **Allow Anyway**. |
| Plugin termuat, tetapi log berisi warning seperti `Scenes list not found` | Tampilan OBS versi itu berubah, sehingga fitur yang terdampak menonaktifkan diri. Lihat [Catatan teknis](#catatan-teknis). |

## Cara countdown memilih video

Plugin membaca sumber yang benar-benar keluar di output Program (lewat transisi), termasuk nested scene dan group, lalu mengambil media yang sedang diputar atau di-pause. Kalau ada beberapa, yang dipakai adalah video yang durasinya diketahui, tidak *looping*, sedang diputar (tidak di-pause), dan paling lama selesainya. Media lain tampil sebagai badge `+N`; arahkan kursor ke bar untuk melihat daftarnya.
Sisa waktu memperhitungkan *speed* di Media Source (misalnya 200% berarti sisa waktunya jadi separuh).

## Build macOS (tanpa Xcode)

Cukup Command Line Tools (`clang++`, `make`):

```bash
make deps      # sekali saja: header OBS 32.2.2, Qt 6.11.1 (obs-deps), SIMDe -> .deps/
make           # hasil: build/scene-color-countdown.plugin
make install   # salin ke ~/Library/Application Support/obs-studio/plugins/
```

Plugin di-link langsung ke `libobs`, `obs-frontend-api`, dan Qt milik `/Applications/OBS.app`, jadi versinya selalu sama dengan OBS yang terpasang. Restart OBS setelah `make install`. Untuk menghapus: `make uninstall`.

## Build lewat CMake (Windows dan macOS universal)

Setup CMake mengikuti [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate) resmi:

- `buildspec.json`: nama dan versi plugin, plus versi dan hash SHA-256 sumber OBS 32.2.2 serta obs-deps 2026-07-15 (Qt 6.11.1).
- `CMakePresets.json`: preset `windows-x64` dan `macos`, plus varian `-ci` yang menjadikan warning sebagai error. Preset `ubuntu-x86_64` dari template masih ada, tetapi Linux tidak dibuild di CI.
- `cmake/`: modul dari template, dengan dua perubahan: C++20, dan di Windows hasil install memakai susunan folder OBS (`obs-plugins/64bit`, `data/obs-plugins/<nama>`).

Configure pertama mengunduh sumber OBS dan obs-deps ke `.deps/`, lalu mem-build `libobs` dan `obs-frontend-api` sekali (beberapa menit).

Workflow `.github/workflows/build.yaml` berjalan saat ada push ke `main`, tag, pull request, atau dijalankan manual (**Run workflow**). Hasilnya berupa artifact:

| Artifact | Isi |
|---|---|
| `…-windows-x64` | `obs-plugins/64bit/scene-color-countdown.dll` dan `data/obs-plugins/scene-color-countdown/locale/`, siap disalin ke folder OBS |
| `…-windows-x64-pdb` | simbol debug untuk `.dll` |
| `…-macos-universal` | zip berisi `scene-color-countdown.plugin` (arm64 + x86_64, ad-hoc signed) untuk `~/Library/Application Support/obs-studio/plugins/`. Kalau macOS memblokirnya, jalankan `xattr -dr com.apple.quarantine` pada folder `.plugin` |

Build manual di Windows (Visual Studio 2022 dengan workload C++, CMake 3.28+):

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
cmake --install build_x64 --config RelWithDebInfo --prefix C:\temp\scene-color-countdown
```

Tanpa `--prefix`, `cmake --install` langsung memasang ke `C:\Program Files\obs-studio`, sehingga butuh terminal administrator.

## Membuat rilis

1. Naikkan versi di `buildspec.json` dan `Makefile`, lalu commit dan push.
2. Buat tag dengan nomor versi yang sama, lalu push tag-nya:

   ```bash
   git tag -a 1.2.0 -m "Scene Color & Program Countdown 1.2.0"
   git push origin 1.2.0
   ```

Workflow lalu mem-build Windows dan macOS, kemudian membuat GitHub Release berisi `…-windows-x64.zip`, `…-windows-x64-pdb.zip`, dan `…-macos-universal.zip`. Catatan rilisnya diambil dari `.github/release-notes.md`. Kalau nama tag tidak sama dengan versi di `buildspec.json`, rilis tidak dibuat.

## Setelah OBS di-update

- Build macOS (`make`): ubah `OBS_VERSION`, `OBS_DEPS_VERSION`, dan hash Qt di `scripts/setup-deps.sh` (versi obs-deps tercantum di `CMakePresets.json` repo OBS untuk tiap rilis), hapus `.deps/`, lalu jalankan lagi `make deps && make && make install`.
- Build CMake: ubah versi dan hash di `buildspec.json`. Hash `prebuilt` dan `qt6` ada di bagian `dependencies` pada `CMakePresets.json` repo OBS (kunci `macos-universal` di sana sama dengan `macos` di sini). Hash `obs-studio` adalah SHA-256 arsip tag dari GitHub (`<versi>.tar.gz` untuk `macos`, `<versi>.zip` untuk `windows-x64`); kalau salah, configure berhenti dan menampilkan hash yang sebenarnya.

Versi plugin ditulis di dua tempat: `version` di `buildspec.json` dan `VERSION` di `Makefile`.

## Catatan teknis

- OBS tidak menyediakan API untuk panel Scenes maupun area Program, jadi plugin menempel ke widget internal OBS (`scenes`, `canvasEditor`, `previewLayout`, `previewTextLayout`). Kalau versi OBS berikutnya mengubah UI, fitur yang terdampak nonaktif sendiri dan menulis warning `[scene-color-countdown]` di log OBS, tanpa membuat OBS crash.
- Nama widget itu berasal dari `OBSBasic.ui`, jadi sama di macOS, Windows, dan Linux. Kode plugin tidak punya bagian khusus platform dan tidak memakai moc.
- Warna dilukis oleh delegate pembungkus; rename scene, ukuran item, dan mode grid tetap ditangani delegate asli OBS.
- Bahasa UI plugin mengikuti bahasa OBS (`data/locale/en-US.ini`, `id-ID.ini`).
