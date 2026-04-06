# pic-viewer

[![Cross-platform](https://img.shields.io/badge/platform-cross--platform-blue)](https://github.com/example/pic-viewer)
[![C++20](https://img.shields.io/badge/language-C%2B%2B20-brightgreen)](https://en.cppreference.com/w/cpp/20)
[![Qt 6](https://img.shields.io/badge/Qt-6.0%2B-orange)](https://www.qt.io/)
[![CMake](https://img.shields.io/badge/build-CMake-yellowgreen)](https://cmake.org/)

一个功能强大的跨平台桌面图片查看器，使用C++20、Qt 6和CMake构建。

## 项目简介

pic-viewer 是一个轻量级但功能丰富的图片查看器，支持多种图片格式，提供流畅的浏览体验和丰富的功能。无论是查看单张图片还是浏览整个文件夹，pic-viewer 都能为您提供直观、高效的图片查看体验。

## 主要功能

### 图片格式支持
- **JPEG**：通过 `jpeg-turbo` 提供高性能解码
- **HEIF/HEIC/HIF**：通过 `libheif` 支持现代高效图片格式
- **AVIF/AVIFS**：通过 `libheif` 支持 AV1 编码的图片格式
- **光栅格式**：通过 Qt 图像插件支持 PNG、GIF、BMP、TIFF、WebP、ICO、SVG、JPEG 2000、EXR 等
- **RAW 格式**：通过 `LibRaw` 支持 ARW、CR2、CR3、DNG、NEF、ORF、RAF、RW2、SRW、X3F 等相机原始格式

### 核心功能
- 打开单个文件或整个文件夹
- 前后导航：使用 `Left`、`Right`、`PageUp`、`PageDown` 键
- 幻灯片播放：按 `Space` 键开始/暂停
- 全屏模式：按 `F11` 或 `F` 键
- 缩放控制：鼠标滚轮缩放，`Cmd/Ctrl +` 放大，`Cmd/Ctrl -` 缩小，`Cmd/Ctrl 0` 重置缩放
- 显示模式：适应窗口、实际大小、填充窗口
- 拖动平移：放大后可拖动图片
- 底部缩略图条：独立后台缩略图加载和缓存
- 后台解码：对大型格式提供预览优先加载

### 用户体验
- 响应式界面：自动适应窗口大小
- 流畅的图片切换：后台预加载机制
- 低内存占用：智能缓存管理
- 直观的操作：符合用户习惯的快捷键和界面布局

## 系统要求

### macOS
- macOS 10.15 (Catalina) 或更高版本
- Homebrew 包管理器
- Xcode 命令行工具或完整 Xcode

### Windows
- Windows 10 或 Windows 11
- Visual Studio 2022 Build Tools（安装 "使用 C++ 的桌面开发" 工作负载）
- Scoop 包管理器（将由安装脚本自动安装）

## 安装依赖

### macOS (Homebrew)

```bash
# 安装核心依赖
brew install cmake qt libheif libraw jpeg-turbo pkg-config

# 如果 Qt 二进制文件不在 PATH 中，添加以下环境变量
export PATH="/opt/homebrew/opt/qt/bin:$PATH"
export PKG_CONFIG_PATH="/opt/homebrew/opt/qt/lib/pkgconfig:/opt/homebrew/lib/pkgconfig:$PKG_CONFIG_PATH"
```

### Windows (Scoop)

1. 安装 Visual Studio 2022 Build Tools，确保选择 "使用 C++ 的桌面开发" 工作负载
2. 运行环境安装脚本：

```bat
scripts\install-dev-env.bat
```

该脚本会自动安装 Scoop、核心工具、Qt 以及可以从 Scoop 桶中解析的图像库，然后写入相关的用户环境变量。

## 构建项目

### 手动构建

#### macOS

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build
```

#### Windows

```bat
set QT_PREFIX=C:\Qt\6.9.0\msvc2022_64
cmake -S . -B build
cmake --build build
```

### 使用脚本构建

#### macOS

```bash
./scripts/build.sh
```

#### Windows

```bat
scripts\build.bat
```

如果您已经运行了 Windows 环境安装程序，`QT_PREFIX` 应该已经为新的 shell 设置好了。

## 运行项目

### 手动运行

#### macOS

```bash
# 不带参数运行，使用应用程序的 "打开文件" / "打开文件夹" 对话框
./build/pic-viewer

# 直接打开图片或文件夹
./build/pic-viewer /path/to/image-or-folder
```

#### Windows

```bat
# 不带参数运行
scripts\run.bat

# 直接打开图片或文件夹
scripts\run.bat C:\path\to\image-or-folder
```

### 使用脚本运行

#### macOS

```bash
./scripts/run.sh /path/to/image-or-folder
```

#### Windows

```bat
scripts\run.bat C:\path\to\image-or-folder
```

## 快捷键

| 快捷键 | 功能 |
|-------|------|
| `Left` / `PageUp` | 上一张图片 |
| `Right` / `PageDown` | 下一张图片 |
| `Space` | 开始/暂停幻灯片播放 |
| `F11` / `F` | 切换全屏模式 |
| `Mouse Wheel` | 缩放图片 |
| `Cmd/Ctrl +` | 放大图片 |
| `Cmd/Ctrl -` | 缩小图片 |
| `Cmd/Ctrl 0` | 重置缩放 |
| `Drag` | 拖动平移（放大后） |

## 项目结构

```
pic-viewer/
├── .vscode/            # VS Code 配置文件
├── assets/             # 资源文件
│   ├── app_icon.ico    # 应用图标（Windows）
│   ├── app_icon.rc     # 应用图标资源文件
│   ├── app_icon.svg    # 应用图标（矢量）
│   ├── app_icon.xpm    # 应用图标（XPM 格式）
│   └── resources.qrc   # Qt 资源文件
├── scripts/            # 构建和运行脚本
│   ├── build.bat       # Windows 构建脚本
│   ├── build.sh        # macOS 构建脚本
│   ├── generate-app-icon.py  # 应用图标生成脚本
│   ├── install-dev-env.bat   # Windows 环境安装脚本
│   ├── install-dev-env.ps1   # Windows PowerShell 环境安装脚本
│   ├── install-dev-env.sh    # macOS 环境安装脚本
│   ├── run.bat         # Windows 运行脚本
│   └── run.sh          # macOS 运行脚本
├── src/                # 源代码
│   ├── app/            # 应用程序核心
│   ├── catalog/        # 图像目录管理
│   ├── core/           # 核心功能
│   ├── decoder/        # 图像解码器
│   └── viewer/         # 图像查看器组件
├── tests/              # 测试代码
├── .gitignore          # Git 忽略文件
├── CMakeLists.txt      # CMake 配置文件
├── LICENSE             # 许可证文件
├── README.md           # 项目说明文件
└── vcpkg.json          # vcpkg 配置文件
```

### 核心模块

- **app**：应用程序入口和主窗口
- **catalog**：图像目录管理，处理文件系统操作
- **core**：核心功能，包括图像缓存、预加载调度和幻灯片控制
- **decoder**：图像解码器，支持多种格式
- **viewer**：图像查看器组件，处理显示和用户交互

## 测试

运行测试套件以确保功能正常：

```bash
ctest --test-dir build --output-on-failure
```

测试文件位于 `tests/` 目录，包括：
- `test_image_catalog.cpp`：测试图像目录功能
- `test_image_decoder.cpp`：测试图像解码功能
- `test_prefetch_scheduler.cpp`：测试预加载调度器
- `test_slide_show_controller.cpp`：测试幻灯片控制器

## VS Code 调试

按 `F5` 并选择平台特定的启动配置：

- VS Code 将首先运行 `.vscode/tasks.json` 中的 `build pic-viewer` 任务
- 在 macOS 上，它会调用 `scripts/build.sh` 并使用 `CodeLLDB` 启动 `build/pic-viewer`
- 在 Windows 上，它会调用 `scripts/build.bat` 并使用 `cppvsdbg` 启动 `build\pic-viewer.exe`
- 启动时不带路径参数，因此您可以直接使用应用程序自己的 "打开文件" / "打开文件夹" 对话框

## 许可证

本项目采用 MIT 许可证。详情请参阅 [LICENSE](LICENSE) 文件。

## 贡献指南

欢迎贡献代码和提出问题！如果您有任何改进建议或发现了 bug，请在 GitHub 仓库中提交 issue 或 pull request。

### 贡献步骤
1. Fork 本仓库
2. 创建您的特性分支 (`git checkout -b feature/amazing-feature`)
3. 提交您的更改 (`git commit -m 'Add some amazing feature'`)
4. 推送到分支 (`git push origin feature/amazing-feature`)
5. 打开一个 Pull Request

## 鸣谢

- [Qt](https://www.qt.io/) - 跨平台应用程序框架
- [libheif](https://github.com/strukturag/libheif) - HEIF/AVIF 图像格式支持
- [LibRaw](https://www.libraw.org/) - RAW 图像格式支持
- [jpeg-turbo](https://libjpeg-turbo.org/) - 高性能 JPEG 解码
- [CMake](https://cmake.org/) - 跨平台构建系统

## 联系方式

如有问题或建议，请通过 GitHub Issues 联系我们。

---

**享受使用 pic-viewer 查看您的图片！** 📷
