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

## Cara countdown memilih video

Plugin membaca sumber yang benar-benar keluar di output Program (lewat transisi), termasuk nested scene dan group, lalu mengambil media yang sedang diputar atau di-pause. Kalau ada beberapa, yang dipakai adalah video yang durasinya diketahui, tidak *looping*, sedang diputar (tidak di-pause), dan paling lama selesainya. Media lain tampil sebagai badge `+N`; arahkan kursor ke bar untuk melihat daftarnya.
Sisa waktu memperhitungkan *speed* di Media Source (misalnya 200% berarti sisa waktunya jadi separuh).

## Pasang di Windows

Butuh OBS Studio 32 versi 64-bit. File plugin dibuat oleh GitHub Actions (lihat [Build lewat CMake](#build-lewat-cmake-windows-macos-universal-linux)).

1. Buka tab **Actions** repo ini, pilih run **Build** terbaru yang berhasil, unduh artifact `scene-color-countdown-1.2.0-windows-x64`, lalu ekstrak zip-nya.
2. Tutup OBS.
3. Salin folder `obs-plugins` dan `data` dari hasil ekstrak ke folder OBS (biasanya `C:\Program Files\obs-studio`) dan gabungkan dengan folder yang sudah ada. Windows akan meminta izin administrator. Hasilnya:
   - `C:\Program Files\obs-studio\obs-plugins\64bit\scene-color-countdown.dll`
   - `C:\Program Files\obs-studio\data\obs-plugins\scene-color-countdown\locale\` (berisi `en-US.ini` dan `id-ID.ini`)
4. Buka OBS. Kalau plugin termuat, menu **Tools → Scene Color & Countdown…** muncul dan log OBS (**Help → Log Files**) berisi `[scene-color-countdown] Loaded version 1.2.0`.

Untuk menghapus: tutup OBS, lalu hapus `scene-color-countdown.dll` dan folder `data\obs-plugins\scene-color-countdown`.
Artifact `…-windows-x64-pdb` berisi simbol debug. File ini tidak perlu untuk memakai plugin, tetapi kalau ditaruh di sebelah `.dll`, crash log OBS jadi lebih mudah dibaca.

## Build macOS (tanpa Xcode)

Cukup Command Line Tools (`clang++`, `make`):

```bash
make deps      # sekali saja: header OBS 32.2.2, Qt 6.11.1 (obs-deps), SIMDe -> .deps/
make           # hasil: build/scene-color-countdown.plugin
make install   # salin ke ~/Library/Application Support/obs-studio/plugins/
```

Plugin di-link langsung ke `libobs`, `obs-frontend-api`, dan Qt milik `/Applications/OBS.app`, jadi versinya selalu sama dengan OBS yang terpasang. Restart OBS setelah `make install`. Untuk menghapus: `make uninstall`.

## Build lewat CMake (Windows, macOS universal, Linux)

Setup CMake mengikuti [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate) resmi:

- `buildspec.json`: nama dan versi plugin, plus versi dan hash SHA-256 sumber OBS 32.2.2 serta obs-deps 2026-07-15 (Qt 6.11.1).
- `CMakePresets.json`: preset `windows-x64`, `macos`, dan `ubuntu-x86_64`, plus varian `-ci` yang menjadikan warning sebagai error.
- `cmake/`: modul dari template, dengan dua perubahan: C++20, dan di Windows hasil install memakai susunan folder OBS (`obs-plugins/64bit`, `data/obs-plugins/<nama>`).

Configure pertama mengunduh sumber OBS dan obs-deps ke `.deps/`, lalu mem-build `libobs` dan `obs-frontend-api` sekali (beberapa menit).

Workflow `.github/workflows/build.yaml` berjalan saat ada push ke `main`, tag, pull request, atau dijalankan manual (**Run workflow**). Hasilnya berupa artifact:

| Artifact | Isi |
|---|---|
| `…-windows-x64` | `obs-plugins/64bit/scene-color-countdown.dll` dan `data/obs-plugins/scene-color-countdown/locale/`, siap disalin ke folder OBS |
| `…-windows-x64-pdb` | simbol debug untuk `.dll` |
| `…-macos-universal` | zip berisi `scene-color-countdown.plugin` (arm64 + x86_64, ad-hoc signed) untuk `~/Library/Application Support/obs-studio/plugins/`. Kalau macOS memblokirnya, jalankan `xattr -dr com.apple.quarantine` pada folder `.plugin` |
| `…-ubuntu-24.04-x86_64` | folder `scene-color-countdown/` untuk `~/.config/obs-studio/plugins/` |

Build manual di Windows (Visual Studio 2022 dengan workload C++, CMake 3.28+):

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
cmake --install build_x64 --config RelWithDebInfo --prefix C:\temp\scene-color-countdown
```

Tanpa `--prefix`, `cmake --install` langsung memasang ke `C:\Program Files\obs-studio`, sehingga butuh terminal administrator.

## Setelah OBS di-update

- Build macOS (`make`): ubah `OBS_VERSION`, `OBS_DEPS_VERSION`, dan hash Qt di `scripts/setup-deps.sh` (versi obs-deps tercantum di `CMakePresets.json` repo OBS untuk tiap rilis), hapus `.deps/`, lalu jalankan lagi `make deps && make && make install`.
- Build CMake: ubah versi dan hash di `buildspec.json`. Hash `prebuilt` dan `qt6` ada di bagian `dependencies` pada `CMakePresets.json` repo OBS (kunci `macos-universal` di sana sama dengan `macos` di sini). Hash `obs-studio` adalah SHA-256 arsip tag dari GitHub (`<versi>.tar.gz` untuk `macos`, `<versi>.zip` untuk `windows-x64`); kalau salah, configure berhenti dan menampilkan hash yang sebenarnya.

Versi plugin ditulis di dua tempat: `version` di `buildspec.json` dan `VERSION` di `Makefile`.

## Catatan teknis

- OBS tidak menyediakan API untuk panel Scenes maupun area Program, jadi plugin menempel ke widget internal OBS (`scenes`, `canvasEditor`, `previewLayout`, `previewTextLayout`). Kalau versi OBS berikutnya mengubah UI, fitur yang terdampak nonaktif sendiri dan menulis warning `[scene-color-countdown]` di log OBS, tanpa membuat OBS crash.
- Nama widget itu berasal dari `OBSBasic.ui`, jadi sama di macOS, Windows, dan Linux. Kode plugin tidak punya bagian khusus platform dan tidak memakai moc.
- Warna dilukis oleh delegate pembungkus; rename scene, ukuran item, dan mode grid tetap ditangani delegate asli OBS.
- Bahasa UI plugin mengikuti bahasa OBS (`data/locale/en-US.ini`, `id-ID.ini`).
