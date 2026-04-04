#pragma once

#include "core/image_types.h"

#include <QHash>
#include <QString>
#include <QStringList>

class ImageCache
{
public:
    explicit ImageCache(int capacity = 3);

    void put(const QString& path, DecodeMode mode, const DecodedImage& image);
    [[nodiscard]] bool contains(const QString& path, DecodeMode mode) const;
    [[nodiscard]] DecodedImage get(const QString& path, DecodeMode mode) const;
    void clear();

private:
    [[nodiscard]] QString keyFor(const QString& path, DecodeMode mode) const;
    void touch(const QString& key);
    void evictIfNeeded();

    int capacity_;
    QHash<QString, DecodedImage> entries_;
    QStringList lruKeys_;
};
