#pragma once

#include <QImage>
#include <QPushButton>
#include <QPointF>
#include <QVBoxLayout>
#include <QWidget>

class ImageViewerWidget : public QWidget
{
    Q_OBJECT

public:
    enum class DisplayMode {
        FitToWindow,
        ActualSize,
        FillWindow,
    };

    explicit ImageViewerWidget(QWidget* parent = nullptr);

    void setImage(const QImage& image);
    void setMessage(const QString& title, const QString& detail = QString());
    void clear();
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void setDisplayMode(DisplayMode mode);
    [[nodiscard]] double zoomFactor() const;
    [[nodiscard]] DisplayMode displayMode() const;
    [[nodiscard]] bool hasImage() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;

signals:
    void openFileRequested();
    void openFolderRequested();

private:
    void applyZoom(double factor, const QPointF& anchor);
    void clampPanOffset();
    [[nodiscard]] QSize scaledSize() const;
    [[nodiscard]] QRectF targetRect() const;
    [[nodiscard]] QSize baseSize() const;
    [[nodiscard]] bool shouldUseSmoothSampling(const QRectF& target) const;
    void resetViewTransform();
    void updateEmptyStateUi();

    QImage image_;
    QString title_;
    QString detail_;
    double zoomFactor_ = 1.0;
    DisplayMode displayMode_ = DisplayMode::FitToWindow;
    QPointF panOffset_;
    bool dragging_ = false;
    QPoint lastDragPos_;
    QWidget* emptyStateContainer_ = nullptr;
    QPushButton* openFileButton_ = nullptr;
    QPushButton* openFolderButton_ = nullptr;
};
