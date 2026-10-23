# Scene Color & Program Countdown

Plugin OBS Studio (diuji di OBS 32.2.2, macOS Apple Silicon) dengan dua fitur:

1. **Warna scene di panel Scenes**: klik kanan scene → **Scene Color / Warna Scene** → pilih warna preset, *Custom Color…*, atau *Remove Color*.
   Warna disimpan di *private settings* scene, jadi ikut tersimpan di scene collection.
2. **Countdown di bawah Program**: bar yang menampilkan nama video, progress, waktu berjalan/durasi, dan **sisa waktu** video yang sedang tayang di Program.
   - Mode biasa: bar ada di bawah area preview (yang memang adalah Program). Bar tetap tampil walaupun preview dimatikan.
   - Studio Mode: bar ada tepat di bawah tampilan Program.
   - Kuning saat sisa ≤ 10 detik, merah (berkedip) saat ≤ 5 detik. Di dua zona ini sisa waktu tampil sampai persepuluh detik.

Pengaturan ada di **Tools → Scene Color & Countdown…**: ambang peringatan/kritis, kedip, tampilan waktu berjalan, dan gaya warna scene (seluruh baris atau strip di kiri).

## Cara countdown memilih video

Plugin membaca sumber yang benar-benar keluar di output Program (lewat transisi), termasuk nested scene dan group, lalu mengambil media yang sedang diputar atau di-pause. Kalau ada beberapa, yang dipakai adalah video yang durasinya diketahui, tidak *looping*, sedang diputar (tidak di-pause), dan paling lama selesainya. Media lain tampil sebagai badge `+N`; arahkan kursor ke bar untuk melihat daftarnya.
Sisa waktu memperhitungkan *speed* di Media Source (misalnya 200% berarti sisa waktunya jadi separuh).

## Build (macOS, tanpa Xcode)

Cukup Command Line Tools (`clang++`, `make`):

```bash
make deps      # sekali saja: header OBS 32.2.2, Qt 6.11.1 (obs-deps), SIMDe -> .deps/
make           # hasil: build/scene-color-countdown.plugin
make install   # salin ke ~/Library/Application Support/obs-studio/plugins/
```

Plugin di-link langsung ke `libobs`, `obs-frontend-api`, dan Qt milik `/Applications/OBS.app`, jadi versinya selalu sama dengan OBS yang terpasang. Restart OBS setelah `make install`. Untuk menghapus: `make uninstall`.

### Setelah OBS di-update

Ubah `OBS_VERSION`, `OBS_DEPS_VERSION`, dan hash Qt di `scripts/setup-deps.sh` (versi obs-deps tercantum di `CMakePresets.json` repo OBS untuk tiap rilis), hapus `.deps/`, lalu jalankan lagi `make deps && make && make install`.

## Catatan teknis

- OBS tidak menyediakan API untuk panel Scenes maupun area Program, jadi plugin menempel ke widget internal OBS (`scenes`, `canvasEditor`, `previewLayout`). Kalau versi OBS berikutnya mengubah UI, fitur yang terdampak nonaktif sendiri dan menulis warning `[scene-color-countdown]` di log OBS, tanpa membuat OBS crash.
- Warna dilukis oleh delegate pembungkus; rename scene, ukuran item, dan mode grid tetap ditangani delegate asli OBS.
- Bahasa UI plugin mengikuti bahasa OBS (`data/locale/en-US.ini`, `id-ID.ini`).
