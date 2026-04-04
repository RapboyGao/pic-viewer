#pragma once

#include <QObject>
#include <QTimer>

class SlideShowController : public QObject
{
    Q_OBJECT

public:
    explicit SlideShowController(QObject* parent = nullptr);

    void start();
    void stop();
    void toggle();
    void setIntervalMs(int intervalMs);

    [[nodiscard]] bool isPlaying() const;
    [[nodiscard]] int intervalMs() const;

signals:
    void playingChanged(bool playing);
    void intervalChanged(int intervalMs);
    void advanceRequested();

private:
    QTimer timer_;
};
