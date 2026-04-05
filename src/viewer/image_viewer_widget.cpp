#include "viewer/image_viewer_widget.h"

#include <algorithm>

#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QShowEvent>
#include <QWheelEvent>

ImageViewerWidget::ImageViewerWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(320, 240);
    setAutoFillBackground(true);
    setMouseTracking(true);
    setStyleSheet("background-color: black;");

    emptyStateContainer_ = new QWidget(this);
    emptyStateContainer_->setStyleSheet("background: transparent;");
    auto* layout = new QVBoxLayout(emptyStateContainer_);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);
    layout->setAlignment(Qt::AlignCenter);

    emptyTitleLabel_ = new QLabel("Open a file or folder", emptyStateContainer_);
    QFont titleFont = emptyTitleLabel_->font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    emptyTitleLabel_->setFont(titleFont);
    emptyTitleLabel_->setAlignment(Qt::AlignCenter);
    emptyTitleLabel_->setStyleSheet("color: #f2f2f2;");
    layout->addWidget(emptyTitleLabel_, 0, Qt::AlignHCenter);

    emptyDetailLabel_ = new QLabel("Supported: common image, HEIF/AVIF, and RAW formats", emptyStateContainer_);
    emptyDetailLabel_->setAlignment(Qt::AlignCenter);
    emptyDetailLabel_->setStyleSheet("color: #d0d0d0;");
    layout->addWidget(emptyDetailLabel_, 0, Qt::AlignHCenter);

    openFileButton_ = new QPushButton("Open File...", emptyStateContainer_);
    openFolderButton_ = new QPushButton("Open Folder...", emptyStateContainer_);
    openFileButton_->setMinimumWidth(180);
    openFolderButton_->setMinimumWidth(180);
    openFileButton_->setStyleSheet(
        "QPushButton { color: white; background: #2d2d2d; border: 1px solid #555; border-radius: 8px; padding: 8px 14px; }"
        "QPushButton:hover { background: #3a3a3a; }"
        "QPushButton:pressed { background: #1f1f1f; }");
    openFolderButton_->setStyleSheet(openFileButton_->styleSheet());
    layout->addWidget(openFileButton_, 0, Qt::AlignHCenter);
    layout->addWidget(openFolderButton_, 0, Qt::AlignHCenter);
    connect(openFileButton_, &QPushButton::clicked, this, &ImageViewerWidget::openFileRequested);
    connect(openFolderButton_, &QPushButton::clicked, this, &ImageViewerWidget::openFolderRequested);

    loadingStateContainer_ = new QWidget(this);
    loadingStateContainer_->setStyleSheet("background: transparent;");
    auto* loadingLayout = new QVBoxLayout(loadingStateContainer_);
    loadingLayout->setContentsMargins(0, 0, 0, 0);
    loadingLayout->setSpacing(8);
    loadingLayout->setAlignment(Qt::AlignCenter);

    loadingTitleLabel_ = new QLabel("Loading image...", loadingStateContainer_);
    QFont loadingTitleFont = loadingTitleLabel_->font();
    loadingTitleFont.setPointSize(loadingTitleFont.pointSize() + 4);
    loadingTitleFont.setBold(true);
    loadingTitleLabel_->setFont(loadingTitleFont);
    loadingTitleLabel_->setAlignment(Qt::AlignCenter);
    loadingTitleLabel_->setStyleSheet("color: #f2f2f2;");
    loadingLayout->addWidget(loadingTitleLabel_, 0, Qt::AlignHCenter);

    loadingDetailLabel_ = new QLabel("Preparing preview and full-quality render...", loadingStateContainer_);
    loadingDetailLabel_->setAlignment(Qt::AlignCenter);
    loadingDetailLabel_->setStyleSheet("color: #d0d0d0;");
    loadingLayout->addWidget(loadingDetailLabel_, 0, Qt::AlignHCenter);

    transientHudContainer_ = new QWidget(this);
    transientHudContainer_->setAttribute(Qt::WA_TransparentForMouseEvents);
    transientHudContainer_->setStyleSheet(
        "background: rgba(20, 20, 20, 190);"
        "border: 1px solid rgba(255, 255, 255, 35);"
        "border-radius: 12px;");
    auto* hudLayout = new QVBoxLayout(transientHudContainer_);
    hudLayout->setContentsMargins(22, 14, 22, 14);
    hudLayout->setSpacing(0);
    hudLayout->setAlignment(Qt::AlignCenter);

    transientHudLabel_ = new QLabel(transientHudContainer_);
    transientHudLabel_->setAlignment(Qt::AlignCenter);
    transientHudLabel_->setStyleSheet("color: #f7f7f7; font-size: 20px; font-weight: 600;");
    transientHudLabel_->setWordWrap(true);
    hudLayout->addWidget(transientHudLabel_, 0, Qt::AlignCenter);

    transientHudTimer_ = new QTimer(this);
    transientHudTimer_->setSingleShot(true);
    transientHudTimer_->setInterval(1000);
    connect(transientHudTimer_, &QTimer::timeout, this, &ImageViewerWidget::hideTransientHud);

    setOverlayState(OverlayState::Empty);
    setMessage("Open a file or folder", "Supported: JPG, JPEG, HEIF, HEIC, HIF, ARW");
}

void ImageViewerWidget::setImage(const QImage& image)
{
    image_ = image;
    resetViewTransform();
    setOverlayState(OverlayState::Image);
    updateLoadingStateUi();
    updateEmptyStateUi();
    updateTransientHudUi();
    update();
}

void ImageViewerWidget::setMessage(const QString& title, const QString& detail)
{
    image_ = QImage();
    resetViewTransform();
    hideTransientHud();
    setOverlayState(OverlayState::Empty);
    emptyTitleLabel_->setText(title.isEmpty() ? "Open a file or folder" : title);
    emptyDetailLabel_->setText(detail.isEmpty() ? "Supported: common image, HEIF/AVIF, and RAW formats" : detail);
    updateEmptyStateUi();
    updateLoadingStateUi();
    updateTransientHudUi();
    update();
}

void ImageViewerWidget::setLoading(const QString& title, const QString& detail)
{
    image_ = QImage();
    resetViewTransform();
    hideTransientHud();
    setOverlayState(OverlayState::Loading);
    loadingTitleLabel_->setText(title.isEmpty() ? "Loading image..." : title);
    loadingDetailLabel_->setText(detail.isEmpty() ? "Preparing preview and full-quality render..." : detail);
    updateLoadingStateUi();
    updateEmptyStateUi();
    updateTransientHudUi();
    update();
}

void ImageViewerWidget::showTransientZoom(const QString& text)
{
    showTransientHud(text);
}

void ImageViewerWidget::showTransientImageInfo(const QString& text)
{
    showTransientHud(text);
}

void ImageViewerWidget::clear()
{
    setMessage({}, {});
}

void ImageViewerWidget::zoomIn()
{
    applyZoom(zoomFactor_ * 1.2, rect().center());
}

void ImageViewerWidget::zoomOut()
{
    applyZoom(zoomFactor_ / 1.2, rect().center());
}

void ImageViewerWidget::resetZoom()
{
    const bool changed = zoomFactor_ != 1.0 || !panOffset_.isNull();
    zoomFactor_ = 1.0;
    panOffset_ = {};
    if (changed) {
        emit zoomFactorChanged(zoomFactor_);
    }
    update();
}

void ImageViewerWidget::setDisplayMode(DisplayMode mode)
{
    if (displayMode_ == mode) {
        return;
    }
    displayMode_ = mode;
    resetViewTransform();
    clampPanOffset();
    update();
}

double ImageViewerWidget::zoomFactor() const
{
    return zoomFactor_;
}

ImageViewerWidget::DisplayMode ImageViewerWidget::displayMode() const
{
    return displayMode_;
}

bool ImageViewerWidget::hasImage() const
{
    return !image_.isNull();
}

void ImageViewerWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.fillRect(rect(), QColor(0, 0, 0));

    if (!image_.isNull()) {
        const QRectF target = targetRect();
        painter.setRenderHint(QPainter::SmoothPixmapTransform, shouldUseSmoothSampling(target));
        painter.drawImage(target, image_, QRectF(image_.rect()));
        return;
    }

    // Empty-state text and actions are provided by child widgets.
}

void ImageViewerWidget::wheelEvent(QWheelEvent* event)
{
    if (image_.isNull()) {
        event->ignore();
        return;
    }

    const double factor = event->angleDelta().y() > 0 ? zoomFactor_ * 1.15 : zoomFactor_ / 1.15;
    applyZoom(factor, event->position());
    event->accept();
}

void ImageViewerWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && !image_.isNull()) {
        dragging_ = true;
        lastDragPos_ = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void ImageViewerWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (dragging_ && !image_.isNull()) {
        const QPoint delta = event->pos() - lastDragPos_;
        panOffset_ += delta;
        lastDragPos_ = event->pos();
        clampPanOffset();
        update();
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void ImageViewerWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && dragging_) {
        dragging_ = false;
        unsetCursor();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void ImageViewerWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    if (!image_.isNull()) {
        if (zoomFactor_ > 1.01) {
            resetZoom();
        } else {
            applyZoom(2.0, rect().center());
        }
    }
}

void ImageViewerWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    clampPanOffset();
    updateEmptyStateUi();
    updateLoadingStateUi();
    updateTransientHudUi();
}

void ImageViewerWidget::applyZoom(double factor, const QPointF& anchor)
{
    if (image_.isNull()) {
        return;
    }

    const QRectF before = targetRect();
    const double previousZoom = zoomFactor_;
    const QPointF scenePoint = previousZoom > 0.0 ? (anchor - before.topLeft()) / previousZoom : QPointF();

    zoomFactor_ = std::clamp(factor, 1.0, 8.0);

    const QSize scaled = scaledSize();
    const QPointF centeredTopLeft((width() - scaled.width()) / 2.0, (height() - scaled.height()) / 2.0);
    panOffset_ = anchor - centeredTopLeft - (scenePoint * zoomFactor_);
    clampPanOffset();
    emit zoomFactorChanged(zoomFactor_);
    update();
}

void ImageViewerWidget::clampPanOffset()
{
    if (image_.isNull()) {
        panOffset_ = {};
        return;
    }

    const QSize scaled = scaledSize();
    const double overflowX = std::max(0.0, (scaled.width() - width()) / 2.0);
    const double overflowY = std::max(0.0, (scaled.height() - height()) / 2.0);

    panOffset_.setX(std::clamp(panOffset_.x(), -overflowX, overflowX));
    panOffset_.setY(std::clamp(panOffset_.y(), -overflowY, overflowY));
}

QSize ImageViewerWidget::scaledSize() const
{
    const QSize base = baseSize();
    if (base.isEmpty()) {
        return {};
    }

    return QSize(
        std::max(1, static_cast<int>(base.width() * zoomFactor_)),
        std::max(1, static_cast<int>(base.height() * zoomFactor_)));
}

QRectF ImageViewerWidget::targetRect() const
{
    const QSize scaled = scaledSize();
    const QPointF topLeft(
        (width() - scaled.width()) / 2.0 + panOffset_.x(),
        (height() - scaled.height()) / 2.0 + panOffset_.y());
    return QRectF(topLeft, scaled);
}

QSize ImageViewerWidget::baseSize() const
{
    if (image_.isNull()) {
        return {};
    }

    switch (displayMode_) {
    case DisplayMode::ActualSize:
        return image_.size();
    case DisplayMode::FillWindow:
        return image_.size().scaled(size(), Qt::KeepAspectRatioByExpanding);
    case DisplayMode::FitToWindow:
    default:
        return image_.size().scaled(size(), Qt::KeepAspectRatio);
    }
}

bool ImageViewerWidget::shouldUseSmoothSampling(const QRectF& target) const
{
    if (image_.isNull() || image_.width() <= 0 || image_.height() <= 0) {
        return true;
    }

    const double scaleX = target.width() / static_cast<double>(image_.width());
    const double scaleY = target.height() / static_cast<double>(image_.height());
    const double effectiveScale = std::max(scaleX, scaleY);
    const int shorterEdge = std::min(image_.width(), image_.height());

    if (effectiveScale <= 1.0) {
        return true;
    }

    // Small images look better with pixel edges sooner; large originals can stay
    // slightly smoothed up to about 2x before switching to crisp pixels.
    if (shorterEdge <= 720) {
        return effectiveScale < 1.5;
    }
    if (shorterEdge <= 1600) {
        return effectiveScale < 1.8;
    }
    return effectiveScale < 2.0;
}

void ImageViewerWidget::resetViewTransform()
{
    zoomFactor_ = 1.0;
    panOffset_ = {};
}

void ImageViewerWidget::setOverlayState(OverlayState state)
{
    overlayState_ = state;
}

void ImageViewerWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    updateEmptyStateUi();
    updateLoadingStateUi();
    updateTransientHudUi();
}

void ImageViewerWidget::updateEmptyStateUi()
{
    if (!emptyStateContainer_) {
        return;
    }

    const bool visible = overlayState_ == OverlayState::Empty;
    emptyStateContainer_->setVisible(visible);
    if (visible) {
        const QSize panelSize(420, 180);
        emptyStateContainer_->setGeometry(
            (width() - panelSize.width()) / 2,
            (height() - panelSize.height()) / 2 - 24,
            panelSize.width(),
            panelSize.height());
        emptyStateContainer_->raise();
    }
}

void ImageViewerWidget::updateLoadingStateUi()
{
    if (!loadingStateContainer_) {
        return;
    }

    const bool visible = overlayState_ == OverlayState::Loading;
    loadingStateContainer_->setVisible(visible);
    if (visible) {
        const QSize panelSize(520, 140);
        loadingStateContainer_->setGeometry(
            (width() - panelSize.width()) / 2,
            (height() - panelSize.height()) / 2 - 24,
            panelSize.width(),
            panelSize.height());
        loadingStateContainer_->raise();
    }
}

void ImageViewerWidget::updateTransientHudUi()
{
    if (!transientHudContainer_) {
        return;
    }

    if (!transientHudContainer_->isVisible()) {
        return;
    }

    transientHudContainer_->adjustSize();
    const QSize panelSize = transientHudContainer_->sizeHint();
    transientHudContainer_->setGeometry(
        (width() - panelSize.width()) / 2,
        (height() - panelSize.height()) / 2,
        panelSize.width(),
        panelSize.height());
    transientHudContainer_->raise();
}

void ImageViewerWidget::showTransientHud(const QString& text)
{
    if (!transientHudContainer_ || text.isEmpty()) {
        return;
    }

    transientHudLabel_->setText(text);
    transientHudContainer_->show();
    transientHudTimer_->stop();
    updateTransientHudUi();
    transientHudTimer_->start();
}

void ImageViewerWidget::hideTransientHud()
{
    if (!transientHudContainer_) {
        return;
    }
    transientHudTimer_->stop();
    transientHudContainer_->hide();
}
