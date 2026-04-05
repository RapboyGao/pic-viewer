#include "core/image_cache.h"

#include <algorithm>

ImageCache::ImageCache(int capacity, qint64 budgetBytes)
    : capacity_(capacity)
    , budgetBytes_(budgetBytes)
{
}

void ImageCache::put(const QString& path, DecodeMode mode, const DecodedImage& image)
{
    const QString key = keyFor(path, mode);
    const qint64 newCost = costFor(image);
    if (budgetBytes_ > 0 && newCost > budgetBytes_) {
        return;
    }
    if (entries_.contains(key)) {
        bytesInUse_ -= entries_.value(key).cost;
    }
    entries_.insert(key, Entry{image, newCost});
    bytesInUse_ += newCost;
    touch(key);
    evictIfNeeded();
}

bool ImageCache::contains(const QString& path, DecodeMode mode) const
{
    return entries_.contains(keyFor(path, mode));
}

DecodedImage ImageCache::get(const QString& path, DecodeMode mode) const
{
    return entries_.value(keyFor(path, mode)).image;
}

void ImageCache::clear()
{
    entries_.clear();
    lruKeys_.clear();
    bytesInUse_ = 0;
}

QString ImageCache::keyFor(const QString& path, DecodeMode mode) const
{
    return path + "::" + QString::number(static_cast<int>(mode));
}

void ImageCache::touch(const QString& key)
{
    lruKeys_.removeAll(key);
    lruKeys_.push_front(key);
}

void ImageCache::evictIfNeeded()
{
    while ((!lruKeys_.isEmpty()) && (entries_.size() > capacity_ || bytesInUse_ > budgetBytes_)) {
        const QString oldest = lruKeys_.takeLast();
        const auto it = entries_.find(oldest);
        if (it != entries_.end()) {
            bytesInUse_ -= it->cost;
            entries_.erase(it);
        }
    }
}

qint64 ImageCache::costFor(const DecodedImage& image) const
{
    if (image.image.isNull()) {
        return 0;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const qint64 bytes = static_cast<qint64>(image.image.sizeInBytes());
#else
    const qint64 bytes = static_cast<qint64>(image.image.byteCount());
#endif
    return std::max<qint64>(bytes, 0);
}
