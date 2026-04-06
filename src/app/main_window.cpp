#include "app/main_window.h"

#include "app/app_language.h"
#include "core/slide_show_controller.h"
#include "decoder/image_decoder.h"
#include "viewer/image_viewer_widget.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QColorSpace>
#include <QDateTime>
#include <QCursor>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QIcon>
#include <QKeyEvent>
#include <QKeySequence>
#include <QList>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QDockWidget>
#include <QDir>
#include <QMessageBox>
#include <QPoint>
#include <QSignalBlocker>
#include <QShortcut>
#include <QScrollBar>
#include <QFrame>
#include <QPlainTextEdit>
#include <QFile>
#include <QTimer>
#include <QVBoxLayout>
#include <QThread>
#include <QtConcurrent>
#include <algorithm>
#include <string>

#ifdef Q_OS_WIN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#endif

using AppI18n::uiText;

namespace {

constexpr int kDefaultIntervalMs = 3000;
constexpr int kThumbnailSize = 96;
constexpr int kMaxThumbnailInflight = 2;
constexpr int kDecodeThreadStackSize = 8 * 1024 * 1024;
constexpr int kThumbnailStripExpandedHeight = kThumbnailSize + 48;
constexpr int kThumbnailRevealMargin = 96;
constexpr int kImagePrefetchWindow = 6;
constexpr int kImmediatePrefetchDistance = 1;
constexpr int kDeferredPrefetchPriorityBase = 100;
constexpr int kMaxQueuedThumbnailRequests = 64;
constexpr int kMaxQueuedImagePrefetchRequests = 24;
constexpr int kFullscreenMenuRevealMargin = 8;
constexpr int kFullscreenMenuAutoHideDelayMs = 1000;
constexpr qint64 kThumbnailCacheBudgetBytes = 48LL * 1024 * 1024;
constexpr qint64 kImageCacheBudgetBytes = 256LL * 1024 * 1024;

int cpuThreadCount()
{
    const int count = QThread::idealThreadCount();
    return count > 0 ? count : 1;
}

int imagePrefetchThreadCount()
{
    return std::max(1, cpuThreadCount() - 1);
}

QString modeLabelForAction(int intervalMs)
{
    return uiText("%1 s", "%1 秒").arg(intervalMs / 1000);
}

qint64 estimatePixmapCost(const QPixmap& pixmap)
{
    if (pixmap.isNull()) {
        return 0;
    }
    return std::max<qint64>(1, static_cast<qint64>(pixmap.width()) * pixmap.height() * 4);
}

QString formatBytes(qint64 bytes)
{
    static const char* const units[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    constexpr int kUnitCount = static_cast<int>(sizeof(units) / sizeof(units[0]));
    double value = static_cast<double>(bytes);
    int unitIndex = 0;
    while (value >= 1024.0 && unitIndex < kUnitCount - 1) {
        value /= 1024.0;
        ++unitIndex;
    }
    if (unitIndex == 0) {
        return QString("%1 %2").arg(bytes).arg(units[unitIndex]);
    }
    return QString("%1 %2").arg(QString::number(value, 'f', value >= 10.0 ? 1 : 2)).arg(units[unitIndex]);
}

QString formatDateTime(const QDateTime& dateTime)
{
    if (!dateTime.isValid()) {
        return uiText("Unknown", "未知");
    }
    return dateTime.toString("yyyy-MM-dd HH:mm:ss");
}

QString formatColorSpace(const QColorSpace& colorSpace)
{
    if (!colorSpace.isValid()) {
        return uiText("Unknown", "未知");
    }

    const QString description = colorSpace.description();
    return description.isEmpty() ? uiText("Valid color space", "有效色彩空间") : description;
}

QString formatImageFormat(QImage::Format format)
{
    switch (format) {
    case QImage::Format_Invalid:
        return uiText("Invalid", "无效");
    case QImage::Format_Mono:
        return uiText("Mono", "1 位单色");
    case QImage::Format_MonoLSB:
        return uiText("Mono LSB", "1 位单色（LSB 优先）");
    case QImage::Format_Indexed8:
        return uiText("Indexed8", "索引色 8 位（调色板）");
    case QImage::Format_RGB32:
        return uiText("RGB32", "XRGB 32 位");
    case QImage::Format_ARGB32:
        return uiText("ARGB32", "ARGB 32 位（8-8-8-8）");
    case QImage::Format_ARGB32_Premultiplied:
        return uiText("ARGB32 Premultiplied", "ARGB 32 位（预乘 alpha）");
    case QImage::Format_RGB16:
        return uiText("RGB16", "RGB 16 位（5-6-5）");
    case QImage::Format_ARGB8565_Premultiplied:
        return uiText("ARGB8565 Premultiplied", "ARGB 8-5-6-5（预乘 alpha）");
    case QImage::Format_RGB666:
        return uiText("RGB666", "RGB 18 位（6-6-6）");
    case QImage::Format_ARGB6666_Premultiplied:
        return uiText("ARGB6666 Premultiplied", "ARGB 24 位（6-6-6-6，预乘 alpha）");
    case QImage::Format_RGB555:
        return uiText("RGB555", "RGB 15 位（5-5-5）");
    case QImage::Format_ARGB8555_Premultiplied:
        return uiText("ARGB8555 Premultiplied", "ARGB 20 位（8-5-5-5，预乘 alpha）");
    case QImage::Format_RGB888:
        return uiText("RGB888", "RGB 24 位（8-8-8）");
    case QImage::Format_RGB444:
        return uiText("RGB444", "RGB 12 位（4-4-4）");
    case QImage::Format_ARGB4444_Premultiplied:
        return uiText("ARGB4444 Premultiplied", "ARGB 16 位（4-4-4-4，预乘 alpha）");
    case QImage::Format_BGR888:
        return uiText("BGR888", "BGR 24 位（8-8-8）");
    case QImage::Format_RGBA8888:
        return uiText("RGBA8888", "RGBA 32 位（8-8-8-8）");
    case QImage::Format_RGBA8888_Premultiplied:
        return uiText("RGBA8888 Premultiplied", "RGBA 32 位（预乘 alpha）");
    case QImage::Format_RGBX8888:
        return uiText("RGBX8888", "RGBX 32 位（无 alpha）");
    case QImage::Format_Grayscale8:
        return uiText("Grayscale8", "灰度 8 位（单通道）");
    case QImage::Format_Alpha8:
        return uiText("Alpha8", "Alpha 8 位（遮罩）");
    case QImage::Format_Grayscale16:
        return uiText("Grayscale16", "灰度 16 位（单通道）");
    case QImage::Format_RGBX64:
        return uiText("RGBX64", "RGBX 64 位（16-16-16-16）");
    case QImage::Format_RGBA64:
        return uiText("RGBA64", "RGBA 64 位（16-16-16-16）");
    case QImage::Format_RGBA64_Premultiplied:
        return uiText("RGBA64 Premultiplied", "RGBA 64 位（预乘 alpha）");
    case QImage::Format_BGR30:
        return uiText("BGR30", "BGR 30 位（10-10-10）");
    case QImage::Format_A2BGR30_Premultiplied:
        return uiText("A2BGR30 Premultiplied", "A2BGR 30 位（10-10-10，预乘 alpha）");
    case QImage::Format_RGB30:
        return uiText("RGB30", "RGB 30 位（10-10-10）");
    case QImage::Format_A2RGB30_Premultiplied:
        return uiText("A2RGB30 Premultiplied", "A2RGB 30 位（10-10-10，预乘 alpha）");
    case QImage::Format_CMYK8888:
        return uiText("CMYK8888", "CMYK 32 位（8-8-8-8）");
    case QImage::Format_RGBX16FPx4:
        return uiText("RGBX16FPx4", "RGBX 16 位半精度浮点");
    case QImage::Format_RGBA16FPx4:
        return uiText("RGBA16FPx4", "RGBA 16 位半精度浮点");
    case QImage::Format_RGBA16FPx4_Premultiplied:
        return uiText("RGBA16FPx4 Premultiplied", "RGBA 16 位半精度浮点（预乘 alpha）");
    case QImage::Format_RGBX32FPx4:
        return uiText("RGBX32FPx4", "RGBX 32 位单精度浮点");
    case QImage::Format_RGBA32FPx4:
        return uiText("RGBA32FPx4", "RGBA 32 位单精度浮点");
    case QImage::Format_RGBA32FPx4_Premultiplied:
        return uiText("RGBA32FPx4 Premultiplied", "RGBA 32 位单精度浮点（预乘 alpha）");
    default:
        return uiText("Format %1", "格式 %1").arg(static_cast<int>(format));
    }
}

QString formatAspectRatio(const QSize& size)
{
    if (!size.isValid() || size.height() <= 0) {
        return uiText("Unknown", "未知");
    }

    return QString("%1:1").arg(QString::number(static_cast<double>(size.width()) / size.height(), 'f', 2));
}

QString formatPixels(const QSize& size)
{
    if (!size.isValid()) {
        return uiText("Unknown", "未知");
    }

    const qint64 pixels = static_cast<qint64>(size.width()) * static_cast<qint64>(size.height());
    return QString("%1").arg(pixels);
}

QString formatDpi(double dotsPerMeter)
{
    if (dotsPerMeter <= 0.0) {
        return uiText("Unknown", "未知");
    }

    return QString::number(dotsPerMeter / 39.37007874, 'f', 1);
}

QString translateMetadataLabel(const QString& label)
{
    const QString normalized = label.trimmed().toLower();
    if (normalized == "camera") {
        return uiText("Camera", "相机");
    }
    if (normalized == "lens") {
        return uiText("Lens", "镜头");
    }
    if (normalized == "iso") {
        return uiText("ISO", "ISO");
    }
    if (normalized == "shutter") {
        return uiText("Shutter", "快门");
    }
    if (normalized == "aperture") {
        return uiText("Aperture", "光圈");
    }
    if (normalized == "focal length") {
        return uiText("Focal length", "焦距");
    }
    if (normalized == "capture time") {
        return uiText("Capture time", "拍摄时间");
    }
    if (normalized == "shot order") {
        return uiText("Shot order", "拍摄序号");
    }
    if (normalized == "description") {
        return uiText("Description", "说明");
    }
    if (normalized == "artist") {
        return uiText("Artist", "作者");
    }
    if (normalized == "gps latitude") {
        return uiText("GPS latitude", "GPS 纬度");
    }
    if (normalized == "gps longitude") {
        return uiText("GPS longitude", "GPS 经度");
    }
    if (normalized == "gps altitude") {
        return uiText("GPS altitude", "GPS 海拔");
    }
    return label;
}

QString translateMetadataLine(const QString& line)
{
    const int separatorIndex = line.indexOf(':');
    if (separatorIndex <= 0) {
        return line;
    }

    const QString label = line.left(separatorIndex).trimmed();
    const QString value = line.mid(separatorIndex + 1).trimmed();
    const QString translatedLabel = translateMetadataLabel(label);
    if (translatedLabel == label) {
        return line;
    }
    return QString("%1: %2").arg(translatedLabel, value);
}

struct FileAssociationGroup
{
    QString englishTitle;
    QString chineseTitle;
    QStringList extensions;
};

QStringList normalizedExtensions(QStringList extensions)
{
    extensions.removeDuplicates();
    extensions.sort(Qt::CaseInsensitive);
    return extensions;
}

QList<FileAssociationGroup> commonAssociationGroups()
{
    return {
        {"JPEG / HEIF", "JPEG / HEIF", {"jpg", "jpeg", "jpe", "jfif", "heic", "heif", "hif", "avif", "avifs"}},
        {"PNG / GIF", "PNG / GIF", {"png", "apng", "gif"}},
        {"Web / Modern", "Web / 现代", {"webp", "jxl", "jp2", "jxr"}},
        {"Bitmap / Legacy", "位图 / 传统", {"bmp", "dib", "ico", "icon", "pbm", "pfm", "pgm", "pic", "pnm", "ppm", "qoi", "ras", "sr", "tga"}},
        {"Editor / Print", "编辑 / 打印", {"psd", "exr", "hdr", "svg", "tif", "tiff"}},
    };
}

QList<FileAssociationGroup> rawAssociationGroups()
{
    return {
        {"Sony RAW", "索尼 RAW", {"arw", "sr2", "srf"}},
        {"Canon RAW", "佳能 RAW", {"cr2", "cr3", "crw"}},
        {"Nikon RAW", "尼康 RAW", {"nef", "nrw"}},
        {"Fujifilm RAW", "富士 RAW", {"raf"}},
        {"Panasonic RAW", "松下 RAW", {"rw2"}},
        {"Olympus / OM RAW", "奥林巴斯 / OM RAW", {"orf"}},
        {"Pentax RAW", "宾得 RAW", {"pef"}},
        {"Leica / DNG", "徕卡 / DNG", {"dng"}},
        {"Other RAW", "其他 RAW", {"3fr", "ari", "bay", "cap", "dcr", "dcs", "drf", "eip", "erf", "fff", "gpr", "iiq", "k25", "kdc", "mdc", "mef", "mos", "mrw", "ptx", "r3d", "raw", "rwl", "rwz", "srw", "x3f"}},
    };
}

QStringList allAssociationExtensions()
{
    QStringList extensions;
    for (const FileAssociationGroup& group : commonAssociationGroups()) {
        extensions += group.extensions;
    }
    for (const FileAssociationGroup& group : rawAssociationGroups()) {
        extensions += group.extensions;
    }
    return normalizedExtensions(extensions);
}

QString fileAssociationProgId(const QString& extension)
{
    return QStringLiteral("pic-viewer.%1").arg(extension.toLower());
}

QString fileAssociationDisplayName(const QString& extension)
{
    return QStringLiteral("%1 (*.%2)").arg(extension.toUpper(), extension.toLower());
}

QString fileAssociationCommand()
{
    const QString exePath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    return QStringLiteral("\"%1\" \"%2\"").arg(exePath, QStringLiteral("%1"));
}

QString fileAssociationIcon()
{
    const QString exePath = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    return QStringLiteral("\"%1\",0").arg(exePath);
}

#ifdef Q_OS_WIN
QString windowsErrorMessage(DWORD errorCode)
{
    LPWSTR buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD size = FormatMessageW(flags, nullptr, errorCode, 0, reinterpret_cast<LPWSTR>(&buffer), 0, nullptr);
    QString message = size > 0 && buffer
        ? QString::fromWCharArray(buffer).trimmed()
        : uiText("Windows error %1", "Windows 错误 %1").arg(errorCode);
    if (buffer) {
        LocalFree(buffer);
    }
    return message;
}

QString registryPathForExtension(const QString& extension)
{
    return QStringLiteral("Software\\Classes\\.%1").arg(extension.toLower());
}

QString registryPathForProgId(const QString& progId)
{
    return QStringLiteral("Software\\Classes\\%1").arg(progId);
}

bool openRegistryKey(HKEY root, const QString& path, REGSAM access, HKEY* handle)
{
    const std::wstring widePath = path.toStdWString();
    return RegCreateKeyExW(root, widePath.c_str(), 0, nullptr, 0, access, nullptr, handle, nullptr) == ERROR_SUCCESS;
}

bool setRegistryDefaultString(HKEY root, const QString& path, const QString& value, QString* errorMessage)
{
    HKEY key = nullptr;
    if (!openRegistryKey(root, path, KEY_SET_VALUE, &key)) {
        if (errorMessage) {
            *errorMessage = uiText("Unable to open registry key: %1", "无法打开注册表项：%1").arg(path);
        }
        return false;
    }

    const std::wstring wideValue = value.toStdWString();
    const LONG status = RegSetValueExW(
        key,
        nullptr,
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(wideValue.c_str()),
        static_cast<DWORD>((wideValue.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    if (status != ERROR_SUCCESS) {
        if (errorMessage) {
            *errorMessage = uiText("Unable to write registry value: %1", "无法写入注册表值：%1").arg(windowsErrorMessage(status));
        }
        return false;
    }
    return true;
}

bool readRegistryDefaultString(HKEY root, const QString& path, QString* value)
{
    HKEY key = nullptr;
    const std::wstring widePath = path.toStdWString();
    if (RegOpenKeyExW(root, widePath.c_str(), 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) {
        return false;
    }

    DWORD type = 0;
    DWORD dataSize = 0;
    LONG status = RegQueryValueExW(key, nullptr, nullptr, &type, nullptr, &dataSize);
    if (status != ERROR_SUCCESS || type != REG_SZ || dataSize == 0) {
        RegCloseKey(key);
        return false;
    }

    std::wstring buffer(dataSize / sizeof(wchar_t), L'\0');
    status = RegQueryValueExW(
        key,
        nullptr,
        nullptr,
        &type,
        reinterpret_cast<LPBYTE>(buffer.data()),
        &dataSize);
    RegCloseKey(key);
    if (status != ERROR_SUCCESS || type != REG_SZ) {
        return false;
    }

    if (value) {
        *value = QString::fromWCharArray(buffer.c_str());
    }
    return true;
}

bool deleteRegistryTreeIfExists(HKEY root, const QString& path, QString* errorMessage)
{
    const std::wstring widePath = path.toStdWString();
    const LONG status = RegDeleteTreeW(root, widePath.c_str());
    if (status == ERROR_FILE_NOT_FOUND) {
        return true;
    }
    if (status != ERROR_SUCCESS) {
        if (errorMessage) {
            *errorMessage = uiText("Unable to delete registry key: %1", "无法删除注册表项：%1").arg(windowsErrorMessage(status));
        }
        return false;
    }
    return true;
}

bool deleteRegistryDefaultValue(HKEY root, const QString& path, QString* errorMessage)
{
    HKEY key = nullptr;
    const std::wstring widePath = path.toStdWString();
    if (RegOpenKeyExW(root, widePath.c_str(), 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) {
        return true;
    }

    const LONG status = RegDeleteValueW(key, nullptr);
    RegCloseKey(key);
    if (status == ERROR_FILE_NOT_FOUND) {
        return true;
    }
    if (status != ERROR_SUCCESS) {
        if (errorMessage) {
            *errorMessage = uiText("Unable to clear registry value: %1", "无法清除注册表值：%1").arg(windowsErrorMessage(status));
        }
        return false;
    }
    return true;
}
#endif

} // namespace

MainWindow::MainWindow(const QString& startupPath, QWidget* parent)
    : QMainWindow(parent)
    , cache_(6, kImageCacheBudgetBytes)
{
    setWindowTitle("pic-viewer");
    setWindowIcon(QIcon(":/icons/app_icon.xpm"));
    resize(1200, 800);

    imageDecodePool_.setMaxThreadCount(imagePrefetchThreadCount());
    imageDecodePool_.setStackSize(kDecodeThreadStackSize);

    thumbnailDecodePool_.setMaxThreadCount(kMaxThumbnailInflight);
    thumbnailDecodePool_.setStackSize(kDecodeThreadStackSize);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    central->setStyleSheet("background-color: black;");

    viewer_ = new ImageViewerWidget(central);
    viewer_->setStyleSheet("background-color: black;");
    layout->addWidget(viewer_, 1);
    createThumbnailStrip();
    layout->addWidget(thumbnailStripContainer_);
    setCentralWidget(central);

    slideshow_ = new SlideShowController(this);
    slideshow_->setIntervalMs(kDefaultIntervalMs);
    thumbnailStripAnimation_ = new QPropertyAnimation(thumbnailStripContainer_, "maximumHeight", this);
    thumbnailStripAnimation_->setDuration(180);
    connect(thumbnailStripAnimation_, &QPropertyAnimation::valueChanged, this, [this](const QVariant& value) {
        const int height = value.toInt();
        thumbnailStripContainer_->setMinimumHeight(height);
    });
    thumbnailAutoHideTimer_ = new QTimer(this);
    thumbnailAutoHideTimer_->setSingleShot(true);
    thumbnailAutoHideTimer_->setInterval(1200);
    thumbnailAutoHideEnabled_ = true;
    thumbnailStripContainer_->hide();

    fullscreenMenuBarAutoHideTimer_ = new QTimer(this);
    fullscreenMenuBarAutoHideTimer_->setSingleShot(true);
    fullscreenMenuBarAutoHideTimer_->setInterval(kFullscreenMenuAutoHideDelayMs);

    createMenus();
    createInfoPanel();
    applyFullscreenMenuBarVisibility(true);
    auto registerNavigationShortcut = [this](int key, void (MainWindow::*handler)()) {
        auto* shortcut = new QShortcut(QKeySequence(key), this);
        shortcut->setContext(Qt::ApplicationShortcut);
        connect(shortcut, &QShortcut::activated, this, handler);
    };
    registerNavigationShortcut(Qt::Key_Left, &MainWindow::showPreviousImage);
    registerNavigationShortcut(Qt::Key_A, &MainWindow::showPreviousImage);
    registerNavigationShortcut(Qt::Key_PageUp, &MainWindow::showPreviousImage);
    registerNavigationShortcut(Qt::Key_Right, &MainWindow::showNextImage);
    registerNavigationShortcut(Qt::Key_D, &MainWindow::showNextImage);
    registerNavigationShortcut(Qt::Key_PageDown, &MainWindow::showNextImage);

    connect(slideshow_, &SlideShowController::advanceRequested, this, &MainWindow::showNextImage);
    connect(slideshow_, &SlideShowController::playingChanged, this, [this](bool playing) {
        if (playPauseAction_) {
            playPauseAction_->setText(playing ? uiText("Pause Slideshow", "暂停幻灯片放映") : uiText("Start Slideshow", "开始幻灯片放映"));
        }
    });
    connect(slideshow_, &SlideShowController::intervalChanged, this, [this](int intervalMs) {
        setIntervalActionChecked(intervalMs);
    });
    connect(thumbnailList_, &QListWidget::itemActivated, this, &MainWindow::thumbnailActivated);
    connect(thumbnailList_, &QListWidget::itemClicked, this, &MainWindow::thumbnailActivated);
    connect(viewer_, &ImageViewerWidget::openFileRequested, this, &MainWindow::openFile);
    connect(viewer_, &ImageViewerWidget::openFolderRequested, this, &MainWindow::openFolder);
    connect(viewer_, &ImageViewerWidget::zoomFactorChanged, this, [this](double zoomFactor) {
        if (viewer_ && viewer_->hasImage()) {
            viewer_->showTransientZoom(uiText("Zoom %1%", "缩放 %1%").arg(static_cast<int>(zoomFactor * 100.0)));
        }
    });
    connect(thumbnailList_->horizontalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        requestVisibleThumbnails();
    });
    connect(thumbnailAutoHideTimer_, &QTimer::timeout, this, [this]() {
        if (thumbnailAutoHideEnabled_) {
            applyThumbnailStripVisibility(false, true);
        }
    });
    connect(fullscreenMenuBarAutoHideTimer_, &QTimer::timeout, this, [this]() {
        if (isFullScreen() && fullscreenMenuBarVisible_ && !shouldKeepFullscreenMenuBarVisible()) {
            applyFullscreenMenuBarVisibility(false);
        }
    });

    qApp->installEventFilter(this);
    viewer_->installEventFilter(this);
    thumbnailList_->installEventFilter(this);
    thumbnailList_->viewport()->installEventFilter(this);
    centralWidget()->installEventFilter(this);
    menuBar()->installEventFilter(this);

    if (!startupPath.isEmpty()) {
        openPath(startupPath);
    } else {
        viewer_->setMessage(
            uiText("Open a file or folder", "打开文件或文件夹"),
            uiText("Supported: common image, HEIF/AVIF, and RAW formats", "支持常见图片、HEIF/AVIF 和 RAW 格式"));
    }
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Left:
    case Qt::Key_PageUp:
    case Qt::Key_A:
        setBrowseDirection(PrefetchScheduler::Direction::Backward);
        showPreviousImage();
        return;
    case Qt::Key_Right:
    case Qt::Key_PageDown:
    case Qt::Key_D:
        setBrowseDirection(PrefetchScheduler::Direction::Forward);
        showNextImage();
        return;
    case Qt::Key_Space:
        toggleSlideshow();
        return;
    case Qt::Key_F:
    case Qt::Key_F11:
        toggleFullscreen();
        return;
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        zoomIn();
        return;
    case Qt::Key_Minus:
        zoomOut();
        return;
    case Qt::Key_0:
        resetZoom();
        return;
    case Qt::Key_Escape:
        if (isFullScreen()) {
            toggleFullscreen();
            return;
        }
        break;
    case Qt::Key_T:
        toggleThumbnailStrip();
        return;
    case Qt::Key_I:
        toggleInfoPanel();
        return;
    default:
        break;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::openFile()
{
    const QString filter = uiText(
        "Images (*.apng *.avif *.avifs *.bmp *.dib *.exr *.gif *.hdr *.heic *.heif *.hif *.ico *.icon *.jfif *.jp2 *.jpe *.jpeg *.jpg *.jxl *.jxr *.pbm *.pfm *.pgm *.pic *.png *.pnm *.ppm *.psd *.pxm *.qoi *.ras *.sr *.svg *.tga *.tif *.tiff *.webp *.wp2 *.3fr *.ari *.arw *.bay *.cap *.cr2 *.cr3 *.crw *.dcr *.dcs *.dng *.drf *.eip *.erf *.fff *.gpr *.iiq *.k25 *.kdc *.mdc *.mef *.mos *.mrw *.nef *.nrw *.orf *.pef *.ptx *.r3d *.raf *.raw *.rw2 *.rwl *.rwz *.sr2 *.srf *.srw *.x3f)",
        "图片 (*.apng *.avif *.avifs *.bmp *.dib *.exr *.gif *.hdr *.heic *.heif *.hif *.ico *.icon *.jfif *.jp2 *.jpe *.jpeg *.jpg *.jxl *.jxr *.pbm *.pfm *.pgm *.pic *.png *.pnm *.ppm *.psd *.pxm *.qoi *.ras *.sr *.svg *.tga *.tif *.tiff *.webp *.wp2 *.3fr *.ari *.arw *.bay *.cap *.cr2 *.cr3 *.crw *.dcr *.dcs *.dng *.drf *.eip *.erf *.fff *.gpr *.iiq *.k25 *.kdc *.mdc *.mef *.mos *.mrw *.nef *.nrw *.orf *.pef *.ptx *.r3d *.raf *.raw *.rw2 *.rwl *.rwz *.sr2 *.srf *.srw *.x3f)");
    const QString path = QFileDialog::getOpenFileName(
        this,
        uiText("Open Image", "打开图片"),
        {},
        filter);
    if (!path.isEmpty()) {
        openPath(path);
    }
}

void MainWindow::openFolder()
{
    const QString path = QFileDialog::getExistingDirectory(this, uiText("Open Folder", "打开文件夹"));
    if (!path.isEmpty()) {
        openPath(path);
    }
}

void MainWindow::showNextImage()
{
    if (!catalog_.moveNext()) {
        return;
    }
    setBrowseDirection(PrefetchScheduler::Direction::Forward);
    refreshCurrentImage();
}

void MainWindow::showPreviousImage()
{
    if (!catalog_.movePrevious()) {
        return;
    }
    setBrowseDirection(PrefetchScheduler::Direction::Backward);
    refreshCurrentImage();
}

void MainWindow::toggleSlideshow()
{
    slideshow_->toggle();
}

void MainWindow::createMenus()
{
    fileMenu_ = menuBar()->addMenu(uiText("&File", "&文件"));
    openFileAction_ = fileMenu_->addAction(uiText("&Open File...", "&打开文件..."), QKeySequence::Open, this, &MainWindow::openFile);
    openFolderAction_ = fileMenu_->addAction(uiText("Open &Folder...", "打开&文件夹..."), this, &MainWindow::openFolder);
    fileMenu_->addSeparator();
    exitAction_ = fileMenu_->addAction(uiText("E&xit", "退&出"), QKeySequence::Quit, this, &QWidget::close);

    playbackMenu_ = menuBar()->addMenu(uiText("&Playback", "&播放"));
    playPauseAction_ = playbackMenu_->addAction(uiText("Start Slideshow", "开始幻灯片放映"), Qt::Key_Space, this, &MainWindow::toggleSlideshow);
    playbackMenu_->addSeparator();

    intervalActionGroup_ = new QActionGroup(this);
    intervalActionGroup_->setExclusive(true);
    intervalActions_.clear();

    for (const int interval : {500, 1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000, 9000, 10000, 15000, 20000, 30000, 60000}) {
        QAction* action = playbackMenu_->addAction(modeLabelForAction(interval));
        action->setCheckable(true);
        action->setData(interval);
        intervalActionGroup_->addAction(action);
        intervalActions_.append(action);
        connect(action, &QAction::triggered, this, [this, interval]() {
            slideshow_->setIntervalMs(interval);
            setIntervalActionChecked(interval);
        });
    }
    setIntervalActionChecked(kDefaultIntervalMs);

    viewMenu_ = menuBar()->addMenu(uiText("&View", "&视图"));
    fullscreenAction_ = viewMenu_->addAction(uiText("Enter Fullscreen", "进入全屏"), Qt::Key_F11, this, &MainWindow::toggleFullscreen);
    infoPanelAction_ = viewMenu_->addAction(uiText("Show Info Panel", "显示信息面板"), Qt::Key_I, this, &MainWindow::toggleInfoPanel);
    infoPanelAction_->setCheckable(true);
    zoomInAction_ = viewMenu_->addAction(uiText("Zoom In", "放大"), QKeySequence::ZoomIn, this, &MainWindow::zoomIn);
    zoomOutAction_ = viewMenu_->addAction(uiText("Zoom Out", "缩小"), QKeySequence::ZoomOut, this, &MainWindow::zoomOut);
    resetZoomAction_ = viewMenu_->addAction(uiText("Reset Zoom", "重置缩放"), QKeySequence(Qt::CTRL | Qt::Key_0), this, &MainWindow::resetZoom);
    viewMenu_->addSeparator();

    displayModeActionGroup_ = new QActionGroup(this);
    displayModeActionGroup_->setExclusive(true);
    displayModeActions_.clear();

    fitToWindowAction_ = viewMenu_->addAction(uiText("Fit to Window", "适合窗口"), this, &MainWindow::setFitToWindowMode);
    fitToWindowAction_->setCheckable(true);
    fitToWindowAction_->setData(static_cast<int>(ImageViewerWidget::DisplayMode::FitToWindow));
    displayModeActionGroup_->addAction(fitToWindowAction_);
    displayModeActions_.append(fitToWindowAction_);

    actualSizeAction_ = viewMenu_->addAction(uiText("Actual Size", "实际大小"), this, &MainWindow::setActualSizeMode);
    actualSizeAction_->setCheckable(true);
    actualSizeAction_->setData(static_cast<int>(ImageViewerWidget::DisplayMode::ActualSize));
    displayModeActionGroup_->addAction(actualSizeAction_);
    displayModeActions_.append(actualSizeAction_);

    fillWindowAction_ = viewMenu_->addAction(uiText("Fill Window", "填充窗口"), this, &MainWindow::setFillWindowMode);
    fillWindowAction_->setCheckable(true);
    fillWindowAction_->setData(static_cast<int>(ImageViewerWidget::DisplayMode::FillWindow));
    displayModeActionGroup_->addAction(fillWindowAction_);
    displayModeActions_.append(fillWindowAction_);

    setDisplayModeChecked(viewer_->displayMode());
    viewMenu_->addSeparator();
    thumbnailStripAction_ = viewMenu_->addAction(uiText("Show Thumbnail Strip", "显示缩略图栏"), Qt::Key_T, this, &MainWindow::toggleThumbnailStrip);
    thumbnailStripAction_->setCheckable(true);
    thumbnailAutoHideAction_ = viewMenu_->addAction(uiText("Auto-hide Thumbnail Strip", "自动隐藏缩略图栏"), this, &MainWindow::toggleAutoHideThumbnailStrip);
    thumbnailAutoHideAction_->setCheckable(true);
    updateThumbnailActions();
    applyThumbnailStripVisibility(false, false);

    createFileAssociationMenu();

    languageMenu_ = menuBar()->addMenu(uiText("&Language", "&语言"));
    languageActionGroup_ = new QActionGroup(this);
    languageActionGroup_->setExclusive(true);
    englishLanguageAction_ = languageMenu_->addAction(AppI18n::languageDisplayName(AppI18n::Language::English));
    englishLanguageAction_->setCheckable(true);
    languageActionGroup_->addAction(englishLanguageAction_);
    chineseLanguageAction_ = languageMenu_->addAction(AppI18n::languageDisplayName(AppI18n::Language::Chinese));
    chineseLanguageAction_->setCheckable(true);
    languageActionGroup_->addAction(chineseLanguageAction_);
    connect(englishLanguageAction_, &QAction::toggled, this, [this](bool checked) {
        if (checked) {
            AppI18n::setCurrentLanguage(AppI18n::Language::English);
            retranslateUi();
        }
    });
    connect(chineseLanguageAction_, &QAction::toggled, this, [this](bool checked) {
        if (checked) {
            AppI18n::setCurrentLanguage(AppI18n::Language::Chinese);
            retranslateUi();
        }
    });

    if (englishLanguageAction_) {
        QSignalBlocker blocker(englishLanguageAction_);
        englishLanguageAction_->setChecked(AppI18n::currentLanguage() == AppI18n::Language::English);
    }
    if (chineseLanguageAction_) {
        QSignalBlocker blocker(chineseLanguageAction_);
        chineseLanguageAction_->setChecked(AppI18n::currentLanguage() == AppI18n::Language::Chinese);
    }
}

void MainWindow::createFileAssociationMenu()
{
    fileAssociationMenu_ = menuBar()->addMenu(uiText("File &Associations", "文件关联"));

    associateAllAction_ = fileAssociationMenu_->addAction(uiText("Associate All", "全部关联"));
    clearAllAction_ = fileAssociationMenu_->addAction(uiText("Clear All", "清除关联"));
    fileAssociationMenu_->addSeparator();

    auto addGroupMenu = [this](QMenu* parent, const FileAssociationGroup& group) {
        QMenu* groupMenu = parent->addMenu(uiText(group.englishTitle, group.chineseTitle));
        fileAssociationGroupMenus_.push_back({groupMenu, group.englishTitle, group.chineseTitle});
        for (const QString& extension : normalizedExtensions(group.extensions)) {
            QAction* action = groupMenu->addAction(fileAssociationDisplayName(extension));
            action->setCheckable(true);
            fileAssociationActions_.insert(extension, action);
            connect(action, &QAction::toggled, this, [this, extension, action](bool enabled) {
                if (setFileAssociationEnabled(extension, enabled)) {
                    refreshFileAssociationActions();
                    return;
                }

                QSignalBlocker blocker(action);
                action->setChecked(isFileAssociationEnabled(extension));
                QMessageBox::warning(
                    this,
                    uiText("File Associations", "文件关联"),
#ifdef Q_OS_WIN
                    uiText(
                        "Failed to update the file association for *.%1.\n\nWindows registry changes were not applied.",
                        "无法更新 *.%1 的文件关联。\n\nWindows 注册表更改未生效。").arg(extension)
#else
                    uiText(
                        "File associations are only supported on Windows in this build.",
                        "当前构建仅在 Windows 上支持文件关联。")
#endif
                );
            });
        }
    };

    commonAssociationMenu_ = fileAssociationMenu_->addMenu(uiText("Common Formats", "常见格式"));
    for (const FileAssociationGroup& group : commonAssociationGroups()) {
        addGroupMenu(commonAssociationMenu_, group);
    }

    rawAssociationMenu_ = fileAssociationMenu_->addMenu(uiText("RAW Formats", "RAW 格式"));
    for (const FileAssociationGroup& group : rawAssociationGroups()) {
        addGroupMenu(rawAssociationMenu_, group);
    }

    fileAssociationMenu_->addSeparator();
    refreshFileAssociationAction_ = fileAssociationMenu_->addAction(uiText("Refresh Status", "刷新状态"));

    connect(associateAllAction_, &QAction::triggered, this, [this]() {
        if (!setFileAssociationsEnabled(allAssociationExtensions(), true)) {
            QMessageBox::warning(
                this,
                uiText("File Associations", "文件关联"),
#ifdef Q_OS_WIN
                uiText(
                    "Some or all file associations could not be written. Check Windows permissions.",
                    "部分或全部文件关联无法写入，请检查 Windows 权限。")
#else
                uiText(
                    "File associations are only supported on Windows in this build.",
                    "当前构建仅在 Windows 上支持文件关联。")
#endif
            );
        }
        refreshFileAssociationActions();
    });

    connect(clearAllAction_, &QAction::triggered, this, [this]() {
        if (!setFileAssociationsEnabled(allAssociationExtensions(), false)) {
            QMessageBox::warning(
                this,
                uiText("File Associations", "文件关联"),
#ifdef Q_OS_WIN
                uiText(
                    "Some or all file associations could not be cleared. Check Windows permissions.",
                    "部分或全部文件关联无法清除，请检查 Windows 权限。")
#else
                uiText(
                    "File associations are only supported on Windows in this build.",
                    "当前构建仅在 Windows 上支持文件关联。")
#endif
            );
        }
        refreshFileAssociationActions();
    });

    connect(refreshFileAssociationAction_, &QAction::triggered, this, &MainWindow::refreshFileAssociationActions);
    connect(fileAssociationMenu_, &QMenu::aboutToShow, this, &MainWindow::refreshFileAssociationActions);

#ifndef Q_OS_WIN
    fileAssociationMenu_->setEnabled(false);
#endif

    refreshFileAssociationActions();
}

void MainWindow::refreshFileAssociationActions()
{
    for (auto it = fileAssociationActions_.cbegin(); it != fileAssociationActions_.cend(); ++it) {
        syncFileAssociationAction(it.key());
    }
}

void MainWindow::syncFileAssociationAction(const QString& extension)
{
    if (QAction* action = fileAssociationActions_.value(extension)) {
        QSignalBlocker blocker(action);
        action->setChecked(isFileAssociationEnabled(extension));
    }
}

void MainWindow::createInfoPanel()
{
    infoDock_ = new QDockWidget(uiText("Image Info", "图片信息"), this);
    infoDock_->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    infoDock_->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    infoDock_->setMinimumWidth(280);
    infoDock_->setVisible(false);

    infoText_ = new QPlainTextEdit(infoDock_);
    infoText_->setReadOnly(true);
    infoText_->setFrameShape(QFrame::NoFrame);
    infoText_->setStyleSheet(R"(
        QPlainTextEdit {
            background: #101010;
            color: #f0f0f0;
            border: none;
            selection-background-color: #2a82da;
        }
    )");
    infoDock_->setWidget(infoText_);
    addDockWidget(Qt::LeftDockWidgetArea, infoDock_);
    connect(infoDock_, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        if (infoPanelAction_) {
            infoPanelAction_->setChecked(visible);
            infoPanelAction_->setText(visible ? uiText("Hide Info Panel", "隐藏信息面板") : uiText("Show Info Panel", "显示信息面板"));
        }
    });
}

void MainWindow::retranslateUi()
{
    if (fileMenu_) {
        fileMenu_->setTitle(uiText("&File", "&文件"));
    }
    if (openFileAction_) {
        openFileAction_->setText(uiText("&Open File...", "&打开文件..."));
    }
    if (openFolderAction_) {
        openFolderAction_->setText(uiText("Open &Folder...", "打开&文件夹..."));
    }
    if (exitAction_) {
        exitAction_->setText(uiText("E&xit", "退&出"));
    }
    if (playbackMenu_) {
        playbackMenu_->setTitle(uiText("&Playback", "&播放"));
    }
    if (playPauseAction_) {
        playPauseAction_->setText(slideshow_ && slideshow_->isPlaying()
            ? uiText("Pause Slideshow", "暂停幻灯片放映")
            : uiText("Start Slideshow", "开始幻灯片放映"));
    }
    for (QAction* action : intervalActions_) {
        if (action) {
            action->setText(modeLabelForAction(action->data().toInt()));
        }
    }
    if (viewMenu_) {
        viewMenu_->setTitle(uiText("&View", "&视图"));
    }
    if (fullscreenAction_) {
        fullscreenAction_->setText(isFullScreen()
            ? uiText("Exit Fullscreen", "退出全屏")
            : uiText("Enter Fullscreen", "进入全屏"));
    }
    if (infoPanelAction_) {
        infoPanelAction_->setText(infoDock_ && infoDock_->isVisible()
            ? uiText("Hide Info Panel", "隐藏信息面板")
            : uiText("Show Info Panel", "显示信息面板"));
    }
    if (zoomInAction_) {
        zoomInAction_->setText(uiText("Zoom In", "放大"));
    }
    if (zoomOutAction_) {
        zoomOutAction_->setText(uiText("Zoom Out", "缩小"));
    }
    if (resetZoomAction_) {
        resetZoomAction_->setText(uiText("Reset Zoom", "重置缩放"));
    }
    for (QAction* action : displayModeActions_) {
        if (!action) {
            continue;
        }
        switch (static_cast<ImageViewerWidget::DisplayMode>(action->data().toInt())) {
        case ImageViewerWidget::DisplayMode::FitToWindow:
            action->setText(uiText("Fit to Window", "适合窗口"));
            break;
        case ImageViewerWidget::DisplayMode::ActualSize:
            action->setText(uiText("Actual Size", "实际大小"));
            break;
        case ImageViewerWidget::DisplayMode::FillWindow:
            action->setText(uiText("Fill Window", "填充窗口"));
            break;
        }
    }
    if (thumbnailStripAction_) {
        thumbnailStripAction_->setText(thumbnailStripVisible_
            ? uiText("Hide Thumbnail Strip", "隐藏缩略图栏")
            : uiText("Show Thumbnail Strip", "显示缩略图栏"));
    }
    if (thumbnailAutoHideAction_) {
        thumbnailAutoHideAction_->setText(uiText("Auto-hide Thumbnail Strip", "自动隐藏缩略图栏"));
    }
    if (languageMenu_) {
        languageMenu_->setTitle(uiText("&Language", "&语言"));
    }
    if (englishLanguageAction_) {
        englishLanguageAction_->setText(AppI18n::languageDisplayName(AppI18n::Language::English));
    }
    if (chineseLanguageAction_) {
        chineseLanguageAction_->setText(AppI18n::languageDisplayName(AppI18n::Language::Chinese));
    }
    if (englishLanguageAction_) {
        QSignalBlocker blocker(englishLanguageAction_);
        englishLanguageAction_->setChecked(AppI18n::currentLanguage() == AppI18n::Language::English);
    }
    if (chineseLanguageAction_) {
        QSignalBlocker blocker(chineseLanguageAction_);
        chineseLanguageAction_->setChecked(AppI18n::currentLanguage() == AppI18n::Language::Chinese);
    }
    if (fileAssociationMenu_) {
        fileAssociationMenu_->setTitle(uiText("File &Associations", "文件关联"));
    }
    if (associateAllAction_) {
        associateAllAction_->setText(uiText("Associate All", "全部关联"));
    }
    if (clearAllAction_) {
        clearAllAction_->setText(uiText("Clear All", "清除关联"));
    }
    if (refreshFileAssociationAction_) {
        refreshFileAssociationAction_->setText(uiText("Refresh Status", "刷新状态"));
    }
    if (commonAssociationMenu_) {
        commonAssociationMenu_->setTitle(uiText("Common Formats", "常见格式"));
    }
    if (rawAssociationMenu_) {
        rawAssociationMenu_->setTitle(uiText("RAW Formats", "RAW 格式"));
    }
    for (const FileAssociationGroupUi& groupUi : fileAssociationGroupMenus_) {
        if (groupUi.menu) {
            groupUi.menu->setTitle(uiText(groupUi.englishTitle, groupUi.chineseTitle));
        }
    }
    if (infoDock_) {
        infoDock_->setWindowTitle(uiText("Image Info", "图片信息"));
    }
    if (viewer_) {
        viewer_->retranslateUi();
        if (currentPath_.isEmpty()) {
            viewer_->setMessage(uiText("No image selected", "未选择图片"));
        } else if (!lastDecodedImage_.errorMessage.isEmpty() && lastDecodedImage_.filePath == currentPath_) {
            viewer_->setMessage(uiText("Failed to decode image", "图片解码失败"), lastDecodedImage_.errorMessage);
        } else if (displayedPath_ != currentPath_ && !viewer_->hasImage()) {
            viewer_->setLoading(uiText("Loading image...", "正在加载图片..."), QFileInfo(currentPath_).fileName());
        }
    }
    updateInfoPanel(currentPath_.isEmpty() ? nullptr : (lastDecodedImage_.filePath == currentPath_ ? &lastDecodedImage_ : nullptr));
}

void MainWindow::openPath(const QString& path)
{
    if (!catalog_.loadFromPath(path)) {
        viewer_->setMessage(uiText("No supported images found", "未找到支持的图片"), QFileInfo(path).absoluteFilePath());
        currentPath_.clear();
        displayedPath_.clear();
        lastDecodedImage_ = {};
        cache_.clear();
        thumbnailCache_.clear();
        thumbnailCacheLru_.clear();
        thumbnailCacheBytes_ = 0;
        thumbnailRequestsInFlight_.clear();
        thumbnailRequestQueue_.clear();
        rebuildThumbnailStrip();
        thumbnailStripContainer_->hide();
        thumbnailStripVisible_ = false;
        updateThumbnailActions();
        imagePrefetchRequestsInFlight_.clear();
        imagePrefetchRequestQueue_.clear();
        updateInfoPanel();
        return;
    }

    cache_.clear();
    thumbnailCache_.clear();
    thumbnailCacheLru_.clear();
    thumbnailCacheBytes_ = 0;
    thumbnailRequestsInFlight_.clear();
    thumbnailRequestQueue_.clear();
    imagePrefetchRequestsInFlight_.clear();
    imagePrefetchRequestQueue_.clear();
    rebuildThumbnailStrip();
    const bool hasImages = !catalog_.isEmpty();
    thumbnailStripContainer_->setVisible(hasImages);
    thumbnailStripVisible_ = hasImages;
    updateThumbnailActions();
    refreshCurrentImage();
}

void MainWindow::refreshCurrentImage()
{
    currentPath_ = catalog_.currentPath();
    if (currentPath_.isEmpty()) {
        viewer_->setMessage(uiText("No image selected", "未选择图片"));
        displayedPath_.clear();
        lastDecodedImage_ = {};
        updateInfoPanel();
        return;
    }

    const bool hasImmediateImage = cache_.contains(currentPath_, DecodeMode::FastPreview)
        || cache_.contains(currentPath_, DecodeMode::FullQuality);
    if (!hasImmediateImage && displayedPath_ != currentPath_) {
        viewer_->setLoading(uiText("Loading image...", "正在加载图片..."), QFileInfo(currentPath_).fileName());
    }
    updateThumbnailSelection();

    requestImage(currentPath_, DecodeMode::FastPreview, true);
    queuePrefetchForCurrentContext();
}

void MainWindow::requestImage(const QString& path, DecodeMode mode, bool displayWhenReady)
{
    if (cache_.contains(path, mode)) {
        const DecodedImage cached = cache_.get(path, mode);
        if (displayWhenReady && path == currentPath_) {
            displayDecodedImage(cached);
            const ImageFormatKind kind = ImageDecoder::detectFormat(path);
            if (mode == DecodeMode::FastPreview
                && ImageDecoder::supportsFullQuality(kind)
                && !cache_.contains(path, DecodeMode::FullQuality)) {
                requestImage(path, DecodeMode::FullQuality, true);
            }
        }
        return;
    }

    const qint64 sequence = displayWhenReady ? ++displaySequence_ : displaySequence_;
    auto* watcher = new QFutureWatcher<DecodedImage>(this);
    connect(watcher, &QFutureWatcher<DecodedImage>::finished, this, [this, watcher, path, mode, sequence, displayWhenReady]() {
        const DecodedImage image = watcher->result();
        watcher->deleteLater();
        cache_.put(path, mode, image);
        if (displayWhenReady) {
            handleDecodedImage(path, mode, sequence, image);
        }
    });
    watcher->setFuture(QtConcurrent::run(&imageDecodePool_, [path, mode]() {
        return ImageDecoder::decode(path, mode);
    }));
}

void MainWindow::queuePrefetchForCurrentContext()
{
    const QStringList paths = catalog_.paths();
    if (paths.isEmpty() || currentPath_.isEmpty()) {
        return;
    }

    const int currentIndex = catalog_.currentIndex();
    const QList<int> hotIndices = prefetchScheduler_.orderedIndices(
        paths.size(),
        currentIndex,
        browseDirection_,
        kImmediatePrefetchDistance,
        3);

    QSet<QString> hotPaths;
    QList<PrefetchScheduler::Request> hotRequests;
    hotRequests.reserve(hotIndices.size());
    int hotPriority = 0;
    for (const int index : hotIndices) {
        if (index < 0 || index >= paths.size()) {
            continue;
        }
        const QString path = paths.at(index);
        if (path == currentPath_) {
            continue;
        }
        hotPaths.insert(path);
        hotRequests.push_back({path, DecodeMode::FastPreview, hotPriority++});
    }

    const QList<PrefetchScheduler::Request> requests = prefetchScheduler_.planImageRequests(
        paths,
        currentIndex,
        browseDirection_,
        kImagePrefetchWindow,
        kMaxQueuedImagePrefetchRequests);
    QList<PrefetchScheduler::Request> deferredRequests;
    deferredRequests.reserve(requests.size());
    for (const auto& request : requests) {
        if (request.path == currentPath_ || hotPaths.contains(request.path)) {
            continue;
        }
        deferredRequests.push_back(
            {request.path, request.mode, kDeferredPrefetchPriorityBase + request.priority});
    }

    imagePrefetchRequestQueue_.clear();
    enqueueImagePrefetchRequests(hotRequests);
    enqueueImagePrefetchRequests(deferredRequests);
}

void MainWindow::enqueueImagePrefetchRequests(const QList<PrefetchScheduler::Request>& requests)
{
    for (const auto& request : requests) {
        enqueueImagePrefetchRequest(request.path, request.mode, request.priority);
    }
    processPendingImagePrefetchRequests();
}

void MainWindow::handleDecodedImage(const QString& path, DecodeMode mode, qint64 sequence, const DecodedImage& image)
{
    if (path != currentPath_ || sequence != displaySequence_) {
        return;
    }

    displayDecodedImage(image);

    if (mode == DecodeMode::FastPreview) {
        const ImageFormatKind kind = ImageDecoder::detectFormat(path);
        if (ImageDecoder::supportsFullQuality(kind) && !cache_.contains(path, DecodeMode::FullQuality)) {
            requestImage(path, DecodeMode::FullQuality, true);
        }
    }
}

void MainWindow::displayDecodedImage(const DecodedImage& image)
{
    if (image.isValid()) {
        const bool changedPath = displayedPath_ != image.filePath;
        lastDecodedImage_ = image;
        viewer_->setImage(image.image);
        displayedPath_ = image.filePath;
        updateThumbnailForPath(image.filePath, image.image);
        preloadThumbnailNeighbors();
        requestVisibleThumbnails();
        if (changedPath) {
            const int index = std::max(0, catalog_.currentIndex());
            const int total = catalog_.size();
            viewer_->showTransientImageInfo(QString("%1  %2 / %3")
                .arg(QFileInfo(image.filePath).fileName())
                .arg(total > 0 ? index + 1 : 0)
                .arg(total));
        }
    } else {
        if (!viewer_->hasImage()) {
            viewer_->setMessage(uiText("Failed to decode image", "图片解码失败"), image.errorMessage);
            displayedPath_.clear();
            lastDecodedImage_ = image;
        }
    }
    updateInfoPanel(&image);
}

void MainWindow::toggleFullscreen()
{
    if (isFullScreen()) {
        showNormal();
        applyFullscreenMenuBarVisibility(true);
    } else {
        showFullScreen();
        applyFullscreenMenuBarVisibility(false);
    }
    if (fullscreenAction_) {
        fullscreenAction_->setText(isFullScreen()
            ? uiText("Exit Fullscreen", "退出全屏")
            : uiText("Enter Fullscreen", "进入全屏"));
    }
}

void MainWindow::zoomIn()
{
    viewer_->zoomIn();
}

void MainWindow::zoomOut()
{
    viewer_->zoomOut();
}

void MainWindow::resetZoom()
{
    viewer_->resetZoom();
}

void MainWindow::setFitToWindowMode()
{
    viewer_->setDisplayMode(ImageViewerWidget::DisplayMode::FitToWindow);
    setDisplayModeChecked(viewer_->displayMode());
}

void MainWindow::setActualSizeMode()
{
    viewer_->setDisplayMode(ImageViewerWidget::DisplayMode::ActualSize);
    setDisplayModeChecked(viewer_->displayMode());
}

void MainWindow::setFillWindowMode()
{
    viewer_->setDisplayMode(ImageViewerWidget::DisplayMode::FillWindow);
    setDisplayModeChecked(viewer_->displayMode());
}

void MainWindow::thumbnailActivated(QListWidgetItem* item)
{
    if (!item) {
        return;
    }

    const QString path = item->data(Qt::UserRole).toString();
    if (catalog_.setCurrentPath(path)) {
        refreshCurrentImage();
    }
}


void MainWindow::setIntervalActionChecked(int intervalMs)
{
    for (QAction* action : intervalActionGroup_->actions()) {
        action->setChecked(action->data().toInt() == intervalMs);
    }
}

void MainWindow::createThumbnailStrip()
{
    thumbnailStripContainer_ = new QWidget(this);
    thumbnailStripContainer_->setMaximumHeight(kThumbnailStripExpandedHeight);
    thumbnailStripContainer_->setStyleSheet("background-color: black;");
    auto* layout = new QVBoxLayout(thumbnailStripContainer_);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    thumbnailList_ = new QListWidget(thumbnailStripContainer_);
    thumbnailList_->setViewMode(QListView::IconMode);
    thumbnailList_->setFlow(QListView::LeftToRight);
    thumbnailList_->setResizeMode(QListView::Adjust);
    thumbnailList_->setMovement(QListView::Static);
    thumbnailList_->setWrapping(false);
    thumbnailList_->setSpacing(8);
    thumbnailList_->setIconSize(QSize(kThumbnailSize, kThumbnailSize));
    thumbnailList_->setMaximumHeight(kThumbnailStripExpandedHeight);
    thumbnailList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    thumbnailList_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    thumbnailList_->setStyleSheet(R"(
        QListWidget {
            background: #000000;
            color: #f0f0f0;
            border: none;
        }
        QListWidget::item {
            color: #f0f0f0;
            padding: 4px 2px;
        }
        QListWidget::item:selected {
            background: #2a82da;
            color: #ffffff;
        }
    )");
    layout->addWidget(thumbnailList_);
    thumbnailStripContainer_->hide();
}

void MainWindow::rebuildThumbnailStrip()
{
    thumbnailList_->clear();
    for (const QString& path : catalog_.paths()) {
        auto* item = new QListWidgetItem(QFileInfo(path).fileName(), thumbnailList_);
        item->setData(Qt::UserRole, path);
        item->setTextAlignment(Qt::AlignHCenter);
        const auto cached = thumbnailCache_.value(thumbnailKey(path));
        if (!cached.isNull()) {
            item->setIcon(QIcon(cached));
        }
    }
    updateThumbnailSelection();
    QTimer::singleShot(0, this, [this]() {
        requestVisibleThumbnails();
    });
}

void MainWindow::updateThumbnailSelection()
{
    for (int i = 0; i < thumbnailList_->count(); ++i) {
        QListWidgetItem* item = thumbnailList_->item(i);
        const bool selected = item->data(Qt::UserRole).toString() == currentPath_;
        item->setSelected(selected);
        if (selected) {
            thumbnailList_->scrollToItem(item, QAbstractItemView::PositionAtCenter);
        }
    }
}

void MainWindow::updateThumbnailForPath(const QString& path, const QImage& image)
{
    if (path.isEmpty() || image.isNull()) {
        return;
    }

    const QPixmap thumb = QPixmap::fromImage(
        image.scaled(kThumbnailSize, kThumbnailSize, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    const QString key = thumbnailKey(path);
    if (thumbnailCache_.contains(key)) {
        thumbnailCacheBytes_ -= std::max<qint64>(1, static_cast<qint64>(thumbnailCache_.value(key).width()) * thumbnailCache_.value(key).height() * 4);
        thumbnailCacheLru_.removeAll(key);
    }
    thumbnailCache_.insert(key, thumb);
    thumbnailCacheBytes_ += estimatePixmapCost(thumb);
    thumbnailCacheLru_.prepend(key);
    while (thumbnailCacheBytes_ > kThumbnailCacheBudgetBytes && !thumbnailCacheLru_.isEmpty()) {
        const QString oldest = thumbnailCacheLru_.takeLast();
        if (!thumbnailCache_.contains(oldest)) {
            continue;
        }
        const QPixmap removed = thumbnailCache_.value(oldest);
        thumbnailCacheBytes_ -= estimatePixmapCost(removed);
        thumbnailCache_.remove(oldest);
    }

    for (int i = 0; i < thumbnailList_->count(); ++i) {
        QListWidgetItem* item = thumbnailList_->item(i);
        if (item->data(Qt::UserRole).toString() == path) {
            item->setIcon(QIcon(thumb));
            return;
        }
    }
}

void MainWindow::requestThumbnail(const QString& path)
{
    enqueueThumbnailRequest(path, 0);
    processPendingThumbnailRequests();
}

void MainWindow::enqueueThumbnailRequest(const QString& path, int priority)
{
    if (path.isEmpty()
        || thumbnailCache_.contains(thumbnailKey(path))
        || thumbnailRequestsInFlight_.contains(path)
        || std::any_of(thumbnailRequestQueue_.begin(), thumbnailRequestQueue_.end(), [&path](const ThumbnailJob& job) {
               return job.path == path;
           })) {
        return;
    }

    if (thumbnailRequestQueue_.size() >= kMaxQueuedThumbnailRequests) {
        const ThumbnailJob worst = thumbnailRequestQueue_.back();
        if (priority >= worst.priority) {
            return;
        }
        thumbnailRequestQueue_.removeLast();
    }
    thumbnailRequestQueue_.push_back({path, priority});
    std::stable_sort(thumbnailRequestQueue_.begin(), thumbnailRequestQueue_.end(), [](const ThumbnailJob& lhs, const ThumbnailJob& rhs) {
        return lhs.priority < rhs.priority;
    });
}

void MainWindow::preloadThumbnailNeighbors()
{
    const QStringList paths = catalog_.paths();
    if (paths.isEmpty()) {
        return;
    }

    const int index = std::max(0, catalog_.currentIndex());
    const int total = paths.size();
    for (int offset = -4; offset <= 4; ++offset) {
        const int wrapped = (index + offset + total) % total;
        enqueueThumbnailRequest(paths.at(wrapped), std::abs(offset));
    }
    processPendingThumbnailRequests();
}

void MainWindow::requestVisibleThumbnails()
{
    if (!thumbnailList_ || thumbnailList_->count() == 0) {
        return;
    }

    const QRect viewportRect = thumbnailList_->viewport()->rect();
    for (int i = 0; i < thumbnailList_->count(); ++i) {
        QListWidgetItem* item = thumbnailList_->item(i);
        const QRect itemRect = thumbnailList_->visualItemRect(item);
        if (itemRect.right() < viewportRect.left() - kThumbnailSize
            || itemRect.left() > viewportRect.right() + kThumbnailSize) {
            continue;
        }
        enqueueThumbnailRequest(item->data(Qt::UserRole).toString(), i);
    }
    processPendingThumbnailRequests();
}

void MainWindow::processPendingThumbnailRequests()
{
    while (!thumbnailRequestQueue_.isEmpty() && thumbnailRequestsInFlight_.size() < kMaxThumbnailInflight) {
        const ThumbnailJob job = thumbnailRequestQueue_.takeFirst();
        const QString path = job.path;
        thumbnailRequestsInFlight_.insert(path);
        auto* watcher = new QFutureWatcher<QImage>(this);
        connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, path]() {
            const QImage image = watcher->result();
            watcher->deleteLater();
            thumbnailRequestsInFlight_.remove(path);
            if (!image.isNull()) {
                updateThumbnailForPath(path, image);
            }
            processPendingThumbnailRequests();
        });
        watcher->setFuture(QtConcurrent::run(&thumbnailDecodePool_, [path]() -> QImage {
            return ImageDecoder::decodeThumbnail(path, kThumbnailSize);
        }));
    }
}

void MainWindow::enqueueImagePrefetchRequest(const QString& path, DecodeMode mode, int priority)
{
    const QString key = prefetchKey(path, mode);
    if (path.isEmpty()
        || cache_.contains(path, mode)
        || imagePrefetchRequestsInFlight_.contains(key)
        || std::any_of(imagePrefetchRequestQueue_.begin(), imagePrefetchRequestQueue_.end(), [this, &key](const ImagePrefetchJob& job) {
               return prefetchKey(job.path, job.mode) == key;
           })) {
        return;
    }

    if (imagePrefetchRequestQueue_.size() >= kMaxQueuedImagePrefetchRequests) {
        const ImagePrefetchJob worst = imagePrefetchRequestQueue_.back();
        if (priority >= worst.priority) {
            return;
        }
        imagePrefetchRequestQueue_.removeLast();
    }
    imagePrefetchRequestQueue_.push_back({path, mode, priority});
    std::stable_sort(imagePrefetchRequestQueue_.begin(), imagePrefetchRequestQueue_.end(), [](const ImagePrefetchJob& lhs, const ImagePrefetchJob& rhs) {
        return lhs.priority < rhs.priority;
    });
}

void MainWindow::processPendingImagePrefetchRequests()
{
    const int maxInflight = imagePrefetchThreadCount();
    while (!imagePrefetchRequestQueue_.isEmpty() && imagePrefetchRequestsInFlight_.size() < maxInflight) {
        const ImagePrefetchJob job = imagePrefetchRequestQueue_.takeFirst();
        const QString path = job.path;
        const DecodeMode mode = job.mode;
        const QString key = prefetchKey(path, mode);
        imagePrefetchRequestsInFlight_.insert(key);
        auto* watcher = new QFutureWatcher<DecodedImage>(this);
        connect(watcher, &QFutureWatcher<DecodedImage>::finished, this, [this, watcher, path, mode, key]() {
            const DecodedImage image = watcher->result();
            watcher->deleteLater();
            imagePrefetchRequestsInFlight_.remove(key);
            if (image.isValid()) {
                cache_.put(path, mode, image);
            }
            processPendingImagePrefetchRequests();
        });
        watcher->setFuture(QtConcurrent::run(&imageDecodePool_, [path, mode]() {
            return ImageDecoder::decode(path, mode);
        }));
    }
}

QString MainWindow::prefetchKey(const QString& path, DecodeMode mode) const
{
    return path + "::" + QString::number(static_cast<int>(mode));
}

void MainWindow::setBrowseDirection(PrefetchScheduler::Direction direction)
{
    browseDirection_ = direction;
}

void MainWindow::setDisplayModeChecked(ImageViewerWidget::DisplayMode mode)
{
    if (!displayModeActionGroup_) {
        return;
    }

    for (QAction* action : displayModeActionGroup_->actions()) {
        action->setChecked(action->data().toInt() == static_cast<int>(mode));
    }
}

bool MainWindow::isFileAssociationEnabled(const QString& extension) const
{
#ifdef Q_OS_WIN
    QString currentProgId;
    if (!readRegistryDefaultString(HKEY_CURRENT_USER, registryPathForExtension(extension), &currentProgId)) {
        return false;
    }
    return currentProgId.compare(fileAssociationProgId(extension), Qt::CaseInsensitive) == 0;
#else
    Q_UNUSED(extension);
    return false;
#endif
}

bool MainWindow::setFileAssociationEnabled(const QString& extension, bool enabled, bool notifyShell)
{
#ifndef Q_OS_WIN
    Q_UNUSED(extension);
    Q_UNUSED(enabled);
    Q_UNUSED(notifyShell);
    return false;
#else
    const QString normalizedExtension = extension.toLower();
    const QString progId = fileAssociationProgId(normalizedExtension);
    const QString extensionKey = registryPathForExtension(normalizedExtension);
    const QString progIdKey = registryPathForProgId(progId);
    QString errorMessage;

    if (isFileAssociationEnabled(normalizedExtension) == enabled) {
        return true;
    }

    if (enabled) {
        if (!setRegistryDefaultString(HKEY_CURRENT_USER, progIdKey + "\\DefaultIcon", fileAssociationIcon(), &errorMessage)) {
            return false;
        }
        if (!setRegistryDefaultString(HKEY_CURRENT_USER, progIdKey + "\\shell\\open\\command", fileAssociationCommand(), &errorMessage)) {
            deleteRegistryTreeIfExists(HKEY_CURRENT_USER, progIdKey, nullptr);
            return false;
        }
        if (!setRegistryDefaultString(HKEY_CURRENT_USER, extensionKey, progId, &errorMessage)) {
            deleteRegistryTreeIfExists(HKEY_CURRENT_USER, progIdKey, nullptr);
            return false;
        }
        if (notifyShell) {
            SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
        }
        return true;
    }

    QString currentProgId;
    const bool hasOurAssociation = readRegistryDefaultString(HKEY_CURRENT_USER, extensionKey, &currentProgId)
        && currentProgId.compare(progId, Qt::CaseInsensitive) == 0;
    bool ok = true;
    if (hasOurAssociation) {
        ok = deleteRegistryDefaultValue(HKEY_CURRENT_USER, extensionKey, &errorMessage);
    }
    ok = ok && deleteRegistryTreeIfExists(HKEY_CURRENT_USER, progIdKey, &errorMessage);
    if (ok && notifyShell) {
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    }
    return ok;
#endif
}

bool MainWindow::setFileAssociationsEnabled(const QStringList& extensions, bool enabled)
{
#ifndef Q_OS_WIN
    Q_UNUSED(extensions);
    Q_UNUSED(enabled);
    return false;
#else
    bool ok = true;
    bool changed = false;
    for (const QString& extension : extensions) {
        const bool result = setFileAssociationEnabled(extension, enabled, false);
        ok = ok && result;
        changed = changed || result;
    }
    if (changed) {
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    }
    return ok;
#endif
}

QString MainWindow::thumbnailKey(const QString& path) const
{
    return path;
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (isFullScreen()) {
        if (event->type() == QEvent::KeyPress) {
            const auto* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Alt) {
                applyFullscreenMenuBarVisibility(true);
                fullscreenMenuBarAutoHideTimer_->start();
            }
        } else if (event->type() == QEvent::MouseMove || event->type() == QEvent::Enter) {
            if (watched == menuBar()) {
                applyFullscreenMenuBarVisibility(true);
                fullscreenMenuBarAutoHideTimer_->stop();
            } else {
                maybeShowFullscreenMenuBarForCursor();
            }
        } else if (event->type() == QEvent::Leave) {
            if (watched == menuBar()) {
                fullscreenMenuBarAutoHideTimer_->start();
            }
        }
    }

    if (thumbnailAutoHideEnabled_) {
        if (event->type() == QEvent::MouseMove || event->type() == QEvent::Enter) {
            maybeShowThumbnailStripForCursor();
        } else if (event->type() == QEvent::Leave) {
            if (watched == thumbnailList_ || watched == thumbnailList_->viewport()) {
                thumbnailAutoHideTimer_->start();
            }
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::toggleThumbnailStrip()
{
    setThumbnailStripVisible(!thumbnailStripVisible_);
}

void MainWindow::toggleInfoPanel()
{
    const bool nextVisible = !infoDock_ || !infoDock_->isVisible();
    if (infoDock_) {
        infoDock_->setVisible(nextVisible);
    }
    if (infoPanelAction_) {
        infoPanelAction_->setChecked(nextVisible);
        infoPanelAction_->setText(nextVisible ? uiText("Hide Info Panel", "隐藏信息面板") : uiText("Show Info Panel", "显示信息面板"));
    }
}

void MainWindow::setThumbnailStripVisible(bool visible)
{
    thumbnailAutoHideEnabled_ = false;
    if (thumbnailAutoHideAction_) {
        thumbnailAutoHideAction_->setChecked(false);
    }
    applyThumbnailStripVisibility(visible, true);
}

void MainWindow::toggleAutoHideThumbnailStrip()
{
    thumbnailAutoHideEnabled_ = !thumbnailAutoHideEnabled_;
    if (!thumbnailAutoHideEnabled_) {
        thumbnailAutoHideTimer_->stop();
        applyThumbnailStripVisibility(true, true);
    } else {
        applyThumbnailStripVisibility(false, true);
    }
    updateThumbnailActions();
}

void MainWindow::applyThumbnailStripVisibility(bool visible, bool animated)
{
    thumbnailStripVisible_ = visible;
    updateThumbnailActions();

    if (visible) {
        thumbnailStripContainer_->show();
    }

    thumbnailStripAnimation_->stop();
    const int targetHeight = visible ? kThumbnailStripExpandedHeight : 0;
    if (!animated) {
        thumbnailStripContainer_->setMaximumHeight(targetHeight);
        thumbnailStripContainer_->setMinimumHeight(targetHeight);
        if (!visible) {
            thumbnailStripContainer_->hide();
        }
        return;
    }

    thumbnailStripAnimation_->setStartValue(thumbnailStripContainer_->maximumHeight());
    thumbnailStripAnimation_->setEndValue(targetHeight);
    if (!visible) {
        connect(thumbnailStripAnimation_, &QPropertyAnimation::finished, this, [this]() {
            if (!thumbnailStripVisible_) {
                thumbnailStripContainer_->hide();
            }
        }, Qt::UniqueConnection);
    }
    thumbnailStripAnimation_->start();
}

void MainWindow::applyFullscreenMenuBarVisibility(bool visible)
{
    if (!menuBar()) {
        return;
    }

    fullscreenMenuBarVisible_ = visible;
    if (isFullScreen()) {
        menuBar()->setVisible(visible);
        if (visible) {
            menuBar()->raise();
        }
    } else {
        menuBar()->setVisible(true);
    }
}

bool MainWindow::shouldKeepFullscreenMenuBarVisible() const
{
    if (!isFullScreen()) {
        return true;
    }

    if (menuBar() && menuBar()->isVisible() && menuBar()->underMouse()) {
        return true;
    }

    if (!centralWidget()) {
        return false;
    }

    const QPoint localPos = centralWidget()->mapFromGlobal(QCursor::pos());
    return localPos.y() <= kFullscreenMenuRevealMargin;
}

void MainWindow::updateThumbnailActions()
{
    if (thumbnailStripAction_) {
        thumbnailStripAction_->setChecked(thumbnailStripVisible_);
        thumbnailStripAction_->setText(thumbnailStripVisible_ ? uiText("Hide Thumbnail Strip", "隐藏缩略图栏") : uiText("Show Thumbnail Strip", "显示缩略图栏"));
    }
    if (thumbnailAutoHideAction_) {
        thumbnailAutoHideAction_->setChecked(thumbnailAutoHideEnabled_);
        thumbnailAutoHideAction_->setText(uiText("Auto-hide Thumbnail Strip", "自动隐藏缩略图栏"));
    }
}

void MainWindow::updateInfoPanel(const DecodedImage* decoded)
{
    if (!infoText_) {
        return;
    }

    if (currentPath_.isEmpty()) {
        infoText_->setPlainText(uiText("No image selected.", "未选择图片"));
        return;
    }

    const QFileInfo fileInfo(currentPath_);
    const ImageFormatKind formatKind = ImageDecoder::detectFormat(currentPath_);
    const QString formatLabel = [formatKind]() {
        switch (formatKind) {
        case ImageFormatKind::Jpeg:
            return uiText("JPEG", "JPEG");
        case ImageFormatKind::Heif:
            return uiText("HEIF / AVIF", "HEIF / AVIF");
        case ImageFormatKind::Arw:
            return uiText("RAW", "RAW");
        case ImageFormatKind::Unknown:
        default:
            return uiText("Unknown", "未知");
        }
    }();

    QStringList lines;
    auto addSection = [&lines](const QString& title) {
        if (!lines.isEmpty()) {
            lines << "";
        }
        lines << title;
    };
    auto addLine = [&lines](const QString& key, const QString& value) {
        lines << QString("  %1: %2").arg(key, value);
    };

    addSection(uiText("File", "文件"));
    addLine(uiText("Name", "名称"), fileInfo.fileName().isEmpty() ? currentPath_ : fileInfo.fileName());
    addLine(uiText("Folder", "文件夹"), fileInfo.absolutePath());
    addLine(uiText("Path", "路径"), fileInfo.absoluteFilePath());
    addLine(uiText("Extension", "扩展名"), fileInfo.suffix().isEmpty() ? uiText("None", "无") : QString(".%1").arg(fileInfo.suffix().toLower()));
    addLine(uiText("Format", "格式"), formatLabel);
    addLine(uiText("Size on disk", "磁盘大小"), fileInfo.exists() ? formatBytes(fileInfo.size()) : uiText("Unknown", "未知"));
    addLine(uiText("Modified", "修改时间"), formatDateTime(fileInfo.lastModified()));
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    addLine(uiText("Created", "创建时间"), formatDateTime(fileInfo.birthTime()));
#endif
    const int currentIndex = std::max(0, catalog_.currentIndex());
    const int total = catalog_.size();
    if (total > 0) {
        addLine(uiText("Catalog position", "目录位置"), QString("%1 / %2").arg(currentIndex + 1).arg(total));
    }

    if (decoded && decoded->isValid()) {
        const QImage& image = decoded->image;
        const QSize imageSize = image.size();
        const QSize sourceSize = decoded->sourceSize.isValid() ? decoded->sourceSize : imageSize;

        addSection(uiText("Decode", "解码"));
        addLine(uiText("Decoder", "解码器"), decoded->decoderName);
        addLine(uiText("Mode", "模式"), decoded->isPreview ? uiText("Fast Preview", "快速预览") : uiText("Full Quality", "完整质量"));
        addLine(uiText("Source size", "源尺寸"), QString("%1 x %2").arg(sourceSize.width()).arg(sourceSize.height()));
        addLine(uiText("Preview", "预览"), decoded->isPreview ? uiText("Yes", "是") : uiText("No", "否"));
        addLine(uiText("Metadata lines", "元数据行数"), QString::number(decoded->metadataLines.size()));

        addSection(uiText("Image", "图像"));
        addLine(uiText("Dimensions", "尺寸"), QString("%1 x %2").arg(imageSize.width()).arg(imageSize.height()));
        addLine(uiText("Pixels", "像素数"), formatPixels(imageSize));
        addLine(uiText("Aspect ratio", "宽高比"), formatAspectRatio(imageSize));
        addLine(uiText("Format", "格式"), formatImageFormat(image.format()));
        addLine(uiText("Depth", "位深"), QString("%1 bits").arg(image.depth()));
        addLine(uiText("Alpha", "透明通道"), image.hasAlphaChannel() ? uiText("Yes", "是") : uiText("No", "否"));
        addLine(uiText("Bytes per line", "每行字节数"), formatBytes(image.bytesPerLine()));
        addLine(uiText("Memory", "内存占用"), formatBytes(image.sizeInBytes()));
        addLine(uiText("DPI X", "DPI X"), formatDpi(image.dotsPerMeterX()));
        addLine(uiText("DPI Y", "DPI Y"), formatDpi(image.dotsPerMeterY()));
        addLine(uiText("Color space", "色彩空间"), formatColorSpace(image.colorSpace()));

        if (!decoded->metadataLines.isEmpty()) {
            addSection(uiText("Metadata", "元数据"));
            for (const QString& line : decoded->metadataLines) {
                lines << translateMetadataLine(line);
            }
        }
    } else if (decoded && !decoded->errorMessage.isEmpty()) {
        addSection(uiText("Decode", "解码"));
        addLine(uiText("Decoder", "解码器"), decoded->decoderName);
        addLine(uiText("Status", "状态"), uiText("Failed", "失败"));
        addLine(uiText("Error", "错误"), decoded->errorMessage);
    } else {
        addSection(uiText("Decode", "解码"));
        addLine(uiText("Status", "状态"), uiText("Loading...", "加载中..."));
    }

    infoText_->setPlainText(lines.join('\n'));
}

void MainWindow::maybeShowThumbnailStripForCursor()
{
    if (!thumbnailAutoHideEnabled_ || !centralWidget()) {
        return;
    }

    const QPoint localPos = centralWidget()->mapFromGlobal(QCursor::pos());
    const QRect area = centralWidget()->rect();
    const bool nearBottom = localPos.y() >= area.height() - kThumbnailRevealMargin;
    const bool insideStrip = thumbnailStripContainer_->geometry().contains(localPos);

    if (nearBottom || insideStrip) {
        thumbnailAutoHideTimer_->stop();
        applyThumbnailStripVisibility(true, true);
    } else {
        thumbnailAutoHideTimer_->start();
    }
}

void MainWindow::maybeShowFullscreenMenuBarForCursor()
{
    if (!isFullScreen() || !menuBar() || !centralWidget()) {
        return;
    }

    const QPoint localPos = centralWidget()->mapFromGlobal(QCursor::pos());
    if (localPos.y() <= kFullscreenMenuRevealMargin) {
        applyFullscreenMenuBarVisibility(true);
        fullscreenMenuBarAutoHideTimer_->start();
    } else if (fullscreenMenuBarVisible_) {
        fullscreenMenuBarAutoHideTimer_->start();
    }
}
