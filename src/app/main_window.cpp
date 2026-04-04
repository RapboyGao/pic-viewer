#include "app/main_window.h"

#include "core/slide_show_controller.h"
#include "decoder/image_decoder.h"
#include "viewer/image_viewer_widget.h"

#include <QAction>
#include <QActionGroup>
#include <QFileDialog>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QKeyEvent>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QtConcurrent>

namespace {

constexpr int kDefaultIntervalMs = 3000;

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
    resize(1200, 800);

    viewer_ = new ImageViewerWidget(this);
    setCentralWidget(viewer_);

    slideshow_ = new SlideShowController(this);
    slideshow_->setIntervalMs(kDefaultIntervalMs);

    createMenus();
    createStatusBar();

    connect(slideshow_, &SlideShowController::advanceRequested, this, &MainWindow::showNextImage);
    connect(slideshow_, &SlideShowController::playingChanged, this, [this](bool playing) {
        playPauseAction_->setText(playing ? "Pause Slideshow" : "Start Slideshow");
    });

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
    fileMenu->addAction("&Open File...", this, &MainWindow::openFile, QKeySequence::Open);
    fileMenu->addAction("Open &Folder...", this, &MainWindow::openFolder);
    fileMenu->addSeparator();
    fileMenu->addAction("E&xit", this, &QWidget::close, QKeySequence::Quit);

    QMenu* playbackMenu = menuBar()->addMenu("&Playback");
    playPauseAction_ = playbackMenu->addAction("Start Slideshow", this, &MainWindow::toggleSlideshow, Qt::Key_Space);
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
        updateStatus();
        return;
    }

    cache_.clear();
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
    updateStatus();

    requestImage(currentPath_, DecodeMode::FastPreview, true);
    preloadNeighbors();
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

    for (const QString& neighbor : {paths.at(previousIndex), paths.at(nextIndex)}) {
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
    } else {
        viewer_->setMessage("Failed to decode image", image.errorMessage);
    }
    updateStatus(&image);
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
