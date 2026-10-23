Plugin OBS Studio 32: warna scene dan tanda ON AIR/PREVIEW di panel Scenes, countdown video di bawah Program, serta tombol putar video di Preview (Studio Mode).

Butuh OBS Studio 32.2 atau lebih baru (diuji di 32.2.2). Panduan lengkap, termasuk solusi kalau plugin tidak muncul, ada di [README → Cara pasang](https://github.com/le-syafian/plugin-obs#cara-pasang).

### Windows 10/11 (64-bit)

1. Tutup OBS.
2. Unduh `scene-color-countdown-…-windows-x64.zip`, klik kanan → **Extract All…**. Isinya dua folder: `obs-plugins` dan `data`.
3. Salin kedua folder itu (bukan folder pembungkusnya) ke folder OBS, biasanya `C:\Program Files\obs-studio`, lalu klik **Continue** saat diminta izin administrator. Hasilnya `obs-studio\obs-plugins\64bit\scene-color-countdown.dll` dan `obs-studio\data\obs-plugins\scene-color-countdown\locale\`.
4. Buka OBS. Kalau plugin termuat, menu **Tools → Scene Color & Countdown…** muncul.

`…-windows-x64-pdb.zip` hanya berisi simbol debug, jadi tidak perlu dipasang.

### macOS 13+ (Apple Silicon dan Intel)

1. Tutup OBS.
2. Ekstrak `scene-color-countdown-…-macos-universal.zip` sampai muncul `scene-color-countdown.plugin`.
3. Pindahkan ke `~/Library/Application Support/obs-studio/plugins/`. Di Finder pakai **Go → Go to Folder…**, dan buat folder `plugins` kalau belum ada.
4. Buka Terminal, lalu jalankan:
   ```bash
   xattr -dr com.apple.quarantine ~/Library/Application\ Support/obs-studio/plugins/scene-color-countdown.plugin
   ```
5. Buka OBS. Kalau plugin termuat, menu **Tools → Scene Color & Countdown…** muncul.
