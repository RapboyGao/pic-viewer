#pragma once

#include "core/image_types.h"

#include <QHash>
#include <QString>
#include <QStringList>

class ImageCache
{
public:
    explicit ImageCache(int capacity = 3, qint64 budgetBytes = 256LL * 1024 * 1024);

    void put(const QString& path, DecodeMode mode, const DecodedImage& image);
    [[nodiscard]] bool contains(const QString& path, DecodeMode mode) const;
    [[nodiscard]] DecodedImage get(const QString& path, DecodeMode mode) const;
    void clear();

private:
    [[nodiscard]] QString keyFor(const QString& path, DecodeMode mode) const;
    [[nodiscard]] qint64 costFor(const DecodedImage& image) const;
    void touch(const QString& key);
    void evictIfNeeded();

    int capacity_;
    qint64 budgetBytes_;
    qint64 bytesInUse_ = 0;
    struct Entry {
        DecodedImage image;
        qint64 cost = 0;
    };
    QHash<QString, Entry> entries_;
    QStringList lruKeys_;
};
