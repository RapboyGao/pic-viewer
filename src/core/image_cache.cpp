#include "core/image_cache.h"

ImageCache::ImageCache(int capacity)
    : capacity_(capacity)
{
}

void ImageCache::put(const QString& path, DecodeMode mode, const DecodedImage& image)
{
    const QString key = keyFor(path, mode);
    entries_.insert(key, image);
    touch(key);
    evictIfNeeded();
}

bool ImageCache::contains(const QString& path, DecodeMode mode) const
{
    return entries_.contains(keyFor(path, mode));
}

DecodedImage ImageCache::get(const QString& path, DecodeMode mode) const
{
    return entries_.value(keyFor(path, mode));
}

void ImageCache::clear()
{
    entries_.clear();
    lruKeys_.clear();
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
    while (entries_.size() > capacity_ && !lruKeys_.isEmpty()) {
        const QString oldest = lruKeys_.takeLast();
        entries_.remove(oldest);
    }
}
