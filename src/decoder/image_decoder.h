#pragma once

#include "core/image_types.h"

#include <QStringList>

class ImageDecoder
{
public:
    static DecodedImage decode(const QString& path, DecodeMode mode);
    static ImageFormatKind detectFormat(const QString& path);
    static QString decoderName(ImageFormatKind kind);
    static QStringList supportedExtensions();
    static bool supportsFullQuality(ImageFormatKind kind);
};
