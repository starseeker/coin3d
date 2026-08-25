/*
 * Modern GTest coverage for SbModernUtils.
 *
 * This replaces the legacy ObolTest executable.  The tests are individual
 * discoverable cases, so a failure identifies the exact RAII/lookup contract
 * rather than an opaque hand-written subtest name.
 */

#include "framework/test_context.h"

#include <gtest/gtest.h>

#include <Inventor/SbName.h>
#include <Inventor/SoType.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoNode.h>
#include <Inventor/tools/SbModernUtils.h>

namespace {

TEST(SbModernUtils, NameEqualsUsesStringViewWithoutChangingSemantics)
{
    ObolTestSupport::initializeObol();
    EXPECT_TRUE(SbModernUtils::nameEquals(SbName("MyNode"), "MyNode"));
    EXPECT_FALSE(SbModernUtils::nameEquals(SbName("Alpha"), "Beta"));
    EXPECT_TRUE(SbModernUtils::nameEquals(SbName(""), ""));
    EXPECT_FALSE(SbModernUtils::nameEquals(SbName(""), "something"));
}

TEST(SbModernUtils, FindNodeByNameReportsMissingAndExistingNodes)
{
    ObolTestSupport::initializeObol();
    EXPECT_FALSE(SbModernUtils::findNodeByName(SbName("__missing_tools_node__")));

    auto * cube = new SoCube;
    cube->ref();
    cube->setName("modern_utils_lookup_cube");
    const auto found = SbModernUtils::findNodeByName(SbName("modern_utils_lookup_cube"));
    ASSERT_TRUE(found);
    EXPECT_EQ(*found, cube);
    cube->unref();
}

TEST(SbModernUtils, SoNodeRefOwnsAndMovesNodeReferences)
{
    ObolTestSupport::initializeObol();
    auto * cube = new SoCube;
    SbModernUtils::SoNodeRef first(cube);
    ASSERT_TRUE(first);
    EXPECT_EQ(first.get(), cube);
    EXPECT_EQ(first->getTypeId(), SoCube::getClassTypeId());
    EXPECT_EQ((*first).getTypeId(), SoCube::getClassTypeId());

    SbModernUtils::SoNodeRef second(std::move(first));
    EXPECT_FALSE(first);
    ASSERT_TRUE(second);
    EXPECT_EQ(second.get(), cube);

    SbModernUtils::SoNodeRef third(nullptr);
    third = std::move(second);
    EXPECT_FALSE(second);
    EXPECT_EQ(third.get(), cube);
}

TEST(SbModernUtils, SoNodeRefReleaseTransfersTheReference)
{
    ObolTestSupport::initializeObol();
    auto * cube = new SoCube;
    SbModernUtils::SoNodeRef reference(cube);

    SoNode * released = reference.release();
    EXPECT_EQ(released, cube);
    EXPECT_FALSE(reference);
    released->unref();
}

TEST(SbModernUtils, NodeRefFactoriesAndTypedReferenceWrappersOwnNodes)
{
    ObolTestSupport::initializeObol();
    auto * first_cube = new SoCube;
    auto first = SbModernUtils::makeNodeRef(first_cube);
    ASSERT_TRUE(first);
    EXPECT_EQ(first.get(), first_cube);

    auto * second_cube = new SoCube;
    SbModernUtils::RefCountedPtr<SoCube> second(second_cube);
    ASSERT_TRUE(second);
    EXPECT_EQ(second.get(), second_cube);
    EXPECT_EQ(second->getTypeId(), SoCube::getClassTypeId());
    EXPECT_EQ((*second).getTypeId(), SoCube::getClassTypeId());

    auto * third_cube = new SoCube;
    second.reset(third_cube);
    EXPECT_EQ(second.get(), third_cube);

    auto fourth = SbModernUtils::makeRefCountedPtr(new SoCube);
    ASSERT_TRUE(fourth);
    EXPECT_EQ(fourth->getTypeId(), SoCube::getClassTypeId());
}

TEST(SbModernUtils, RefCountedPtrMoveAndReleaseTransferOwnership)
{
    ObolTestSupport::initializeObol();
    auto * cube = new SoCube;
    SbModernUtils::RefCountedPtr<SoCube> first(cube);
    SbModernUtils::RefCountedPtr<SoCube> second(std::move(first));
    EXPECT_FALSE(first);
    ASSERT_TRUE(second);
    EXPECT_EQ(second.get(), cube);

    SoCube * released = second.release();
    EXPECT_EQ(released, cube);
    EXPECT_FALSE(second);
    released->unref();
}

} // namespace
