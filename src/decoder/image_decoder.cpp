#include "decoder/image_decoder.h"

#include <QFileInfo>
#include <QImage>

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
    while (cinfo.output_scanline < cinfo.output_height) {
        unsigned char* row = image.scanLine(static_cast<int>(cinfo.output_scanline));
        jpeg_read_scanlines(&cinfo, &row, 1);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    std::fclose(file);

    DecodedImage result;
    result.filePath = path;
    result.image = image.copy();
    result.sourceSize = result.image.size();
    result.decoderName = "jpeg-turbo";
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
                result.image = preview.convertToFormat(QImage::Format_RGBA8888);
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
            result.image = preview.convertToFormat(QImage::Format_RGBA8888);
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

    rawProcessor.imgdata.params.use_camera_wb = 1;
    rawProcessor.imgdata.params.no_auto_bright = 1;

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
        result.image = qimage.convertToFormat(QImage::Format_RGBA8888);
        result.sourceSize = result.image.size();
        result.isPreview = false;
    } else if (processedImage->type == LIBRAW_IMAGE_JPEG) {
        QImage qimage;
        qimage.loadFromData(
            reinterpret_cast<const uchar*>(processedImage->data),
            processedImage->data_size,
            "JPEG");
        result.image = qimage.convertToFormat(QImage::Format_RGBA8888);
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

} // namespace

DecodedImage ImageDecoder::decode(const QString& path, DecodeMode mode)
{
    const ImageFormatKind kind = detectFormat(path);
    switch (kind) {
    case ImageFormatKind::Jpeg:
        return decodeJpeg(path);
    case ImageFormatKind::Heif:
        return decodeHeif(path);
    case ImageFormatKind::Arw:
        return decodeArw(path, mode);
    case ImageFormatKind::Unknown:
    default:
        return makeError(path, "unknown", "Unsupported image format.");
    }
}

ImageFormatKind ImageDecoder::detectFormat(const QString& path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == "jpg" || suffix == "jpeg") {
        return ImageFormatKind::Jpeg;
    }
    if (suffix == "heif" || suffix == "heic" || suffix == "hif") {
        return ImageFormatKind::Heif;
    }
    if (suffix == "arw") {
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
    return {"jpg", "jpeg", "heif", "heic", "hif", "arw"};
}

bool ImageDecoder::supportsFullQuality(ImageFormatKind kind)
{
    return kind == ImageFormatKind::Arw;
}
