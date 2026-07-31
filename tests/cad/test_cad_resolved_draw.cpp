/**
 * @file test_cad_resolved_draw.cpp
 *
 * Cross-tier semantic contract for sparse retained draw ranges.  These tests
 * intentionally mix live, hidden, subpixel-replaced, rebound, and out-of-range
 * slots: every renderer executor must see the same occurrence set.
 */

#include "CadResolvedDraw.h"

#include <cstdio>
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

} // namespace

int
main()
{
    int failures = 0;

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

    failures += check(
        cadDrawableInstanceCount(plan, item, CadDrawChannel::Points) == 3,
        "point resolution must retain authored points at proxy slots");
    failures += check(
        cadDrawableInstanceCount(plan, item, CadDrawChannel::Wire) == 2,
        "wire resolution must omit hidden, proxy, rebound, and absent slots");
    failures += check(
        cadDrawableInstanceCount(plan, item, CadDrawChannel::Shaded) == 2,
        "shaded resolution must match wire replacement semantics");
    failures += check(
        cadFirstDrawableInstance(plan, item, CadDrawChannel::Shaded) == 0,
        "first resolved shaded occurrence is wrong");
    failures += check(
        !cadInstanceDrawable(plan, item, 2, CadDrawChannel::Shaded) &&
        cadInstanceDrawable(plan, item, 2, CadDrawChannel::Points),
        "subpixel replacement must be channel specific");
    failures += check(
        !cadInstanceDrawable(plan, item, 3, CadDrawChannel::Wire),
        "a rebound sparse slot must not be owned by its old draw range");
    failures += check(
        !cadInstanceDrawable(plan, item, 6, CadDrawChannel::Wire),
        "an out-of-range sparse slot must resolve safely");

    CadDrawItem empty;
    empty.partIndex = 7;
    empty.baseInstance = std::numeric_limits<uint32_t>::max();
    empty.instanceCount = 2;
    failures += check(
        cadDrawableInstanceCount(plan, empty, CadDrawChannel::Wire) == 0,
        "large sparse range indices must remain bounds safe");

    failures += check(cadResolvedProgressiveLevel(2, 4, 10) == 4,
        "progressive request must clamp to minimum");
    failures += check(cadResolvedProgressiveLevel(12, 4, 10) == 10,
        "progressive request must clamp to resident level");
    failures += check(cadResolvedProgressiveLevel(7, 4, 10) == 7,
        "progressive request inside the resident interval changed");
    failures += check(cadResolvedProgressiveLevel(0, 0, 16) == 15,
        "non-progressive/full-resident sentinel must resolve exact");

    if (!failures)
        std::printf("Cad resolved-draw semantic contract passed\n");
    return failures ? 1 : 0;
}
