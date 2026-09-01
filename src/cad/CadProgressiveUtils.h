#ifndef OBOL_CAD_PROGRESSIVE_UTILS_H
#define OBOL_CAD_PROGRESSIVE_UTILS_H

/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the conditions in COPYING are
 * met.
\**************************************************************************/

/**
 * @file CadProgressiveUtils.h
 * @brief Shared CPU rules for resident progressive cuts and quantization.
 *
 * These helpers mirror the progressive vertex-shader encoding.  Picking,
 * software rendering, and GL preparation must use the same cut clamp and
 * cell-center reconstruction so presentation and interaction agree.
 */

#include <Obol/cad/CadGeometry.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Obol {
namespace internal {

/** Resolve one producer cut request against a resident cut interval. */
inline uint8_t
cadResolvedProgressiveCut(uint8_t requested, uint8_t minimum,
                          uint8_t resident) noexcept
{
    /* A newly admitted or compacted population can transiently have less
     * resident data than the producer's preferred minimum.  Residency is a
     * hard safety ceiling; minimum is only a quality floor inside the
     * available interval. */
    const uint8_t availableMinimum = minimum > resident ? resident : minimum;
    return requested < availableMinimum ? availableMinimum :
        (requested > resident ? resident : requested);
}

/** Resolve a cut conservatively when no camera frustum is available. */
template <typename Progressive>
inline uint8_t
cadFullyResidentProgressiveCut(
    const Progressive& progressive, uint8_t requested) noexcept
{
    uint8_t level = cadResolvedProgressiveCut(requested,
        progressive.progressiveMinimumCut,
        progressive.progressiveResidentCut);
    if (!progressive.hasAdaptiveProgressiveClusters())
        return level;

    for (const auto& cluster : progressive.progressiveClusters) {
        if (cluster.ranges.empty())
            continue;
        const uint8_t resident =
            cluster.residentCut == Obol::ProgressiveCutUnspecified ?
            progressive.progressiveResidentCut : cluster.residentCut;
        level = cadResolvedProgressiveCut(level,
            progressive.progressiveMinimumCut, resident);
    }
    return level;
}

inline float
cadProgressiveSnapCoordinate(float value, float minimum, float maximum,
                             uint8_t bits) noexcept
{
    if (!bits || !(maximum > minimum))
        return value;
    const double mask = std::ldexp(
        1.0, 16 - std::min<int>(16, bits));
    const double scaled =
        (static_cast<double>(value) - minimum) /
        (static_cast<double>(maximum) - minimum) * 65535.0;
    const double code =
        std::floor(std::max(0.0, std::min(65535.0, scaled)));
    const double cell = std::floor(code / mask);
    const double snapped = std::min(65535.0, (cell + 0.5) * mask);
    return static_cast<float>((snapped / 65535.0) *
        (static_cast<double>(maximum) - minimum) + minimum);
}

inline SbVec3f
cadProgressiveSnapPoint(
    const SbVec3f& point, const SbVec3f& minimum,
    const SbVec3f& maximum,
    Obol::ProgressiveQuantization quantization) noexcept
{
    if (quantization.isExact())
        return point;
    return SbVec3f(
        cadProgressiveSnapCoordinate(
            point[0], minimum[0], maximum[0], quantization.xBits),
        cadProgressiveSnapCoordinate(
            point[1], minimum[1], maximum[1], quantization.yBits),
        cadProgressiveSnapCoordinate(
            point[2], minimum[2], maximum[2], quantization.zBits));
}

} // namespace internal
} // namespace Obol

#endif // OBOL_CAD_PROGRESSIVE_UTILS_H
