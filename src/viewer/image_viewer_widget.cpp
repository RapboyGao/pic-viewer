#include "viewer/image_viewer_widget.h"

#include <algorithm>

#include <QPainter>

ImageViewerWidget::ImageViewerWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(320, 240);
    setAutoFillBackground(true);
    setMessage("Open a file or folder", "Supported: JPG, JPEG, HEIF, HEIC, HIF, ARW");
}

void ImageViewerWidget::setImage(const QImage& image)
{
    pixmap_ = QPixmap::fromImage(image);
    title_.clear();
    detail_.clear();
    update();
}

void ImageViewerWidget::setMessage(const QString& title, const QString& detail)
{
    pixmap_ = QPixmap();
    title_ = title;
    detail_ = detail;
    update();
}

void ImageViewerWidget::clear()
{
    setMessage({}, {});
}

void ImageViewerWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.fillRect(rect(), palette().window());

    if (!pixmap_.isNull()) {
        const QSize scaled = pixmap_.size().scaled(size(), Qt::KeepAspectRatio);
        const QRect target(
            QPoint((width() - scaled.width()) / 2, (height() - scaled.height()) / 2),
            scaled);
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.drawPixmap(target, pixmap_);
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
