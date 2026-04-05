# pic-viewer

Cross-platform desktop image viewer built with C++20, Qt 6, and CMake.

The app includes a built-in viewer icon resource at `assets/app_icon.xpm`.

## Features

- JPEG via `jpeg-turbo`
- HEIF / HEIC / HIF via `libheif`
- AVIF / AVIFS via `libheif`
- Broad raster support via Qt image plugins: PNG, GIF, BMP, TIFF, WebP, ICO, SVG, JPEG 2000, EXR, and more when the plugin is present
- Broad RAW support via `LibRaw`: ARW, CR2, CR3, DNG, NEF, ORF, RAF, RW2, SRW, X3F, and other LibRaw-supported cameras
- Open a file or folder
- Previous / next navigation with `Left`, `Right`, `PageUp`, `PageDown`
- Slideshow playback with `Space`
- Fullscreen with `F11` / `F`
- Mouse wheel zoom, `Cmd/Ctrl +` zoom in, `Cmd/Ctrl -` zoom out, `Cmd/Ctrl 0` reset zoom
- Display modes: fit to window, actual size, fill window
- Drag to pan when zoomed in
- Bottom thumbnail strip with independent background thumbnail loading and caching
- Background decoding with preview-first loading for heavy formats

## Dependencies

macOS (Homebrew):

```bash
brew install cmake qt libheif libraw jpeg-turbo pkg-config
```

If Qt binaries are not on your `PATH`, add:

```bash
export PATH="/opt/homebrew/opt/qt/bin:$PATH"
export PKG_CONFIG_PATH="/opt/homebrew/opt/qt/lib/pkgconfig:/opt/homebrew/lib/pkgconfig:$PKG_CONFIG_PATH"
```

Windows (Scoop):

- Install Visual Studio 2022 Build Tools with the "Desktop development with C++" workload
- Then run:

```bat
scripts\install-dev-env.bat
```

The script installs Scoop, core tools, Qt, and the image libraries it can resolve from Scoop buckets, then writes the relevant user environment variables.

## Build

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build
```

Or use the helper script:

```bash
./scripts/build.sh
```

Windows:

```bat
set QT_PREFIX=C:\Qt\6.9.0\msvc2022_64
scripts\build.bat
```

If you already ran the Windows environment installer, `QT_PREFIX` should already be set for new shells.

## Run

```bash
./build/pic-viewer
./build/pic-viewer /path/to/image-or-folder
```

Or use the helper script:

```bash
./scripts/run.sh /path/to/image-or-folder
```

Windows:

```bat
scripts\run.bat C:\path\to\image-or-folder
```

## VS Code Debug

Press `F5` and choose the platform-specific launch configuration.

- VS Code will run `.vscode/tasks.json` task `build pic-viewer` first
- On macOS it calls `scripts/build.sh` and launches `build/pic-viewer` with `CodeLLDB`
- On Windows it calls `scripts/build.bat` and launches `build\pic-viewer.exe` with `cppvsdbg`
- Launch now starts with no path arguments, so you can use the app's own "Open File" / "Open Folder" dialog directly

## Test

```bash
ctest --test-dir build --output-on-failure
```
