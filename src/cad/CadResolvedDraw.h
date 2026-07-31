#ifndef OBOL_CAD_CADRESOLVEDDRAW_H
#define OBOL_CAD_CADRESOLVEDDRAW_H

/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the conditions in COPYING are
 * met.
\**************************************************************************/

/**
 * @file CadResolvedDraw.h
 * @brief Allocation-free semantic view over a retained CadFramePlan.
 *
 * Every renderer tier must agree on which sparse instance slots are live.
 * CadFramePlan intentionally retains tombstones and stale range slots so
 * box-to-mesh promotion, erase, and LoD changes do not rebuild an entire
 * large scene.  Consequently range containment alone is never sufficient.
 *
 * This file is the single contract for resolving a draw item into live point,
 * wire, or shaded occurrences.  It is a non-owning view: resolution performs
 * no allocation, copying, hashing, virtual dispatch, or whole-scene pass.
 */

#include "CadFramePlan.h"

#include <cstddef>
#include <cstdint>
#include <limits>

namespace Obol {
namespace internal {

enum class CadDrawChannel : uint8_t {
    Points,
    Wire,
    Shaded
};

inline bool
cadDrawItemInstanceIndex(const CadDrawItem& item, uint32_t offset,
                         size_t& instanceIndex) noexcept
{
    if (offset >= item.instanceCount)
        return false;
    const size_t base = static_cast<size_t>(item.baseInstance);
    const size_t delta = static_cast<size_t>(offset);
    if (base > std::numeric_limits<size_t>::max() - delta)
        return false;
    instanceIndex = base + delta;
    return true;
}

inline bool
cadDrawItemOwnsInstance(const CadFramePlan& plan, const CadDrawItem& item,
                        size_t instanceIndex) noexcept
{
    return instanceIndex < plan.visibleInstances.size() &&
        plan.visibleInstances[instanceIndex].partIndex == item.partIndex;
}

inline bool
cadInstanceHidden(const CadFramePlan& plan, size_t instanceIndex) noexcept
{
    return instanceIndex >= plan.visibleInstances.size() ||
        (plan.visibleInstances[instanceIndex].flags &
         CadInstanceHidden) != 0;
}

inline bool
cadInstanceSubpixelReplaced(const CadFramePlan& plan,
                            size_t instanceIndex) noexcept
{
    return instanceIndex < plan.subpixelProxyMask.size() &&
        plan.subpixelProxyMask[instanceIndex] != 0;
}

/**
 * Return whether one retained sparse slot contributes to this render channel.
 *
 * A subpixel proxy replaces wire and shaded geometry, but not authored point
 * geometry.  Hidden and rebound/tombstoned slots contribute to no channel.
 */
inline bool
cadInstanceDrawable(const CadFramePlan& plan, const CadDrawItem& item,
                    size_t instanceIndex, CadDrawChannel channel) noexcept
{
    if (!cadDrawItemOwnsInstance(plan, item, instanceIndex) ||
            cadInstanceHidden(plan, instanceIndex))
        return false;
    return channel == CadDrawChannel::Points ||
        !cadInstanceSubpixelReplaced(plan, instanceIndex);
}

inline size_t
cadFirstDrawableInstance(const CadFramePlan& plan, const CadDrawItem& item,
                         CadDrawChannel channel) noexcept
{
    for (uint32_t offset = 0; offset < item.instanceCount; ++offset) {
        size_t instanceIndex = 0;
        if (!cadDrawItemInstanceIndex(item, offset, instanceIndex))
            break;
        if (cadInstanceDrawable(plan, item, instanceIndex, channel))
            return instanceIndex;
    }
    return plan.visibleInstances.size();
}

inline size_t
cadDrawableInstanceCount(const CadFramePlan& plan, const CadDrawItem& item,
                         CadDrawChannel channel) noexcept
{
    size_t count = 0;
    for (uint32_t offset = 0; offset < item.instanceCount; ++offset) {
        size_t instanceIndex = 0;
        if (!cadDrawItemInstanceIndex(item, offset, instanceIndex))
            break;
        if (cadInstanceDrawable(plan, item, instanceIndex, channel))
            ++count;
    }
    return count;
}

/** Resolve one producer request against a progressive resident interval. */
inline uint8_t
cadResolvedProgressiveLevel(uint8_t requested, uint8_t minimum,
                            uint8_t resident) noexcept
{
    if (resident >= 16)
        return 15;
    return requested < minimum ? minimum :
        (requested > resident ? resident : requested);
}

} // namespace internal
} // namespace Obol

#endif // OBOL_CAD_CADRESOLVEDDRAW_H
