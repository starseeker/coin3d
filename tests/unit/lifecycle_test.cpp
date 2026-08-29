#include <gtest/gtest.h>

#include <Inventor/SoDB.h>
#include <Inventor/annex/Profiler/SoProfiler.h>
#include <Inventor/annex/Profiler/nodes/SoProfilerStats.h>
#include <Inventor/misc/SoContextHandler.h>
#include <Inventor/nodes/SoSeparator.h>

namespace {

class LifecycleContextManager final : public SoDB::ContextManager {
public:
    void * createOffscreenContext(unsigned int, unsigned int) override { return nullptr; }
    SbBool makeContextCurrent(void *) override { return FALSE; }
    void restorePreviousContext(void *) override { }
    void destroyContext(void *) override { }
};

void exercise_initialized_database()
{
    EXPECT_TRUE(SoDB::isInitialized());
    EXPECT_FALSE(SoSeparator::getClassTypeId().isBad());

    SoDB::writelock();
    SoDB::writeunlock();
    SoDB::startNotify();
    SoDB::endNotify();

    SoSeparator * root = new SoSeparator;
    root->ref();
    EXPECT_EQ(root->getNumChildren(), 0);
    root->unref();
}

void unused_context_callback(uint32_t, void *)
{
}

} // namespace

TEST(DatabaseLifecycle, SupportsRepeatedInitFinishCyclesWithoutContextManager)
{
    ASSERT_FALSE(SoDB::isInitialized());

    for (int generation = 0; generation < 3; ++generation) {
        SoDB::init(nullptr);
        EXPECT_EQ(SoDB::getContextManager(), nullptr);
        exercise_initialized_database();
        SoContextHandler::addContextDestructionCallback(unused_context_callback,
                                                         &generation);
        SoDB::finish();
        EXPECT_FALSE(SoDB::isInitialized());
        // Main-thread TLS destructors can perform this same late unregister.
        SoContextHandler::removeContextDestructionCallback(unused_context_callback,
                                                            &generation);
    }
}

TEST(DatabaseLifecycle, ReinitializesProfilerOwnedTypes)
{
    ASSERT_FALSE(SoDB::isInitialized());

    for (int generation = 0; generation < 3; ++generation) {
        SoDB::init(nullptr);
        SoProfiler::init();
        EXPECT_FALSE(SoProfiler::isEnabled())
            << "profiler must remain opt-in after initialization";
        ASSERT_FALSE(SoProfilerStats::getClassTypeId().isBad())
            << "generation " << generation;

        SoProfilerStats * stats = new SoProfilerStats;
        stats->ref();
        stats->unref();

        SoDB::finish();
        EXPECT_FALSE(SoDB::isInitialized());
        EXPECT_TRUE(SoProfilerStats::getClassTypeId().isBad());
    }
}

TEST(DatabaseLifecycle, NullReinitPreservesAnInstalledContextManager)
{
    ASSERT_FALSE(SoDB::isInitialized());
    LifecycleContextManager manager;

    SoDB::init(&manager);
    ASSERT_EQ(SoDB::getContextManager(), &manager);
    SoDB::init(nullptr);
    EXPECT_EQ(SoDB::getContextManager(), &manager);

    SoDB::finish();
    EXPECT_EQ(SoDB::getContextManager(), nullptr);
}
