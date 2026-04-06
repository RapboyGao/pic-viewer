#pragma once

#include "catalog/image_catalog.h"
#include "core/image_cache.h"
#include "core/image_types.h"
#include "core/prefetch_scheduler.h"
#include "viewer/image_viewer_widget.h"

#include <QFutureWatcher>
#include <QHash>
#include <QList>
#include <QListWidget>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QSet>
#include <QStringList>
#include <QThreadPool>
#include <QTimer>
#include <QVector>

class QAction;
class QActionGroup;
class QDockWidget;
class QMenuBar;
class QMenu;
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
    bool eventFilter(QObject* watched, QEvent* event) override;

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
    void toggleThumbnailStrip();
    void setThumbnailStripVisible(bool visible);
    void toggleAutoHideThumbnailStrip();
    void thumbnailActivated(QListWidgetItem* item);
    void toggleInfoPanel();

private:
    void createMenus();
    void createFileAssociationMenu();
    void retranslateUi();
    void refreshFileAssociationActions();
    void syncFileAssociationAction(const QString& extension);
    void createInfoPanel();
    void createThumbnailStrip();
    void applyThumbnailStripVisibility(bool visible, bool animated);
    void applyFullscreenMenuBarVisibility(bool visible);
    void updateThumbnailActions();
    void maybeShowThumbnailStripForCursor();
    void maybeShowFullscreenMenuBarForCursor();
    [[nodiscard]] bool shouldKeepFullscreenMenuBarVisible() const;
    void openPath(const QString& path);
    void refreshCurrentImage();
    void requestImage(const QString& path, DecodeMode mode, bool displayWhenReady);
    void queuePrefetchForCurrentContext();
    void handleDecodedImage(const QString& path, DecodeMode mode, qint64 sequence, const DecodedImage& image);
    void displayDecodedImage(const DecodedImage& image);
    void rebuildThumbnailStrip();
    void updateThumbnailSelection();
    void updateThumbnailForPath(const QString& path, const QImage& image);
    void requestThumbnail(const QString& path);
    void preloadThumbnailNeighbors();
    void requestVisibleThumbnails();
    void processPendingThumbnailRequests();
    void processPendingImagePrefetchRequests();
    void updateInfoPanel(const DecodedImage* decoded = nullptr);
    void setIntervalActionChecked(int intervalMs);
    void setDisplayModeChecked(ImageViewerWidget::DisplayMode mode);
    [[nodiscard]] bool isFileAssociationEnabled(const QString& extension) const;
    bool setFileAssociationEnabled(const QString& extension, bool enabled, bool notifyShell = true);
    bool setFileAssociationsEnabled(const QStringList& extensions, bool enabled);
    [[nodiscard]] QString thumbnailKey(const QString& path) const;
    void enqueueThumbnailRequest(const QString& path, int priority);
    void enqueueImagePrefetchRequests(const QList<PrefetchScheduler::Request>& requests);
    void enqueueImagePrefetchRequest(const QString& path, DecodeMode mode, int priority);
    [[nodiscard]] QString prefetchKey(const QString& path, DecodeMode mode) const;
    void setBrowseDirection(PrefetchScheduler::Direction direction);

    struct ThumbnailJob {
        QString path;
        int priority = 0;
    };

    struct ImagePrefetchJob {
        QString path;
        DecodeMode mode = DecodeMode::FastPreview;
        int priority = 0;
    };

    ImageCatalog catalog_;
    ImageCache cache_;
    PrefetchScheduler prefetchScheduler_;
    ImageViewerWidget* viewer_ = nullptr;
    SlideShowController* slideshow_ = nullptr;
    QListWidget* thumbnailList_ = nullptr;
    QWidget* thumbnailStripContainer_ = nullptr;
    QThreadPool imageDecodePool_;
    QThreadPool thumbnailDecodePool_;

    QAction* playPauseAction_ = nullptr;
    QAction* fullscreenAction_ = nullptr;
    QAction* infoPanelAction_ = nullptr;
    QAction* thumbnailStripAction_ = nullptr;
    QAction* thumbnailAutoHideAction_ = nullptr;
    QAction* openFileAction_ = nullptr;
    QAction* openFolderAction_ = nullptr;
    QAction* exitAction_ = nullptr;
    QAction* zoomInAction_ = nullptr;
    QAction* zoomOutAction_ = nullptr;
    QAction* resetZoomAction_ = nullptr;
    QAction* fitToWindowAction_ = nullptr;
    QAction* actualSizeAction_ = nullptr;
    QAction* fillWindowAction_ = nullptr;
    QAction* associateAllAction_ = nullptr;
    QAction* clearAllAction_ = nullptr;
    QAction* refreshFileAssociationAction_ = nullptr;
    QAction* englishLanguageAction_ = nullptr;
    QAction* chineseLanguageAction_ = nullptr;
    QActionGroup* languageActionGroup_ = nullptr;
    QMenu* fileMenu_ = nullptr;
    QMenu* playbackMenu_ = nullptr;
    QMenu* viewMenu_ = nullptr;
    QMenu* languageMenu_ = nullptr;
    QMenu* fileAssociationMenu_ = nullptr;
    QMenu* commonAssociationMenu_ = nullptr;
    QMenu* rawAssociationMenu_ = nullptr;
    QActionGroup* intervalActionGroup_ = nullptr;
    QActionGroup* displayModeActionGroup_ = nullptr;
    QList<QAction*> intervalActions_;
    QList<QAction*> displayModeActions_;
    QHash<QString, QAction*> fileAssociationActions_;
    struct FileAssociationGroupUi {
        QMenu* menu = nullptr;
        QString englishTitle;
        QString chineseTitle;
    };
    QVector<FileAssociationGroupUi> fileAssociationGroupMenus_;

    qint64 displaySequence_ = 0;
    QString currentPath_;
    QString displayedPath_;
    DecodedImage lastDecodedImage_;
    QHash<QString, QPixmap> thumbnailCache_;
    QStringList thumbnailCacheLru_;
    qint64 thumbnailCacheBytes_ = 0;
    QSet<QString> thumbnailRequestsInFlight_;
    QList<ThumbnailJob> thumbnailRequestQueue_;
    QSet<QString> imagePrefetchRequestsInFlight_;
    QList<ImagePrefetchJob> imagePrefetchRequestQueue_;
    QPropertyAnimation* thumbnailStripAnimation_ = nullptr;
    QTimer* thumbnailAutoHideTimer_ = nullptr;
    QTimer* fullscreenMenuBarAutoHideTimer_ = nullptr;
    QDockWidget* infoDock_ = nullptr;
    QPlainTextEdit* infoText_ = nullptr;
    PrefetchScheduler::Direction browseDirection_ = PrefetchScheduler::Direction::Unknown;
    bool thumbnailStripVisible_ = true;
    bool thumbnailAutoHideEnabled_ = false;
    bool fullscreenMenuBarVisible_ = true;
};
