/**************************************************************************\
 * Copyright (c) 2026
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
\**************************************************************************/

#ifndef OBOL_CAD_CADPRESENTATIONPREPARATION_H
#define OBOL_CAD_CADPRESENTATIONPREPARATION_H

#include <array>
#include <cstdint>

namespace Obol {

/** Retained work which must finish before one exact CAD frame can draw. */
enum class CadPresentationPreparationKind : uint8_t {
    NoPreparation = 0,
    SubpixelClassification,
    FlatShadedPlanning,
    FlatShadedAtlas,
    RetainedIndirect
};

/** Terminality of one exact-target retained preparation obligation. */
enum class CadPresentationPreparationState : uint8_t {
    NoPreparation = 0,
    Preparing,
    Complete,
    Constrained,
    Failed
};

/**
 * Exact identity of a retained presentation-preparation target.
 *
 * Floating-point controls are stored as their object-representation bits.
 * Preparation is safe to resume only when every field is identical; a hash
 * is deliberately insufficient because this value has control authority.
 */
struct CadPresentationPreparationTarget {
    CadPresentationPreparationKind kind =
        CadPresentationPreparationKind::NoPreparation;
    uint64_t obligationRevision = 0;
    uint64_t viewId = 0;
    uint32_t contextId = 0;
    uint64_t planRevision = 0;
    uint64_t geometryRevision = 0;
    int32_t progressiveCutCeiling = -1;
    uint32_t progressiveCutNextFractionBits = 0;
    uint64_t classifierInputRevision = 0;
    uint64_t classifierAppendRevision = 0;
    int32_t viewportWidth = 0;
    int32_t viewportHeight = 0;
    uint32_t pointProxyPixelThresholdBits = 0;
    std::array<uint32_t, 16> viewProjectionBits = {};

    bool operator==(const CadPresentationPreparationTarget& other) const
    {
        return kind == other.kind &&
            obligationRevision == other.obligationRevision &&
            viewId == other.viewId &&
            contextId == other.contextId &&
            planRevision == other.planRevision &&
            geometryRevision == other.geometryRevision &&
            progressiveCutCeiling == other.progressiveCutCeiling &&
            progressiveCutNextFractionBits ==
                other.progressiveCutNextFractionBits &&
            classifierInputRevision == other.classifierInputRevision &&
            classifierAppendRevision == other.classifierAppendRevision &&
            viewportWidth == other.viewportWidth &&
            viewportHeight == other.viewportHeight &&
            pointProxyPixelThresholdBits ==
                other.pointProxyPixelThresholdBits &&
            viewProjectionBits == other.viewProjectionBits;
    }

    bool operator!=(const CadPresentationPreparationTarget& other) const
    {
        return !(*this == other);
    }
};

/**
 * Finite progress certificate for one preparation target.
 *
 * totalUnits is immutable for a target.  completedUnits is monotone and may
 * never exceed totalUnits.  reservedBytes is the admitted upper bound for
 * transient renderer-owned storage, not process or driver-private memory.
 */
struct CadPresentationPreparationSnapshot {
    CadPresentationPreparationTarget target;
    CadPresentationPreparationState state =
        CadPresentationPreparationState::NoPreparation;
    uint64_t totalUnits = 0;
    uint64_t completedUnits = 0;
    uint64_t reservedBytes = 0;

    bool hasTarget() const
    {
        return target.kind !=
                CadPresentationPreparationKind::NoPreparation &&
            state != CadPresentationPreparationState::NoPreparation;
    }

    uint64_t remainingUnits() const
    {
        return completedUnits < totalUnits ?
            totalUnits - completedUnits : 0;
    }
};

} // namespace Obol

#endif // OBOL_CAD_CADPRESENTATIONPREPARATION_H
