#include "core/slide_show_controller.h"

SlideShowController::SlideShowController(QObject* parent)
    : QObject(parent)
{
    timer_.setInterval(3000);
    connect(&timer_, &QTimer::timeout, this, &SlideShowController::advanceRequested);
}

void SlideShowController::start()
{
    if (timer_.isActive()) {
        return;
    }
    timer_.start();
    emit playingChanged(true);
}

void SlideShowController::stop()
{
    if (!timer_.isActive()) {
        return;
    }
    timer_.stop();
    emit playingChanged(false);
}

void SlideShowController::toggle()
{
    if (timer_.isActive()) {
        stop();
    } else {
        start();
    }
}

void SlideShowController::setIntervalMs(int intervalMs)
{
    if (intervalMs <= 0 || timer_.interval() == intervalMs) {
        return;
    }
    const bool active = timer_.isActive();
    timer_.setInterval(intervalMs);
    if (active) {
        timer_.start();
    }
    emit intervalChanged(intervalMs);
}

bool SlideShowController::isPlaying() const
{
    return timer_.isActive();
}

int SlideShowController::intervalMs() const
{
    return timer_.interval();
}
