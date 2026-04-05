#pragma once

#include <QImage>
#include <QPushButton>
#include <QPointF>
#include <QVBoxLayout>
#include <QWidget>
#include <QTimer>

class QLabel;
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
    void setLoading(const QString& title = QString("Loading image..."), const QString& detail = QString());
    void showTransientZoom(const QString& text);
    void showTransientImageInfo(const QString& text);
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
    void zoomFactorChanged(double zoomFactor);

private:
    enum class OverlayState {
        Empty,
        Loading,
        Image,
    };

    void applyZoom(double factor, const QPointF& anchor);
    void clampPanOffset();
    [[nodiscard]] QSize scaledSize() const;
    [[nodiscard]] QRectF targetRect() const;
    [[nodiscard]] QSize baseSize() const;
    [[nodiscard]] bool shouldUseSmoothSampling(const QRectF& target) const;
    void resetViewTransform();
    void setOverlayState(OverlayState state);
    void updateEmptyStateUi();
    void updateLoadingStateUi();
    void updateTransientHudUi();
    void showTransientHud(const QString& text);
    void hideTransientHud();

    QImage image_;
    double zoomFactor_ = 1.0;
    DisplayMode displayMode_ = DisplayMode::FitToWindow;
    OverlayState overlayState_ = OverlayState::Empty;
    QPointF panOffset_;
    bool dragging_ = false;
    QPoint lastDragPos_;
    QWidget* emptyStateContainer_ = nullptr;
    QLabel* emptyTitleLabel_ = nullptr;
    QLabel* emptyDetailLabel_ = nullptr;
    QPushButton* openFileButton_ = nullptr;
    QPushButton* openFolderButton_ = nullptr;
    QWidget* loadingStateContainer_ = nullptr;
    QLabel* loadingTitleLabel_ = nullptr;
    QLabel* loadingDetailLabel_ = nullptr;
    QWidget* transientHudContainer_ = nullptr;
    QLabel* transientHudLabel_ = nullptr;
    QTimer* transientHudTimer_ = nullptr;
};
