#include <gtest/gtest.h>

#include <Inventor/SoDB.h>
#include <Inventor/SoPath.h>
#include <Inventor/SbTime.h>
#include <Inventor/fields/SoSFFloat.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/sensors/SoFieldSensor.h>
#include <Inventor/sensors/SoNodeSensor.h>
#include <Inventor/sensors/SoOneShotSensor.h>
#include <Inventor/sensors/SoPathSensor.h>
#include <Inventor/sensors/SoIdleSensor.h>
#include <Inventor/sensors/SoSensor.h>
#include <Inventor/sensors/SoSensorManager.h>
#include <Inventor/sensors/SoTimerSensor.h>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

namespace {

void increment(void * user_data, SoSensor *)
{
    ++*static_cast<int *>(user_data);
}

void incrementAtomic(void * user_data, SoSensor *)
{
    static_cast<std::atomic<int> *>(user_data)->fetch_add(
        1, std::memory_order_relaxed);
}

void processPendingSensors()
{
    auto * manager = SoDB::getSensorManager();
    manager->processTimerQueue();
    manager->processDelayQueue(TRUE);
}

TEST(Sensors, FieldSensorAttachesFiresAndDetaches)
{
    SoSFFloat field;
    field.setValue(1.0f);
    int fired = 0;
    SoFieldSensor sensor(increment, &fired);
    sensor.attach(&field);
    EXPECT_EQ(sensor.getAttachedField(), &field);

    field.setValue(2.0f);
    processPendingSensors();
    EXPECT_GT(fired, 0);

    sensor.detach();
    EXPECT_EQ(sensor.getAttachedField(), nullptr);
}

TEST(Sensors, NodeSensorAttachesFiresAndDetaches)
{
    auto * cube = new SoCube;
    cube->ref();
    int fired = 0;
    SoNodeSensor sensor(increment, &fired);
    sensor.attach(cube);
    EXPECT_EQ(sensor.getAttachedNode(), cube);

    cube->width.setValue(5.0f);
    processPendingSensors();
    EXPECT_GT(fired, 0);

    sensor.detach();
    EXPECT_EQ(sensor.getAttachedNode(), nullptr);
    cube->unref();
}

TEST(Sensors, PathSensorAttachesFiresAndDetaches)
{
    auto * root = new SoSeparator;
    root->ref();
    auto * cube = new SoCube;
    root->addChild(cube);

    auto * path = new SoPath(root);
    path->ref();
    path->append(0);

    int fired = 0;
    SoPathSensor sensor(increment, &fired);
    sensor.attach(path);
    EXPECT_EQ(sensor.getAttachedPath(), path);

    cube->width.setValue(5.0f);
    processPendingSensors();
    EXPECT_GT(fired, 0);

    sensor.detach();
    EXPECT_EQ(sensor.getAttachedPath(), nullptr);
    path->unref();
    root->unref();
}

TEST(Sensors, TimerSensorRetainsSchedulingConfiguration)
{
    SoTimerSensor sensor;
    sensor.setInterval(SbTime(0.1));
    EXPECT_NEAR(sensor.getInterval().getValue(), 0.1, 1e-3);

    const SbTime base_time(1000.0);
    sensor.setBaseTime(base_time);
    EXPECT_NEAR(sensor.getBaseTime().getValue(), 1000.0, 1e-3);

    sensor.schedule();
    sensor.unschedule();
}

TEST(Sensors, ManagerProcessesOneShotAndIdleSensors)
{
    int one_shot_fired = 0;
    SoOneShotSensor one_shot(increment, &one_shot_fired);
    one_shot.schedule();
    processPendingSensors();
    EXPECT_GT(one_shot_fired, 0);

    int idle_fired = 0;
    SoIdleSensor idle(increment, &idle_fired);
    idle.schedule();
    processPendingSensors();
    EXPECT_GT(idle_fired, 0);
}

TEST(Sensors, ConcurrentQueueProcessorsDeliverEachSensorOnce)
{
    constexpr int sensorCount = 64;
    std::atomic<int> fired{0};
    std::vector<std::unique_ptr<SoOneShotSensor>> sensors;
    sensors.reserve(sensorCount);
    for (int i = 0; i < sensorCount; ++i) {
        auto sensor = std::make_unique<SoOneShotSensor>(incrementAtomic, &fired);
        sensor->schedule();
        sensors.push_back(std::move(sensor));
    }

    std::vector<std::thread> processors;
    for (int thread = 0; thread < 8; ++thread) {
        processors.emplace_back([] {
            SoDB::getSensorManager()->processDelayQueue(TRUE);
        });
    }
    for (std::thread & processor : processors) processor.join();

    EXPECT_EQ(fired.load(std::memory_order_relaxed), sensorCount);
    for (const auto & sensor : sensors) EXPECT_FALSE(sensor->isScheduled());
}

TEST(Sensors, DelayTimeoutConfigurationSupportsConcurrentSnapshots)
{
    SoSensorManager * manager = SoDB::getSensorManager();
    std::atomic<bool> failed{false};
    std::vector<std::thread> threads;
    for (int thread = 0; thread < 8; ++thread) {
        threads.emplace_back([&, thread] {
            for (int iteration = 0; iteration < 250; ++iteration) {
                manager->setDelaySensorTimeout(SbTime(0.01 * (thread + 1)));
                const double value = manager->getDelaySensorTimeout().getValue();
                if (!(value >= 0.01 && value <= 0.08)) {
                    failed.store(true, std::memory_order_relaxed);
                }
            }
        });
    }
    for (std::thread & thread : threads) thread.join();
    EXPECT_FALSE(failed.load(std::memory_order_relaxed));
    manager->setDelaySensorTimeout(SbTime(1.0 / 12.0));
}

} // namespace
