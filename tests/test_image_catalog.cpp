#include "catalog/image_catalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

class ImageCatalogTest : public QObject
{
    Q_OBJECT

private slots:
    void loadsFolderWithNaturalSorting();
    void cyclesForwardAndBackward();
};

void ImageCatalogTest::loadsFolderWithNaturalSorting()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile(QString("%1/image10.jpg").arg(dir.path())).open(QIODevice::WriteOnly);
    QFile(QString("%1/image2.jpg").arg(dir.path())).open(QIODevice::WriteOnly);
    QFile(QString("%1/skip.txt").arg(dir.path())).open(QIODevice::WriteOnly);

    ImageCatalog catalog;
    QVERIFY(catalog.loadFromPath(dir.path()));
    QCOMPARE(catalog.size(), 2);
    QCOMPARE(QFileInfo(catalog.paths().at(0)).fileName(), QString("image2.jpg"));
    QCOMPARE(QFileInfo(catalog.paths().at(1)).fileName(), QString("image10.jpg"));
}

void ImageCatalogTest::cyclesForwardAndBackward()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QFile(QString("%1/a.jpg").arg(dir.path())).open(QIODevice::WriteOnly);
    QFile(QString("%1/b.jpg").arg(dir.path())).open(QIODevice::WriteOnly);

    ImageCatalog catalog;
    QVERIFY(catalog.loadFromPath(QString("%1/a.jpg").arg(dir.path())));
    QCOMPARE(QFileInfo(catalog.currentPath()).fileName(), QString("a.jpg"));

    QVERIFY(catalog.moveNext());
    QCOMPARE(QFileInfo(catalog.currentPath()).fileName(), QString("b.jpg"));

    QVERIFY(catalog.moveNext());
    QCOMPARE(QFileInfo(catalog.currentPath()).fileName(), QString("a.jpg"));

    QVERIFY(catalog.movePrevious());
    QCOMPARE(QFileInfo(catalog.currentPath()).fileName(), QString("b.jpg"));
}

QTEST_MAIN(ImageCatalogTest)

#include "test_image_catalog.moc"
