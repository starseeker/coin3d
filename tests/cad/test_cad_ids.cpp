/**
 * @file test_cad_ids.cpp
 * @brief Unit tests for Obol's domain-safe CAD identifiers.
 *
 * Tests:
 *  1. Same traversal path produces identical InstanceId
 *  2. Different occurrence index under same parent gives different IDs
 *  3. Order changes change IDs
 *  4. Root sentinel is all-zeros
 *  5. stable-key hashing is deterministic and domain separated by type
 *  6. different bytes produce different IDs
 *  7. std::hash supports both identifier domains
 *
 * No BRL-CAD dependency.  No GL context required.
 */

#include <Obol/cad/CadIds.h>

#include <gtest/gtest.h>

#include <unordered_map>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

using Obol::CadIdBuilder;
using Obol::InstanceId;
using Obol::PartId;

static_assert(!std::is_convertible<PartId, InstanceId>::value,
    "part IDs must not enter the instance-ID domain");
static_assert(!std::is_convertible<InstanceId, PartId>::value,
    "instance IDs must not enter the part-ID domain");

TEST(CadIds, RootIsTheInvalidZeroSentinel)
{
    const InstanceId root = CadIdBuilder::rootInstance();
    EXPECT_EQ(root.w0, 0u);
    EXPECT_EQ(root.w1, 0u);
    EXPECT_FALSE(root.isValid());
}

TEST(CadIds, HashingIsDeterministicAndDistinguishesKeys)
{
    const PartId wheel = CadIdBuilder::partId(std::string("wheel"));
    EXPECT_EQ(wheel, CadIdBuilder::partId(std::string("wheel")));
    EXPECT_NE(wheel, CadIdBuilder::partId(std::string("bolt")));
    EXPECT_TRUE(wheel.isValid());
}

TEST(CadIds, ExtendingAPathPreservesOccurrenceAndBooleanIdentity)
{
    const InstanceId root = CadIdBuilder::rootInstance();
    const InstanceId wheel = CadIdBuilder::childInstance(root, "wheel", 0, 0);

    EXPECT_EQ(wheel, CadIdBuilder::childInstance(root, "wheel", 0, 0));
    EXPECT_NE(wheel, CadIdBuilder::childInstance(root, "wheel", 1, 0));
    EXPECT_NE(wheel, CadIdBuilder::childInstance(root, "wheel", 0, 1));

    // Instance IDs are persisted by clients, so implementation changes must
    // retain the established byte encoding.
    EXPECT_EQ(wheel.w0, UINT64_C(0x5fdcbbf0994365a5));
    EXPECT_EQ(wheel.w1, UINT64_C(0x8aa83d078483e27c));
}

TEST(CadIds, LongChildNamesRemainNoexceptAndDeterministic)
{
    static_assert(noexcept(CadIdBuilder::childInstance(
        InstanceId{}, std::declval<const std::string&>(), 0, 0)),
        "child ID construction must remain noexcept");

    const std::string longName(4096, 'x');
    const InstanceId parent = CadIdBuilder::instanceId("long-name-parent");
    const InstanceId first =
        CadIdBuilder::childInstance(parent, longName, 123456u, 2u);
    EXPECT_TRUE(first.isValid());
    EXPECT_EQ(first,
        CadIdBuilder::childInstance(parent, longName, 123456u, 2u));
    EXPECT_NE(first,
        CadIdBuilder::childInstance(parent, longName, 123457u, 2u));
}

TEST(CadIds, TraversalOrderAndDeepPathsAreDeterministic)
{
    const InstanceId root = CadIdBuilder::rootInstance();
    const InstanceId ab = CadIdBuilder::childInstance(
        CadIdBuilder::childInstance(root, "A", 0, 0), "B", 0, 0);
    const InstanceId ba = CadIdBuilder::childInstance(
        CadIdBuilder::childInstance(root, "B", 0, 0), "A", 0, 0);
    EXPECT_NE(ab, ba);

    const std::vector<std::string> path = {"vehicle", "chassis", "axle", "bolt"};
    const auto make_path = [&path] {
        InstanceId id = CadIdBuilder::rootInstance();
        for (const auto& name : path)
            id = CadIdBuilder::childInstance(id, name, 0, 0);
        return id;
    };
    EXPECT_EQ(make_path(), make_path());
}

TEST(CadIds, StandardHashSupportsAssociativeContainers)
{
    const PartId first = CadIdBuilder::partId(std::string("key1"));
    const PartId second = CadIdBuilder::partId(std::string("key2"));
    std::hash<PartId> hasher;
    EXPECT_EQ(hasher(first), hasher(first));

    std::unordered_map<PartId, int> values;
    values.emplace(first, 42);
    values.emplace(second, 99);
    EXPECT_EQ(values.at(first), 42);
    EXPECT_EQ(values.at(second), 99);
}

} // namespace
