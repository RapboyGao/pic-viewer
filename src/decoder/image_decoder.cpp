#include "decoder/image_decoder.h"

#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QTransform>

#include <cstdio>
#include <cstring>
#include <memory>
#include <algorithm>
#include <vector>

#include <jpeglib.h>
#include <libheif/heif.h>
#include <libraw/libraw.h>

namespace {

DecodedImage makeError(const QString& path, const QString& decoder, const QString& message)
{
    DecodedImage result;
    result.filePath = path;
    result.decoderName = decoder;
    result.errorMessage = message;
    return result;
}

QString safeLatin1(const char* value)
{
    if (!value || !*value) {
        return {};
    }
    return QString::fromLatin1(value);
}

DecodedImage decodeWithQtReader(const QString& path, const QString& decoder)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);

    DecodedImage result;
    result.filePath = path;
    result.decoderName = decoder;
    result.image = reader.read();
    result.sourceSize = result.image.size();
    if (result.image.isNull()) {
        result.errorMessage = reader.errorString();
    }
    return result;
}

QImage applyRawOrientation(const QImage& image, int flip)
{
    if (image.isNull()) {
        return image;
    }

    switch (flip) {
    case 3:
        return image.transformed(QTransform().rotate(180));
    case 5:
        return image.transformed(QTransform().rotate(-90));
    case 6:
        return image.transformed(QTransform().rotate(90));
    default:
        return image;
    }
}

QImage loadScaledWithQtReader(const QString& path, int maxEdge)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const QSize sourceSize = reader.size();
    if (sourceSize.isValid() && maxEdge > 0) {
        reader.setScaledSize(sourceSize.scaled(maxEdge, maxEdge, Qt::KeepAspectRatio));
    }
    return reader.read();
}

DecodedImage decodeJpeg(const QString& path)
{
    FILE* file = std::fopen(path.toUtf8().constData(), "rb");
    if (!file) {
        return makeError(path, "jpeg-turbo", "Failed to open JPEG file.");
    }

    jpeg_decompress_struct cinfo {};
    jpeg_error_mgr jerr {};
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, file);
    jpeg_read_header(&cinfo, TRUE);
#ifdef JCS_EXT_RGB
    cinfo.out_color_space = JCS_EXT_RGB;
#else
    cinfo.out_color_space = JCS_RGB;
#endif
    jpeg_start_decompress(&cinfo);

    QImage image(cinfo.output_width, cinfo.output_height, QImage::Format_RGB888);
    if (image.isNull()) {
        jpeg_finish_decompress(&cinfo);
        jpeg_destroy_decompress(&cinfo);
        std::fclose(file);
        return decodeWithQtReader(path, "qt-imageio");
    }
    while (cinfo.output_scanline < cinfo.output_height) {
        unsigned char* row = image.scanLine(static_cast<int>(cinfo.output_scanline));
        jpeg_read_scanlines(&cinfo, &row, 1);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    std::fclose(file);

    DecodedImage result;
    result.filePath = path;
    result.image = image.copy().convertToFormat(QImage::Format_RGBA8888);
    result.sourceSize = result.image.size();
    result.decoderName = "jpeg-turbo";
    if (result.image.isNull()) {
        return decodeWithQtReader(path, "qt-imageio");
    }
    return result;
}

DecodedImage decodeHeif(const QString& path)
{
    heif_context* rawContext = heif_context_alloc();
    if (!rawContext) {
        return makeError(path, "libheif", "Failed to allocate HEIF context.");
    }
    std::unique_ptr<heif_context, decltype(&heif_context_free)> context(rawContext, &heif_context_free);

    const heif_error readError = heif_context_read_from_file(context.get(), path.toUtf8().constData(), nullptr);
    if (readError.code != heif_error_Ok) {
        return makeError(path, "libheif", QString::fromUtf8(readError.message));
    }

    heif_image_handle* rawHandle = nullptr;
    const heif_error handleError = heif_context_get_primary_image_handle(context.get(), &rawHandle);
    if (handleError.code != heif_error_Ok) {
        return makeError(path, "libheif", QString::fromUtf8(handleError.message));
    }
    std::unique_ptr<heif_image_handle, decltype(&heif_image_handle_release)> handle(rawHandle, &heif_image_handle_release);

    heif_decoding_options* options = heif_decoding_options_alloc();
    std::unique_ptr<heif_decoding_options, decltype(&heif_decoding_options_free)> optionsGuard(options, &heif_decoding_options_free);

    heif_image* rawImage = nullptr;
    const heif_error decodeError = heif_decode_image(
        handle.get(),
        &rawImage,
        heif_colorspace_RGB,
        heif_chroma_interleaved_RGBA,
        options);
    if (decodeError.code != heif_error_Ok) {
        return makeError(path, "libheif", QString::fromUtf8(decodeError.message));
    }
    std::unique_ptr<heif_image, decltype(&heif_image_release)> image(rawImage, &heif_image_release);

    int stride = 0;
    const uint8_t* data = heif_image_get_plane_readonly(image.get(), heif_channel_interleaved, &stride);
    const int width = heif_image_get_width(image.get(), heif_channel_interleaved);
    const int height = heif_image_get_height(image.get(), heif_channel_interleaved);
    if (!data || width <= 0 || height <= 0) {
        return makeError(path, "libheif", "Decoded HEIF image is empty.");
    }

    QImage qimage(data, width, height, stride, QImage::Format_RGBA8888);

    DecodedImage result;
    result.filePath = path;
    result.image = qimage.copy();
    result.sourceSize = result.image.size();
    result.decoderName = "libheif";
    return result;
}

QImage decodeHeifThumbnail(const QString& path, int maxEdge)
{
    heif_context* rawContext = heif_context_alloc();
    if (!rawContext) {
        return {};
    }
    std::unique_ptr<heif_context, decltype(&heif_context_free)> context(rawContext, &heif_context_free);

    const heif_error readError = heif_context_read_from_file(context.get(), path.toUtf8().constData(), nullptr);
    if (readError.code != heif_error_Ok) {
        return {};
    }

    heif_image_handle* rawHandle = nullptr;
    const heif_error handleError = heif_context_get_primary_image_handle(context.get(), &rawHandle);
    if (handleError.code != heif_error_Ok) {
        return {};
    }
    std::unique_ptr<heif_image_handle, decltype(&heif_image_handle_release)> handle(rawHandle, &heif_image_handle_release);

    const int count = heif_image_handle_get_number_of_thumbnails(handle.get());
    if (count > 0) {
        std::vector<heif_item_id> ids(count);
        heif_image_handle_get_list_of_thumbnail_IDs(handle.get(), ids.data(), count);
        heif_image_handle* rawThumbHandle = nullptr;
        const heif_error thumbError = heif_image_handle_get_thumbnail(handle.get(), ids.front(), &rawThumbHandle);
        if (thumbError.code == heif_error_Ok && rawThumbHandle) {
            std::unique_ptr<heif_image_handle, decltype(&heif_image_handle_release)> thumbHandle(rawThumbHandle, &heif_image_handle_release);
            heif_image* rawThumbImage = nullptr;
            const heif_error decodeError = heif_decode_image(
                thumbHandle.get(),
                &rawThumbImage,
                heif_colorspace_RGB,
                heif_chroma_interleaved_RGBA,
                nullptr);
            if (decodeError.code == heif_error_Ok && rawThumbImage) {
                std::unique_ptr<heif_image, decltype(&heif_image_release)> thumbImage(rawThumbImage, &heif_image_release);
                int stride = 0;
                const uint8_t* data = heif_image_get_plane_readonly(thumbImage.get(), heif_channel_interleaved, &stride);
                const int width = heif_image_get_width(thumbImage.get(), heif_channel_interleaved);
                const int height = heif_image_get_height(thumbImage.get(), heif_channel_interleaved);
                if (data && width > 0 && height > 0) {
                    return QImage(data, width, height, stride, QImage::Format_RGBA8888)
                        .copy()
                        .scaled(maxEdge, maxEdge, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                }
            }
        }
    }

    return loadScaledWithQtReader(path, maxEdge);
}

DecodedImage decodeArw(const QString& path, DecodeMode mode)
{
    LibRaw rawProcessor;
    const int openResult = rawProcessor.open_file(path.toUtf8().constData());
    if (openResult != LIBRAW_SUCCESS) {
        return makeError(path, "LibRaw", QString::fromLatin1(libraw_strerror(openResult)));
    }

    auto recycle = [&rawProcessor]() {
        rawProcessor.recycle();
    };

    DecodedImage result;
    result.filePath = path;
    result.decoderName = "LibRaw";
    const int rawFlip = rawProcessor.imgdata.sizes.flip;
    result.metadataLines = {
        QString("Camera: %1 %2").arg(safeLatin1(rawProcessor.imgdata.idata.make), safeLatin1(rawProcessor.imgdata.idata.model)),
        QString("Lens: %1").arg(safeLatin1(rawProcessor.imgdata.lens.Lens)),
        QString("ISO: %1").arg(rawProcessor.imgdata.other.iso_speed > 0.0f ? QString::number(rawProcessor.imgdata.other.iso_speed, 'f', 0) : QString("Unknown")),
        QString("Shutter: %1").arg(rawProcessor.imgdata.other.shutter > 0.0f ? QString::number(rawProcessor.imgdata.other.shutter, 'f', 4) : QString("Unknown")),
        QString("Aperture: %1").arg(rawProcessor.imgdata.other.aperture > 0.0f ? QString::number(rawProcessor.imgdata.other.aperture, 'f', 1) : QString("Unknown")),
        QString("Focal length: %1").arg(rawProcessor.imgdata.other.focal_len > 0.0f ? QString::number(rawProcessor.imgdata.other.focal_len, 'f', 1) : QString("Unknown")),
    };
    if (rawProcessor.imgdata.other.parsed_gps.gpsparsed) {
        const auto& gps = rawProcessor.imgdata.other.parsed_gps;
        result.metadataLines << QString("GPS: %1, %2")
                                     .arg(QString::number(gps.latitude[0], 'f', 6))
                                     .arg(QString::number(gps.longitude[0], 'f', 6));
    }

    libraw_processed_image_t* previewImage = nullptr;
    if (mode == DecodeMode::FastPreview) {
        const int unpackThumb = rawProcessor.unpack_thumb();
        if (unpackThumb == LIBRAW_SUCCESS) {
            previewImage = rawProcessor.dcraw_make_mem_thumb();
        }
    }

    if (previewImage) {
        if (previewImage->type == LIBRAW_IMAGE_JPEG) {
            QImage preview;
            preview.loadFromData(
                reinterpret_cast<const uchar*>(previewImage->data),
                previewImage->data_size,
                "JPEG");
            if (!preview.isNull()) {
                result.image = applyRawOrientation(preview.convertToFormat(QImage::Format_RGBA8888), rawFlip);
                result.sourceSize = result.image.size();
                result.isPreview = true;
                LibRaw::dcraw_clear_mem(previewImage);
                recycle();
                return result;
            }
        } else if (previewImage->type == LIBRAW_IMAGE_BITMAP) {
            QImage preview(
                previewImage->width,
                previewImage->height,
                QImage::Format_RGB888);
            const int stride = preview.bytesPerLine();
            const int sourceStride = previewImage->width * previewImage->colors;
            for (int y = 0; y < previewImage->height; ++y) {
                memcpy(
                    preview.scanLine(y),
                    previewImage->data + (y * sourceStride),
                    std::min(stride, sourceStride));
            }
            result.image = applyRawOrientation(preview.convertToFormat(QImage::Format_RGBA8888), rawFlip);
            result.sourceSize = result.image.size();
            result.isPreview = true;
            LibRaw::dcraw_clear_mem(previewImage);
            recycle();
            return result;
        }
        LibRaw::dcraw_clear_mem(previewImage);
    }

    const int unpackResult = rawProcessor.unpack();
    if (unpackResult != LIBRAW_SUCCESS) {
        recycle();
        return makeError(path, "LibRaw", QString::fromLatin1(libraw_strerror(unpackResult)));
    }

    // Keep the full RAW output visually consistent with the embedded preview.
    // Disabling auto-brightness makes many Sony RAW files look noticeably darker
    // once the background full-quality pass replaces the preview.
    rawProcessor.imgdata.params.use_camera_wb = 1;
    rawProcessor.imgdata.params.no_auto_bright = 0;

    const int processResult = rawProcessor.dcraw_process();
    if (processResult != LIBRAW_SUCCESS) {
        recycle();
        return makeError(path, "LibRaw", QString::fromLatin1(libraw_strerror(processResult)));
    }

    libraw_processed_image_t* processedImage = rawProcessor.dcraw_make_mem_image();
    if (!processedImage) {
        recycle();
        return makeError(path, "LibRaw", "Failed to produce processed RAW image.");
    }

    if (processedImage->type == LIBRAW_IMAGE_BITMAP) {
        QImage qimage(processedImage->width, processedImage->height, QImage::Format_RGB888);
        const int stride = qimage.bytesPerLine();
        const int sourceStride = processedImage->width * processedImage->colors;
        for (int y = 0; y < processedImage->height; ++y) {
            memcpy(
                qimage.scanLine(y),
                processedImage->data + (y * sourceStride),
                std::min(stride, sourceStride));
        }
        result.image = applyRawOrientation(qimage.convertToFormat(QImage::Format_RGBA8888), rawFlip);
        result.sourceSize = result.image.size();
        result.isPreview = false;
    } else if (processedImage->type == LIBRAW_IMAGE_JPEG) {
        QImage qimage;
        qimage.loadFromData(
            reinterpret_cast<const uchar*>(processedImage->data),
            processedImage->data_size,
            "JPEG");
        result.image = applyRawOrientation(qimage.convertToFormat(QImage::Format_RGBA8888), rawFlip);
        result.sourceSize = result.image.size();
        result.isPreview = false;
    } else {
        LibRaw::dcraw_clear_mem(processedImage);
        recycle();
        return makeError(path, "LibRaw", "Unsupported processed RAW output type.");
    }

    LibRaw::dcraw_clear_mem(processedImage);
    recycle();

    if (result.image.isNull()) {
        return makeError(path, "LibRaw", "Failed to convert RAW output into QImage.");
    }

    return result;
}

QImage decodeArwThumbnail(const QString& path, int maxEdge)
{
    DecodedImage decoded = decodeArw(path, DecodeMode::FastPreview);
    if (!decoded.isValid()) {
        return {};
    }
    return decoded.image.scaled(maxEdge, maxEdge, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

} // namespace

DecodedImage ImageDecoder::decode(const QString& path, DecodeMode mode)
{
    const ImageFormatKind kind = detectFormat(path);
    DecodedImage decoded;
    switch (kind) {
    case ImageFormatKind::Jpeg:
        decoded = decodeJpeg(path);
        break;
    case ImageFormatKind::Heif:
        decoded = decodeHeif(path);
        break;
    case ImageFormatKind::Arw:
        decoded = decodeArw(path, mode);
        break;
    case ImageFormatKind::Unknown:
    default:
        return makeError(path, "unknown", "Unsupported image format.");
    }

    if (!decoded.isValid() && kind != ImageFormatKind::Arw) {
        DecodedImage fallback = decodeWithQtReader(path, "qt-imageio");
        if (fallback.isValid()) {
            return fallback;
        }
    }
    return decoded;
}

QImage ImageDecoder::decodeThumbnail(const QString& path, int maxEdge)
{
    const ImageFormatKind kind = detectFormat(path);
    switch (kind) {
    case ImageFormatKind::Jpeg: {
        QImage image = loadScaledWithQtReader(path, maxEdge);
        if (!image.isNull()) {
            return image;
        }
        DecodedImage decoded = decodeJpeg(path);
        return decoded.isValid()
            ? decoded.image.scaled(maxEdge, maxEdge, Qt::KeepAspectRatio, Qt::SmoothTransformation)
            : QImage();
    }
    case ImageFormatKind::Heif:
        return decodeHeifThumbnail(path, maxEdge);
    case ImageFormatKind::Arw:
        return decodeArwThumbnail(path, maxEdge);
    case ImageFormatKind::Unknown:
    default:
        return {};
    }
}

ImageFormatKind ImageDecoder::detectFormat(const QString& path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == "jpg" || suffix == "jpeg" || suffix == "jpe" || suffix == "jfif") {
        return ImageFormatKind::Jpeg;
    }
    if (suffix == "heif" || suffix == "heic" || suffix == "hif" || suffix == "avif" || suffix == "avifs") {
        return ImageFormatKind::Heif;
    }
    static const QSet<QString> rawExtensions = {
        "3fr", "ari", "arw", "bay", "cap", "cr2", "cr3", "crw", "dcr", "dcs", "dng",
        "drf", "eip", "erf", "fff", "gpr", "iiq", "k25", "kdc", "mdc", "mef", "mos",
        "mrw", "nef", "nrw", "orf", "pef", "ptx", "r3d", "raf", "raw", "rw2", "rwl",
        "rwz", "sr2", "srf", "srw", "x3f"
    };
    if (rawExtensions.contains(suffix)) {
        return ImageFormatKind::Arw;
    }
    return ImageFormatKind::Unknown;
}

QString ImageDecoder::decoderName(ImageFormatKind kind)
{
    switch (kind) {
    case ImageFormatKind::Jpeg:
        return "jpeg-turbo";
    case ImageFormatKind::Heif:
        return "libheif";
    case ImageFormatKind::Arw:
        return "LibRaw";
    case ImageFormatKind::Unknown:
    default:
        return "unknown";
    }
}

QStringList ImageDecoder::supportedExtensions()
{
    return {
        "apng", "avif", "avifs", "bmp", "dib", "exr", "gif", "hdr", "heic", "heif", "hif",
        "ico", "icon", "jfif", "jp2", "jpe", "jpeg", "jpg", "jxl", "jxr", "pbm", "pfm",
        "pgm", "pic", "png", "pnm", "ppm", "psd", "pxm", "qoi", "ras", "sr", "svg",
        "tga", "tif", "tiff", "webp", "wp2",
        "3fr", "ari", "arw", "bay", "cap", "cr2", "cr3", "crw", "dcr", "dcs", "dng",
        "drf", "eip", "erf", "fff", "gpr", "iiq", "k25", "kdc", "mdc", "mef", "mos",
        "mrw", "nef", "nrw", "orf", "pef", "ptx", "r3d", "raf", "raw", "rw2", "rwl",
        "rwz", "sr2", "srf", "srw", "x3f"
    };
}

bool ImageDecoder::supportsFullQuality(ImageFormatKind kind)
{
    return kind == ImageFormatKind::Arw;
}
