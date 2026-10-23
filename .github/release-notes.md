An OBS Studio 32 plugin: scene colors and ON AIR/PREVIEW marks in the Scenes dock, a video countdown below Program, and play controls for the video in Preview (Studio Mode).

Requires OBS Studio 32.2 or newer (tested on 32.2.2). The full guide, including what to do if the plugin doesn't show up, is in the [README → Installation](https://github.com/le-syafian/plugin-obs#installation).

### Windows 10/11 (64-bit)

1. Close OBS.
2. Download `scene-color-countdown-…-windows-x64.zip`, then right-click it → **Extract All…**. It contains two folders: `obs-plugins` and `data`.
3. Copy those two folders (not the folder that wraps them) into the OBS folder, usually `C:\Program Files\obs-studio`, and click **Continue** when asked for administrator permission. The result is `obs-studio\obs-plugins\64bit\scene-color-countdown.dll` and `obs-studio\data\obs-plugins\scene-color-countdown\locale\`.
4. Start OBS. If the plugin loaded, **Tools → Scene Color & Countdown…** appears.

`…-windows-x64-pdb.zip` only holds debug symbols and doesn't need to be installed.

### macOS 13+ (Apple Silicon and Intel)

1. Quit OBS.
2. Extract `scene-color-countdown-…-macos-universal.zip` to get `scene-color-countdown.plugin`.
3. Move it into `~/Library/Application Support/obs-studio/plugins/`. In Finder, use **Go → Go to Folder…**, and create the `plugins` folder if it doesn't exist.
4. Open Terminal and run:
   ```bash
   xattr -dr com.apple.quarantine ~/Library/Application\ Support/obs-studio/plugins/scene-color-countdown.plugin
   ```
5. Start OBS. If the plugin loaded, **Tools → Scene Color & Countdown…** appears.
