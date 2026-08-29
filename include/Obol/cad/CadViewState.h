#ifndef OBOL_CAD_VIEW_STATE_H
#define OBOL_CAD_VIEW_STATE_H

/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
\**************************************************************************/

/**
 * @file CadViewState.h
 * @brief Generic per-view render policy for compiled CAD assemblies.
 */

#include <Obol/cad/CadIds.h>
#include <Obol/cad/CadProgressive.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace Obol {

/**
 * @brief Software-backend wireframe rasterization policy.
 *
 * AUTO lets the active backend choose its preferred path, QUALITY forces the
 * retained GL renderer, and FAST permits direct one-pixel CPU rasterization
 * into a software framebuffer. FAST intentionally trades GL depth testing and
 * antialiasing for lower interaction latency.
 */
enum class CadSoftwareWireMode : int {
    AUTO    = 0,
    QUALITY = 1,
    FAST    = 2
};

/** View-local CAD drawing channel selection. */
enum class CadDrawMode : uint8_t {
    Shaded = 0,
    Wireframe,
    ShadedWithEdges,
    HiddenLine
};

constexpr bool
cadDrawModeHasWire(CadDrawMode mode) noexcept
{
    return mode == CadDrawMode::Wireframe ||
        mode == CadDrawMode::ShadedWithEdges ||
        mode == CadDrawMode::HiddenLine;
}

constexpr bool
cadDrawModeHasShaded(CadDrawMode mode) noexcept
{
    return mode == CadDrawMode::Shaded ||
        mode == CadDrawMode::ShadedWithEdges ||
        mode == CadDrawMode::HiddenLine;
}

/** View-local CAD picking policy. */
enum class CadPickMode : uint8_t {
    Automatic = 0,
    Edge,
    Triangle,
    Bounds,
    Hybrid
};

static constexpr float CadDefaultEdgePickTolerancePixels = 5.0f;
static constexpr float CadMinimumPointProxyPixels = 1.0f;
static constexpr float CadMaximumPointProxyPixels = 64.0f;

/**
 * @brief Traversal-time state supplied by a view.
 *
 * This struct is deliberately generic: applications map their own model,
 * view, and selection concepts onto these fields before rendering.
 */
struct CadViewState {
    uint64_t viewId = 0;
    CadDrawMode drawMode = CadDrawMode::Wireframe;
    CadPickMode pickMode = CadPickMode::Automatic;
    float edgePickTolerancePixels = CadDefaultEdgePickTolerancePixels;
    bool wireframeOcclusion = false;
    int32_t progressiveCutCeiling = -1;
    float progressiveCutNextFraction = 0.0f;
    float pointProxyPixelThreshold = CadMinimumPointProxyPixels;
    bool cameraMotionFrameReuse = false;
    CadSoftwareWireMode softwareWireMode = CadSoftwareWireMode::AUTO;

    bool operator==(const CadViewState& other) const noexcept
    {
        return viewId == other.viewId &&
            drawMode == other.drawMode && pickMode == other.pickMode &&
            edgePickTolerancePixels == other.edgePickTolerancePixels &&
            wireframeOcclusion == other.wireframeOcclusion &&
            progressiveCutCeiling == other.progressiveCutCeiling &&
            progressiveCutNextFraction ==
                other.progressiveCutNextFraction &&
            pointProxyPixelThreshold == other.pointProxyPixelThreshold &&
            cameraMotionFrameReuse == other.cameraMotionFrameReuse &&
            softwareWireMode == other.softwareWireMode;
    }

    bool operator!=(const CadViewState& other) const noexcept
    {
        return !(*this == other);
    }
};

inline uint8_t
cadEffectiveProgressiveCut(const CadViewState& view, uint8_t requested)
{
    const int ceiling = view.progressiveCutCeiling;
    if (ceiling < 0 || ceiling >= ProgressiveCutUnspecified)
        return requested;
    return std::min(requested, static_cast<uint8_t>(ceiling));
}

inline uint64_t
cadProgressiveFractionHash(PartId part) noexcept
{
    static constexpr uint64_t HashCombineConstant =
        0x9e3779b97f4a7c15ULL;
    static constexpr uint64_t SplitMixFirstMultiplier =
        0xbf58476d1ce4e5b9ULL;
    static constexpr uint64_t SplitMixSecondMultiplier =
        0x94d049bb133111ebULL;
    uint64_t value = part.w0 ^
        (part.w1 + HashCombineConstant +
         (part.w0 << 6) + (part.w0 >> 2));
    value ^= value >> 30;
    value *= SplitMixFirstMultiplier;
    value ^= value >> 27;
    value *= SplitMixSecondMultiplier;
    value ^= value >> 31;
    return value;
}

inline uint8_t
cadEffectiveProgressiveCut(const CadViewState& view, PartId part,
                           uint8_t requested)
{
    const uint8_t base = cadEffectiveProgressiveCut(view, requested);
    const float fraction = view.progressiveCutNextFraction;
    if (base >= requested || view.progressiveCutCeiling < 0 ||
            !std::isfinite(fraction) || fraction <= 0.0f)
        return base;
    if (fraction >= 1.0f)
        return static_cast<uint8_t>(base + 1u);

    const long double normalized = static_cast<long double>(
        cadProgressiveFractionHash(part)) /
        static_cast<long double>((std::numeric_limits<uint64_t>::max)());
    return normalized < static_cast<long double>(fraction) ?
        static_cast<uint8_t>(base + 1u) : base;
}

inline uint8_t
cadMaximumEffectiveProgressiveCut(const CadViewState& view,
                                  uint8_t requested)
{
    const uint8_t base = cadEffectiveProgressiveCut(view, requested);
    const float fraction = view.progressiveCutNextFraction;
    return base < requested && std::isfinite(fraction) && fraction > 0.0f ?
        static_cast<uint8_t>(base + 1u) : base;
}

} // namespace Obol

#endif // OBOL_CAD_VIEW_STATE_H
