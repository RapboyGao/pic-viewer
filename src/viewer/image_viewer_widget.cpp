#include "viewer/image_viewer_widget.h"

#include <algorithm>

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

ImageViewerWidget::ImageViewerWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(320, 240);
    setAutoFillBackground(true);
    setMouseTracking(true);
    setMessage("Open a file or folder", "Supported: JPG, JPEG, HEIF, HEIC, HIF, ARW");
}

void ImageViewerWidget::setImage(const QImage& image)
{
    image_ = image;
    title_.clear();
    detail_.clear();
    resetViewTransform();
    update();
}

void ImageViewerWidget::setMessage(const QString& title, const QString& detail)
{
    image_ = QImage();
    title_ = title;
    detail_ = detail;
    resetViewTransform();
    update();
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
    zoomFactor_ = 1.0;
    panOffset_ = {};
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

void ImageViewerWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.fillRect(rect(), palette().window());

    if (!image_.isNull()) {
        const QRectF target = targetRect();
        painter.setRenderHint(QPainter::SmoothPixmapTransform, shouldUseSmoothSampling(target));
        painter.drawImage(target, image_, QRectF(image_.rect()));
        return;
    }

    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setPen(palette().text().color());

    QFont titleFont = painter.font();
    titleFont.setPointSize(titleFont.pointSize() + 4);
    titleFont.setBold(true);
    painter.setFont(titleFont);
    painter.drawText(rect().adjusted(24, 0, -24, -16), Qt::AlignCenter | Qt::TextWordWrap, title_);

    if (!detail_.isEmpty()) {
        QFont detailFont = painter.font();
        detailFont.setBold(false);
        detailFont.setPointSize(std::max(detailFont.pointSize() - 2, 10));
        painter.setFont(detailFont);
        painter.drawText(rect().adjusted(40, 80, -40, 0), Qt::AlignCenter | Qt::TextWordWrap, detail_);
    }
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
