#pragma once

#include <QPixmap>
#include <QWidget>

class ImageViewerWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ImageViewerWidget(QWidget* parent = nullptr);

    void setImage(const QImage& image);
    void setMessage(const QString& title, const QString& detail = QString());
    void clear();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QPixmap pixmap_;
    QString title_;
    QString detail_;
};
