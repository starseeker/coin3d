#include <gtest/gtest.h>

#include <Inventor/SbName.h>
#include <Inventor/SoPath.h>
#include <Inventor/actions/SoSearchAction.h>
#include <Inventor/nodes/SoCube.h>
#include <Inventor/nodes/SoGroup.h>
#include <Inventor/nodes/SoSeparator.h>
#include <Inventor/nodes/SoSphere.h>

TEST(Paths, ManualConstructionAppendAndTruncationPreserveHierarchy)
{
    auto * root = new SoSeparator;
    root->ref();
    auto * group = new SoGroup;
    auto * cube = new SoCube;
    root->addChild(group);
    group->addChild(cube);

    auto * path = new SoPath(root);
    path->ref();
    path->append(0);
    path->append(0);
    EXPECT_EQ(path->getLength(), 3);
    EXPECT_EQ(path->getHead(), root);
    EXPECT_EQ(path->getNode(1), group);
    EXPECT_EQ(path->getTail(), cube);
    EXPECT_TRUE(path->containsNode(cube));
    auto * unrelated_sphere = new SoSphere;
    unrelated_sphere->ref();
    EXPECT_FALSE(path->containsNode(unrelated_sphere));
    unrelated_sphere->unref();
    path->truncate(2);
    EXPECT_EQ(path->getLength(), 2);
    path->push(0);
    EXPECT_EQ(path->getLength(), 3);
    path->pop();
    EXPECT_EQ(path->getLength(), 2);
    path->unref();
    root->unref();
}

TEST(Paths, SearchResultsExposeTailRelativeAccess)
{
    auto * root = new SoSeparator;
    root->ref();
    auto * group = new SoGroup;
    auto * cube = new SoCube;
    cube->setName("modern-path-target");
    root->addChild(group);
    group->addChild(cube);

    SoSearchAction search;
    search.setName(SbName("modern-path-target"));
    search.setFind(SoSearchAction::NAME);
    search.apply(root);
    SoPath * path = search.getPath();
    ASSERT_NE(path, nullptr);
    EXPECT_GE(path->getLength(), 3);
    EXPECT_EQ(path->getNodeFromTail(0), cube);
    root->unref();
}

TEST(Paths, CopiesForksAndMembershipRetainTheOriginalHierarchy)
{
    auto * root = new SoSeparator;
    root->ref();
    auto * group = new SoGroup;
    auto * cube = new SoCube;
    auto * sphere = new SoSphere;
    root->addChild(group);
    group->addChild(cube);
    root->addChild(sphere);

    auto * path = new SoPath(root);
    path->ref();
    path->append(0);
    path->append(0);
    auto * copy = path->copy();
    copy->ref();
    EXPECT_EQ(copy->getLength(), path->getLength());
    EXPECT_EQ(copy->getTail(), cube);
    EXPECT_EQ(path->findFork(copy), path->getLength() - 1);
    EXPECT_EQ(path->getIndexFromTail(0), 0);
    EXPECT_TRUE(path->containsNode(cube));
    EXPECT_FALSE(path->containsNode(sphere));
    auto * equivalent_path = new SoPath(root);
    equivalent_path->ref();
    equivalent_path->append(0);
    equivalent_path->append(0);
    EXPECT_TRUE(path->containsPath(equivalent_path));

    equivalent_path->unref();
    copy->unref();
    path->unref();
    root->unref();
}

TEST(Paths, SearchedPathsCanBeCopiedAndAppended)
{
    auto * root = new SoSeparator;
    root->ref();
    auto * group = new SoGroup;
    auto * cube = new SoCube;
    group->addChild(cube);
    root->addChild(group);

    SoSearchAction group_search;
    group_search.setNode(group);
    group_search.apply(root);
    ASSERT_NE(group_search.getPath(), nullptr);
    SoPath * parent_path = group_search.getPath();
    parent_path->ref();

    SoSearchAction cube_search;
    cube_search.setNode(cube);
    cube_search.apply(group);
    ASSERT_NE(cube_search.getPath(), nullptr);
    SoPath * child_path = cube_search.getPath();
    child_path->ref();

    auto * combined_path = parent_path->copy();
    combined_path->ref();
    combined_path->append(child_path);
    EXPECT_EQ(combined_path->getLength(), 3);
    EXPECT_EQ(combined_path->getHead(), root);
    EXPECT_EQ(combined_path->getNode(1), group);
    EXPECT_EQ(combined_path->getTail(), cube);

    combined_path->unref();
    child_path->unref();
    parent_path->unref();
    root->unref();
}
