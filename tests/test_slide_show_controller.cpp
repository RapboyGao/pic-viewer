#include "core/slide_show_controller.h"

#include <QSignalSpy>
#include <QtTest>

class SlideShowControllerTest : public QObject
{
    Q_OBJECT

private slots:
    void togglesPlayback();
    void emitsAdvanceRequests();
};

void SlideShowControllerTest::togglesPlayback()
{
    SlideShowController controller;
    QSignalSpy playingSpy(&controller, &SlideShowController::playingChanged);

    controller.start();
    QVERIFY(controller.isPlaying());
    QCOMPARE(playingSpy.size(), 1);

    controller.stop();
    QVERIFY(!controller.isPlaying());
    QCOMPARE(playingSpy.size(), 2);
}

void SlideShowControllerTest::emitsAdvanceRequests()
{
    SlideShowController controller;
    controller.setIntervalMs(25);

    QSignalSpy advanceSpy(&controller, &SlideShowController::advanceRequested);
    controller.start();
    QTRY_VERIFY_WITH_TIMEOUT(advanceSpy.size() >= 1, 200);
}

QTEST_MAIN(SlideShowControllerTest)

#include "test_slide_show_controller.moc"
