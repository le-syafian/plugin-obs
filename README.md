# Scene Color & Program Countdown

An OBS Studio 32 plugin for macOS and Windows (tested on OBS 32.2.2 on Apple Silicon macOS; the Windows x64 build is made by GitHub Actions and has not been tested on Windows yet) with four features:

1. **Scene colors in the Scenes dock**: right-click a scene → **Scene Color** → pick a preset color, *Custom Color…*, or *Remove Color*.
   The color is stored in the scene's *private settings*, so it is saved with the scene collection.
2. **ON AIR / PREVIEW marks in the Scenes dock**: the scene showing in Program gets a red frame and a **blinking red dot**; the scene in Preview (Studio Mode) gets a green frame and an **eye symbol**. Hover over a scene to see what the marks mean.
   - The marks follow the actual output, so during a transition both the outgoing and the incoming scene are ON AIR.
   - If the same scene is in both Program and Preview, both marks show (red frame with a green inner frame, eye and dot).
3. **Countdown below Program**: a bar with the video's name, progress, elapsed/total time, and the **remaining time** of the video playing in Program.
   - Regular mode: the bar sits under the preview area (which is Program in this mode) and stays visible even when the preview is disabled.
   - Studio Mode: the bar sits right under the Program display.
   - Yellow with ≤ 10 seconds left, red (blinking) with ≤ 5 seconds left. In these two zones the remaining time shows tenths of a second.
4. **Play the video in Preview (Studio Mode)**: a bar under Preview with Play/Pause, restart-from-the-beginning, and Stop buttons, plus a countdown. Click or drag the progress bar to seek.
   - In OBS, a video in the Preview scene usually doesn't play until it goes on air; Play starts it from the beginning so you can check it first.
   - If the same video is ON AIR (for example when the Preview scene is also the Program scene), the controls are locked and a blinking red dot appears, so the live output is never disturbed.

Settings are under **Tools → Scene Color & Countdown…**: warning/critical thresholds, blinking, elapsed time display, Preview controls, ON AIR/PREVIEW colors, mark style (frame + symbol, or fill the whole row), and scene color style (fill the whole row, or a stripe on the left).
The plugin's UI follows OBS's language (English or Indonesian); this README uses the English labels.

## Installation

### Before you start

- You need **OBS Studio 32.2 or newer** (tested on 32.2.2). Check the version under **Help → About** (Windows) or **OBS → About OBS** (macOS). Older OBS versions refuse to load the plugin.
- Download the plugin from the [**Releases**](https://github.com/le-syafian/plugin-obs/releases/latest) page:

  | System | File |
  |---|---|
  | Windows 10/11 (64-bit) | `scene-color-countdown-<version>-windows-x64.zip` |
  | macOS 13 or newer (Apple Silicon and Intel) | `scene-color-countdown-<version>-macos-universal.zip` |

  `…-windows-x64-pdb.zip` only holds debug symbols for reading crash logs, so you don't need to install it. The latest unreleased build is available as an artifact in the **Actions** tab.

### Windows

1. **Close OBS.**
2. Right-click the downloaded zip → **Extract All…** → **Extract**. The extracted folder contains two folders: `obs-plugins` and `data`.
3. Open the OBS install folder, usually `C:\Program Files\obs-studio`. If it isn't there, right-click the OBS shortcut → **Open file location**, then go up two levels from the `bin\64bit` folder.
4. Copy the `obs-plugins` and `data` folders into that `obs-studio` folder. Copy those two folders themselves, not the folder that wraps them (`scene-color-countdown-<version>-windows-x64`). Windows asks for administrator permission; click **Continue**. When updating, choose **Replace** if asked about existing files.
   The result must look like this:
   ```
   C:\Program Files\obs-studio\obs-plugins\64bit\scene-color-countdown.dll
   C:\Program Files\obs-studio\data\obs-plugins\scene-color-countdown\locale\en-US.ini
   C:\Program Files\obs-studio\data\obs-plugins\scene-color-countdown\locale\id-ID.ini
   ```
5. Start OBS, then [check the installation](#checking-the-installation).

**Updating:** repeat the steps above; the old files are overwritten.
**Uninstalling:** close OBS, then delete `obs-plugins\64bit\scene-color-countdown.dll` and the `data\obs-plugins\scene-color-countdown` folder.

### macOS

1. **Quit OBS.**
2. Double-click the downloaded zip to get `scene-color-countdown.plugin`. Safari usually extracts it automatically.
3. In Finder, choose **Go → Go to Folder…** (⇧⌘G), type `~/Library/Application Support/obs-studio/`, and press Return. If there is no `plugins` folder there, create a new folder with that name.
4. Move `scene-color-countdown.plugin` into the `plugins` folder.
5. macOS flags files downloaded from the internet and can block them when OBS loads the plugin. Open **Terminal** and run:
   ```bash
   xattr -dr com.apple.quarantine ~/Library/Application\ Support/obs-studio/plugins/scene-color-countdown.plugin
   ```
6. Start OBS, then [check the installation](#checking-the-installation).

**Updating:** delete the old `scene-color-countdown.plugin`, then repeat the steps above.
**Uninstalling:** quit OBS, then delete `scene-color-countdown.plugin` from the `plugins` folder.
To build it yourself from source, see [Building on macOS](#building-on-macos-no-xcode); `make install` installs into the same folder.

### Checking the installation

- The **Tools** menu has **Scene Color & Countdown…**, and right-clicking a scene in the Scenes dock shows a **Scene Color** submenu.
- In the OBS log (**Help → Log Files → View Current Log**), search for `scene-color-countdown`. The line `[scene-color-countdown] Loaded version 1.2.0` means the plugin loaded.

### If the plugin does not show up

| What you see | Cause and fix |
|---|---|
| No `scene-color-countdown` line in the log at all | The files are not in the right place. On Windows, compare with the result in step 4; usually the wrapping folder was copied too. On macOS, `scene-color-countdown.plugin` must sit directly inside the `plugins` folder. |
| `compiled with newer libobs 32.2` | OBS is too old. Update OBS to 32.2 or newer. |
| Windows: `LoadLibrary failed … (126)` or `(127)` | A file is missing, or OBS is too old for the Qt version the plugin uses. Update OBS, then reinstall. |
| macOS: "cannot be opened because the developer cannot be verified" | Run the `xattr` command from step 5, or open **System Settings → Privacy & Security** and choose **Allow Anyway**. |
| The plugin loads, but the log has warnings such as `Scenes list not found` | That OBS version changed its UI, so the affected feature turned itself off. See [Technical notes](#technical-notes). |

## How the countdown picks a video

The plugin reads the sources that are actually on the Program output (through the transition), including nested scenes and groups, and takes the media that is playing or paused. If there are several, it uses the video that has a known duration, isn't *looping*, is playing (not paused), and ends last. Other media show up as a `+N` badge; hover over the bar to see the list.
The remaining time takes the Media Source *speed* into account (for example, at 200% the remaining time is halved).

## Building on macOS (no Xcode)

The Command Line Tools (`clang++`, `make`) are enough:

```bash
make deps      # once: OBS 32.2.2 headers, Qt 6.11.1 (obs-deps), SIMDe -> .deps/
make           # output: build/scene-color-countdown.plugin
make install   # copy into ~/Library/Application Support/obs-studio/plugins/
```

The plugin links directly against the `libobs`, `obs-frontend-api`, and Qt inside `/Applications/OBS.app`, so it always matches the installed OBS. Restart OBS after `make install`. To remove it: `make uninstall`.

## Building with CMake (Windows and universal macOS)

The CMake setup follows the official [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate):

- `buildspec.json`: plugin name and version, plus the versions and SHA-256 hashes of the OBS 32.2.2 sources and obs-deps 2026-07-15 (Qt 6.11.1).
- `CMakePresets.json`: the `windows-x64` and `macos` presets, plus `-ci` variants that treat warnings as errors. The template's `ubuntu-x86_64` preset is still there, but Linux is not built on CI.
- `cmake/`: the template's modules with two changes: C++20, and on Windows the install uses OBS's folder layout (`obs-plugins/64bit`, `data/obs-plugins/<name>`).

The first configure downloads the OBS sources and obs-deps into `.deps/`, then builds `libobs` and `obs-frontend-api` once (a few minutes).

The `.github/workflows/build.yaml` workflow runs on pushes to `main`, tags, pull requests, or manually (**Run workflow**), and produces these artifacts:

| Artifact | Contents |
|---|---|
| `…-windows-x64` | `obs-plugins/64bit/scene-color-countdown.dll` and `data/obs-plugins/scene-color-countdown/locale/`, ready to copy into the OBS folder |
| `…-windows-x64-pdb` | debug symbols for the `.dll` |
| `…-macos-universal` | a zip with `scene-color-countdown.plugin` (arm64 + x86_64, ad-hoc signed) for `~/Library/Application Support/obs-studio/plugins/`. If macOS blocks it, run `xattr -dr com.apple.quarantine` on the `.plugin` folder |

Building by hand on Windows (Visual Studio 2022 with the C++ workload, CMake 3.28+):

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
cmake --install build_x64 --config RelWithDebInfo --prefix C:\temp\scene-color-countdown
```

Without `--prefix`, `cmake --install` installs straight into `C:\Program Files\obs-studio`, which needs an administrator terminal.

## Making a release

1. Bump the version in `buildspec.json` and the `Makefile`, then commit and push.
2. Create a tag with the same version number and push it:

   ```bash
   git tag -a 1.2.0 -m "Scene Color & Program Countdown 1.2.0"
   git push origin 1.2.0
   ```

The workflow then builds Windows and macOS and creates a GitHub Release with `…-windows-x64.zip`, `…-windows-x64-pdb.zip`, and `…-macos-universal.zip`. The release notes come from `.github/release-notes.md`. If the tag name doesn't match the version in `buildspec.json`, no release is created.

## After an OBS update

- macOS build (`make`): update `OBS_VERSION`, `OBS_DEPS_VERSION`, and the Qt hashes in `scripts/setup-deps.sh` (the obs-deps version for each release is listed in the OBS repo's `CMakePresets.json`), delete `.deps/`, then run `make deps && make && make install` again.
- CMake build: update the versions and hashes in `buildspec.json`. The `prebuilt` and `qt6` hashes are in the `dependencies` section of the OBS repo's `CMakePresets.json` (its `macos-universal` key is `macos` here). The `obs-studio` hashes are the SHA-256 of GitHub's tag archives (`<version>.tar.gz` for `macos`, `<version>.zip` for `windows-x64`); if one is wrong, configure stops and shows the actual hash.

The plugin version lives in two places: `version` in `buildspec.json` and `VERSION` in the `Makefile`.

## Technical notes

- OBS has no API for the Scenes dock or the Program area, so the plugin attaches to OBS's internal widgets (`scenes`, `canvasEditor`, `previewLayout`, `previewTextLayout`). If a later OBS version changes the UI, the affected feature turns itself off and writes a `[scene-color-countdown]` warning to the OBS log, without crashing OBS.
- Those widget names come from `OBSBasic.ui`, so they are the same on macOS, Windows, and Linux. The plugin has no platform-specific code and does not use moc.
- The colors are painted by a wrapping delegate; scene renaming, item sizes, and grid mode are still handled by OBS's own delegate.
- Translations live in `data/locale/` (`en-US.ini`, `id-ID.ini`).
