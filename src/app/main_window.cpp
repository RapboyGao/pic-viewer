#include "app/main_window.h"

#include "core/slide_show_controller.h"
#include "decoder/image_decoder.h"
#include "viewer/image_viewer_widget.h"

#include <QAction>
#include <QActionGroup>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QIcon>
#include <QKeyEvent>
#include <QKeySequence>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace {

constexpr int kDefaultIntervalMs = 3000;
constexpr int kThumbnailSize = 96;

QString modeLabelForAction(int intervalMs)
{
    return QString("%1 s").arg(intervalMs / 1000);
}

} // namespace

MainWindow::MainWindow(const QString& startupPath, QWidget* parent)
    : QMainWindow(parent)
    , cache_(6)
{
    setWindowTitle("pic-viewer");
    setWindowIcon(QIcon(":/icons/app_icon.xpm"));
    resize(1200, 800);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    viewer_ = new ImageViewerWidget(central);
    layout->addWidget(viewer_, 1);
    createThumbnailStrip();
    layout->addWidget(thumbnailList_);
    setCentralWidget(central);

    slideshow_ = new SlideShowController(this);
    slideshow_->setIntervalMs(kDefaultIntervalMs);

    createMenus();
    createStatusBar();

    connect(slideshow_, &SlideShowController::advanceRequested, this, &MainWindow::showNextImage);
    connect(slideshow_, &SlideShowController::playingChanged, this, [this](bool playing) {
        playPauseAction_->setText(playing ? "Pause Slideshow" : "Start Slideshow");
    });
    connect(thumbnailList_, &QListWidget::itemActivated, this, &MainWindow::thumbnailActivated);
    connect(thumbnailList_, &QListWidget::itemClicked, this, &MainWindow::thumbnailActivated);

    if (!startupPath.isEmpty()) {
        openPath(startupPath);
    } else {
        updateStatus();
    }
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Left:
    case Qt::Key_PageUp:
        showPreviousImage();
        return;
    case Qt::Key_Right:
    case Qt::Key_PageDown:
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
        updateStatus();
        return;
    case Qt::Key_Escape:
        if (isFullScreen()) {
            toggleFullscreen();
            return;
        }
        break;
    default:
        break;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::openFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Open Image",
        {},
        "Images (*.jpg *.jpeg *.heif *.heic *.hif *.arw)");
    if (!path.isEmpty()) {
        openPath(path);
    }
}

void MainWindow::openFolder()
{
    const QString path = QFileDialog::getExistingDirectory(this, "Open Folder");
    if (!path.isEmpty()) {
        openPath(path);
    }
}

void MainWindow::showNextImage()
{
    if (!catalog_.moveNext()) {
        return;
    }
    refreshCurrentImage();
}

void MainWindow::showPreviousImage()
{
    if (!catalog_.movePrevious()) {
        return;
    }
    refreshCurrentImage();
}

void MainWindow::toggleSlideshow()
{
    slideshow_->toggle();
}

void MainWindow::createMenus()
{
    QMenu* fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("&Open File...", QKeySequence::Open, this, &MainWindow::openFile);
    fileMenu->addAction("Open &Folder...", this, &MainWindow::openFolder);
    fileMenu->addSeparator();
    fileMenu->addAction("E&xit", QKeySequence::Quit, this, &QWidget::close);

    QMenu* playbackMenu = menuBar()->addMenu("&Playback");
    playPauseAction_ = playbackMenu->addAction("Start Slideshow", Qt::Key_Space, this, &MainWindow::toggleSlideshow);
    playbackMenu->addSeparator();

    intervalActionGroup_ = new QActionGroup(this);
    intervalActionGroup_->setExclusive(true);

    for (const int interval : {1000, 3000, 5000, 10000}) {
        QAction* action = playbackMenu->addAction(modeLabelForAction(interval));
        action->setCheckable(true);
        action->setData(interval);
        intervalActionGroup_->addAction(action);
        connect(action, &QAction::triggered, this, [this, interval]() {
            slideshow_->setIntervalMs(interval);
            setIntervalActionChecked(interval);
        });
    }
    setIntervalActionChecked(kDefaultIntervalMs);

    QMenu* viewMenu = menuBar()->addMenu("&View");
    fullscreenAction_ = viewMenu->addAction("Toggle Fullscreen", Qt::Key_F11, this, &MainWindow::toggleFullscreen);
    viewMenu->addAction("Zoom In", QKeySequence::ZoomIn, this, &MainWindow::zoomIn);
    viewMenu->addAction("Zoom Out", QKeySequence::ZoomOut, this, &MainWindow::zoomOut);
    viewMenu->addAction("Reset Zoom", QKeySequence(Qt::CTRL | Qt::Key_0), this, &MainWindow::resetZoom);
    viewMenu->addSeparator();

    displayModeActionGroup_ = new QActionGroup(this);
    displayModeActionGroup_->setExclusive(true);

    QAction* fitAction = viewMenu->addAction("Fit to Window", this, &MainWindow::setFitToWindowMode);
    fitAction->setCheckable(true);
    fitAction->setData(static_cast<int>(ImageViewerWidget::DisplayMode::FitToWindow));
    displayModeActionGroup_->addAction(fitAction);

    QAction* actualAction = viewMenu->addAction("Actual Size", this, &MainWindow::setActualSizeMode);
    actualAction->setCheckable(true);
    actualAction->setData(static_cast<int>(ImageViewerWidget::DisplayMode::ActualSize));
    displayModeActionGroup_->addAction(actualAction);

    QAction* fillAction = viewMenu->addAction("Fill Window", this, &MainWindow::setFillWindowMode);
    fillAction->setCheckable(true);
    fillAction->setData(static_cast<int>(ImageViewerWidget::DisplayMode::FillWindow));
    displayModeActionGroup_->addAction(fillAction);

    setDisplayModeChecked(viewer_->displayMode());
}

void MainWindow::createStatusBar()
{
    fileStatusLabel_ = new QLabel(this);
    indexStatusLabel_ = new QLabel(this);
    metaStatusLabel_ = new QLabel(this);

    statusBar()->addWidget(fileStatusLabel_, 1);
    statusBar()->addPermanentWidget(indexStatusLabel_);
    statusBar()->addPermanentWidget(metaStatusLabel_);
}

void MainWindow::openPath(const QString& path)
{
    if (!catalog_.loadFromPath(path)) {
        viewer_->setMessage("No supported images found", QFileInfo(path).absoluteFilePath());
        currentPath_.clear();
        cache_.clear();
        thumbnailCache_.clear();
        thumbnailRequestsInFlight_.clear();
        rebuildThumbnailStrip();
        updateStatus();
        return;
    }

    cache_.clear();
    thumbnailCache_.clear();
    thumbnailRequestsInFlight_.clear();
    rebuildThumbnailStrip();
    refreshCurrentImage();
}

void MainWindow::refreshCurrentImage()
{
    currentPath_ = catalog_.currentPath();
    if (currentPath_.isEmpty()) {
        viewer_->setMessage("No image selected");
        updateStatus();
        return;
    }

    viewer_->setMessage("Loading image...", QFileInfo(currentPath_).fileName());
    updateThumbnailSelection();
    updateStatus();

    requestImage(currentPath_, DecodeMode::FastPreview, true);
    preloadNeighbors();
    preloadThumbnailNeighbors();
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
    watcher->setFuture(QtConcurrent::run([path, mode]() {
        return ImageDecoder::decode(path, mode);
    }));
}

void MainWindow::preloadNeighbors()
{
    const QStringList paths = catalog_.paths();
    if (paths.size() <= 1 || currentPath_.isEmpty()) {
        return;
    }

    const int index = catalog_.currentIndex();
    const int previousIndex = (index - 1 + paths.size()) % paths.size();
    const int nextIndex = (index + 1) % paths.size();
    const int nextNextIndex = (index + 2) % paths.size();

    for (const QString& neighbor : {paths.at(previousIndex), paths.at(nextIndex), paths.at(nextNextIndex)}) {
        if (!cache_.contains(neighbor, DecodeMode::FastPreview)) {
            requestImage(neighbor, DecodeMode::FastPreview, false);
        }
    }
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
        viewer_->setImage(image.image);
        updateThumbnailForPath(image.filePath, image.image);
    } else {
        viewer_->setMessage("Failed to decode image", image.errorMessage);
    }
    updateStatus(&image);
}

void MainWindow::toggleFullscreen()
{
    if (isFullScreen()) {
        showNormal();
    } else {
        showFullScreen();
    }
}

void MainWindow::zoomIn()
{
    viewer_->zoomIn();
    updateStatus();
}

void MainWindow::zoomOut()
{
    viewer_->zoomOut();
    updateStatus();
}

void MainWindow::resetZoom()
{
    viewer_->resetZoom();
    updateStatus();
}

void MainWindow::setFitToWindowMode()
{
    viewer_->setDisplayMode(ImageViewerWidget::DisplayMode::FitToWindow);
    setDisplayModeChecked(viewer_->displayMode());
    updateStatus();
}

void MainWindow::setActualSizeMode()
{
    viewer_->setDisplayMode(ImageViewerWidget::DisplayMode::ActualSize);
    setDisplayModeChecked(viewer_->displayMode());
    updateStatus();
}

void MainWindow::setFillWindowMode()
{
    viewer_->setDisplayMode(ImageViewerWidget::DisplayMode::FillWindow);
    setDisplayModeChecked(viewer_->displayMode());
    updateStatus();
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

void MainWindow::updateStatus(const DecodedImage* decoded)
{
    if (currentPath_.isEmpty()) {
        fileStatusLabel_->setText("No file");
        indexStatusLabel_->setText("0 / 0");
        metaStatusLabel_->setText("Idle");
        return;
    }

    fileStatusLabel_->setText(QFileInfo(currentPath_).fileName());
    indexStatusLabel_->setText(QString("%1 / %2").arg(catalog_.currentIndex() + 1).arg(catalog_.size()));

    QStringList parts;
    if (decoded && decoded->isValid()) {
        parts << QString("%1 x %2").arg(decoded->sourceSize.width()).arg(decoded->sourceSize.height());
        parts << decoded->decoderName;
        parts << currentDisplayModeLabel(decoded);
        parts << [this]() {
            switch (viewer_->displayMode()) {
            case ImageViewerWidget::DisplayMode::ActualSize:
                return QString("Actual");
            case ImageViewerWidget::DisplayMode::FillWindow:
                return QString("Fill");
            case ImageViewerWidget::DisplayMode::FitToWindow:
            default:
                return QString("Fit");
            }
        }();
        parts << QString("Zoom %1%").arg(static_cast<int>(viewer_->zoomFactor() * 100.0));
    } else if (decoded && !decoded->errorMessage.isEmpty()) {
        parts << decoded->decoderName;
        parts << "Error";
    } else {
        parts << "Loading";
    }
    metaStatusLabel_->setText(parts.join(" | "));
}

void MainWindow::setIntervalActionChecked(int intervalMs)
{
    for (QAction* action : intervalActionGroup_->actions()) {
        action->setChecked(action->data().toInt() == intervalMs);
    }
}

QString MainWindow::currentDisplayModeLabel(const DecodedImage* decoded) const
{
    if (!decoded) {
        return "Unknown";
    }
    return decoded->isPreview ? "Fast Preview" : "Full Quality";
}

void MainWindow::createThumbnailStrip()
{
    thumbnailList_ = new QListWidget(this);
    thumbnailList_->setViewMode(QListView::IconMode);
    thumbnailList_->setFlow(QListView::LeftToRight);
    thumbnailList_->setResizeMode(QListView::Adjust);
    thumbnailList_->setMovement(QListView::Static);
    thumbnailList_->setWrapping(false);
    thumbnailList_->setSpacing(8);
    thumbnailList_->setIconSize(QSize(kThumbnailSize, kThumbnailSize));
    thumbnailList_->setMaximumHeight(kThumbnailSize + 48);
    thumbnailList_->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    thumbnailList_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
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
    preloadThumbnailNeighbors();
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
    thumbnailCache_.insert(thumbnailKey(path), thumb);

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
    if (path.isEmpty() || thumbnailCache_.contains(thumbnailKey(path)) || thumbnailRequestsInFlight_.contains(path)) {
        return;
    }

    thumbnailRequestsInFlight_.insert(path);
    auto* watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, path]() {
        const QImage image = watcher->result();
        watcher->deleteLater();
        thumbnailRequestsInFlight_.remove(path);
        if (!image.isNull()) {
            updateThumbnailForPath(path, image);
        }
    });
    watcher->setFuture(QtConcurrent::run([path]() -> QImage {
        const DecodedImage decoded = ImageDecoder::decode(path, DecodeMode::FastPreview);
        if (!decoded.isValid()) {
            return {};
        }
        return decoded.image;
    }));
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
        requestThumbnail(paths.at(wrapped));
    }
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

QString MainWindow::thumbnailKey(const QString& path) const
{
    return path;
}
