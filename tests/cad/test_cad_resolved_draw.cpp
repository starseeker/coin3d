/**
 * @file test_cad_resolved_draw.cpp
 *
 * Cross-tier semantic contract for sparse retained draw ranges.  These tests
 * intentionally mix live, hidden, subpixel-replaced, rebound, and out-of-range
 * slots: every renderer executor must see the same occurrence set.
 */

#include "CadResolvedDraw.h"

#include <Obol/cad/CadProgressive.h>

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdint>
#include <limits>

namespace {

using namespace Obol;
using namespace Obol::internal;

int
check(bool condition, const char *message)
{
    if (condition)
        return 0;
    std::fprintf(stderr, "FAIL: %s\n", message);
    return 1;
}

CadVisibleInstance
instance(uint32_t partIndex, uint32_t flags = 0)
{
    CadVisibleInstance value;
    value.partIndex = partIndex;
    value.flags = flags;
    return value;
}

bool
oracleDrawable(const CadFramePlan& plan, const CadDrawItem& item,
               size_t instanceIndex, CadDrawChannel channel)
{
    if (instanceIndex >= plan.visibleInstances.size())
        return false;
    const CadVisibleInstance& visible = plan.visibleInstances[instanceIndex];
    if (visible.partIndex != item.partIndex ||
            (visible.flags & CadInstanceHidden) != 0)
        return false;
    return channel == CadDrawChannel::Points ||
        instanceIndex >= plan.subpixelProxyMask.size() ||
        plan.subpixelProxyMask[instanceIndex] == 0;
}

int
testExhaustiveSparseResolution()
{
    int failures = 0;
    uint64_t cases = 0;
    constexpr size_t slotCount = 5;
    constexpr CadDrawChannel channels[] = {
        CadDrawChannel::Points,
        CadDrawChannel::Wire,
        CadDrawChannel::Shaded
    };

    /* Four independently meaningful states per retained slot: owned or
     * rebound, each visible or hidden.  Cross that with every possible proxy
     * mask prefix, every retained item subrange, and all three channels. */
    uint32_t ownershipStates = 1;
    for (size_t i = 0; i < slotCount; ++i)
        ownershipStates *= 4;
    for (uint32_t stateBits = 0; stateBits < ownershipStates; ++stateBits) {
        CadFramePlan plan;
        uint32_t states = stateBits;
        for (size_t i = 0; i < slotCount; ++i) {
            const uint32_t state = states & 3u;
            states >>= 2;
            plan.visibleInstances.push_back(instance(
                (state & 1u) ? 91u : 7u,
                (state & 2u) ? CadInstanceHidden : 0u));
        }
        for (size_t maskLength = 0; maskLength <= slotCount; ++maskLength) {
            const uint32_t proxyStates = 1u << maskLength;
            for (uint32_t proxyBits = 0; proxyBits < proxyStates;
                 ++proxyBits) {
                plan.subpixelProxyMask.assign(maskLength, 0);
                for (size_t i = 0; i < maskLength; ++i)
                    plan.subpixelProxyMask[i] =
                        (proxyBits & (1u << i)) ? 1 : 0;
                for (uint32_t base = 0; base <= slotCount + 1; ++base) {
                    for (uint32_t count = 0; count <= slotCount + 2;
                         ++count) {
                        CadDrawItem item;
                        item.partIndex = 7;
                        item.baseInstance = base;
                        item.instanceCount = count;
                        for (CadDrawChannel channel : channels) {
                            size_t expectedFirst =
                                plan.visibleInstances.size();
                            size_t expectedCount = 0;
                            for (uint32_t offset = 0; offset < count;
                                 ++offset) {
                                const size_t index =
                                    static_cast<size_t>(base) + offset;
                                const bool expected = oracleDrawable(
                                    plan, item, index, channel);
                                failures += check(
                                    cadInstanceDrawable(
                                        plan, item, index, channel) == expected,
                                    "per-slot sparse resolution differs from oracle");
                                if (expected) {
                                    if (!expectedCount)
                                        expectedFirst = index;
                                    ++expectedCount;
                                }
                            }
                            failures += check(
                                cadFirstDrawableInstance(plan, item, channel) ==
                                    expectedFirst,
                                "first sparse occurrence differs from oracle");
                            failures += check(
                                cadDrawableInstanceCount(plan, item, channel) ==
                                    expectedCount,
                                "sparse occurrence count differs from oracle");
                            ++cases;
                            if (failures)
                                return failures;
                        }
                    }
                }
            }
        }
    }

    /* Exercise the complete serialized cut domain, including malformed or
     * transitional minimum>resident inputs.  No result may exceed the
     * published resident prefix. */
    for (uint32_t requested = 0; requested <= 255; ++requested) {
        for (uint32_t minimum = 0; minimum <= 255; ++minimum) {
            for (uint32_t resident = 0; resident <= 255; ++resident) {
                const uint8_t availableMinimum = static_cast<uint8_t>(
                    minimum > resident ? resident : minimum);
                const uint8_t expected = static_cast<uint8_t>(
                    requested < availableMinimum ? availableMinimum :
                    (requested > resident ? resident : requested));
                failures += check(cadResolvedProgressiveCut(
                        static_cast<uint8_t>(requested),
                        static_cast<uint8_t>(minimum),
                        static_cast<uint8_t>(resident)) == expected,
                    "progressive cut differs from residency-safe oracle");
                ++cases;
                if (failures)
                    return failures;
            }
        }
    }
    std::printf("Checked %llu resolved-draw contract cases\n",
        static_cast<unsigned long long>(cases));
    return failures;
}

} // namespace

TEST(CadResolvedDraw, PreservesSparseOwnershipAndProgressiveBoundaries)
{
    /* A planar or linear source has exact zero-extent axes.  Requiring all
     * three bit counts to reach 16 misclassified the terminal cut and kept
     * the renderer/picker on their quantized paths indefinitely. */
    EXPECT_TRUE((ProgressiveQuantization{0, 0, 0}.isExact()));
    EXPECT_TRUE((ProgressiveQuantization{16, 16, 0}.isExact()));
    EXPECT_TRUE((ProgressiveQuantization{16, 0, 16}.isExact()));
    EXPECT_FALSE((ProgressiveQuantization{16, 15, 0}.isExact()));

    CadFramePlan plan;
    plan.visibleInstances = {
        instance(7),
        instance(7, CadInstanceHidden),
        instance(7),
        instance(99), // rebound slot retained in the old part range
        instance(7)
    };
    plan.subpixelProxyMask = {0, 0, 1, 0, 0};

    CadDrawItem item;
    item.partIndex = 7;
    item.baseInstance = 0;
    item.instanceCount = 7; // last two slots are conservatively out of range

    EXPECT_EQ(cadDrawableInstanceCount(plan, item, CadDrawChannel::Points), 3u);
    EXPECT_EQ(cadDrawableInstanceCount(plan, item, CadDrawChannel::Wire), 2u);
    EXPECT_EQ(cadDrawableInstanceCount(plan, item, CadDrawChannel::Shaded), 2u);
    EXPECT_EQ(cadFirstDrawableInstance(plan, item, CadDrawChannel::Shaded), 0u);
    EXPECT_FALSE(cadInstanceDrawable(plan, item, 2, CadDrawChannel::Shaded));
    EXPECT_TRUE(cadInstanceDrawable(plan, item, 2, CadDrawChannel::Points));
    EXPECT_FALSE(cadInstanceDrawable(plan, item, 3, CadDrawChannel::Wire));
    EXPECT_FALSE(cadInstanceDrawable(plan, item, 6, CadDrawChannel::Wire));

    CadDrawItem empty;
    empty.partIndex = 7;
    empty.baseInstance = std::numeric_limits<uint32_t>::max();
    empty.instanceCount = 2;
    EXPECT_EQ(cadDrawableInstanceCount(plan, empty, CadDrawChannel::Wire), 0u);
    EXPECT_EQ(cadResolvedProgressiveCut(2, 4, 10), 4u);
    EXPECT_EQ(cadResolvedProgressiveCut(12, 4, 10), 10u);
    EXPECT_EQ(cadResolvedProgressiveCut(7, 4, 10), 7u);
    EXPECT_EQ(cadResolvedProgressiveCut(16, 0, 16), 16u);
    EXPECT_EQ(cadResolvedProgressiveCut(5, 9, 3), 3u);
}

TEST(CadResolvedDraw, MatchesTheExhaustiveSparseResolutionOracle)
{
    EXPECT_EQ(testExhaustiveSparseResolution(), 0);
}
