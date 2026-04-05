#include "catalog/image_catalog.h"

#include <QCollator>
#include <QDir>
#include <QFileInfo>

#include <algorithm>

namespace {

QCollator makeNaturalCollator()
{
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    return collator;
}

} // namespace

bool ImageCatalog::loadFromPath(const QString& path)
{
    QFileInfo info(path);
    if (!info.exists()) {
        clear();
        return false;
    }

    if (info.isDir()) {
        QDir dir(info.absoluteFilePath());
        QStringList files;
        const auto extensions = supportedExtensions();
        for (const QFileInfo& entry : dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name)) {
            if (extensions.contains(entry.suffix(), Qt::CaseInsensitive)) {
                files.push_back(entry.absoluteFilePath());
            }
        }
        setEntries(files, files.value(0));
        return !entries_.isEmpty();
    }

    if (!isSupportedFile(info.absoluteFilePath())) {
        clear();
        return false;
    }

    QDir dir = info.absoluteDir();
    QStringList files;
    const auto extensions = supportedExtensions();
    for (const QFileInfo& entry : dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name)) {
        if (extensions.contains(entry.suffix(), Qt::CaseInsensitive)) {
            files.push_back(entry.absoluteFilePath());
        }
    }
    setEntries(files, info.absoluteFilePath());
    return !entries_.isEmpty();
}

void ImageCatalog::clear()
{
    entries_.clear();
    currentIndex_ = -1;
}

bool ImageCatalog::isEmpty() const
{
    return entries_.isEmpty();
}

int ImageCatalog::size() const
{
    return entries_.size();
}

int ImageCatalog::currentIndex() const
{
    return currentIndex_;
}

QString ImageCatalog::currentPath() const
{
    if (currentIndex_ < 0 || currentIndex_ >= entries_.size()) {
        return {};
    }
    return entries_.at(currentIndex_);
}

QStringList ImageCatalog::paths() const
{
    return entries_;
}

bool ImageCatalog::setCurrentPath(const QString& path)
{
    const int index = entries_.indexOf(QFileInfo(path).absoluteFilePath());
    if (index < 0) {
        return false;
    }
    currentIndex_ = index;
    return true;
}

bool ImageCatalog::moveNext()
{
    if (entries_.isEmpty()) {
        return false;
    }
    currentIndex_ = (currentIndex_ + 1) % entries_.size();
    return true;
}

bool ImageCatalog::movePrevious()
{
    if (entries_.isEmpty()) {
        return false;
    }
    currentIndex_ = (currentIndex_ - 1 + entries_.size()) % entries_.size();
    return true;
}

QStringList ImageCatalog::supportedExtensions()
{
    return {
        // Common raster / container formats handled by Qt image plugins.
        "apng", "avif", "avifs", "bmp", "dib", "exr", "gif", "hdr", "heic", "heif", "hif",
        "ico", "icon", "jfif", "jp2", "jpe", "jpeg", "jpg", "jxl", "jxr", "pbm", "pfm",
        "pgm", "pic", "png", "pnm", "ppm", "psd", "pxm", "qoi", "ras", "sr", "svg",
        "tga", "tif", "tiff", "webp", "wp2",
        // RAW families handled by LibRaw.
        "3fr", "ari", "arw", "bay", "cap", "cr2", "cr3", "crw", "dcr", "dcs", "dng",
        "drf", "eip", "erf", "fff", "gpr", "iiq", "k25", "kdc", "mdc", "mef", "mos",
        "mrw", "nef", "nrw", "orf", "pef", "ptx", "r3d", "raf", "raw", "rw2", "rwl",
        "rwz", "sr2", "srf", "srw", "x3f"
    };
}

bool ImageCatalog::isSupportedFile(const QString& path)
{
    const QFileInfo info(path);
    return supportedExtensions().contains(info.suffix(), Qt::CaseInsensitive);
}

void ImageCatalog::setEntries(QStringList entries, const QString& selectedPath)
{
    const auto collator = makeNaturalCollator();
    std::sort(entries.begin(), entries.end(), [&collator](const QString& lhs, const QString& rhs) {
        return collator.compare(QFileInfo(lhs).fileName(), QFileInfo(rhs).fileName()) < 0;
    });

    entries_ = entries;
    currentIndex_ = entries_.indexOf(QFileInfo(selectedPath).absoluteFilePath());
    if (currentIndex_ < 0 && !entries_.isEmpty()) {
        currentIndex_ = 0;
    }
}
