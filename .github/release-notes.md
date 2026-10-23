Plugin OBS Studio 32: warna scene dan tanda ON AIR/PREVIEW di panel Scenes, countdown video di bawah Program, serta tombol putar video di Preview (Studio Mode). Penjelasan lengkap ada di README.

Dibuat terhadap OBS 32.2.2 (Qt 6.11.1). OBS yang lebih tua dari 32.2 akan menolak plugin ini.

### Windows (x64)

1. Tutup OBS.
2. Ekstrak `scene-color-countdown-…-windows-x64.zip` ke folder OBS (biasanya `C:\Program Files\obs-studio`), sehingga folder `obs-plugins` dan `data` di dalamnya tergabung dengan yang sudah ada. Windows akan meminta izin administrator.
3. Buka OBS. Pengaturan ada di **Tools → Scene Color & Countdown…**.

`…-windows-x64-pdb.zip` berisi simbol debug dan tidak wajib dipasang.

### macOS (Apple Silicon dan Intel)

Ekstrak `scene-color-countdown-…-macos-universal.zip` ke `~/Library/Application Support/obs-studio/plugins/`, lalu restart OBS. Kalau macOS memblokirnya, jalankan `xattr -dr com.apple.quarantine` pada folder `scene-color-countdown.plugin`.
