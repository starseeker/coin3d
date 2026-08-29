/**************************************************************************\
 * Copyright (c) 2026
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
\**************************************************************************/

#ifndef OBOL_CAD_PROJECTED_PROXY_H
#define OBOL_CAD_PROJECTED_PROXY_H

#include <Inventor/SbMatrix.h>
#include <Inventor/SbVec2s.h>
#include <Inventor/SbVec3f.h>

#include <Inventor/basic.h>

#include <cstddef>

namespace Obol {

/**
 * Largest projected extent which remains legible as one raster point.
 * Larger aggregate proxies retain their bounds until mesh admission.
 */
constexpr float CadMaximumPointProxyExtentPixels = 5.0f;

/** Physical batch work needed to preserve an aggregate proxy as an AABB. */
constexpr size_t CadAggregateProxyBoxLineCount = 12u;
constexpr size_t CadAggregateProxyBoxPositionCount =
    CadAggregateProxyBoxLineCount * 2u;
constexpr size_t CadAggregateProxyBoxTriangleCount = 12u;
constexpr size_t CadAggregateProxyBoxTrianglePositionCount =
    CadAggregateProxyBoxTriangleCount * 3u;

class PartGeometry;
struct PartGeometryBuilder;

/** Exact camera-local classification shared by CAD scheduling and drawing. */
struct CadProjectedProxy {
    bool visible = false;
    bool fullyContained = false;
    bool pointEligible = false;
    float pixelWidth = 0.0f;
    float pixelHeight = 0.0f;
    SbVec3f point;
};

/**
 * Project an eight-corner CAD proxy through the renderer's row-vector
 * model/view/projection convention.  A point is eligible only when every
 * corner is inside the clip volume and both screen extents meet the supplied
 * limit.  This is deliberately the one authority used by both view planning
 * and SoCADAssembly presentation.
 */
OBOL_DLL_API CadProjectedProxy classifyCadProjectedProxy(
    const SbVec3f corners[8], const SbMatrix& localToWorld,
    const SbMatrix& viewProjection, const SbVec2s& viewportSize,
    float pixelLimit);

/** Return the exact conservative corners SoCADAssembly point-classifies. */
OBOL_DLL_API bool cadPartGeometryProxyCorners(
    const PartGeometry& geometry, SbVec3f corners[8]);

/** Builder overload used to validate a snapshot before admission. */
OBOL_DLL_API bool cadPartGeometryProxyCorners(
    const PartGeometryBuilder& geometry, SbVec3f corners[8]);

} // namespace Obol

#endif // OBOL_CAD_PROJECTED_PROXY_H
