#include <gtest/gtest.h>

#include <Inventor/SoDB.h>
#include <Inventor/nodes/SoSeparator.h>

namespace {

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

} // namespace

TEST(DatabaseLifecycle, SupportsRepeatedInitFinishCyclesWithoutContextManager)
{
    ASSERT_FALSE(SoDB::isInitialized());

    for (int generation = 0; generation < 3; ++generation) {
        SoDB::init(nullptr);
        exercise_initialized_database();
        SoDB::finish();
        EXPECT_FALSE(SoDB::isInitialized());
    }
}
