#include "core/prefetch_scheduler.h"

#include <algorithm>

#include <QtTest>

class PrefetchSchedulerTest : public QObject
{
    Q_OBJECT

private slots:
    void ordersCurrentThenDirectionallyAdjacent();
    void fallsBackToRemainingImages();
    void limitsRequestedCount();
};

void PrefetchSchedulerTest::ordersCurrentThenDirectionallyAdjacent()
{
    PrefetchScheduler scheduler;
    const QStringList paths = {"a.jpg", "b.jpg", "c.jpg", "d.jpg"};

    const QList<int> indices = scheduler.orderedIndices(paths.size(), 1, PrefetchScheduler::Direction::Forward, 2);
    QCOMPARE(indices.value(0), 1);
    QCOMPARE(indices.value(1), 2);
    QCOMPARE(indices.value(2), 0);
    QCOMPARE(indices.value(3), 3);
}

void PrefetchSchedulerTest::fallsBackToRemainingImages()
{
    PrefetchScheduler scheduler;
    const QStringList paths = {"a.jpg", "b.jpg", "c.jpg", "d.jpg"};

    const QList<PrefetchScheduler::Request> requests = scheduler.planImageRequests(paths, 1, PrefetchScheduler::Direction::Backward, 1);
    QVERIFY(!requests.isEmpty());
    QCOMPARE(requests.value(0).path, QString("b.jpg"));
    QCOMPARE(requests.value(1).path, QString("a.jpg"));
    QVERIFY(std::any_of(requests.begin(), requests.end(), [](const PrefetchScheduler::Request& request) {
        return request.path == "c.jpg";
    }));
    QVERIFY(std::any_of(requests.begin(), requests.end(), [](const PrefetchScheduler::Request& request) {
        return request.path == "d.jpg";
    }));
}

void PrefetchSchedulerTest::limitsRequestedCount()
{
    PrefetchScheduler scheduler;
    const QStringList paths = {"a.jpg", "b.jpg", "c.jpg", "d.jpg", "e.jpg"};

    const QList<PrefetchScheduler::Request> requests = scheduler.planImageRequests(
        paths,
        2,
        PrefetchScheduler::Direction::Forward,
        4,
        3);

    QCOMPARE(requests.size(), 3);
    QCOMPARE(requests.value(0).path, QString("c.jpg"));
    QVERIFY(requests.size() <= 3);
}

QTEST_MAIN(PrefetchSchedulerTest)

#include "test_prefetch_scheduler.moc"
