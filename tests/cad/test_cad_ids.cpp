/**
 * @file test_cad_ids.cpp
 * @brief Unit tests for Obol::CadIdBuilder and Obol::CadId128.
 *
 * Tests:
 *  1. Same traversal path produces identical InstanceId
 *  2. Different occurrence index under same parent gives different IDs
 *  3. Order changes change IDs
 *  4. Root sentinel is all-zeros
 *  5. hash128 of same bytes is deterministic
 *  6. hash128 of different bytes produces different IDs
 *  7. std::hash<CadId128> compiles and gives consistent values
 *
 * No BRL-CAD dependency.  No GL context required.
 */

#include <Obol/cad/CadIds.h>

#include <gtest/gtest.h>

#include <unordered_map>
#include <string>
#include <vector>

namespace {

using Obol::CadId128;
using Obol::CadIdBuilder;

TEST(CadIds, RootIsTheInvalidZeroSentinel)
{
    const CadId128 root = CadIdBuilder::Root();
    EXPECT_EQ(root.w0, 0u);
    EXPECT_EQ(root.w1, 0u);
    EXPECT_FALSE(root.isValid());
}

TEST(CadIds, HashingIsDeterministicAndDistinguishesKeys)
{
    const CadId128 wheel = CadIdBuilder::hash128(std::string("wheel"));
    EXPECT_EQ(wheel, CadIdBuilder::hash128(std::string("wheel")));
    EXPECT_NE(wheel, CadIdBuilder::hash128(std::string("bolt")));
    EXPECT_TRUE(wheel.isValid());
}

TEST(CadIds, ExtendingAPathPreservesOccurrenceAndBooleanIdentity)
{
    const CadId128 root = CadIdBuilder::Root();
    const CadId128 wheel = CadIdBuilder::extendNameOccBool(root, "wheel", 0, 0);

    EXPECT_EQ(wheel, CadIdBuilder::extendNameOccBool(root, "wheel", 0, 0));
    EXPECT_NE(wheel, CadIdBuilder::extendNameOccBool(root, "wheel", 1, 0));
    EXPECT_NE(wheel, CadIdBuilder::extendNameOccBool(root, "wheel", 0, 1));
}

TEST(CadIds, TraversalOrderAndDeepPathsAreDeterministic)
{
    const CadId128 root = CadIdBuilder::Root();
    const CadId128 ab = CadIdBuilder::extendNameOccBool(
        CadIdBuilder::extendNameOccBool(root, "A", 0, 0), "B", 0, 0);
    const CadId128 ba = CadIdBuilder::extendNameOccBool(
        CadIdBuilder::extendNameOccBool(root, "B", 0, 0), "A", 0, 0);
    EXPECT_NE(ab, ba);

    const std::vector<std::string> path = {"vehicle", "chassis", "axle", "bolt"};
    const auto make_path = [&path] {
        CadId128 id = CadIdBuilder::Root();
        for (const auto & name : path) id = CadIdBuilder::extendNameOccBool(id, name, 0, 0);
        return id;
    };
    EXPECT_EQ(make_path(), make_path());
}

TEST(CadIds, StandardHashSupportsAssociativeContainers)
{
    const CadId128 first = CadIdBuilder::hash128(std::string("key1"));
    const CadId128 second = CadIdBuilder::hash128(std::string("key2"));
    std::hash<CadId128> hasher;
    EXPECT_EQ(hasher(first), hasher(first));

    std::unordered_map<CadId128, int> values;
    values.emplace(first, 42);
    values.emplace(second, 99);
    EXPECT_EQ(values.at(first), 42);
    EXPECT_EQ(values.at(second), 99);
}

} // namespace
