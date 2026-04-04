#pragma once

#include "catalog/image_catalog.h"
#include "core/image_cache.h"
#include "core/image_types.h"

#include <QFutureWatcher>
#include <QLabel>
#include <QMainWindow>

class QAction;
class QActionGroup;
class ImageViewerWidget;
class SlideShowController;

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

private:
    void createMenus();
    void createStatusBar();
    void openPath(const QString& path);
    void refreshCurrentImage();
    void requestImage(const QString& path, DecodeMode mode, bool displayWhenReady);
    void preloadNeighbors();
    void handleDecodedImage(const QString& path, DecodeMode mode, qint64 sequence, const DecodedImage& image);
    void displayDecodedImage(const DecodedImage& image);
    void updateStatus(const DecodedImage* decoded = nullptr);
    void setIntervalActionChecked(int intervalMs);
    [[nodiscard]] QString currentDisplayModeLabel(const DecodedImage* decoded) const;

    ImageCatalog catalog_;
    ImageCache cache_;
    ImageViewerWidget* viewer_ = nullptr;
    SlideShowController* slideshow_ = nullptr;

    QLabel* fileStatusLabel_ = nullptr;
    QLabel* indexStatusLabel_ = nullptr;
    QLabel* metaStatusLabel_ = nullptr;

    QAction* playPauseAction_ = nullptr;
    QActionGroup* intervalActionGroup_ = nullptr;

    qint64 displaySequence_ = 0;
    QString currentPath_;
};
