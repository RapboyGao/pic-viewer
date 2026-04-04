# pic-viewer

Cross-platform desktop image viewer built with C++20, Qt 6, and CMake.

## Features

- JPEG via `jpeg-turbo`
- HEIF / HEIC / HIF via `libheif`
- ARW via `LibRaw`
- Open a file or folder
- Previous / next navigation with `Left`, `Right`, `PageUp`, `PageDown`
- Slideshow playback with `Space`
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

## Build

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build
```

## Run

```bash
./build/pic-viewer
./build/pic-viewer /path/to/image-or-folder
```

## Test

```bash
ctest --test-dir build --output-on-failure
```
