#include "core/prefetch_scheduler.h"

#include <algorithm>

namespace {

int clampMaxDistance(int count, int maxDistance)
{
    if (count <= 0) {
        return 0;
    }
    if (maxDistance <= 0) {
        return count - 1;
    }
    return std::min(count - 1, maxDistance);
}

QList<int> buildOffsetOrder(PrefetchScheduler::Direction direction, int maxDistance)
{
    QList<int> offsets;
    if (maxDistance <= 0) {
        return offsets;
    }

    const int firstSign = direction == PrefetchScheduler::Direction::Backward ? -1 : 1;
    const int secondSign = -firstSign;
    for (int distance = 1; distance <= maxDistance; ++distance) {
        offsets.push_back(firstSign * distance);
        offsets.push_back(secondSign * distance);
    }
    return offsets;
}

} // namespace

QList<PrefetchScheduler::Request> PrefetchScheduler::planImageRequests(
    const QStringList& paths,
    int currentIndex,
    Direction direction,
    int maxDistance) const
{
    QList<Request> requests;
    if (paths.isEmpty() || currentIndex < 0 || currentIndex >= paths.size()) {
        return requests;
    }

    const int limit = clampMaxDistance(paths.size(), maxDistance);
    const QString currentPath = paths.at(currentIndex);
    requests.push_back({currentPath, DecodeMode::FastPreview, 0});

    const QList<int> offsets = buildOffsetOrder(direction, limit);
    int priority = 1;
    for (int offset : offsets) {
        const int index = currentIndex + offset;
        if (index < 0 || index >= paths.size()) {
            continue;
        }
        const QString path = paths.at(index);
        if (path == currentPath) {
            continue;
        }
        requests.push_back({path, DecodeMode::FastPreview, priority++});
    }

    if (requests.size() < paths.size()) {
        for (int index = 0; index < paths.size(); ++index) {
            const QString path = paths.at(index);
            if (path == currentPath || std::any_of(requests.begin(), requests.end(), [&path](const Request& request) {
                    return request.path == path;
                })) {
                continue;
            }
            requests.push_back({path, DecodeMode::FastPreview, priority++});
        }
    }

    return requests;
}

QList<int> PrefetchScheduler::orderedIndices(
    int count,
    int currentIndex,
    Direction direction,
    int maxDistance) const
{
    QList<int> indices;
    if (count <= 0 || currentIndex < 0 || currentIndex >= count) {
        return indices;
    }

    const int limit = clampMaxDistance(count, maxDistance);
    indices.push_back(currentIndex);
    const QList<int> offsets = buildOffsetOrder(direction, limit);
    for (int offset : offsets) {
        const int index = currentIndex + offset;
        if (index >= 0 && index < count && !indices.contains(index)) {
            indices.push_back(index);
        }
    }
    for (int index = 0; index < count; ++index) {
        if (!indices.contains(index)) {
            indices.push_back(index);
        }
    }
    return indices;
}
