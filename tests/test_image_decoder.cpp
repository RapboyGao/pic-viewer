#include "decoder/image_decoder.h"

#include <QtTest>

class ImageDecoderTest : public QObject
{
    Q_OBJECT

private slots:
    void detectsFormats();
    void reportsFullQualitySupport();
};

void ImageDecoderTest::detectsFormats()
{
    QCOMPARE(ImageDecoder::detectFormat("foo.jpg"), ImageFormatKind::Jpeg);
    QCOMPARE(ImageDecoder::detectFormat("foo.jfif"), ImageFormatKind::Jpeg);
    QCOMPARE(ImageDecoder::detectFormat("foo.HEIC"), ImageFormatKind::Heif);
    QCOMPARE(ImageDecoder::detectFormat("foo.avif"), ImageFormatKind::Heif);
    QCOMPARE(ImageDecoder::detectFormat("foo.arw"), ImageFormatKind::Arw);
    QCOMPARE(ImageDecoder::detectFormat("foo.cr3"), ImageFormatKind::Arw);
    QCOMPARE(ImageDecoder::detectFormat("foo.png"), ImageFormatKind::Unknown);
}

void ImageDecoderTest::reportsFullQualitySupport()
{
    QVERIFY(!ImageDecoder::supportsFullQuality(ImageFormatKind::Jpeg));
    QVERIFY(!ImageDecoder::supportsFullQuality(ImageFormatKind::Heif));
    QVERIFY(ImageDecoder::supportsFullQuality(ImageFormatKind::Arw));
}

QTEST_MAIN(ImageDecoderTest)

#include "test_image_decoder.moc"
