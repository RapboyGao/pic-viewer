#include "app/main_window.h"

#include "core/slide_show_controller.h"
#include "decoder/image_decoder.h"
#include "viewer/image_viewer_widget.h"

#include <QAction>
#include <QActionGroup>
#include <QCursor>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QIcon>
#include <QKeyEvent>
#include <QKeySequence>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QDockWidget>
#include <QPoint>
#include <QScrollBar>
#include <QFrame>
#include <QStatusBar>
#include <QPlainTextEdit>
#include <QFile>
#include <QTimer>
#include <QVBoxLayout>
#include <QThread>
#include <QtConcurrent>
#include <algorithm>

namespace {

constexpr int kDefaultIntervalMs = 3000;
constexpr int kThumbnailSize = 96;
constexpr int kMaxThumbnailInflight = 2;
constexpr int kMaxImagePrefetchInflight = 3;
constexpr int kDecodeThreadStackSize = 8 * 1024 * 1024;
constexpr int kThumbnailStripExpandedHeight = kThumbnailSize + 48;
constexpr int kThumbnailRevealMargin = 96;
constexpr int kImagePrefetchWindow = 6;

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

    imageDecodePool_.setMaxThreadCount(std::max(2, QThread::idealThreadCount() / 2));
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

    createMenus();
    createInfoPanel();
    createStatusBar();

    connect(slideshow_, &SlideShowController::advanceRequested, this, &MainWindow::showNextImage);
    connect(slideshow_, &SlideShowController::playingChanged, this, [this](bool playing) {
        playPauseAction_->setText(playing ? "Pause Slideshow" : "Start Slideshow");
    });
    connect(thumbnailList_, &QListWidget::itemActivated, this, &MainWindow::thumbnailActivated);
    connect(thumbnailList_, &QListWidget::itemClicked, this, &MainWindow::thumbnailActivated);
    connect(viewer_, &ImageViewerWidget::openFileRequested, this, &MainWindow::openFile);
    connect(viewer_, &ImageViewerWidget::openFolderRequested, this, &MainWindow::openFolder);
    connect(thumbnailList_->horizontalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        requestVisibleThumbnails();
    });
    connect(thumbnailAutoHideTimer_, &QTimer::timeout, this, [this]() {
        if (thumbnailAutoHideEnabled_) {
            applyThumbnailStripVisibility(false, true);
        }
    });

    viewer_->installEventFilter(this);
    thumbnailList_->installEventFilter(this);
    thumbnailList_->viewport()->installEventFilter(this);
    centralWidget()->installEventFilter(this);

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
        setBrowseDirection(PrefetchScheduler::Direction::Backward);
        showPreviousImage();
        return;
    case Qt::Key_Right:
    case Qt::Key_PageDown:
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
        updateStatus();
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
    const QString path = QFileDialog::getOpenFileName(
        this,
        "Open Image",
        {},
        "Images (*.apng *.avif *.avifs *.bmp *.dib *.exr *.gif *.hdr *.heic *.heif *.hif *.ico *.icon *.jfif *.jp2 *.jpe *.jpeg *.jpg *.jxl *.jxr *.pbm *.pfm *.pgm *.pic *.png *.pnm *.ppm *.psd *.pxm *.qoi *.ras *.sr *.svg *.tga *.tif *.tiff *.webp *.wp2 *.3fr *.ari *.arw *.bay *.cap *.cr2 *.cr3 *.crw *.dcr *.dcs *.dng *.drf *.eip *.erf *.fff *.gpr *.iiq *.k25 *.kdc *.mdc *.mef *.mos *.mrw *.nef *.nrw *.orf *.pef *.ptx *.r3d *.raf *.raw *.rw2 *.rwl *.rwz *.sr2 *.srf *.srw *.x3f)");
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
    infoPanelAction_ = viewMenu->addAction("Toggle Info Panel", Qt::Key_I, this, &MainWindow::toggleInfoPanel);
    infoPanelAction_->setCheckable(true);
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
    viewMenu->addSeparator();
    thumbnailStripAction_ = viewMenu->addAction("Show Thumbnail Strip", Qt::Key_T, this, &MainWindow::toggleThumbnailStrip);
    thumbnailStripAction_->setCheckable(true);
    thumbnailAutoHideAction_ = viewMenu->addAction("Auto-hide Thumbnail Strip", this, &MainWindow::toggleAutoHideThumbnailStrip);
    thumbnailAutoHideAction_->setCheckable(true);
    updateThumbnailActions();
    applyThumbnailStripVisibility(false, false);
}

void MainWindow::createStatusBar()
{
    fileStatusLabel_ = new QLabel(this);
    indexStatusLabel_ = new QLabel(this);
    metaStatusLabel_ = new QLabel(this);
    fileStatusLabel_->setStyleSheet("color: #f0f0f0;");
    indexStatusLabel_->setStyleSheet("color: #f0f0f0;");

    statusBar()->addWidget(fileStatusLabel_, 1);
    statusBar()->addPermanentWidget(indexStatusLabel_);
    metaStatusLabel_->hide();
    statusBar()->setStyleSheet("background: #000000; color: #f0f0f0;");
}

void MainWindow::createInfoPanel()
{
    infoDock_ = new QDockWidget("Image Info", this);
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
        }
    });
}

void MainWindow::openPath(const QString& path)
{
    if (!catalog_.loadFromPath(path)) {
        viewer_->setMessage("No supported images found", QFileInfo(path).absoluteFilePath());
        currentPath_.clear();
        cache_.clear();
        thumbnailCache_.clear();
        thumbnailRequestsInFlight_.clear();
        thumbnailRequestQueue_.clear();
        rebuildThumbnailStrip();
        thumbnailStripContainer_->hide();
        thumbnailStripVisible_ = false;
        updateThumbnailActions();
        imagePrefetchRequestsInFlight_.clear();
        imagePrefetchRequestQueue_.clear();
        updateInfoPanel();
        updateStatus();
        return;
    }

    cache_.clear();
    thumbnailCache_.clear();
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
        viewer_->setMessage("No image selected");
        updateInfoPanel();
        updateStatus();
        return;
    }

    if (!viewer_->hasImage()) {
        viewer_->setMessage("Loading image...", QFileInfo(currentPath_).fileName());
    }
    updateThumbnailSelection();
    updateStatus();

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

    const QList<PrefetchScheduler::Request> requests = prefetchScheduler_.planImageRequests(
        paths,
        catalog_.currentIndex(),
        browseDirection_,
        kImagePrefetchWindow);
    QList<PrefetchScheduler::Request> filtered;
    filtered.reserve(requests.size());
    for (const auto& request : requests) {
        if (request.path != currentPath_) {
            filtered.push_back(request);
        }
    }
    enqueueImagePrefetchRequests(filtered);
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
        viewer_->setImage(image.image);
        updateThumbnailForPath(image.filePath, image.image);
        preloadThumbnailNeighbors();
        requestVisibleThumbnails();
    } else {
        if (!viewer_->hasImage()) {
            viewer_->setMessage("Failed to decode image", image.errorMessage);
        }
    }
    updateInfoPanel(&image);
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
        statusBar()->hide();
        return;
    }

    statusBar()->show();
    fileStatusLabel_->setText(QFileInfo(currentPath_).fileName());
    indexStatusLabel_->setText(QString("%1 / %2").arg(catalog_.currentIndex() + 1).arg(catalog_.size()));

    QStringList parts;
    if (decoded && decoded->isValid()) {
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
    }
    if (decoded && !decoded->errorMessage.isEmpty()) {
        parts << "Error";
    }
    statusBar()->showMessage(parts.join(" | "));
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

    imagePrefetchRequestQueue_.push_back({path, mode, priority});
    std::stable_sort(imagePrefetchRequestQueue_.begin(), imagePrefetchRequestQueue_.end(), [](const ImagePrefetchJob& lhs, const ImagePrefetchJob& rhs) {
        return lhs.priority < rhs.priority;
    });
}

void MainWindow::processPendingImagePrefetchRequests()
{
    while (!imagePrefetchRequestQueue_.isEmpty() && imagePrefetchRequestsInFlight_.size() < kMaxImagePrefetchInflight) {
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

QString MainWindow::thumbnailKey(const QString& path) const
{
    return path;
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
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

void MainWindow::updateThumbnailActions()
{
    if (thumbnailStripAction_) {
        thumbnailStripAction_->setChecked(thumbnailStripVisible_);
        thumbnailStripAction_->setText(thumbnailStripVisible_ ? "Hide Thumbnail Strip" : "Show Thumbnail Strip");
    }
    if (thumbnailAutoHideAction_) {
        thumbnailAutoHideAction_->setChecked(thumbnailAutoHideEnabled_);
    }
}

void MainWindow::updateInfoPanel(const DecodedImage* decoded)
{
    if (!infoText_) {
        return;
    }

    if (currentPath_.isEmpty()) {
        infoText_->setPlainText("No image selected.");
        return;
    }

    QStringList lines;
    lines << QString("File: %1").arg(QFileInfo(currentPath_).fileName());
    lines << QString("Path: %1").arg(currentPath_);
    if (decoded && decoded->isValid()) {
        lines << QString("Decoder: %1").arg(decoded->decoderName);
        lines << QString("Mode: %1").arg(decoded->isPreview ? "Fast Preview" : "Full Quality");
        lines << QString("Size: %1 x %2").arg(decoded->sourceSize.width()).arg(decoded->sourceSize.height());
        if (!decoded->metadataLines.isEmpty()) {
            lines << "";
            lines << "Metadata:";
            lines << decoded->metadataLines;
        }
    } else if (decoded && !decoded->errorMessage.isEmpty()) {
        lines << QString("Decoder: %1").arg(decoded->decoderName);
        lines << QString("Error: %1").arg(decoded->errorMessage);
    } else {
        lines << "Loading...";
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
