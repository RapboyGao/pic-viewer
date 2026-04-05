#pragma once

#include <QImage>
#include <QSize>
#include <QString>
#include <QStringList>

enum class DecodeMode {
    FastPreview,
    FullQuality,
};

enum class ImageFormatKind {
    Unknown,
    Jpeg,
    Heif,
    Arw,
};

struct DecodedImage {
    QString filePath;
    QImage image;
    QSize sourceSize;
    QString decoderName;
    QString errorMessage;
    QStringList metadataLines;
    bool isPreview = false;

    [[nodiscard]] bool isValid() const
    {
        return !image.isNull() && errorMessage.isEmpty();
    }
};
