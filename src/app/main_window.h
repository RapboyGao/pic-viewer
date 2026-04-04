#pragma once

#include "catalog/image_catalog.h"
#include "core/image_cache.h"
#include "core/image_types.h"
#include "viewer/image_viewer_widget.h"

#include <QFutureWatcher>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QPixmap>
#include <QSet>

class QAction;
class QActionGroup;
class SlideShowController;
class QListWidgetItem;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const QString& startupPath = {}, QWidget* parent = nullptr);
    ~MainWindow() override = default;

protected:
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void openFile();
    void openFolder();
    void showNextImage();
    void showPreviousImage();
    void toggleSlideshow();
    void toggleFullscreen();
    void zoomIn();
    void zoomOut();
    void resetZoom();
    void setFitToWindowMode();
    void setActualSizeMode();
    void setFillWindowMode();
    void thumbnailActivated(QListWidgetItem* item);

private:
    void createMenus();
    void createStatusBar();
    void createThumbnailStrip();
    void openPath(const QString& path);
    void refreshCurrentImage();
    void requestImage(const QString& path, DecodeMode mode, bool displayWhenReady);
    void preloadNeighbors();
    void handleDecodedImage(const QString& path, DecodeMode mode, qint64 sequence, const DecodedImage& image);
    void displayDecodedImage(const DecodedImage& image);
    void rebuildThumbnailStrip();
    void updateThumbnailSelection();
    void updateThumbnailForPath(const QString& path, const QImage& image);
    void requestThumbnail(const QString& path);
    void preloadThumbnailNeighbors();
    void requestVisibleThumbnails();
    void updateStatus(const DecodedImage* decoded = nullptr);
    void setIntervalActionChecked(int intervalMs);
    void setDisplayModeChecked(ImageViewerWidget::DisplayMode mode);
    [[nodiscard]] QString currentDisplayModeLabel(const DecodedImage* decoded) const;
    [[nodiscard]] QString thumbnailKey(const QString& path) const;

    ImageCatalog catalog_;
    ImageCache cache_;
    ImageViewerWidget* viewer_ = nullptr;
    SlideShowController* slideshow_ = nullptr;
    QListWidget* thumbnailList_ = nullptr;

    QLabel* fileStatusLabel_ = nullptr;
    QLabel* indexStatusLabel_ = nullptr;
    QLabel* metaStatusLabel_ = nullptr;

    QAction* playPauseAction_ = nullptr;
    QAction* fullscreenAction_ = nullptr;
    QActionGroup* intervalActionGroup_ = nullptr;
    QActionGroup* displayModeActionGroup_ = nullptr;

    qint64 displaySequence_ = 0;
    QString currentPath_;
    QHash<QString, QPixmap> thumbnailCache_;
    QSet<QString> thumbnailRequestsInFlight_;
};
