#pragma once

#include "core/image_types.h"

#include <QList>
#include <QString>
#include <QStringList>

class PrefetchScheduler
{
public:
    enum class Direction {
        Unknown,
        Forward,
        Backward,
    };

    struct Request {
        QString path;
        DecodeMode mode = DecodeMode::FastPreview;
        int priority = 0;
    };

    QList<Request> planImageRequests(
        const QStringList& paths,
        int currentIndex,
        Direction direction,
        int maxDistance = 0,
        int maxRequests = 0) const;

    QList<int> orderedIndices(
        int count,
        int currentIndex,
        Direction direction,
        int maxDistance = 0,
        int maxCount = 0) const;
};
