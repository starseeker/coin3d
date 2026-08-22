/**************************************************************************\\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the conditions in COPYING are
 * met.
\\**************************************************************************/

/**
 * @file CadRendererGLExecutors.cpp
 * @brief Specialized flat, VBO, immediate, instanced, and indirect executors.
 *
 * CadRendererGL.cpp owns capability selection, state-boundary guards, resource
 * lifetime, and routing.  This unit owns submission mechanics only.  The split
 * is intentionally representation preserving: retained packets, sparse patch
 * paths, and exact indirect replay are unchanged.
 */

#include "CadRendererGL.h"
#include "CadRendererConfiguration.h"
#include "CadResolvedDraw.h"
#include "CadShaderSources.h"

#include <Obol/cad/SoCADAssembly.h>

#include <Inventor/system/gl.h>
#include "glue/glp.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#ifndef GL_DRAW_INDIRECT_BUFFER
#define GL_DRAW_INDIRECT_BUFFER 0x8F3F
#endif

namespace Obol {
namespace internal {

static_assert(sizeof(SbVec3f) == 3 * sizeof(float),
              "SbVec3f must remain tightly packed for CAD GPU uploads");

static const float *
executorPackedVec3fData(const std::vector<SbVec3f>& values)
{
    return values.empty() ? nullptr : values[0].getValue();
}

static void
executorAppendPackedPoint(std::vector<float>& packed, const SbVec3f& point)
{
    packed.push_back(point[0]);
    packed.push_back(point[1]);
    packed.push_back(point[2]);
}

static void
setImmediateMaterialFromRgba(const SoGLContext *glue, const uint8_t rgba[4])
{
    const float r = rgba[0] / 255.0f;
    const float g = rgba[1] / 255.0f;
    const float b = rgba[2] / 255.0f;
    const float a = rgba[3] / 255.0f;

    const GLfloat ambient[4] = {r * 0.2f, g * 0.2f, b * 0.2f, a};
    const GLfloat diffuse[4] = {r * 0.6f, g * 0.6f, b * 0.6f, a};
    const GLfloat specular[4] = {r * 0.2f, g * 0.2f, b * 0.2f, a};
    const GLfloat emission[4] = {0.0f, 0.0f, 0.0f, a};

    glue->glColor4ub(rgba[0], rgba[1], rgba[2], rgba[3]);
    glue->glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, emission);
    glue->glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, ambient);
    glue->glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular);
    glue->glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, diffuse);
    glue->glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 0.0f);
}

static void
setCadBackfaceCulling(const SoGLContext *glue, bool enabled)
{
    if (enabled)
        SoGLContext_glEnable(glue, GL_CULL_FACE);
    else
        SoGLContext_glDisable(glue, GL_CULL_FACE);
}

/* A closed, consistently wound source mesh is safe to cull only while its
 * displayed coordinates preserve that source topology.  PoP quantization can
 * collapse a skinny triangle or move it through a locally adjacent face;
 * until the exact cut is selected, the snapped approximation is therefore a
 * two-sided surface even when its source is a verified solid. */
static bool
cadProgressiveCutCullSafe(bool sourceCullSafe, const TriMesh *mesh,
                          uint8_t cut)
{
    return sourceCullSafe &&
        (!mesh || mesh->quantizationAtCut(cut).isExact());
}

static const float kLightDir[3] = { 0.577f, 0.577f, 0.577f };

struct ExecutorFrustumPlanes {
    float planes[6][4];
};

static ExecutorFrustumPlanes
extractExecutorFrustumPlanes(const SbMatrix& vp) noexcept
{
    ExecutorFrustumPlanes fp;
    for (int column = 0; column < 3; ++column) {
        for (int signIndex = 0; signIndex < 2; ++signIndex) {
            const int planeIndex = column * 2 + signIndex;
            const float sign = signIndex == 0 ? 1.0f : -1.0f;
            fp.planes[planeIndex][0] =
                sign * vp[0][column] + vp[0][3];
            fp.planes[planeIndex][1] =
                sign * vp[1][column] + vp[1][3];
            fp.planes[planeIndex][2] =
                sign * vp[2][column] + vp[2][3];
            fp.planes[planeIndex][3] =
                sign * vp[3][column] + vp[3][3];
        }
    }
    return fp;
}

static bool
isBoxOutsideExecutorFrustum(const float minimum[3], const float maximum[3],
                            const ExecutorFrustumPlanes& frustum) noexcept
{
    for (int plane = 0; plane < 6; ++plane) {
        const float x = frustum.planes[plane][0] < 0.0f ?
            minimum[0] : maximum[0];
        const float y = frustum.planes[plane][1] < 0.0f ?
            minimum[1] : maximum[1];
        const float z = frustum.planes[plane][2] < 0.0f ?
            minimum[2] : maximum[2];
        if (frustum.planes[plane][0] * x +
                frustum.planes[plane][1] * y +
                frustum.planes[plane][2] * z +
                frustum.planes[plane][3] < 0.0f)
            return true;
    }
    return false;
}

static bool
isBoxInsideExecutorFrustum(const float minimum[3], const float maximum[3],
                           const ExecutorFrustumPlanes& frustum) noexcept
{
    for (int plane = 0; plane < 6; ++plane) {
        const float x = frustum.planes[plane][0] < 0.0f ?
            maximum[0] : minimum[0];
        const float y = frustum.planes[plane][1] < 0.0f ?
            maximum[1] : minimum[1];
        const float z = frustum.planes[plane][2] < 0.0f ?
            maximum[2] : minimum[2];
        if (frustum.planes[plane][0] * x +
                frustum.planes[plane][1] * y +
                frustum.planes[plane][2] * z +
                frustum.planes[plane][3] < 0.0f)
            return false;
    }
    return true;
}

static void
executorTransformedBox(const SbBox3f& local, const SbMatrix& transform,
                       float minimum[3], float maximum[3]) noexcept
{
    const SbVec3f localMin = local.getMin();
    const SbVec3f localMax = local.getMax();
    minimum[0] = minimum[1] = minimum[2] =
        std::numeric_limits<float>::infinity();
    maximum[0] = maximum[1] = maximum[2] =
        -std::numeric_limits<float>::infinity();
    for (unsigned int corner = 0; corner < 8; ++corner) {
        const SbVec3f point(
            corner & 1u ? localMax[0] : localMin[0],
            corner & 2u ? localMax[1] : localMin[1],
            corner & 4u ? localMax[2] : localMin[2]);
        SbVec3f world;
        transform.multVecMatrix(point, world);
        for (int axis = 0; axis < 3; ++axis) {
            minimum[axis] = std::min(minimum[axis], world[axis]);
            maximum[axis] = std::max(maximum[axis], world[axis]);
        }
    }
}

static uint8_t
executorVisibleProgressiveCut(const TriMesh& mesh,
                              const CadVisibleInstance& instance,
                              const ExecutorFrustumPlanes& frustum,
                              uint8_t requested) noexcept
{
    uint8_t level = cadResolvedProgressiveCut(
        requested, mesh.progressiveMinimumCut,
        mesh.progressiveResidentCut);
    if (!mesh.hasAdaptiveProgressiveClusters()) return level;

    SbMatrix model;
    model.setValue(instance.transform.data());
    for (const ProgressiveTriangleCluster& cluster :
            mesh.progressiveClusters) {
        if (cluster.ranges.empty()) continue;
        float minimum[3];
        float maximum[3];
        executorTransformedBox(cluster.bounds, model, minimum, maximum);
        if (isBoxOutsideExecutorFrustum(minimum, maximum, frustum)) continue;
        const uint8_t resident =
            cluster.residentCut == ProgressiveCutUnspecified ?
                mesh.progressiveResidentCut : cluster.residentCut;
        level = std::min(level,
            std::max(mesh.progressiveMinimumCut, resident));
    }
    return level;
}

static uint8_t
executorVisibleProgressiveCut(const WireRep& wire,
                              const CadVisibleInstance& instance,
                              const ExecutorFrustumPlanes& frustum,
                              uint8_t requested) noexcept
{
    uint8_t level = cadResolvedProgressiveCut(
        requested, wire.progressiveMinimumCut,
        wire.progressiveResidentCut);
    if (!wire.hasAdaptiveProgressiveClusters()) return level;

    SbMatrix model;
    model.setValue(instance.transform.data());
    for (const ProgressiveWireCluster& cluster : wire.progressiveClusters) {
        if (cluster.ranges.empty()) continue;
        float minimum[3];
        float maximum[3];
        executorTransformedBox(cluster.bounds, model, minimum, maximum);
        if (isBoxOutsideExecutorFrustum(minimum, maximum, frustum)) continue;
        const uint8_t resident =
            cluster.residentCut == ProgressiveCutUnspecified ?
                wire.progressiveResidentCut : cluster.residentCut;
        level = std::min(level,
            std::max(wire.progressiveMinimumCut, resident));
    }
    return level;
}

/* Relative projected area is sufficient for admission ordering: viewport
 * dimensions multiply every candidate by the same constant.  The explicit
 * homogeneous transform preserves the intended orthographic contract (depth
 * has no effect) while perspective naturally favors near visible geometry. */
static double
executorProjectedBoxImportance(const float minimum[3], const float maximum[3],
                               const SbMatrix& viewProjection) noexcept
{
    double minX = std::numeric_limits<double>::infinity();
    double minY = std::numeric_limits<double>::infinity();
    double maxX = -std::numeric_limits<double>::infinity();
    double maxY = -std::numeric_limits<double>::infinity();
    bool behindNearPlane = false;
    size_t projected = 0;
    for (unsigned int corner = 0; corner < 8u; ++corner) {
        const double x = (corner & 1u) ? maximum[0] : minimum[0];
        const double y = (corner & 2u) ? maximum[1] : minimum[1];
        const double z = (corner & 4u) ? maximum[2] : minimum[2];
        const double clipX = x * viewProjection[0][0] +
            y * viewProjection[1][0] + z * viewProjection[2][0] +
            viewProjection[3][0];
        const double clipY = x * viewProjection[0][1] +
            y * viewProjection[1][1] + z * viewProjection[2][1] +
            viewProjection[3][1];
        const double clipW = x * viewProjection[0][3] +
            y * viewProjection[1][3] + z * viewProjection[2][3] +
            viewProjection[3][3];
        if (!(clipW > 1.0e-12) || !std::isfinite(clipW)) {
            behindNearPlane = true;
            continue;
        }
        const double ndcX = std::max(-2.0,
            std::min(2.0, clipX / clipW));
        const double ndcY = std::max(-2.0,
            std::min(2.0, clipY / clipW));
        if (!std::isfinite(ndcX) || !std::isfinite(ndcY))
            continue;
        minX = std::min(minX, ndcX);
        minY = std::min(minY, ndcY);
        maxX = std::max(maxX, ndcX);
        maxY = std::max(maxY, ndcY);
        ++projected;
    }
    /* A box crossing the eye/near plane is necessarily prominent.  Give it
     * the maximum bounded viewport score rather than letting an unstable
     * divide dominate ordering. */
    if (behindNearPlane || !projected)
        return 16.0;
    const double width = std::max(0.0, maxX - minX);
    const double height = std::max(0.0, maxY - minY);
    return std::max(1.0e-12, std::min(16.0, width * height));
}

struct CadWireRasterState {
    GLfloat lineWidth = 1.0f;
    GLboolean stippleEnabled = GL_FALSE;
    GLint stipplePattern = 0xffff;
    GLint stippleFactor = 1;
};

static CadWireRasterState
captureWireRasterState(const SoGLContext *glue, bool hasLineStipple)
{
    CadWireRasterState state;
    glue->glGetFloatv(GL_LINE_WIDTH, &state.lineWidth);
    if (hasLineStipple) {
        state.stippleEnabled = glue->glIsEnabled(GL_LINE_STIPPLE);
        glue->glGetIntegerv(
            GL_LINE_STIPPLE_PATTERN, &state.stipplePattern);
        glue->glGetIntegerv(
            GL_LINE_STIPPLE_REPEAT, &state.stippleFactor);
    }
    return state;
}

static void
applyWireRasterStyle(const SoGLContext *glue,
                     const CadVisibleInstance& instance,
                     bool hasLineStipple)
{
    glue->glLineWidth(std::max(1.0f, instance.lineWidth));
    if (!hasLineStipple)
        return;
    if (instance.linePattern != 0xffffu) {
        glue->glLineStipple(
            std::max<GLint>(1, instance.linePatternFactor),
            instance.linePattern);
        glue->glEnable(GL_LINE_STIPPLE);
    } else {
        glue->glDisable(GL_LINE_STIPPLE);
    }
}

static void
restoreWireRasterState(const SoGLContext *glue,
                       const CadWireRasterState& state,
                       bool hasLineStipple)
{
    glue->glLineWidth(state.lineWidth);
    if (!hasLineStipple)
        return;
    glue->glLineStipple(
        state.stippleFactor,
        static_cast<GLushort>(state.stipplePattern));
    if (state.stippleEnabled)
        glue->glEnable(GL_LINE_STIPPLE);
    else
        glue->glDisable(GL_LINE_STIPPLE);
}

namespace {

struct FlatWireStyleKey {
    uint8_t priority = 0;
    uint32_t rgba = 0;
    uint32_t widthBits = 0;
    uint16_t pattern = 0xffffu;
    uint16_t factor = 1u;

    bool operator<(const FlatWireStyleKey& other) const noexcept {
        if (priority != other.priority) return priority < other.priority;
        if (rgba != other.rgba) return rgba < other.rgba;
        if (widthBits != other.widthBits) return widthBits < other.widthBits;
        if (pattern != other.pattern) return pattern < other.pattern;
        return factor < other.factor;
    }

    bool operator==(const FlatWireStyleKey& other) const noexcept {
        return priority == other.priority &&
               rgba == other.rgba &&
               widthBits == other.widthBits &&
               pattern == other.pattern &&
               factor == other.factor;
    }
};

struct FlatWireStyleKeyHash {
    size_t operator()(const FlatWireStyleKey& key) const noexcept {
        size_t value = static_cast<size_t>(key.rgba);
        const auto mix = [&value](size_t component) {
            value ^= component + static_cast<size_t>(0x9e3779b9u) +
                     (value << 6) + (value >> 2);
        };
        mix(static_cast<size_t>(key.widthBits));
        mix(static_cast<size_t>(key.pattern) |
            (static_cast<size_t>(key.factor) << 16));
        mix(static_cast<size_t>(key.priority));
        return value;
    }
};

static FlatWireStyleKey flatWireStyleKey(const CadVisibleInstance& inst)
{
    FlatWireStyleKey key;
    // Draw hover/selection emphasis after ordinary geometry at equal depth.
    key.priority = static_cast<uint8_t>(inst.flags & 3u);
    key.rgba = static_cast<uint32_t>(inst.rgba[0]) |
               (static_cast<uint32_t>(inst.rgba[1]) << 8) |
               (static_cast<uint32_t>(inst.rgba[2]) << 16) |
               (static_cast<uint32_t>(inst.rgba[3]) << 24);
    std::memcpy(&key.widthBits, &inst.lineWidth, sizeof(key.widthBits));
    key.pattern = inst.linePattern;
    key.factor = inst.linePatternFactor;
    return key;
}

static void writeTransformedFlatPoint(
        std::vector<float>& positions,
        size_t& offset,
        const SbVec3f& point,
        const std::array<float, 16>& matrix)
{
    const float x = point[0];
    const float y = point[1];
    const float z = point[2];
    positions[offset++] = x * matrix[0] + y * matrix[4] +
                          z * matrix[8] + matrix[12];
    positions[offset++] = x * matrix[1] + y * matrix[5] +
                          z * matrix[9] + matrix[13];
    positions[offset++] = x * matrix[2] + y * matrix[6] +
                          z * matrix[10] + matrix[14];
}

static SbVec3f transformedFlatPoint(
        const SbVec3f& point,
        const std::array<float, 16>& matrix)
{
    const float x = point[0];
    const float y = point[1];
    const float z = point[2];
    return SbVec3f(x * matrix[0] + y * matrix[4] + z * matrix[8] + matrix[12],
                   x * matrix[1] + y * matrix[5] + z * matrix[9] + matrix[13],
                   x * matrix[2] + y * matrix[6] + z * matrix[10] + matrix[14]);
}

static uint32_t flatRgbaKey(const CadVisibleInstance& inst)
{
    return static_cast<uint32_t>(inst.rgba[0]) |
           (static_cast<uint32_t>(inst.rgba[1]) << 8) |
           (static_cast<uint32_t>(inst.rgba[2]) << 16) |
           (static_cast<uint32_t>(inst.rgba[3]) << 24);
}

} // namespace

static uint64_t
cadSaturatingWorkAdd(uint64_t left, uint64_t right)
{
    return right > UINT64_MAX - left ? UINT64_MAX : left + right;
}

static float
packedProgressiveQuantization(ProgressiveQuantization quantization)
{
    return static_cast<float>(quantization.xBits) +
        17.0f * static_cast<float>(quantization.yBits) +
        289.0f * static_cast<float>(quantization.zBits);
}

static SbVec3f progressiveSnapPoint(
    const SbVec3f& point, const SbVec3f& minimum,
    const SbVec3f& maximum, ProgressiveQuantization quantization);

bool CadRendererGL::renderFlatWire(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext* glue,
        const SbMatrix& viewProj)
{
    constexpr size_t maxPositionBytes = 256u * 1024u * 1024u;
    constexpr size_t progressiveGrowthReserve = 16u;
    size_t deadlineWork = 256u;
    if (renderInterruptedAfter(deadlineWork))
        return false;
    /* Indexed triangle-edge aliases retain their source topology and GPU
     * buffers.  Flattening them here would recreate the endpoint expansion
     * this representation is specifically intended to avoid; the dedicated
     * indexed wire executor renders them separately. */
    for (const CadDrawItem& item : plan.wireItems) {
        if (item.rep.type == CadRepType::Triangles)
            return false;
    }
    const size_t maxVertexCount =
        maxPositionBytes / (3 * sizeof(float));
    const uint64_t presentationRevision = plan.subpixelProxyRevision ?
        plan.subpixelProxyRevision : plan.revision;
    const ExecutorFrustumPlanes fp =
        extractExecutorFrustumPlanes(viewProj);
    struct Occurrence {
        const Obol::WireRep *wire = nullptr;
        const CadVisibleInstance *instance = nullptr;
        CadFlatWireRangeKey rangeKey;
        size_t flatSegments = 0;
        size_t flatSegmentFirst = 0;
        size_t polylineSegments = 0;
        uint8_t cut = Obol::ProgressiveCutUnspecified;
        FlatWireStyleKey style;
        uint32_t styleBucket = 0;
        size_t visibleIndex = 0;
        CadFlatWireRange range;
        bool rangeValid = false;
        bool rangeCachePending = false;
    };

    /*
     * Retain world-space ranges across structural publication batches.  A
     * 50k-leaf warm start otherwise re-expanded and re-uploaded every box
     * after each 64..512 occurrence merge, making total work quadratic.
     */
    std::vector<Occurrence> occurrences;
    occurrences.reserve(plan.visibleInstances.size());
    size_t visibleVertexCount = 0;
    size_t futureRangeVertexCount = 0;
    bool hasProgressiveOccurrence = false;
    for (const CadDrawItem& item : plan.wireItems) {
        if (renderInterruptedAfter(deadlineWork))
            return false;
        if (item.partIndex >= plan.partBindings.size())
            continue;
        const CadPartBinding& binding =
            plan.partBindings[item.partIndex];
        const Obol::PartGeometry *geom = binding.geometry.get();
        if (!geom || !geom->wire.has_value()) continue;
        const Obol::WireRep& wire = *geom->wire;
        size_t polylineSegments = 0;
        for (const Obol::WirePolyline& poly : wire.polylines)
            if (poly.points.size() >= 2)
                polylineSegments += poly.points.size() - 1;
        for (uint32_t ii = 0; ii < item.instanceCount; ++ii) {
            if (renderInterruptedAfter(deadlineWork))
                return false;
            const size_t visibleIndex = item.baseInstance + ii;
            if (!cadInstanceDrawable(
                    plan, item, visibleIndex, CadDrawChannel::Wire))
                continue;
            const CadVisibleInstance& instance =
                plan.visibleInstances[visibleIndex];
            if (isBoxOutsideExecutorFrustum(
                    instance.wbMin, instance.wbMax, fp))
                continue;
            const uint8_t effectiveLevel =
                assembly.effectiveProgressiveCut(instance.lodCut);
            const uint8_t level = cadResolvedProgressiveCut(
                effectiveLevel, wire.progressiveMinimumCut,
                wire.progressiveResidentCut);
            const size_t flatSegments =
                wire.segmentCountAtCut(effectiveLevel);
            const size_t segments = flatSegments + polylineSegments;
            if (segments == 0)
                continue;
            const size_t vertices = segments * 2;
            if (vertices > maxVertexCount ||
                    visibleVertexCount > maxVertexCount - vertices)
                return false;
            visibleVertexCount += vertices;

            uint64_t geometryToken = 1469598103934665603ULL;
            geometryToken ^= binding.generation;
            geometryToken *= 1099511628211ULL;
            geometryToken ^= item.rep.part.w0;
            geometryToken *= 1099511628211ULL;
            geometryToken ^= item.rep.part.w1;
            geometryToken *= 1099511628211ULL;
            for (float value : instance.transform) {
                uint32_t bits = 0;
                std::memcpy(&bits, &value, sizeof(bits));
                geometryToken ^= bits;
                geometryToken *= 1099511628211ULL;
            }

            Occurrence occurrence;
            occurrence.wire = &wire;
            occurrence.instance = &instance;
            occurrence.rangeKey = CadFlatWireRangeKey{
                instance.instanceId, level, geometryToken};
            occurrence.flatSegments = flatSegments;
            occurrence.flatSegmentFirst =
                wire.segmentFirstAtCut(effectiveLevel);
            occurrence.polylineSegments = polylineSegments;
            occurrence.cut = level;
            occurrence.style = flatWireStyleKey(instance);
            occurrence.visibleIndex = visibleIndex;
            occurrence.rangeValid = gpuRes_->lookupFlatWireRange(
                visibleIndex, occurrence.rangeKey, &occurrence.range);
            occurrences.push_back(occurrence);

            if (wire.isProgressive()) {
                hasProgressiveOccurrence = true;
                if (level < wire.progressiveResidentCut) {
                    const size_t residentSegments =
                        wire.segmentCountAtCut(
                            wire.progressiveResidentCut) +
                        polylineSegments;
                    const size_t residentVertices =
                        residentSegments <= maxVertexCount / 2u ?
                            residentSegments * 2u : maxVertexCount;
                    futureRangeVertexCount = std::min(
                        maxVertexCount,
                        futureRangeVertexCount <=
                                maxVertexCount - residentVertices ?
                            futureRangeVertexCount + residentVertices :
                            maxVertexCount);
                }
            }
        }
    }

    /*
     * Structural fallback leaves often share one wire style.  Keep their
     * retained-plan order in that case.  For mixed colors or selection
     * emphasis, group in linear time and sort only the distinct style keys.
     * Sorting the full, relatively large Occurrence record made every
     * incremental publication frame O(N log N) in the number of remaining
     * boxes.
     */
    const bool uniformStyle = occurrences.empty() ||
        std::all_of(
            occurrences.begin() + 1, occurrences.end(),
            [&occurrences](const Occurrence& occurrence) {
                const FlatWireStyleKey& first = occurrences.front().style;
                return occurrence.style == first;
            });
    std::vector<uint32_t> occurrenceOrder;
    if (!uniformStyle) {
        std::unordered_map<FlatWireStyleKey, uint32_t,
                           FlatWireStyleKeyHash> bucketByStyle;
        std::vector<FlatWireStyleKey> bucketStyles;
        std::vector<size_t> bucketCounts;
        bucketByStyle.reserve(std::min<size_t>(
            occurrences.size(), 4096u));
        bucketStyles.reserve(std::min<size_t>(
            occurrences.size(), 4096u));
        bucketCounts.reserve(std::min<size_t>(
            occurrences.size(), 4096u));
        for (Occurrence& occurrence : occurrences) {
            if (renderInterruptedAfter(deadlineWork))
                return false;
            const uint32_t candidate =
                static_cast<uint32_t>(bucketStyles.size());
            const auto inserted =
                bucketByStyle.emplace(occurrence.style, candidate);
            const uint32_t bucket = inserted.first->second;
            if (inserted.second) {
                bucketStyles.push_back(occurrence.style);
                bucketCounts.push_back(0);
            }
            occurrence.styleBucket = bucket;
            ++bucketCounts[bucket];
        }

        std::vector<uint32_t> bucketOrder(bucketStyles.size());
        for (size_t i = 0; i < bucketOrder.size(); ++i)
            bucketOrder[i] = static_cast<uint32_t>(i);
        std::sort(
            bucketOrder.begin(), bucketOrder.end(),
            [&bucketStyles](uint32_t left, uint32_t right) {
                return bucketStyles[left] < bucketStyles[right];
            });

        std::vector<size_t> bucketWrite(bucketStyles.size());
        size_t offset = 0;
        for (const uint32_t bucket : bucketOrder) {
            bucketWrite[bucket] = offset;
            offset += bucketCounts[bucket];
        }
        occurrenceOrder.resize(occurrences.size());
        for (size_t i = 0; i < occurrences.size(); ++i) {
            if (renderInterruptedAfter(deadlineWork))
                return false;
            const uint32_t bucket = occurrences[i].styleBucket;
            occurrenceOrder[bucketWrite[bucket]++] =
                static_cast<uint32_t>(i);
        }
    }
    const auto& orderedOccurrence =
        [&occurrences, &occurrenceOrder](size_t index) -> Occurrence& {
            return occurrences[
                occurrenceOrder.empty() ?
                    index : occurrenceOrder[index]];
        };

    auto buildAtlasRanges = [&](
            bool onlyMissing, GLint baseVertex,
            std::vector<float>& positions,
            std::unordered_map<CadFlatWireRangeKey, CadFlatWireRange,
                               CadFlatWireRangeKeyHash>& ranges) {
        size_t appendVertexCount = 0;
        for (size_t i = 0; i < occurrences.size(); ++i) {
            if (renderInterruptedAfter(deadlineWork))
                return false;
            const Occurrence& occurrence = orderedOccurrence(i);
            if (onlyMissing && occurrence.rangeValid)
                continue;
            const size_t vertices =
                (occurrence.flatSegments +
                 occurrence.polylineSegments) * 2;
            if (vertices > maxVertexCount ||
                    appendVertexCount > maxVertexCount - vertices)
                return false;
            appendVertexCount += vertices;
        }
        positions.clear();
        positions.resize(appendVertexCount * 3);
        ranges.clear();
        size_t positionOffset = 0;
        for (size_t i = 0; i < occurrences.size(); ++i) {
            if (renderInterruptedAfter(deadlineWork))
                return false;
            Occurrence& occurrence = orderedOccurrence(i);
            if (onlyMissing && occurrence.rangeValid)
                continue;
            const Obol::WireRep& wire = *occurrence.wire;
            const CadVisibleInstance& inst = *occurrence.instance;
            const GLint first = baseVertex +
                static_cast<GLint>(positionOffset / 3);
            const size_t flatPointCount = occurrence.flatSegments * 2;
            const size_t flatPointFirst =
                occurrence.flatSegmentFirst * 2;
            const size_t flatPointEnd = flatPointFirst + flatPointCount;
            for (size_t p = flatPointFirst;
                    p + 1 < flatPointEnd; p += 2) {
                if (renderInterruptedAfter(deadlineWork))
                    return false;
                const SbVec3f a = wire.isProgressive() ?
                    progressiveSnapPoint(wire.segmentPoints[p],
                        wire.progressiveQuantizationMinimum,
                        wire.progressiveQuantizationMaximum,
                        wire.quantizationAtCut(occurrence.cut)) :
                    wire.segmentPoints[p];
                const SbVec3f b = wire.isProgressive() ?
                    progressiveSnapPoint(wire.segmentPoints[p + 1],
                        wire.progressiveQuantizationMinimum,
                        wire.progressiveQuantizationMaximum,
                        wire.quantizationAtCut(occurrence.cut)) :
                    wire.segmentPoints[p + 1];
                writeTransformedFlatPoint(
                    positions, positionOffset, a, inst.transform);
                writeTransformedFlatPoint(
                    positions, positionOffset, b, inst.transform);
            }
            for (const Obol::WirePolyline& poly : wire.polylines) {
                for (size_t p = 0; p + 1 < poly.points.size(); ++p) {
                    if (renderInterruptedAfter(deadlineWork))
                        return false;
                    writeTransformedFlatPoint(
                        positions, positionOffset,
                        poly.points[p], inst.transform);
                    writeTransformedFlatPoint(
                        positions, positionOffset,
                        poly.points[p + 1], inst.transform);
                }
            }
            occurrence.range = CadFlatWireRange{
                first,
                static_cast<GLsizei>(
                    (occurrence.flatSegments +
                     occurrence.polylineSegments) * 2)};
            occurrence.rangeValid = true;
            occurrence.rangeCachePending = true;
            ranges.emplace(occurrence.rangeKey, occurrence.range);
        }
        return true;
    };

    bool rebuild = !gpuRes_->flatWire().posBuf;
    std::vector<float> positions;
    std::unordered_map<CadFlatWireRangeKey, CadFlatWireRange,
                       CadFlatWireRangeKeyHash> newRanges;
    if (!rebuild) {
        size_t missingVertexCount = 0;
        const CadFlatWireGpu& flat = gpuRes_->flatWire();
        for (const Occurrence& occurrence : occurrences) {
            if (renderInterruptedAfter(deadlineWork))
                return false;
            if (occurrence.rangeValid)
                continue;
            const size_t vertices =
                (occurrence.flatSegments +
                 occurrence.polylineSegments) * 2;
            if (vertices > maxVertexCount ||
                    missingVertexCount > maxVertexCount - vertices)
                return false;
            missingVertexCount += vertices;
        }
        if (missingVertexCount) {
            if (flat.vertexCount >
                    flat.capacityVertexCount -
                        static_cast<GLsizei>(missingVertexCount)) {
                rebuild = true;
            } else if (!buildAtlasRanges(
                    true, flat.vertexCount, positions, newRanges) ||
                    !gpuRes_->appendFlatWire(
                        positions, newRanges, glue)) {
                return false;
            }
        } else if (hasProgressiveOccurrence &&
                futureRangeVertexCount == 0 &&
                flat.capacityVertexCount > 0 &&
                visibleVertexCount <=
                    static_cast<size_t>(flat.capacityVertexCount) / 4u) {
            /* Once every visible occurrence has reached its resident cut,
             * discard an oversized publication-wave reserve.  The compact
             * buffer keeps one additional active-cut-sized slot below, so a
             * later interactive coarse cut can be appended and the terminal
             * range remains immediately reusable. */
            rebuild = true;
        }
    }
    if (rebuild) {
        if (occurrences.empty())
            return true;
        if (!buildAtlasRanges(false, 0, positions, newRanges))
            return false;
        const size_t vertexCount = positions.size() / 3;
        const size_t growthLimit = vertexCount <=
                maxVertexCount / progressiveGrowthReserve ?
            vertexCount * progressiveGrowthReserve : maxVertexCount;
        const size_t terminalHeadroom = hasProgressiveOccurrence &&
                vertexCount <= maxVertexCount / 2u ?
            vertexCount * 2u : vertexCount;
        const size_t futureReserve = futureRangeVertexCount <=
                maxVertexCount / 2u ?
            futureRangeVertexCount * 2u : maxVertexCount;
        const size_t futureHeadroom = futureReserve <=
                maxVertexCount - vertexCount ?
            vertexCount + futureReserve : maxVertexCount;
        const size_t reserve = std::min(
            growthLimit, std::max(terminalHeadroom, futureHeadroom));
        gpuRes_->uploadFlatWire(
            presentationRevision, plan.geometryRevision, positions,
            std::vector<CadFlatWireGroup>(), newRanges,
            static_cast<GLsizei>(reserve), glue, caps_);
    }
    for (Occurrence& occurrence : occurrences) {
        if (renderInterruptedAfter(deadlineWork))
            return false;
        if (!occurrence.rangeCachePending)
            continue;
        if (!gpuRes_->lookupFlatWireRange(
                occurrence.visibleIndex, occurrence.rangeKey,
                &occurrence.range))
            return false;
        occurrence.rangeCachePending = false;
    }

    std::vector<CadFlatWireGroup> groups;
    FlatWireStyleKey activeKey;
    bool haveGroup = false;
    for (size_t i = 0; i < occurrences.size(); ++i) {
        if (renderInterruptedAfter(deadlineWork))
            return false;
        const Occurrence& occurrence = orderedOccurrence(i);
        if (!occurrence.rangeValid)
            return false;
        const FlatWireStyleKey& key = occurrence.style;
        if (!haveGroup || !(key == activeKey)) {
            CadFlatWireGroup group;
            group.lineWidth = occurrence.instance->lineWidth;
            group.linePattern = occurrence.instance->linePattern;
            group.linePatternFactor =
                occurrence.instance->linePatternFactor;
            std::copy(occurrence.instance->rgba.begin(),
                      occurrence.instance->rgba.end(), group.rgba);
            groups.push_back(group);
            activeKey = key;
            haveGroup = true;
        }
        CadFlatWireGroup& group = groups.back();
        const GLint first = occurrence.range.first;
        const GLsizei count = occurrence.range.count;
        if (!group.firsts.empty() &&
                group.firsts.back() + group.counts.back() == first) {
            group.counts.back() += count;
        } else {
            group.firsts.push_back(first);
            group.counts.push_back(count);
        }
    }
    for (CadFlatWireGroup& group : groups) {
        if (group.firsts.empty()) continue;
        group.first = group.firsts.front();
        group.count = group.counts.front();
    }
    gpuRes_->updateFlatWireGroups(presentationRevision, groups);

    const CadFlatWireGpu& flat = gpuRes_->flatWire();
    if (occurrences.empty()) return true;
    if (!flat.posBuf || flat.groups.empty()) return false;

    const CadWireRasterState rasterState =
        captureWireRasterState(glue, caps_.hasLineStipple);
    const bool fixedFunction = caps_.isSoftwareRenderer;
    GLboolean wasLighting = GL_FALSE;
    GLint locColor = -1;
    GLint locPos = 0;
    if (fixedFunction) {
        glue->glMatrixMode(GL_PROJECTION);
        glue->glPushMatrix();
        glue->glLoadIdentity();
        glue->glMatrixMode(GL_MODELVIEW);
        glue->glPushMatrix();
        glue->glLoadMatrixf(viewProj[0]);
        wasLighting = glue->glIsEnabled(GL_LIGHTING);
        glue->glDisable(GL_LIGHTING);
        glue->glEnableClientState(GL_VERTEX_ARRAY);
        glue->glBindBuffer(GL_ARRAY_BUFFER, flat.posBuf);
        glue->glVertexPointer(3, GL_FLOAT, 3 * sizeof(float), nullptr);
    } else {
        glue->glUseProgramObjectARB(shaders_.wire);
    const GLint locVP = glue->glGetUniformLocationARB(shaders_.wire, "u_viewProj");
    const GLint locModel = glue->glGetUniformLocationARB(shaders_.wire, "u_model");
        locColor = glue->glGetUniformLocationARB(shaders_.wire, "u_color");
        locPos = glue->glGetAttribLocationARB(shaders_.wire, "a_pos");
    if (locPos < 0) locPos = 0;
    const SbMatrix identity = SbMatrix::identity();
    glue->glUniformMatrix4fvARB(locVP, 1, GL_FALSE, viewProj[0]);
    glue->glUniformMatrix4fvARB(locModel, 1, GL_FALSE, identity[0]);

    if (flat.vao && glue->glBindVertexArray) {
        glue->glBindVertexArray(flat.vao);
    } else {
        glue->glBindBuffer(GL_ARRAY_BUFFER, flat.posBuf);
        glue->glVertexAttribPointerARB(static_cast<GLuint>(locPos), 3,
                                       GL_FLOAT, GL_FALSE,
                                       3 * sizeof(float), nullptr);
        glue->glEnableVertexAttribArrayARB(static_cast<GLuint>(locPos));
    }
    }
    for (const CadFlatWireGroup& group : flat.groups) {
        if (fixedFunction) {
            glue->glColor4ub(group.rgba[0], group.rgba[1],
                             group.rgba[2], group.rgba[3]);
        } else {
            const float color[4] = {group.rgba[0] / 255.0f,
                                    group.rgba[1] / 255.0f,
                                    group.rgba[2] / 255.0f,
                                    group.rgba[3] / 255.0f};
            glue->glUniform4fvARB(locColor, 1, color);
        }
        glue->glLineWidth(std::max(1.0f, group.lineWidth));
        if (caps_.hasLineStipple) {
            if (group.linePattern != 0xffffu) {
                glue->glLineStipple(std::max<GLint>(1, group.linePatternFactor),
                                    group.linePattern);
                glue->glEnable(GL_LINE_STIPPLE);
            } else {
                glue->glDisable(GL_LINE_STIPPLE);
            }
        }
        if (group.firsts.empty()) {
            glue->glDrawArrays(GL_LINES, group.first, group.count);
        } else if (group.firsts.size() == 1) {
            glue->glDrawArrays(
                GL_LINES, group.firsts.front(), group.counts.front());
        } else if (glue->glMultiDrawArrays) {
            SoGLContext_glMultiDrawArrays(
                glue, GL_LINES, group.firsts.data(), group.counts.data(),
                static_cast<GLsizei>(group.firsts.size()));
        } else {
            for (size_t range = 0; range < group.firsts.size(); ++range) {
                glue->glDrawArrays(
                    GL_LINES, group.firsts[range], group.counts[range]);
            }
        }
    }
    lastRenderedWork_.lineCount = cadSaturatingWorkAdd(
        lastRenderedWork_.lineCount,
        static_cast<uint64_t>(visibleVertexCount / 2u));
    lastRenderedWork_.positionCount = cadSaturatingWorkAdd(
        lastRenderedWork_.positionCount,
        static_cast<uint64_t>(visibleVertexCount));
    lastRenderedWork_.occurrenceCount = cadSaturatingWorkAdd(
        lastRenderedWork_.occurrenceCount,
        static_cast<uint64_t>(occurrences.size()));
    if (fixedFunction) {
        glue->glDisableClientState(GL_VERTEX_ARRAY);
        glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
        if (wasLighting) glue->glEnable(GL_LIGHTING);
        glue->glMatrixMode(GL_MODELVIEW);
        glue->glPopMatrix();
        glue->glMatrixMode(GL_PROJECTION);
        glue->glPopMatrix();
        glue->glMatrixMode(GL_MODELVIEW);
    } else if (flat.vao && glue->glBindVertexArray)
        glue->glBindVertexArray(0);
    else {
        glue->glDisableVertexAttribArrayARB(static_cast<GLuint>(locPos));
        glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    if (!fixedFunction)
        glue->glUseProgramObjectARB(0);
    restoreWireRasterState(glue, rasterState, caps_.hasLineStipple);
    return true;
}

static void
drawFlatShadedGroup(const SoGLContext *glue, GLenum mode,
                    const CadFlatShadedGroup& group)
{
    if (group.firsts.empty()) {
        glue->glDrawArrays(mode, group.first, group.count);
        return;
    }
    if (group.firsts.size() == 1) {
        glue->glDrawArrays(
            mode, group.firsts.front(), group.counts.front());
        return;
    }
    if (glue->glMultiDrawArrays) {
        SoGLContext_glMultiDrawArrays(
            glue, mode, group.firsts.data(), group.counts.data(),
            static_cast<GLsizei>(group.firsts.size()));
        return;
    }
    for (size_t range = 0; range < group.firsts.size(); ++range)
        glue->glDrawArrays(mode, group.firsts[range], group.counts[range]);
}

bool CadRendererGL::renderFlatShaded(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext* glue,
        const SbMatrix& viewProj,
        const SbMatrix& viewMatrix,
        const SbMatrix& projectionMatrix,
        bool depthOnly)
{
    constexpr size_t maxVertexBytes = 512u * 1024u * 1024u;
    constexpr size_t floatsPerVertex = 6;
    size_t deadlineWork = 256u;
    if (renderInterruptedAfter(deadlineWork))
        return false;
    const size_t maxVertexCount =
        maxVertexBytes / (floatsPerVertex * sizeof(float));
    const ExecutorFrustumPlanes fp =
        extractExecutorFrustumPlanes(viewProj);

    struct Occurrence {
        const Obol::TriMesh *mesh = nullptr;
        const CadVisibleInstance *instance = nullptr;
        CadFlatShadedRangeKey rangeKey;
        size_t indexCount = 0;
        uint8_t cut = Obol::ProgressiveCutUnspecified;
        uint64_t styleKey = 0;
        bool cullBackfaces = false;
    };
    std::vector<Occurrence> occurrences;
    occurrences.reserve(plan.visibleInstances.size());
    size_t currentVertexCount = 0;
    size_t futureRangeVertexCount = 0;
    bool hasProgressiveOccurrence = false;
    for (const CadDrawItem& item : plan.shadedItems) {
        if (renderInterruptedAfter(deadlineWork))
            return false;
        if (item.partIndex >= plan.partBindings.size())
            continue;
        const CadPartBinding& binding =
            plan.partBindings[item.partIndex];
        const Obol::PartGeometry *geom = binding.geometry.get();
        if (!geom || !geom->shaded.has_value()) continue;
        const Obol::TriMesh& mesh = *geom->shaded;
        for (uint32_t ii = 0; ii < item.instanceCount; ++ii) {
            if (renderInterruptedAfter(deadlineWork))
                return false;
            const size_t visibleIndex = item.baseInstance + ii;
            if (!cadInstanceDrawable(
                    plan, item, visibleIndex, CadDrawChannel::Shaded))
                continue;
            const CadVisibleInstance& instance =
                plan.visibleInstances[visibleIndex];
            if (isBoxOutsideExecutorFrustum(
                    instance.wbMin, instance.wbMax, fp))
                continue;
            const uint8_t requested =
                assembly.effectiveProgressiveCut(instance.lodCut);
            const uint8_t level = mesh.isProgressive() ?
                cadResolvedProgressiveCut(requested, mesh.progressiveMinimumCut,
                                 mesh.progressiveResidentCut) :
                Obol::ProgressiveCutUnspecified;
            const size_t indexCount = mesh.isProgressive() ?
                mesh.indexCountAtCut(level) : mesh.indices.size();
            if (indexCount < 3)
                continue;
            if (indexCount > maxVertexCount ||
                    currentVertexCount > maxVertexCount - indexCount)
                return false;
            currentVertexCount += indexCount;
            Occurrence occurrence;
            occurrence.mesh = &mesh;
            occurrence.instance = &instance;
            uint64_t geometryToken = 1469598103934665603ULL;
            geometryToken ^= binding.generation;
            geometryToken *= 1099511628211ULL;
            geometryToken ^= item.rep.part.w0;
            geometryToken *= 1099511628211ULL;
            geometryToken ^= item.rep.part.w1;
            geometryToken *= 1099511628211ULL;
            for (float value : instance.transform) {
                uint32_t bits = 0;
                std::memcpy(&bits, &value, sizeof(bits));
                geometryToken ^= bits;
                geometryToken *= 1099511628211ULL;
            }
            occurrence.rangeKey = CadFlatShadedRangeKey{
                instance.instanceId, level, geometryToken};
            occurrence.indexCount = indexCount;
            occurrence.cut = level;
            occurrence.cullBackfaces = cadProgressiveCutCullSafe(
                item.cullBackfaces, occurrence.mesh, occurrence.cut);
            occurrence.styleKey =
                static_cast<uint64_t>(flatRgbaKey(instance)) |
                (static_cast<uint64_t>(occurrence.cullBackfaces) << 32);
            occurrences.push_back(occurrence);

            if (mesh.isProgressive()) {
                hasProgressiveOccurrence = true;
                if (level < mesh.progressiveResidentCut) {
                    const size_t residentVertices =
                        mesh.indexCountAtCut(
                            mesh.progressiveResidentCut);
                    futureRangeVertexCount = std::min(
                        maxVertexCount,
                        futureRangeVertexCount <=
                                maxVertexCount - residentVertices ?
                            futureRangeVertexCount + residentVertices :
                            maxVertexCount);
                }
            }
        }
    }

    /* Keep equal styles adjacent in both the atlas and the draw-range list.
     * The overwhelmingly common vehicle case is uniformly styled.  Detect
     * that linear case so a 50k-leaf progressive publication does not sort
     * and move 50k occurrence records on every new cut. */
    bool uniformStyle = true;
    for (size_t i = 1; i < occurrences.size(); ++i) {
        if (renderInterruptedAfter(deadlineWork))
            return false;
        if (occurrences[i].styleKey != occurrences.front().styleKey) {
            uniformStyle = false;
            break;
        }
    }
    if (!uniformStyle) {
        std::sort(occurrences.begin(), occurrences.end(),
            [](const Occurrence& a, const Occurrence& b) {
                if (a.styleKey != b.styleKey)
                    return a.styleKey < b.styleKey;
                return a.rangeKey.instance < b.rangeKey.instance;
            });
        if (renderInterrupted())
            return false;
    }

    auto buildAtlasRanges = [&](
            bool onlyMissing, GLint baseVertex,
            std::vector<float>& positions,
            std::vector<float>& normals,
            std::unordered_map<CadFlatShadedRangeKey,
                               CadFlatShadedRange,
                               CadFlatShadedRangeKeyHash>& ranges) {
        const CadFlatShadedGpu& current = gpuRes_->flatShaded();
        size_t appendVertexCount = 0;
        for (const Occurrence& occurrence : occurrences) {
            if (renderInterruptedAfter(deadlineWork))
                return false;
            if (onlyMissing &&
                    current.ranges.find(occurrence.rangeKey) !=
                        current.ranges.end())
                continue;
            if (appendVertexCount >
                    maxVertexCount - occurrence.indexCount)
                return false;
            appendVertexCount += occurrence.indexCount;
        }
        positions.clear();
        normals.clear();
        positions.reserve(appendVertexCount * 3);
        normals.reserve(appendVertexCount * 3);
        ranges.clear();
        for (const Occurrence& occurrence : occurrences) {
            if (renderInterruptedAfter(deadlineWork))
                return false;
            if (onlyMissing &&
                    current.ranges.find(occurrence.rangeKey) !=
                        current.ranges.end())
                continue;
            const Obol::TriMesh& mesh = *occurrence.mesh;
            const CadVisibleInstance& instance = *occurrence.instance;
            const bool hasVertexNormals =
                mesh.normals.size() == mesh.positions.size();
            SbMatrix transform;
            transform.setValue(instance.transform.data());
            const SbMatrix normalMatrix = transform.inverse().transpose();
            const GLint first = baseVertex +
                static_cast<GLint>(positions.size() / 3);
            for (size_t t = 0; t + 2 < occurrence.indexCount; t += 3) {
                if (renderInterruptedAfter(deadlineWork))
                    return false;
                const uint32_t ia = mesh.indices[t];
                const uint32_t ib = mesh.indices[t + 1];
                const uint32_t ic = mesh.indices[t + 2];
                if (ia >= mesh.positions.size() ||
                        ib >= mesh.positions.size() ||
                        ic >= mesh.positions.size())
                    return false;
                const uint32_t indices[3] = {ia, ib, ic};
                SbVec3f triangle[3];
                SbVec3f sourceTriangle[3];
                for (size_t vertex = 0; vertex < 3; ++vertex) {
                    const SbVec3f sourcePoint =
                        mesh.positions[indices[vertex]];
                    SbVec3f point = sourcePoint;
                    if (mesh.isProgressive()) {
                        point = progressiveSnapPoint(
                            point, mesh.progressiveQuantizationMinimum,
                            mesh.progressiveQuantizationMaximum,
                            mesh.quantizationAtCut(occurrence.cut));
                    }
                    triangle[vertex] =
                        transformedFlatPoint(point, instance.transform);
                    sourceTriangle[vertex] = transformedFlatPoint(
                        sourcePoint, instance.transform);
                }
                SbVec3f faceNormal =
                    (sourceTriangle[1] - sourceTriangle[0]).cross(
                        sourceTriangle[2] - sourceTriangle[0]);
                if (faceNormal.sqrLength() > 0.0f)
                    faceNormal.normalize();
                else
                    faceNormal.setValue(0.0f, 0.0f, 1.0f);
                for (size_t vertex = 0; vertex < 3; ++vertex) {
                    SbVec3f normal = faceNormal;
                    if (hasVertexNormals) {
                        normalMatrix.multDirMatrix(
                            mesh.normals[indices[vertex]], normal);
                        if (normal.sqrLength() > 0.0f)
                            normal.normalize();
                        else
                            normal = faceNormal;
                    }
                    executorAppendPackedPoint(
                        positions, triangle[vertex]);
                    executorAppendPackedPoint(normals, normal);
                }
            }
            ranges.emplace(occurrence.rangeKey,
                CadFlatShadedRange{
                    first,
                    static_cast<GLsizei>(occurrence.indexCount)});
        }
        return true;
    };

    bool rebuild = !gpuRes_->flatShaded().posBuf ||
                   !gpuRes_->flatShaded().normBuf;
    std::vector<float> positions;
    std::vector<float> normals;
    std::unordered_map<CadFlatShadedRangeKey, CadFlatShadedRange,
                       CadFlatShadedRangeKeyHash> newRanges;

    if (!rebuild) {
        size_t missingVertexCount = 0;
        const CadFlatShadedGpu& flat = gpuRes_->flatShaded();
        for (const Occurrence& occurrence : occurrences) {
            if (renderInterruptedAfter(deadlineWork))
                return false;
            if (flat.ranges.find(occurrence.rangeKey) != flat.ranges.end())
                continue;
            if (missingVertexCount >
                    maxVertexCount - occurrence.indexCount)
                return false;
            missingVertexCount += occurrence.indexCount;
        }
        if (missingVertexCount) {
            if (flat.vertexCount >
                    flat.capacityVertexCount -
                        static_cast<GLsizei>(missingVertexCount)) {
                /* The retained level history has filled its reserve.  Compact
                 * to just the cuts needed by this view, then leave fresh
                 * headroom for later view changes. */
                rebuild = true;
            } else {
                if (!buildAtlasRanges(
                        true, flat.vertexCount, positions, normals,
                        newRanges) ||
                        !gpuRes_->appendFlatShaded(
                            positions, normals, newRanges, glue))
                    return false;
            }
        } else if (hasProgressiveOccurrence &&
                futureRangeVertexCount == 0 &&
                flat.capacityVertexCount > 0 &&
                currentVertexCount <=
                    static_cast<size_t>(flat.capacityVertexCount) / 4u) {
            /* Convergence should not retain the 16x reserve used to absorb
             * rapid publication waves.  Rebuild the terminal view with one
             * spare active-cut-sized slot; that preserves instant return to
             * this range after an interactive coarse cut without carrying a
             * large-model high-water allocation indefinitely. */
            rebuild = true;
        }
    }

    if (rebuild) {
        if (occurrences.empty())
            return true;
        if (!buildAtlasRanges(
                false, 0, positions, normals, newRanges))
            return false;
        const size_t vertexCount = positions.size() / 3;
        /*
         * Scene admission deliberately grows a cheap measured cut by as much
         * as 4x while substantial frame headroom remains.  A 2x VBO reserve
         * therefore guaranteed a full CPU re-expansion and GPU reallocation
         * at nearly every calibrated population wave.  Keep room for two
         * such waves (including the retained old ranges) so progressive
         * publication normally appends; the 512 MiB aggregate vertex-data
         * ceiling above still bounds the allocation.
         */
        constexpr size_t progressiveGrowthReserve = 16u;
        const size_t growthLimit = vertexCount <=
                maxVertexCount / progressiveGrowthReserve ?
            vertexCount * progressiveGrowthReserve : maxVertexCount;
        const size_t terminalHeadroom = hasProgressiveOccurrence &&
                vertexCount <= maxVertexCount / 2u ?
            vertexCount * 2u : vertexCount;
        const size_t futureReserve = futureRangeVertexCount <=
                maxVertexCount / 2u ?
            futureRangeVertexCount * 2u : maxVertexCount;
        const size_t futureHeadroom = futureReserve <=
                maxVertexCount - vertexCount ?
            vertexCount + futureReserve : maxVertexCount;
        const size_t reserve = std::min(
            growthLimit, std::max(terminalHeadroom, futureHeadroom));
        gpuRes_->uploadFlatShaded(
            plan.revision, plan.geometryRevision,
            positions, normals, std::vector<CadFlatShadedGroup>(),
            newRanges, static_cast<GLsizei>(reserve), glue, caps_);
    }

    std::vector<CadFlatShadedGroup> groups;
    uint64_t activeStyle = 0;
    bool haveGroup = false;
    const CadFlatShadedGpu& atlas = gpuRes_->flatShaded();
    for (const Occurrence& occurrence : occurrences) {
        if (renderInterruptedAfter(deadlineWork))
            return false;
        const auto rangeIt = atlas.ranges.find(occurrence.rangeKey);
        if (rangeIt == atlas.ranges.end())
            return false;
        if (!haveGroup || occurrence.styleKey != activeStyle) {
            CadFlatShadedGroup group;
            std::copy(occurrence.instance->rgba.begin(),
                      occurrence.instance->rgba.end(), group.rgba);
            group.cullBackfaces = occurrence.cullBackfaces;
            groups.push_back(group);
            activeStyle = occurrence.styleKey;
            haveGroup = true;
        }
        CadFlatShadedGroup& group = groups.back();
        const GLint first = rangeIt->second.first;
        const GLsizei count = rangeIt->second.count;
        if (!group.firsts.empty() &&
                group.firsts.back() + group.counts.back() == first) {
            group.counts.back() += count;
        } else {
            group.firsts.push_back(first);
            group.counts.push_back(count);
        }
    }
    for (CadFlatShadedGroup& group : groups) {
        if (group.firsts.empty()) continue;
        group.first = group.firsts.front();
        group.count = group.counts.front();
    }
    gpuRes_->updateFlatShadedGroups(plan.revision, groups);

    const CadFlatShadedGpu& flat = gpuRes_->flatShaded();
    if (occurrences.empty()) return true;
    if (!flat.posBuf || !flat.normBuf || flat.groups.empty()) return false;

    /*
     * The flattened path submits the same view-selected progressive prefixes
     * as the retained indexed paths, but historically left the shared
     * diagnostic at zero.  Besides hiding actual software-renderer work from
     * the HUD and graphical tests, that made a render-only interaction
     * ceiling indistinguishable from rewriting every producer-authored
     * occurrence cut.  currentVertexCount is the exact sum of the flattened
     * triangle vertices selected above.
     */
    lastRenderedTriangleCount_ =
        static_cast<uint64_t>(currentVertexCount / 3u);
    lastRenderedWork_.triangleCount = cadSaturatingWorkAdd(
        lastRenderedWork_.triangleCount,
        static_cast<uint64_t>(currentVertexCount / 3u));
    lastRenderedWork_.positionCount = cadSaturatingWorkAdd(
        lastRenderedWork_.positionCount,
        static_cast<uint64_t>(currentVertexCount));
    if (!depthOnly) {
        lastRenderedWork_.normalCount = cadSaturatingWorkAdd(
            lastRenderedWork_.normalCount,
            static_cast<uint64_t>(currentVertexCount));
    }
    lastRenderedWork_.occurrenceCount = cadSaturatingWorkAdd(
        lastRenderedWork_.occurrenceCount,
        static_cast<uint64_t>(occurrences.size()));

    if (caps_.isSoftwareRenderer) {
        glue->glMatrixMode(GL_PROJECTION);
        glue->glPushMatrix();
        glue->glLoadMatrixf(projectionMatrix[0]);
        glue->glMatrixMode(GL_MODELVIEW);
        glue->glPushMatrix();
        glue->glLoadMatrixf(viewMatrix[0]);
        /* Coin allocates fixed-function light slots monotonically while a
         * traversal is active.  A persistent scene changing from three lights
         * to one can otherwise leave the unused slots enabled for this custom
         * retained node.  Install the complete snapshot used by GLSL so OSMesa
         * cannot inherit lights from the preceding profile or frame. */
        this->uploadFixedLights(glue);
        const GLboolean wasLighting = glue->glIsEnabled(GL_LIGHTING);
        const GLboolean wasColorMaterial =
            glue->glIsEnabled(GL_COLOR_MATERIAL);
        GLint wasTwoSidedLighting = GL_FALSE;
        glue->glGetIntegerv(
            GL_LIGHT_MODEL_TWO_SIDE, &wasTwoSidedLighting);
        if (depthOnly) glue->glDisable(GL_LIGHTING);
        else {
            glue->glEnable(GL_LIGHTING);
            glue->glDisable(GL_COLOR_MATERIAL);
            glue->glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
        }
        glue->glEnableClientState(GL_VERTEX_ARRAY);
        glue->glBindBuffer(GL_ARRAY_BUFFER, flat.posBuf);
        glue->glVertexPointer(3, GL_FLOAT, 3 * sizeof(float), nullptr);
        if (!depthOnly) {
            glue->glEnableClientState(GL_NORMAL_ARRAY);
            glue->glBindBuffer(GL_ARRAY_BUFFER, flat.normBuf);
            glue->glNormalPointer(GL_FLOAT, 3 * sizeof(float), nullptr);
        }
        for (const CadFlatShadedGroup& group : flat.groups) {
            setCadBackfaceCulling(glue, group.cullBackfaces);
            if (!depthOnly) setImmediateMaterialFromRgba(glue, group.rgba);
            drawFlatShadedGroup(glue, GL_TRIANGLES, group);
        }
        if (!depthOnly) glue->glDisableClientState(GL_NORMAL_ARRAY);
        glue->glDisableClientState(GL_VERTEX_ARRAY);
        glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
        if (wasColorMaterial) glue->glEnable(GL_COLOR_MATERIAL);
        else glue->glDisable(GL_COLOR_MATERIAL);
        glue->glLightModeli(
            GL_LIGHT_MODEL_TWO_SIDE, wasTwoSidedLighting);
        if (wasLighting) glue->glEnable(GL_LIGHTING);
        else glue->glDisable(GL_LIGHTING);
        glue->glMatrixMode(GL_MODELVIEW);
        glue->glPopMatrix();
        glue->glMatrixMode(GL_PROJECTION);
        glue->glPopMatrix();
        glue->glMatrixMode(GL_MODELVIEW);
        return true;
    }

    glue->glUseProgramObjectARB(shaders_.shaded);
    const GLint locVP = glue->glGetUniformLocationARB(shaders_.shaded,
                                                       "u_viewProj");
    const GLint locModel = glue->glGetUniformLocationARB(shaders_.shaded,
                                                          "u_model");
    const GLint locColor = glue->glGetUniformLocationARB(shaders_.shaded,
                                                          "u_color");
    const GLint locHasNorm = glue->glGetUniformLocationARB(shaders_.shaded,
                                                            "u_hasNorm");
    const SbMatrix identity = SbMatrix::identity();
    glue->glUniformMatrix4fvARB(locVP, 1, GL_FALSE, viewProj[0]);
    glue->glUniformMatrix4fvARB(locModel, 1, GL_FALSE, identity[0]);
    this->uploadLights(glue, shaders_.shaded);
    glue->glUniform1iARB(locHasNorm, depthOnly ? 0 : 1);
    if (flat.vao && glue->glBindVertexArray) {
        glue->glBindVertexArray(flat.vao);
    } else {
        glue->glBindBuffer(GL_ARRAY_BUFFER, flat.posBuf);
        glue->glVertexAttribPointerARB(0, 3, GL_FLOAT, GL_FALSE,
                                       3 * sizeof(float), nullptr);
        glue->glEnableVertexAttribArrayARB(0);
        glue->glBindBuffer(GL_ARRAY_BUFFER, flat.normBuf);
        glue->glVertexAttribPointerARB(1, 3, GL_FLOAT, GL_FALSE,
                                       3 * sizeof(float), nullptr);
        glue->glEnableVertexAttribArrayARB(1);
    }
    for (const CadFlatShadedGroup& group : flat.groups) {
        setCadBackfaceCulling(glue, group.cullBackfaces);
        const float rgba[4] = {group.rgba[0] / 255.0f,
                               group.rgba[1] / 255.0f,
                               group.rgba[2] / 255.0f,
                               group.rgba[3] / 255.0f};
        glue->glUniform4fvARB(locColor, 1, rgba);
        drawFlatShadedGroup(glue, GL_TRIANGLES, group);
    }
    if (flat.vao && glue->glBindVertexArray) {
        glue->glBindVertexArray(0);
    } else {
        glue->glDisableVertexAttribArrayARB(1);
        glue->glDisableVertexAttribArrayARB(0);
        glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
    glue->glUseProgramObjectARB(0);
    return true;
}

bool CadRendererGL::renderFlatTriangleEdges(
        const CadFramePlan& plan,
        const SoGLContext* glue,
        const SbMatrix& viewProj,
        const SbMatrix& viewMatrix,
        const SbMatrix& projectionMatrix)
{
    const CadFlatShadedGpu& flat = gpuRes_->flatShaded();
    if (flat.planRevision != plan.revision || !flat.posBuf ||
            flat.groups.empty())
        return false;

    uint64_t submittedVertices = 0;
    for (const CadFlatShadedGroup& group : flat.groups) {
        if (group.firsts.empty()) {
            submittedVertices = cadSaturatingWorkAdd(
                submittedVertices,
                static_cast<uint64_t>((std::max)(0, group.count)));
            continue;
        }
        for (const GLsizei count : group.counts) {
            submittedVertices = cadSaturatingWorkAdd(
                submittedVertices,
                static_cast<uint64_t>((std::max)(0, count)));
        }
    }

    GLint polygonMode[2] = {GL_FILL, GL_FILL};
    glue->glGetIntegerv(GL_POLYGON_MODE, polygonMode);
    glue->glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    if (caps_.isSoftwareRenderer) {
        glue->glMatrixMode(GL_PROJECTION);
        glue->glPushMatrix();
        glue->glLoadMatrixf(projectionMatrix[0]);
        glue->glMatrixMode(GL_MODELVIEW);
        glue->glPushMatrix();
        glue->glLoadMatrixf(viewMatrix[0]);
        const GLboolean wasLighting = glue->glIsEnabled(GL_LIGHTING);
        glue->glDisable(GL_LIGHTING);
        glue->glEnableClientState(GL_VERTEX_ARRAY);
        glue->glBindBuffer(GL_ARRAY_BUFFER, flat.posBuf);
        glue->glVertexPointer(3, GL_FLOAT, 3 * sizeof(float), nullptr);
        for (const CadFlatShadedGroup& group : flat.groups) {
            setCadBackfaceCulling(glue, group.cullBackfaces);
            glue->glColor4ub(group.rgba[0], group.rgba[1],
                             group.rgba[2], group.rgba[3]);
            drawFlatShadedGroup(glue, GL_TRIANGLES, group);
        }
        glue->glDisableClientState(GL_VERTEX_ARRAY);
        glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
        if (wasLighting) glue->glEnable(GL_LIGHTING);
        glue->glMatrixMode(GL_MODELVIEW);
        glue->glPopMatrix();
        glue->glMatrixMode(GL_PROJECTION);
        glue->glPopMatrix();
        glue->glMatrixMode(GL_MODELVIEW);
    } else {
        glue->glUseProgramObjectARB(shaders_.wire);
        const GLint locVP = glue->glGetUniformLocationARB(shaders_.wire,
                                                          "u_viewProj");
        const GLint locModel = glue->glGetUniformLocationARB(shaders_.wire,
                                                             "u_model");
        const GLint locColor = glue->glGetUniformLocationARB(shaders_.wire,
                                                             "u_color");
        const SbMatrix identity = SbMatrix::identity();
        glue->glUniformMatrix4fvARB(locVP, 1, GL_FALSE, viewProj[0]);
        glue->glUniformMatrix4fvARB(locModel, 1, GL_FALSE, identity[0]);
        if (flat.vao && glue->glBindVertexArray) {
            glue->glBindVertexArray(flat.vao);
        } else {
            glue->glBindBuffer(GL_ARRAY_BUFFER, flat.posBuf);
            glue->glVertexAttribPointerARB(0, 3, GL_FLOAT, GL_FALSE,
                                           3 * sizeof(float), nullptr);
            glue->glEnableVertexAttribArrayARB(0);
        }
        for (const CadFlatShadedGroup& group : flat.groups) {
            setCadBackfaceCulling(glue, group.cullBackfaces);
            const float rgba[4] = {group.rgba[0] / 255.0f,
                                   group.rgba[1] / 255.0f,
                                   group.rgba[2] / 255.0f,
                                   group.rgba[3] / 255.0f};
            glue->glUniform4fvARB(locColor, 1, rgba);
            drawFlatShadedGroup(glue, GL_TRIANGLES, group);
        }
        if (flat.vao && glue->glBindVertexArray) {
            glue->glBindVertexArray(0);
        } else {
            glue->glDisableVertexAttribArrayARB(0);
            glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
        glue->glUseProgramObjectARB(0);
    }

    glue->glPolygonMode(GL_FRONT, static_cast<GLenum>(polygonMode[0]));
    glue->glPolygonMode(GL_BACK, static_cast<GLenum>(polygonMode[1]));
    lastRenderedWork_.lineCount = cadSaturatingWorkAdd(
        lastRenderedWork_.lineCount, submittedVertices);
    lastRenderedWork_.positionCount = cadSaturatingWorkAdd(
        lastRenderedWork_.positionCount, submittedVertices);
    return true;
}

// ---------------------------------------------------------------------------
// Tier-1: VBO-loop rendering (GL 2.0+)
// ---------------------------------------------------------------------------

static GLsizei progressiveWireSegmentCount(
        const SoCADAssembly& assembly, PartId part,
        const CadVisibleInstance& instance, GLsizei residentCount)
{
    const PartGeometry *geometry = assembly.partGeometry(part);
    if (!geometry || !geometry->wire || !geometry->wire->isProgressive())
        return residentCount;
    return static_cast<GLsizei>(geometry->wire->segmentCountAtCut(
        assembly.effectiveProgressiveCut(instance.lodCut)));
}

static GLsizei progressiveTriangleIndexCount(
        const SoCADAssembly& assembly, PartId part,
        const CadVisibleInstance& instance, GLsizei residentCount)
{
    const PartGeometry *geometry = assembly.partGeometry(part);
    if (!geometry || !geometry->shaded ||
            !geometry->shaded->isProgressive())
        return residentCount;
    return static_cast<GLsizei>(geometry->shaded->indexCountAtCut(
        assembly.effectiveProgressiveCut(instance.lodCut)));
}

/*
 * Software rasterizers execute one large glDrawElements call synchronously.
 * Coin's abort callback cannot be observed until that call returns, which
 * made a single large CAD leaf the minimum endpoint latency (Lucy cut 21 took
 * roughly 250 ms in OSMesa).  Bound each software submission and poll the
 * render transaction between chunks.  The index stream is a triangle list,
 * so every chunk begins and ends on a three-index boundary and has identical
 * raster semantics to the monolithic call.
 *
 * Hardware keeps the single submission.  Driver/GPU command processing is
 * asynchronous there, and splitting a retained batch would only add CPU draw
 * overhead without improving the owner-thread cancellation contract.
 */
static constexpr GLsizei cadSoftwareTriangleChunkIndices =
    64 * 1024 * 3;

static void
uploadProgressivePositionUniforms(
        const SoGLContext *glue, GLint encodeScaleLocation,
        GLint decodeScaleLocation, GLint minLocation,
        ProgressiveQuantization quantization,
        const SbVec3f& minimum, const SbVec3f& maximum)
{
    const uint8_t bits[3] = {
        quantization.xBits, quantization.yBits, quantization.zBits
    };
    SbVec3f encodeScale;
    SbVec3f decodeScale;
    for (int axis = 0; axis < 3; ++axis) {
        const GLfloat mask = bits[axis] > 0 ?
            std::ldexp(1.0f, 16 - std::min<int>(16, bits[axis])) : 1.0f;
        const GLfloat extent = maximum[axis] - minimum[axis];
        encodeScale[axis] = extent > 0.0f ? 65535.0f / extent : 0.0f;
        /* The legacy uniform name is retained inside this private renderer,
         * but its value is now the exact integer prefix mask.  Passing the
         * power of two directly avoids reconstructing it from floating
         * coordinate scales on large extents and on Windows drivers. */
        decodeScale[axis] = mask;
    }
    glue->glUniform3fvARB(
        encodeScaleLocation, 1, encodeScale.getValue());
    glue->glUniform3fvARB(
        decodeScaleLocation, 1, decodeScale.getValue());
    glue->glUniform3fvARB(minLocation, 1, minimum.getValue());
}

static float
progressiveSnapCoordinate(float value, float minimum, float maximum,
                          double mask)
{
    if (!(maximum > minimum)) return value;
    const double scaled =
        (static_cast<double>(value) - minimum) /
        (static_cast<double>(maximum) - minimum) * 65535.0;
    const double code = std::floor(std::max(0.0, std::min(65535.0, scaled)));
    const double cell = std::floor(code / mask);
    const double snapped = std::min(65535.0, (cell + 0.5) * mask);
    return static_cast<float>(
        (snapped / 65535.0) *
        (static_cast<double>(maximum) - minimum) + minimum);
}

static SbVec3f
progressiveSnapPoint(const SbVec3f& point, const SbVec3f& minimum,
                     const SbVec3f& maximum,
                     ProgressiveQuantization quantization)
{
    if (quantization.isExact()) return point;
    const uint8_t bits[3] = {
        quantization.xBits, quantization.yBits, quantization.zBits
    };
    double mask[3] = {1.0, 1.0, 1.0};
    for (int axis = 0; axis < 3; ++axis)
        if (bits[axis] > 0)
            mask[axis] = std::ldexp(
                1.0, 16 - std::min<int>(16, bits[axis]));
    return SbVec3f(
        bits[0] ? progressiveSnapCoordinate(
            point[0], minimum[0], maximum[0], mask[0]) : point[0],
        bits[1] ? progressiveSnapCoordinate(
            point[1], minimum[1], maximum[1], mask[1]) : point[1],
        bits[2] ? progressiveSnapCoordinate(
            point[2], minimum[2], maximum[2], mask[2]) : point[2]);
}

static const CadProgressiveGpu*
ensureProgressiveWireGpu(
        CadGpuResources *resources, PartId part, const WireRep& wire,
        uint8_t level,
        const std::vector<CadProgressiveGpu::PackedRange> *sourceRanges,
        const SoGLContext *glue)
{
    if (!resources || !glue) return nullptr;

    const bool packedAdaptive = wire.hasAdaptiveProgressiveClusters() &&
        sourceRanges;
    std::vector<CadProgressiveGpu::PackedRange> packedRanges;
    if (packedAdaptive) {
        const CadProgressiveGpu *existing =
            resources->progressiveForAny(part, false, level);
        if (existing && existing->packedRanges.empty())
            return existing;
        std::vector<CadProgressiveGpu::PackedRange> requested =
            *sourceRanges;
        if (existing && !existing->packedRanges.empty())
            requested.insert(requested.end(), existing->packedRanges.begin(),
                existing->packedRanges.end());
        std::sort(requested.begin(), requested.end(),
            [](const CadProgressiveGpu::PackedRange& left,
               const CadProgressiveGpu::PackedRange& right) {
                if (left.sourceFirst != right.sourceFirst)
                    return left.sourceFirst < right.sourceFirst;
                return left.sourceCount < right.sourceCount;
            });
        requested.erase(std::unique(requested.begin(), requested.end(),
            [](const CadProgressiveGpu::PackedRange& left,
               const CadProgressiveGpu::PackedRange& right) {
                return left.sourceFirst == right.sourceFirst &&
                    left.sourceCount == right.sourceCount;
            }), requested.end());
        if (existing && !existing->packedRanges.empty() &&
                requested.size() == existing->packedRanges.size())
            return existing;

        uint64_t packedFirst = 0;
        packedRanges.reserve(requested.size());
        const uint64_t availableSegments = wire.segmentPoints.size() / 2u;
        for (const CadProgressiveGpu::PackedRange& source : requested) {
            const uint64_t sourceEnd =
                static_cast<uint64_t>(source.sourceFirst) +
                source.sourceCount;
            if (!source.sourceCount || sourceEnd > availableSegments ||
                    packedFirst > UINT32_MAX ||
                    source.sourceCount > UINT32_MAX - packedFirst)
                return nullptr;
            CadProgressiveGpu::PackedRange packed = source;
            packed.packedFirst = static_cast<uint32_t>(packedFirst);
            packedRanges.push_back(packed);
            packedFirst += source.sourceCount;
        }
        if (packedRanges.empty()) return nullptr;
    }

    const size_t pointFirst = wire.hasAdaptiveProgressiveClusters() ?
        0 : wire.segmentFirstAtCut(level) * 2;
    const size_t pointCount = packedAdaptive ? 0 :
        (wire.hasAdaptiveProgressiveClusters() ? wire.segmentPoints.size() :
            wire.segmentCountAtCut(level) * 2);
    size_t packedPointCount = 0;
    for (const CadProgressiveGpu::PackedRange& range : packedRanges) {
        if (range.sourceCount > (SIZE_MAX - packedPointCount) / 2u)
            return nullptr;
        packedPointCount += static_cast<size_t>(range.sourceCount) * 2u;
    }
    const size_t uploadedPointCount = packedAdaptive ?
        packedPointCount : pointCount;
    if (uploadedPointCount == 0 || pointFirst > wire.segmentPoints.size() ||
            (!packedAdaptive &&
             pointCount > wire.segmentPoints.size() - pointFirst))
        return nullptr;

    /* This signature is the validity domain of the derived cut buffer.  The
     * lineage certifies immutable prefix values and quantization bounds;
     * first/count or the packed source ranges identify exactly which
     * certified points were snapped.  In particular, an adaptive full stream
     * grows uploadedPointCount as pages become resident, invalidating a
     * shorter cached cut without invalidating other cuts or unrelated parts. */
    uint64_t rangeSignature = 1469598103934665603ULL;
    const auto mixSignature = [&rangeSignature](uint64_t value) {
        rangeSignature ^= value;
        rangeSignature *= 1099511628211ULL;
    };
    mixSignature(wire.progressiveLineage);
    mixSignature(static_cast<uint64_t>(pointFirst));
    mixSignature(static_cast<uint64_t>(uploadedPointCount));
    const ProgressiveQuantization quantization =
        wire.quantizationAtCut(level);
    mixSignature(static_cast<uint64_t>(quantization.xBits) |
        (static_cast<uint64_t>(quantization.yBits) << 8u) |
        (static_cast<uint64_t>(quantization.zBits) << 16u));
    for (const CadProgressiveGpu::PackedRange& range : packedRanges) {
        mixSignature(range.sourceFirst);
        mixSignature(range.sourceCount);
    }
    if (!rangeSignature) rangeSignature = 1;
    if (const CadProgressiveGpu *cached =
            resources->progressiveFor(
                part, false, level, rangeSignature))
        return cached;
    std::vector<float> positions;
    positions.reserve(uploadedPointCount * 3);
    const auto appendPoints = [&](size_t first, size_t count) {
        for (size_t i = first; i < first + count; ++i) {
            const SbVec3f point = progressiveSnapPoint(
                wire.segmentPoints[i],
                wire.progressiveQuantizationMinimum,
                wire.progressiveQuantizationMaximum, quantization);
            executorAppendPackedPoint(positions, point);
        }
    };
    if (packedAdaptive) {
        for (const CadProgressiveGpu::PackedRange& range : packedRanges)
            appendPoints(static_cast<size_t>(range.sourceFirst) * 2u,
                static_cast<size_t>(range.sourceCount) * 2u);
    } else {
        appendPoints(pointFirst, pointCount);
    }
    resources->uploadProgressive(
        part, false, level, positions, std::vector<float>(), false,
        rangeSignature, packedRanges, glue);
    if (const CadProgressiveGpu *uploaded = resources->progressiveFor(
            part, false, level, rangeSignature))
        return uploaded;
    /* Allocation pressure may leave the preceding append-only prefix live.
     * The fixed executor clamps every range to this record's vertexCount, so
     * it remains a valid, if temporarily less complete, presentation. */
    return resources->progressiveForAny(part, false, level);
}

static const CadProgressiveGpu*
ensureProgressiveTriGpu(
        CadGpuResources *resources, PartId part, const TriMesh& mesh,
        uint8_t level,
        const std::vector<CadProgressiveGpu::PackedRange> *sourceRanges,
        bool *prepared, const SoGLContext *glue)
{
    if (prepared) *prepared = false;
    if (!resources || !glue) return nullptr;

    const size_t indexCount = mesh.indexCountAtCut(level);
    if (indexCount < 3 || indexCount > mesh.indices.size())
        return nullptr;
    const bool indexed = mesh.normals.size() == mesh.positions.size();
    uint32_t maximumIndex = 0;
    if (indexed) {
        for (size_t i = 0; i < indexCount; ++i) {
            if (mesh.indices[i] >= mesh.positions.size())
                return nullptr;
            maximumIndex = std::max(maximumIndex, mesh.indices[i]);
        }
    }
    uint64_t rangeSignature = 0;
    std::vector<CadProgressiveGpu::PackedRange> packedRanges;
    if (!indexed && sourceRanges) {
        const CadProgressiveGpu *existing =
            resources->progressiveForAny(part, true, level);
        /* Retain a conservative union for this part/cut.  Small camera
         * movements then reuse the already packed visible ranges instead of
         * rebuilding a slightly different buffer every frame.  The ordinary
         * progressive LRU still bounds inactive cuts, and a full-prefix
         * record naturally covers every later view. */
        if (existing && existing->packedRanges.empty())
            return existing;
        std::vector<CadProgressiveGpu::PackedRange> requested =
            *sourceRanges;
        if (existing)
            requested.insert(requested.end(),
                existing->packedRanges.begin(),
                existing->packedRanges.end());
        std::sort(requested.begin(), requested.end(),
            [](const CadProgressiveGpu::PackedRange& left,
               const CadProgressiveGpu::PackedRange& right) {
                if (left.sourceFirst != right.sourceFirst)
                    return left.sourceFirst < right.sourceFirst;
                return left.sourceCount < right.sourceCount;
            });
        requested.erase(std::unique(requested.begin(), requested.end(),
            [](const CadProgressiveGpu::PackedRange& left,
               const CadProgressiveGpu::PackedRange& right) {
                return left.sourceFirst == right.sourceFirst &&
                    left.sourceCount == right.sourceCount;
            }), requested.end());
        if (existing &&
                requested.size() == existing->packedRanges.size())
            return existing;
        rangeSignature = 1469598103934665603ULL;
        packedRanges.reserve(requested.size());
        uint64_t packedFirst = 0;
        for (const CadProgressiveGpu::PackedRange& source : requested) {
            const uint64_t sourceEnd =
                static_cast<uint64_t>(source.sourceFirst) +
                source.sourceCount;
            if (!source.sourceCount || source.sourceFirst % 3u ||
                    source.sourceCount % 3u || sourceEnd > indexCount ||
                    packedFirst > UINT32_MAX ||
                    source.sourceCount > UINT32_MAX - packedFirst)
                return nullptr;
            CadProgressiveGpu::PackedRange packed = source;
            packed.packedFirst = static_cast<uint32_t>(packedFirst);
            packedRanges.push_back(packed);
            packedFirst += source.sourceCount;
            rangeSignature ^= source.sourceFirst;
            rangeSignature *= 1099511628211ULL;
            rangeSignature ^= source.sourceCount;
            rangeSignature *= 1099511628211ULL;
        }
        if (packedRanges.empty()) return nullptr;
        if (!rangeSignature) rangeSignature = 1;
    }
    if (const CadProgressiveGpu *cached =
            resources->progressiveFor(
                part, true, level, rangeSignature))
        return cached;

    std::vector<float> positions;
    std::vector<float> normals;
    if (indexed) {
        positions.reserve((static_cast<size_t>(maximumIndex) + 1) * 3);
        for (size_t i = 0; i <= maximumIndex; ++i) {
            const SbVec3f point = progressiveSnapPoint(
                mesh.positions[i],
                mesh.progressiveQuantizationMinimum,
                mesh.progressiveQuantizationMaximum,
                mesh.quantizationAtCut(level));
            executorAppendPackedPoint(positions, point);
        }
    } else {
        size_t selectedIndexCount = indexCount;
        if (!packedRanges.empty()) {
            selectedIndexCount = 0;
            for (const CadProgressiveGpu::PackedRange& range : packedRanges) {
                if (range.sourceCount > SIZE_MAX - selectedIndexCount)
                    return nullptr;
                selectedIndexCount += range.sourceCount;
            }
        }
        positions.reserve(selectedIndexCount * 3);
        normals.reserve(selectedIndexCount * 3);
        const auto appendRange = [&](size_t first, size_t count) -> bool {
          for (size_t t = first; t + 2 < first + count; t += 3) {
            SbVec3f triangle[3];
            SbVec3f sourceTriangle[3];
            for (int k = 0; k < 3; ++k) {
                const uint32_t index = mesh.indices[t + k];
                if (index >= mesh.positions.size())
                    return false;
                sourceTriangle[k] = mesh.positions[index];
                triangle[k] = progressiveSnapPoint(
                    sourceTriangle[k],
                    mesh.progressiveQuantizationMinimum,
                    mesh.progressiveQuantizationMaximum,
                    mesh.quantizationAtCut(level));
            }
            SbVec3f normal =
                (sourceTriangle[1] - sourceTriangle[0]).cross(
                    sourceTriangle[2] - sourceTriangle[0]);
            if (normal.sqrLength() > 0.0f)
                normal.normalize();
            else
                normal.setValue(0.0f, 0.0f, 1.0f);
            for (int k = 0; k < 3; ++k) {
                executorAppendPackedPoint(positions, triangle[k]);
                executorAppendPackedPoint(normals, normal);
            }
          }
          return true;
        };
        if (packedRanges.empty()) {
            if (!appendRange(0, indexCount)) return nullptr;
        } else {
            for (const CadProgressiveGpu::PackedRange& range : packedRanges)
                if (!appendRange(range.sourceFirst, range.sourceCount))
                    return nullptr;
        }
    }
    if (prepared) *prepared = true;
    resources->uploadProgressive(
        part, true, level, positions, normals, indexed, rangeSignature,
        packedRanges, glue);
    return resources->progressiveFor(
        part, true, level, rangeSignature);
}

/* OSMesa's fixed-function line path is materially faster than submitting
 * triangle polygons in line mode or issuing every edge through immediate
 * mode.  Retain one snapped copy of the indexed mesh positions for each PoP
 * cut and reuse the ordinary compact edge-index buffer.  Unlike the legacy
 * wire representation this is O(vertices), not six copied positions per
 * triangle, and an unchanged frame is a cache lookup. */
static const CadProgressiveGpu*
ensureProgressiveIndexedWireGpu(
        CadGpuResources *resources, PartId part, const TriMesh& mesh,
        uint8_t level, bool *prepared, const SoGLContext *glue)
{
    if (prepared) *prepared = false;
    if (!resources || !glue || !mesh.isProgressive()) return nullptr;

    const size_t positionCount = mesh.positionCountAtCut(level);
    if (!positionCount || positionCount > mesh.positions.size() ||
            positionCount > static_cast<size_t>(
                std::numeric_limits<GLsizei>::max()))
        return nullptr;

    uint64_t signature = 1469598103934665603ULL;
    const auto mix = [&signature](uint64_t value) {
        signature ^= value;
        signature *= 1099511628211ULL;
    };
    const ProgressiveQuantization quantization =
        mesh.quantizationAtCut(level);
    mix(0x696e646578656477ULL); /* "indexedw" representation tag. */
    mix(mesh.progressiveLineage);
    mix(positionCount);
    mix(static_cast<uint64_t>(quantization.xBits) |
        (static_cast<uint64_t>(quantization.yBits) << 8u) |
        (static_cast<uint64_t>(quantization.zBits) << 16u));
    if (!signature) signature = 1;
    if (const CadProgressiveGpu *cached = resources->progressiveFor(
            part, false, level, signature))
        return cached;

    std::vector<float> positions;
    positions.reserve(positionCount * 3u);
    for (size_t index = 0; index < positionCount; ++index)
        executorAppendPackedPoint(positions, progressiveSnapPoint(
            mesh.positions[index], mesh.progressiveQuantizationMinimum,
            mesh.progressiveQuantizationMaximum, quantization));
    if (prepared) *prepared = true;
    resources->uploadProgressive(
        part, false, level, positions, std::vector<float>(), true,
        signature, std::vector<CadProgressiveGpu::PackedRange>(), glue);
    return resources->progressiveFor(part, false, level, signature);
}

struct CadWireDrawRange {
    uint32_t firstSegment = 0;
    uint32_t segmentCount = 0;
};

// Bind a wire VBO once and submit the selected private-page ranges as one
// multi-draw when available.  Chunks are residency units, not draw objects.
static void bindAndDrawWireRanges(
        const CadWireGpu* w, const SoGLContext* glue, GLint locPos,
        const std::vector<CadWireDrawRange>& requestedRanges)
{
    if (!w || w->segCount == 0 || requestedRanges.empty()) return;

    std::vector<GLint> firsts;
    std::vector<GLsizei> counts;
    std::vector<const GLvoid *> offsets;
    firsts.reserve(requestedRanges.size());
    counts.reserve(requestedRanges.size());
    offsets.reserve(requestedRanges.size());
    for (const CadWireDrawRange& range : requestedRanges) {
        if (!range.segmentCount || range.firstSegment >=
                static_cast<uint32_t>(w->segCount))
            continue;
        const GLsizei bounded = static_cast<GLsizei>(std::min<uint64_t>(
            range.segmentCount,
            static_cast<uint64_t>(w->segCount) - range.firstSegment));
        if (w->sequentialSegments) {
            firsts.push_back(static_cast<GLint>(range.firstSegment * 2u));
            counts.push_back(bounded * 2);
        } else {
            counts.push_back(bounded * 2);
            offsets.push_back(reinterpret_cast<const GLvoid *>(
                static_cast<uintptr_t>(range.firstSegment) * 2u *
                sizeof(uint32_t)));
        }
    }
    if (counts.empty()) return;

    if (w->vao && glue->glBindVertexArray) {
        glue->glBindVertexArray(w->vao);
    } else {
        glue->glBindBuffer(GL_ARRAY_BUFFER, w->posBuf);
        glue->glVertexAttribPointerARB(static_cast<GLuint>(locPos), 3,
                                       GL_FLOAT, GL_FALSE,
                                       3 * sizeof(float), nullptr);
        glue->glEnableVertexAttribArrayARB(static_cast<GLuint>(locPos));
        if (!w->sequentialSegments)
            glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, w->segIdxBuf);
    }

    if (w->sequentialSegments && counts.size() > 1 &&
            glue->glMultiDrawArrays) {
        SoGLContext_glMultiDrawArrays(glue, GL_LINES, firsts.data(),
            counts.data(), static_cast<GLsizei>(counts.size()));
    } else if (!w->sequentialSegments && counts.size() > 1 &&
            glue->glMultiDrawElements) {
        SoGLContext_glMultiDrawElements(glue, GL_LINES, counts.data(),
            GL_UNSIGNED_INT, offsets.data(),
            static_cast<GLsizei>(counts.size()));
    } else if (w->sequentialSegments) {
        for (size_t i = 0; i < counts.size(); ++i)
            glue->glDrawArrays(GL_LINES, firsts[i], counts[i]);
    } else {
        for (size_t i = 0; i < counts.size(); ++i)
            glue->glDrawElements(GL_LINES, counts[i], GL_UNSIGNED_INT,
                offsets[i]);
    }
    {
        GLenum err = glue->glGetError();
        if (err != GL_NO_ERROR)
            std::fprintf(stderr, "CadRendererGL: glDrawElements error: 0x%x\n", err);
    }

    if (w->vao && glue->glBindVertexArray) {
        glue->glBindVertexArray(0);
    } else {
        glue->glDisableVertexAttribArrayARB(static_cast<GLuint>(locPos));
        glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
        if (!w->sequentialSegments)
            glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
}

static void bindAndDrawWireRangesFixed(
        const CadWireGpu* wire, GLuint positionBuffer,
        const SoGLContext* glue,
        const std::vector<CadWireDrawRange>& requestedRanges)
{
    if (!wire || !positionBuffer || !wire->segIdxBuf ||
            wire->sequentialSegments || wire->segCount == 0 ||
            requestedRanges.empty())
        return;

    std::vector<GLsizei> counts;
    std::vector<const GLvoid *> offsets;
    counts.reserve(requestedRanges.size());
    offsets.reserve(requestedRanges.size());
    for (const CadWireDrawRange& range : requestedRanges) {
        if (!range.segmentCount || range.firstSegment >=
                static_cast<uint32_t>(wire->segCount))
            continue;
        const GLsizei bounded = static_cast<GLsizei>(std::min<uint64_t>(
            range.segmentCount,
            static_cast<uint64_t>(wire->segCount) - range.firstSegment));
        counts.push_back(bounded * 2);
        offsets.push_back(reinterpret_cast<const GLvoid *>(
            static_cast<uintptr_t>(range.firstSegment) * 2u *
            sizeof(uint32_t)));
    }
    if (counts.empty()) return;

    glue->glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
    glue->glVertexPointer(3, GL_FLOAT, 3 * sizeof(float), nullptr);
    glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wire->segIdxBuf);
    if (counts.size() > 1 && glue->glMultiDrawElements)
        SoGLContext_glMultiDrawElements(glue, GL_LINES, counts.data(),
            GL_UNSIGNED_INT, offsets.data(),
            static_cast<GLsizei>(counts.size()));
    else
        for (size_t index = 0; index < counts.size(); ++index)
            glue->glDrawElements(GL_LINES, counts[index], GL_UNSIGNED_INT,
                offsets[index]);
}

// Bind a tri VBO, set up attributes 0 (position) and 1 (normal), draw.
struct CadTriangleDrawRange {
    uint32_t firstIndex = 0;
    uint32_t indexCount = 0;
};

static void cadAccumulateRenderedWireWork(
    Obol::CadRenderedWork&, uint64_t, uint64_t);

static uint8_t
executorMaximumRequestedCut(const CadFramePlan& plan,
                            const SoCADAssembly& assembly, PartId part)
{
    const auto found = plan.maximumRequestedCutByPart.find(part);
    return found == plan.maximumRequestedCutByPart.end() ?
        Obol::ProgressiveCutUnspecified :
        assembly.effectiveProgressiveCut(found->second);
}

static void bindAndDrawTriRanges(
                            const CadTriGpu* t, const SoGLContext* glue,
                            GLint locPos, GLint locNorm, bool& hasNorm,
                            const std::vector<CadTriangleDrawRange>& ranges)
{
    if (!t || t->idxCount == 0 || ranges.empty()) return;

    hasNorm = (t->normBuf != 0);

    if (t->vao && glue->glBindVertexArray) {
        glue->glBindVertexArray(t->vao);
    } else {
        glue->glBindBuffer(GL_ARRAY_BUFFER, t->posBuf);
        glue->glVertexAttribPointerARB(static_cast<GLuint>(locPos), 3,
                                       GL_FLOAT, GL_FALSE,
                                       3 * sizeof(float), nullptr);
        glue->glEnableVertexAttribArrayARB(static_cast<GLuint>(locPos));

        if (hasNorm && locNorm >= 0) {
            glue->glBindBuffer(GL_ARRAY_BUFFER, t->normBuf);
            glue->glVertexAttribPointerARB(static_cast<GLuint>(locNorm), 3,
                                           GL_FLOAT, GL_FALSE,
                                           3 * sizeof(float), nullptr);
            glue->glEnableVertexAttribArrayARB(static_cast<GLuint>(locNorm));
        }
        glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, t->idxBuf);
    }

    std::vector<GLsizei> counts;
    std::vector<const GLvoid *> offsets;
    counts.reserve(ranges.size());
    offsets.reserve(ranges.size());
    for (const CadTriangleDrawRange& range : ranges) {
        if (!range.indexCount || range.firstIndex >=
                static_cast<uint32_t>(t->idxCount))
            continue;
        const GLsizei count = static_cast<GLsizei>(std::min<uint64_t>(
            range.indexCount,
            static_cast<uint64_t>(t->idxCount) - range.firstIndex));
        counts.push_back(count);
        offsets.push_back(reinterpret_cast<const GLvoid *>(
            static_cast<uintptr_t>(range.firstIndex) * sizeof(uint32_t)));
    }
    if (counts.size() > 1 && glue->glMultiDrawElements)
        SoGLContext_glMultiDrawElements(glue, GL_TRIANGLES, counts.data(),
            GL_UNSIGNED_INT, offsets.data(),
            static_cast<GLsizei>(counts.size()));
    else
        for (size_t i = 0; i < counts.size(); ++i)
            glue->glDrawElements(GL_TRIANGLES, counts[i], GL_UNSIGNED_INT,
                offsets[i]);

    if (t->vao && glue->glBindVertexArray) {
        glue->glBindVertexArray(0);
    } else {
        if (hasNorm && locNorm >= 0) {
            glue->glDisableVertexAttribArrayARB(static_cast<GLuint>(locNorm));
        }
        glue->glDisableVertexAttribArrayARB(static_cast<GLuint>(locPos));
        glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
        glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    }
}

bool CadRendererGL::renderIndexedTriangleWire(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext* glue,
        const SbMatrix& viewProj,
        const SbMatrix& viewMatrix,
        const SbMatrix& projectionMatrix)
{
    bool haveWork = false;
    for (const CadDrawItem& item : plan.wireItems) {
        if (item.rep.type == CadRepType::Triangles) {
            haveWork = true;
            break;
        }
    }
    if (!haveWork)
        return true;

    size_t deadlineWork = 256u;
    if (renderInterruptedAfter(deadlineWork))
        return false;

    for (const CadDrawItem& item : plan.wireItems) {
        if (item.rep.type != CadRepType::Triangles ||
                item.partIndex >= plan.partBindings.size())
            continue;
        const CadPartBinding& binding = plan.partBindings[item.partIndex];
        ensurePartUploaded(item.rep.part, assembly, binding.generation,
            executorMaximumRequestedCut(
                plan, assembly, item.rep.part), glue);
    }

    const ExecutorFrustumPlanes fp = extractExecutorFrustumPlanes(viewProj);
    GLint savedPolygonMode[2] = {GL_FILL, GL_FILL};
    glue->glGetIntegerv(GL_POLYGON_MODE, savedPolygonMode);
    glue->glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    const CadWireRasterState rasterState = captureWireRasterState(
        glue, caps_.hasLineStipple);
    bool interrupted = false;

    const bool shaderPath = caps_.canUseVbo() && shaders_.wire &&
        (!caps_.isSoftwareRenderer || softwareGlslRequested());
    GLint viewProjectionLocation[2] = {-1, -1};
    GLint modelLocation[2] = {-1, -1};
    GLint colorLocation[2] = {-1, -1};
    GLint encodeScaleLocation = -1;
    GLint decodeScaleLocation = -1;
    GLint minimumLocation = -1;
    const GLuint programs[2] = {shaders_.wire, shaders_.wirePop};
    GLboolean savedLighting = GL_FALSE;
    if (shaderPath) {
        for (int variant = 0; variant < 2; ++variant) {
            if (!programs[variant])
                continue;
            viewProjectionLocation[variant] =
                glue->glGetUniformLocationARB(
                    programs[variant], "u_viewProj");
            modelLocation[variant] = glue->glGetUniformLocationARB(
                programs[variant], "u_model");
            colorLocation[variant] = glue->glGetUniformLocationARB(
                programs[variant], "u_color");
        }
        if (programs[1]) {
            encodeScaleLocation = glue->glGetUniformLocationARB(
                programs[1], "u_popEncodeScale");
            decodeScaleLocation = glue->glGetUniformLocationARB(
                programs[1], "u_popDecodeScale");
            minimumLocation = glue->glGetUniformLocationARB(
                programs[1], "u_popMin");
        }
    } else {
        glue->glMatrixMode(GL_PROJECTION);
        glue->glPushMatrix();
        glue->glLoadMatrixf(projectionMatrix[0]);
        glue->glMatrixMode(GL_MODELVIEW);
        glue->glPushMatrix();
        glue->glLoadMatrixf(viewMatrix[0]);
        savedLighting = glue->glIsEnabled(GL_LIGHTING);
        glue->glDisable(GL_LIGHTING);
        if (caps_.canUseFixedVbo())
            glue->glEnableClientState(GL_VERTEX_ARRAY);
    }

    GLuint activeProgram = 0;
    for (const CadDrawItem& item : plan.wireItems) {
        if (item.rep.type != CadRepType::Triangles ||
                item.partIndex >= plan.partBindings.size())
            continue;
        if (renderInterruptedAfter(deadlineWork)) {
            interrupted = true;
            break;
        }
        const CadPartBinding& binding = plan.partBindings[item.partIndex];
        const PartGeometry *geometry = binding.geometry.get();
        const WireRep *wire = geometry && geometry->wire ?
            &*geometry->wire : nullptr;
        const TriMesh *mesh = wire && wire->derivesTriangleEdges() ?
            wire->triangleEdges.get() : nullptr;
        const CadTriGpu *tri = gpuRes_->triFor(item.rep.part);
        const CadWireGpu *derivedWire = caps_.isSoftwareRenderer ?
            gpuRes_->wireFor(item.rep.part) : nullptr;
        if (!mesh || (!tri && !derivedWire))
            continue;

        for (uint32_t occurrence = 0;
                occurrence < item.instanceCount; ++occurrence) {
            if (renderInterruptedAfter(deadlineWork)) {
                interrupted = true;
                break;
            }
            const size_t visibleIndex = item.baseInstance + occurrence;
            if (!cadInstanceDrawable(
                    plan, item, visibleIndex, CadDrawChannel::Wire))
                continue;
            const CadVisibleInstance& instance =
                plan.visibleInstances[visibleIndex];
            if (isBoxOutsideExecutorFrustum(
                    instance.wbMin, instance.wbMax, fp))
                continue;

            const uint8_t level = mesh->isProgressive() ?
                executorVisibleProgressiveCut(
                    *mesh, instance, fp,
                    assembly.effectiveProgressiveCut(instance.lodCut)) :
                Obol::ProgressiveCutUnspecified;
            std::vector<CadTriangleDrawRange> ranges;
            if (mesh->hasAdaptiveProgressiveClusters()) {
                SbMatrix model;
                model.setValue(instance.transform.data());
                ranges.reserve(mesh->progressiveClusters.size());
                for (const ProgressiveTriangleCluster& cluster :
                        mesh->progressiveClusters) {
                    float minimum[3];
                    float maximum[3];
                    executorTransformedBox(
                        cluster.bounds, model, minimum, maximum);
                    if (isBoxOutsideExecutorFrustum(minimum, maximum, fp))
                        continue;
                    CadTriangleDrawRange active;
                    for (const ProgressiveTriangleClusterRange& range :
                            cluster.ranges) {
                        if (range.activationCut > level)
                            break;
                        if (!active.indexCount) {
                            active.firstIndex = range.firstIndex;
                            active.indexCount = range.indexCount;
                        } else if (static_cast<uint64_t>(active.firstIndex) +
                                active.indexCount == range.firstIndex) {
                            active.indexCount = range.indexCount >
                                    UINT32_MAX - active.indexCount ?
                                UINT32_MAX :
                                active.indexCount + range.indexCount;
                        } else {
                            ranges.push_back(active);
                            active.firstIndex = range.firstIndex;
                            active.indexCount = range.indexCount;
                        }
                    }
                    if (active.indexCount)
                        ranges.push_back(active);
                }
            } else {
                const size_t count = mesh->isProgressive() ?
                    mesh->indexCountAtCut(level) : mesh->indices.size();
                if (count)
                    ranges.push_back({0u, static_cast<uint32_t>(
                        std::min<size_t>(count, UINT32_MAX))});
            }

            applyWireRasterStyle(glue, instance, caps_.hasLineStipple);
            uint64_t submittedIndices = 0;
            for (const CadTriangleDrawRange& range : ranges)
                submittedIndices = cadSaturatingWorkAdd(
                    submittedIndices, range.indexCount);

            if (shaderPath) {
                const int variant = mesh->isProgressive() &&
                        !mesh->quantizationAtCut(level).isExact() &&
                        programs[1] ? 1 : 0;
                if (activeProgram != programs[variant]) {
                    activeProgram = programs[variant];
                    glue->glUseProgramObjectARB(activeProgram);
                    glue->glUniformMatrix4fvARB(
                        viewProjectionLocation[variant], 1, GL_FALSE,
                        viewProj[0]);
                }
                glue->glUniformMatrix4fvARB(
                    modelLocation[variant], 1, GL_FALSE,
                    instance.transform.data());
                const float rgba[4] = {
                    instance.rgba[0] / 255.0f,
                    instance.rgba[1] / 255.0f,
                    instance.rgba[2] / 255.0f,
                    instance.rgba[3] / 255.0f
                };
                glue->glUniform4fvARB(colorLocation[variant], 1, rgba);
                if (variant) {
                    uploadProgressivePositionUniforms(
                        glue, encodeScaleLocation, decodeScaleLocation,
                        minimumLocation, mesh->quantizationAtCut(level),
                        mesh->progressiveQuantizationMinimum,
                        mesh->progressiveQuantizationMaximum);
                }
                if (derivedWire) {
                    std::vector<CadWireDrawRange> wireRanges;
                    wireRanges.reserve(ranges.size());
                    for (const CadTriangleDrawRange& range : ranges)
                        wireRanges.push_back({
                            range.firstIndex, range.indexCount});
                    bindAndDrawWireRanges(
                        derivedWire, glue, 0, wireRanges);
                } else {
                    bool hasNormals = false;
                    bindAndDrawTriRanges(
                        tri, glue, 0, -1, hasNormals, ranges);
                }
            } else {
                SbMatrix model;
                model.setValue(instance.transform.data());
                SbMatrix modelView = model;
                modelView.multRight(viewMatrix);
                glue->glLoadMatrixf(modelView[0]);
                glue->glColor4ub(instance.rgba[0], instance.rgba[1],
                    instance.rgba[2], instance.rgba[3]);
                const ProgressiveQuantization quantization =
                    mesh->isProgressive() ? mesh->quantizationAtCut(level) :
                        ProgressiveQuantization();
                bool fixedVboRendered = false;
                if (derivedWire && caps_.canUseFixedVbo()) {
                    GLuint positionBuffer = derivedWire->posBuf;
                    if (mesh->isProgressive() &&
                            !quantization.isExact()) {
                        bool prepared = false;
                        const CadProgressiveGpu *cut =
                            ensureProgressiveIndexedWireGpu(
                                gpuRes_, item.rep.part, *mesh, level,
                                &prepared, glue);
                        if (prepared)
                            noteRenderPreparation(
                                "indexed-wire-progressive-cut");
                        if (cut)
                            positionBuffer = cut->posBuf;
                        else
                            positionBuffer = 0;
                    }
                    if (positionBuffer) {
                        std::vector<CadWireDrawRange> wireRanges;
                        wireRanges.reserve(ranges.size());
                        for (const CadTriangleDrawRange& range : ranges)
                            wireRanges.push_back({
                                range.firstIndex, range.indexCount});
                        bindAndDrawWireRangesFixed(
                            derivedWire, positionBuffer, glue, wireRanges);
                        fixedVboRendered = true;
                    }
                }
                if (!fixedVboRendered) {
                    for (const CadTriangleDrawRange& range : ranges) {
                        const size_t end = std::min<size_t>(
                            mesh->indices.size(),
                            static_cast<size_t>(range.firstIndex) +
                                range.indexCount);
                        size_t index = range.firstIndex;
                        while (index + 2 < end) {
                            const size_t chunkEnd = std::min(
                                end, index + 256u * 3u);
                            if (renderInterruptedAfter(
                                    deadlineWork,
                                    (chunkEnd - index) / 3u)) {
                                interrupted = true;
                                break;
                            }
                            glue->glBegin(GL_LINES);
                            for (; index + 2 < chunkEnd; index += 3) {
                                const uint32_t source[3] = {
                                    mesh->indices[index],
                                    mesh->indices[index + 1],
                                    mesh->indices[index + 2]
                                };
                                if (source[0] >= mesh->positions.size() ||
                                        source[1] >=
                                            mesh->positions.size() ||
                                        source[2] >=
                                            mesh->positions.size()) {
                                    interrupted = true;
                                    break;
                                }
                                SbVec3f point[3];
                                for (int corner = 0; corner < 3; ++corner)
                                    point[corner] = mesh->isProgressive() ?
                                        progressiveSnapPoint(
                                            mesh->positions[source[corner]],
                                            mesh->
                                                progressiveQuantizationMinimum,
                                            mesh->
                                                progressiveQuantizationMaximum,
                                            quantization) :
                                        mesh->positions[source[corner]];
                                for (int edge = 0; edge < 3; ++edge) {
                                    const SbVec3f& first = point[edge];
                                    const SbVec3f& second =
                                        point[(edge + 1) % 3];
                                    glue->glVertex3f(
                                        first[0], first[1], first[2]);
                                    glue->glVertex3f(
                                        second[0], second[1], second[2]);
                                }
                            }
                            glue->glEnd();
                            if (interrupted)
                                break;
                        }
                        if (interrupted)
                            break;
                    }
                }
            }
            cadAccumulateRenderedWireWork(
                lastRenderedWork_, submittedIndices, 1);
            if (interrupted)
                break;
        }
        if (interrupted)
            break;
    }

    if (shaderPath)
        glue->glUseProgramObjectARB(0);
    else {
        if (caps_.canUseFixedVbo())
            glue->glDisableClientState(GL_VERTEX_ARRAY);
        if (savedLighting)
            glue->glEnable(GL_LIGHTING);
        glue->glMatrixMode(GL_MODELVIEW);
        glue->glPopMatrix();
        glue->glMatrixMode(GL_PROJECTION);
        glue->glPopMatrix();
        glue->glMatrixMode(GL_MODELVIEW);
    }
    restoreWireRasterState(glue, rasterState, caps_.hasLineStipple);
    glue->glPolygonMode(GL_FRONT, static_cast<GLenum>(savedPolygonMode[0]));
    glue->glPolygonMode(GL_BACK, static_cast<GLenum>(savedPolygonMode[1]));
    return !interrupted;
}

static void
cadAccumulateRenderedShadedWork(Obol::CadRenderedWork& work,
                                const Obol::TriMesh& mesh,
                                uint8_t level, uint64_t triangles,
                                uint64_t occurrences = 1,
                                uint64_t visiblePositions = UINT64_MAX)
{
    if (!triangles || !occurrences)
        return;
    const uint64_t positions = visiblePositions == UINT64_MAX ?
        static_cast<uint64_t>(mesh.positionCountAtCut(level)) :
        visiblePositions;
    const uint64_t submittedTriangles =
        triangles > UINT64_MAX / occurrences ? UINT64_MAX :
            triangles * occurrences;
    const uint64_t submittedPositions =
        positions > UINT64_MAX / occurrences ? UINT64_MAX :
            positions * occurrences;
    work.triangleCount = cadSaturatingWorkAdd(
        work.triangleCount, submittedTriangles);
    work.positionCount = cadSaturatingWorkAdd(
        work.positionCount, submittedPositions);
    if (!mesh.normals.empty())
        work.normalCount = cadSaturatingWorkAdd(
            work.normalCount, submittedPositions);
    work.occurrenceCount = cadSaturatingWorkAdd(
        work.occurrenceCount, occurrences);
}

static void
cadAccumulateRenderedWireWork(Obol::CadRenderedWork& work,
                              uint64_t segments,
                              uint64_t occurrences = 1)
{
    if (!segments || !occurrences)
        return;
    const uint64_t submitted =
        segments > UINT64_MAX / occurrences ? UINT64_MAX :
            segments * occurrences;
    work.lineCount = cadSaturatingWorkAdd(work.lineCount, submitted);
    work.positionCount = cadSaturatingWorkAdd(
        work.positionCount,
        submitted > UINT64_MAX / 2 ? UINT64_MAX : submitted * 2);
    work.occurrenceCount = cadSaturatingWorkAdd(
        work.occurrenceCount, occurrences);
}

static void
cadReplaceRenderedWorkCount(uint64_t& total, uint64_t oldCount,
                            uint64_t newCount)
{
    if (total == UINT64_MAX)
        return;
    total = oldCount <= total ? total - oldCount : 0;
    total = cadSaturatingWorkAdd(total, newCount);
}

static void
cadReplacePreparedShadedWork(Obol::CadRenderedWork& work,
                             uint64_t oldTriangles,
                             uint64_t oldPositions,
                             bool oldHasNormals,
                             uint64_t newTriangles,
                             uint64_t newPositions,
                             bool newHasNormals)
{
    cadReplaceRenderedWorkCount(
        work.triangleCount, oldTriangles, newTriangles);
    cadReplaceRenderedWorkCount(
        work.positionCount, oldPositions, newPositions);
    cadReplaceRenderedWorkCount(
        work.normalCount,
        oldHasNormals ? oldPositions : 0,
        newHasNormals ? newPositions : 0);
}

void CadRendererGL::renderVboLoop(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext*   glue,
        const SbMatrix&      viewProj,
        const SbViewVolume&  viewVolume,
        bool drawWire,
        bool customWireOnly,
        bool drawShaded)
{
    size_t deadlineWork = 256u;
    uint64_t renderedTriangleCount = 0;
    bool interrupted = renderInterruptedAfter(deadlineWork);
    if (interrupted)
        return;

    // OI stores matrices row-major.  GL reads them column-major.  Passing
    // the raw float[16] with GL_FALSE means GL transposes our row-major
    // matrix into the column-major form the shader expects, which is
    // exactly the GL column-vector convention.  (Same as SoGLSLShaderParameter.)
    const float* vpData = viewProj[0];

    // a_pos=0, a_norm=1 are pinned via glBindAttribLocationARB before linking
    const GLint locPos  = 0;
    const GLint locNorm = 1;

    // Extract frustum planes for per-instance culling.
    const ExecutorFrustumPlanes fp =
        extractExecutorFrustumPlanes(viewProj);

    // --- Wire pass ---
    if (drawWire && !plan.wireItems.empty()) {
        const CadWireRasterState rasterState = captureWireRasterState(
            glue, caps_.hasLineStipple);
        struct WireLocations {
            GLint viewProjection = -1;
            GLint model = -1;
            GLint color = -1;
            GLint encodeScale = -1;
            GLint decodeScale = -1;
            GLint minimum = -1;
        };
        const GLuint programs[2] = {shaders_.wire, shaders_.wirePop};
        WireLocations locations[2];
        for (int variant = 0; variant < 2; ++variant) {
            if (!programs[variant])
                continue;
            locations[variant].viewProjection =
                glue->glGetUniformLocationARB(
                    programs[variant], "u_viewProj");
            locations[variant].model = glue->glGetUniformLocationARB(
                programs[variant], "u_model");
            locations[variant].color = glue->glGetUniformLocationARB(
                programs[variant], "u_color");
        }
        if (programs[1]) {
            locations[1].encodeScale = glue->glGetUniformLocationARB(
                programs[1], "u_popEncodeScale");
            locations[1].decodeScale = glue->glGetUniformLocationARB(
                programs[1], "u_popDecodeScale");
            locations[1].minimum = glue->glGetUniformLocationARB(
                programs[1], "u_popMin");
        }
        GLuint activeProgram = 0;

        for (const auto& item : plan.wireItems) {
            if (renderInterruptedAfter(deadlineWork)) {
                interrupted = true;
                break;
            }
            if (customWireOnly && !item.customWireStyle) continue;
            CadWireGpu* w = gpuRes_->wireFor(item.rep.part);
            if (!w) continue;
            const PartGeometry *geometry =
                assembly.partGeometry(item.rep.part);
            const WireRep *progressive =
                geometry && geometry->wire &&
                geometry->wire->isProgressive() ?
                &*geometry->wire : nullptr;

            for (uint32_t i = 0; i < item.instanceCount; ++i) {
                if (renderInterruptedAfter(deadlineWork)) {
                    interrupted = true;
                    break;
                }
                const size_t visibleIndex = item.baseInstance + i;
                if (!cadInstanceDrawable(
                        plan, item, visibleIndex, CadDrawChannel::Wire))
                    continue;
                const auto& inst = plan.visibleInstances[visibleIndex];
                if (isBoxOutsideExecutorFrustum(
                        inst.wbMin, inst.wbMax, fp))
                    continue;

		const uint8_t level = progressive ?
		    executorVisibleProgressiveCut(
			*progressive, inst, fp,
			assembly.effectiveProgressiveCut(inst.lodCut)) :
                    Obol::ProgressiveCutUnspecified;
                const int variant = progressive &&
                    !progressive->quantizationAtCut(level).isExact() ? 1 : 0;
                const WireLocations& loc = locations[variant];
                if (activeProgram != programs[variant]) {
                    activeProgram = programs[variant];
                    glue->glUseProgramObjectARB(activeProgram);
                    glue->glUniformMatrix4fvARB(
                        loc.viewProjection, 1, GL_FALSE, vpData);
                }
                glue->glUniformMatrix4fvARB(loc.model, 1, GL_FALSE,
                                            inst.transform.data());
                float rgba[4] = {
                    inst.rgba[0] / 255.0f, inst.rgba[1] / 255.0f,
                    inst.rgba[2] / 255.0f, inst.rgba[3] / 255.0f
                };
                glue->glUniform4fvARB(loc.color, 1, rgba);
                if (variant) {
                    uploadProgressivePositionUniforms(
                        glue, loc.encodeScale, loc.decodeScale, loc.minimum,
                        progressive->quantizationAtCut(level),
                        progressive->progressiveQuantizationMinimum,
                        progressive->progressiveQuantizationMaximum);
                }
                applyWireRasterStyle(glue, inst, caps_.hasLineStipple);
                uint64_t submittedSegments = 0;
                std::vector<CadWireDrawRange> wireRanges;
                if (progressive &&
                        progressive->hasAdaptiveProgressiveClusters()) {
                    SbMatrix model;
                    model.setValue(inst.transform.data());
                    wireRanges.reserve(
                        progressive->progressiveClusters.size());
                    for (const ProgressiveWireCluster& cluster :
                            progressive->progressiveClusters) {
                        float clusterMinimum[3];
                        float clusterMaximum[3];
                        executorTransformedBox(
                            cluster.bounds, model,
                            clusterMinimum, clusterMaximum);
                        if (isBoxOutsideExecutorFrustum(
                                clusterMinimum, clusterMaximum, fp))
                            continue;
                        uint32_t first = 0;
                        uint32_t count = 0;
                        for (const ProgressiveWireClusterRange& range :
                                cluster.ranges) {
                            if (range.activationCut > level)
                                break;
                            if (!count) {
                                first = range.firstSegment;
                                count = range.segmentCount;
                            } else if (static_cast<uint64_t>(first) + count ==
                                    range.firstSegment) {
                                count = range.segmentCount >
                                        UINT32_MAX - count ?
                                    UINT32_MAX : count + range.segmentCount;
                            } else {
                                wireRanges.push_back({first, count});
                                submittedSegments = cadSaturatingWorkAdd(
                                    submittedSegments, count);
                                first = range.firstSegment;
                                count = range.segmentCount;
                            }
                        }
                        if (count) {
                            wireRanges.push_back({first, count});
                            submittedSegments = cadSaturatingWorkAdd(
                                submittedSegments, count);
                        }
                    }
                } else {
                    const GLsizei segmentCount = progressiveWireSegmentCount(
                        assembly, item.rep.part, inst, w->segCount);
                    wireRanges.push_back({progressive ?
                        static_cast<uint32_t>(std::min<size_t>(
                            progressive->segmentFirstAtCut(level),
                            UINT32_MAX)) : 0u,
                        static_cast<uint32_t>((std::max)(0, segmentCount))});
                    submittedSegments = static_cast<uint64_t>(
                        (std::max)(0, segmentCount));
                }
                bindAndDrawWireRanges(w, glue, locPos, wireRanges);
                cadAccumulateRenderedWireWork(
                    lastRenderedWork_, submittedSegments);
            }
            if (interrupted)
                break;
        }

        glue->glUseProgramObjectARB(0);
        restoreWireRasterState(glue, rasterState, caps_.hasLineStipple);
    }
    if (interrupted)
        return;

    // --- Shaded pass ---
    if (drawShaded && !plan.shadedItems.empty()) {
        struct ShadedLocations {
            GLint viewProjection = -1;
            GLint model = -1;
            GLint color = -1;
            GLint hasNormal = -1;
            GLint lightVector = -1;
            GLint lightColor = -1;
            GLint encodeScale = -1;
            GLint decodeScale = -1;
            GLint minimum = -1;
        };
        enum ShadedProgramIndex {
            GenericExact = 0,
            GenericPop,
            DirectionalNormExact,
            DirectionalNormPop,
            DirectionalFaceExact,
            DirectionalFacePop,
            ShadedProgramCount
        };
        const GLuint programs[ShadedProgramCount] = {
            shaders_.shaded,
            shaders_.shadedPop,
            shaders_.shadedDirectionalNorm,
            shaders_.shadedPopDirectionalNorm,
            shaders_.shadedDirectionalFace,
            shaders_.shadedPopDirectionalFace
        };
        ShadedLocations locations[ShadedProgramCount];
        for (int programIndex = 0;
                programIndex < ShadedProgramCount; ++programIndex) {
            if (!programs[programIndex])
                continue;
            locations[programIndex].viewProjection =
                glue->glGetUniformLocationARB(
                    programs[programIndex], "u_viewProj");
            locations[programIndex].model =
                glue->glGetUniformLocationARB(
                    programs[programIndex], "u_model");
            locations[programIndex].color =
                glue->glGetUniformLocationARB(
                    programs[programIndex], "u_color");
            locations[programIndex].hasNormal =
                glue->glGetUniformLocationARB(
                    programs[programIndex], "u_hasNorm");
            locations[programIndex].lightVector =
                glue->glGetUniformLocationARB(
                    programs[programIndex], "u_lightVec");
            locations[programIndex].lightColor =
                glue->glGetUniformLocationARB(
                    programs[programIndex], "u_lightColor");
            const bool progressiveProgram =
                programIndex == GenericPop ||
                programIndex == DirectionalNormPop ||
                programIndex == DirectionalFacePop;
            if (progressiveProgram) {
                locations[programIndex].encodeScale =
                    glue->glGetUniformLocationARB(
                        programs[programIndex], "u_popEncodeScale");
                locations[programIndex].decodeScale =
                    glue->glGetUniformLocationARB(
                        programs[programIndex], "u_popDecodeScale");
                locations[programIndex].minimum =
                    glue->glGetUniformLocationARB(
                        programs[programIndex], "u_popMin");
            }
        }

        bool directionalLight = false;
        float directionalVector[3] = {
            kLightDir[0], kLightDir[1], kLightDir[2]
        };
        float directionalColor[3] = {1.0f, 1.0f, 1.0f};
        if (!this->lightsSupplied_) {
            directionalLight = true;
        } else if (this->lights_.size() == 1 &&
                   this->lights_[0].type == 0) {
            directionalLight = true;
            for (int component = 0; component < 3; ++component) {
                directionalVector[component] =
                    this->lights_[0].vec[component];
                directionalColor[component] =
                    this->lights_[0].color[component];
            }
        }
        if (directionalLight) {
            SbVec3f normalized(
                directionalVector[0], directionalVector[1],
                directionalVector[2]);
            if (normalized.normalize() == 0.0f)
                normalized.setValue(kLightDir);
            directionalVector[0] = normalized[0];
            directionalVector[1] = normalized[1];
            directionalVector[2] = normalized[2];
        }

        GLuint activeProgram = 0;
        bool programUploaded[ShadedProgramCount] = {};
        if (cadLightDebugRequested()) {
            static unsigned int uniformLocationReportCount = 0;
            if (uniformLocationReportCount++ < 4) {
                std::fprintf(stderr,
                    "CadRendererGL shaded locations="
                    "{base={vp=%d model=%d color=%d hasNorm=%d} "
                    "pop={vp=%d model=%d color=%d hasNorm=%d "
                    "encode=%d decode=%d min=%d}}\n",
                    locations[0].viewProjection, locations[0].model,
                    locations[0].color, locations[0].hasNormal,
                    locations[1].viewProjection, locations[1].model,
                    locations[1].color, locations[1].hasNormal,
                    locations[1].encodeScale, locations[1].decodeScale,
                    locations[1].minimum);
            }
        }

        for (const auto& item : plan.shadedItems) {
            if (renderInterruptedAfter(deadlineWork)) {
                interrupted = true;
                break;
            }
            const CadTriGpu* t = gpuRes_->triFor(item.rep.part);
            if (!t) continue;
            const PartGeometry *geometry =
                assembly.partGeometry(item.rep.part);
            const TriMesh *progressive =
                geometry && geometry->shaded &&
                geometry->shaded->isProgressive() ?
                &*geometry->shaded : nullptr;

            bool hasNorm = (t->normBuf != 0);
            if (cadLightDebugRequested()) {
                static unsigned int geometryReportCount = 0;
                if (geometryReportCount++ < 32) {
                    const size_t positionCount =
                        geometry && geometry->shaded ?
                        geometry->shaded->positions.size() : 0;
                    const size_t normalCount =
                        geometry && geometry->shaded ?
                        geometry->shaded->normals.size() : 0;
                    const size_t indexCount =
                        geometry && geometry->shaded ?
                        geometry->shaded->indices.size() : 0;
                    std::fprintf(stderr,
                        "CadRendererGL shaded geometry positions=%zu "
                        "normals=%zu indices=%zu gpuVerts=%d gpuIndices=%d "
                        "hasNorm=%d progressive=%d cull=%d\n",
                        positionCount, normalCount, indexCount,
                        static_cast<int>(t->vertCount),
                        static_cast<int>(t->idxCount), hasNorm ? 1 : 0,
                        progressive ? 1 : 0,
                        item.cullBackfaces ? 1 : 0);
                }
            }

            for (uint32_t i = 0; i < item.instanceCount; ++i) {
                if (renderInterruptedAfter(deadlineWork)) {
                    interrupted = true;
                    break;
                }
                const size_t instanceIndex = item.baseInstance + i;
                if (!cadInstanceDrawable(
                        plan, item, instanceIndex, CadDrawChannel::Shaded))
                    continue;
                const auto& inst = plan.visibleInstances[instanceIndex];
                if (isBoxOutsideExecutorFrustum(
                        inst.wbMin, inst.wbMax, fp))
                    continue;

		const uint8_t level = progressive ?
		    executorVisibleProgressiveCut(
			*progressive, inst, fp,
			assembly.effectiveProgressiveCut(inst.lodCut)) :
                    Obol::ProgressiveCutUnspecified;
                setCadBackfaceCulling(glue,
                    cadProgressiveCutCullSafe(
                        item.cullBackfaces, progressive, level));
                const int variant = progressive &&
                    !progressive->quantizationAtCut(level).isExact() ? 1 : 0;
                int programIndex = variant ? GenericPop : GenericExact;
                if (directionalLight) {
                    const int directionalIndex = hasNorm ?
                        (variant ? DirectionalNormPop :
                                   DirectionalNormExact) :
                        (variant ? DirectionalFacePop :
                                   DirectionalFaceExact);
                    if (programs[directionalIndex])
                        programIndex = directionalIndex;
                }
                const ShadedLocations& loc = locations[programIndex];
                if (activeProgram != programs[programIndex]) {
                    activeProgram = programs[programIndex];
                    glue->glUseProgramObjectARB(activeProgram);
                    glue->glUniformMatrix4fvARB(
                        loc.viewProjection, 1, GL_FALSE, vpData);
                    if (!programUploaded[programIndex]) {
                        if (programIndex >= DirectionalNormExact) {
                            glue->glUniform3fvARB(
                                loc.lightVector, 1, directionalVector);
                            glue->glUniform3fvARB(
                                loc.lightColor, 1, directionalColor);
                            this->uploadAmbientLight(
                                glue, activeProgram);
                            if (programIndex == DirectionalFaceExact ||
                                    programIndex == DirectionalFacePop) {
                                this->uploadViewFacing(
                                    glue, activeProgram, viewVolume);
                            }
                        } else {
                            this->uploadLights(glue, activeProgram);
                            this->uploadViewFacing(
                                glue, activeProgram, viewVolume);
                        }
                        programUploaded[programIndex] = true;
                    }
                }
                glue->glUniform1iARB(
                    loc.hasNormal, hasNorm ? 1 : 0);
                if (!hasNorm) {
                    SoGLContext_glVertexAttrib3f(
                        glue, static_cast<GLuint>(locNorm),
                        0.0f, 0.0f, 1.0f);
                }

                if (cadLightDebugRequested() && !this->lights_.empty() &&
                        std::fabs(this->lights_[0].vec[1]) > 0.9f &&
                        geometry && geometry->shaded &&
                        geometry->shaded->indices.size() >= 3) {
                    static unsigned int normalReportCount = 0;
                    if (normalReportCount++ < 32) {
                        const TriMesh& mesh = *geometry->shaded;
                        const uint32_t ia = mesh.indices[0];
                        const uint32_t ib = mesh.indices[1];
                        const uint32_t ic = mesh.indices[2];
                        if (ia < mesh.positions.size() &&
                                ib < mesh.positions.size() &&
                                ic < mesh.positions.size()) {
                            SbVec3f face =
                                (mesh.positions[ib] - mesh.positions[ia]).cross(
                                    mesh.positions[ic] - mesh.positions[ia]);
                            if (face.sqrLength() > 0.0f)
                                face.normalize();
                            const SbVec3f normal =
                                ia < mesh.normals.size() ?
                                mesh.normals[ia] : face;
                            const float *m = inst.transform.data();
                            const auto transformDirection =
                                [m](const SbVec3f& direction) {
                                    SbVec3f transformed(
                                        m[0] * direction[0] +
                                            m[4] * direction[1] +
                                            m[8] * direction[2],
                                        m[1] * direction[0] +
                                            m[5] * direction[1] +
                                            m[9] * direction[2],
                                        m[2] * direction[0] +
                                            m[6] * direction[1] +
                                            m[10] * direction[2]);
                                    if (transformed.sqrLength() > 0.0f)
                                        transformed.normalize();
                                    return transformed;
                                };
                            const SbVec3f worldFace =
                                transformDirection(face);
                            const SbVec3f worldNormal =
                                transformDirection(normal);
                            SbVec3f light(
                                this->lights_[0].vec[0],
                                this->lights_[0].vec[1],
                                this->lights_[0].vec[2]);
                            if (light.sqrLength() > 0.0f)
                                light.normalize();
                            const double determinant =
                                static_cast<double>(m[0]) *
                                    (static_cast<double>(m[5]) * m[10] -
                                     static_cast<double>(m[6]) * m[9]) -
                                static_cast<double>(m[4]) *
                                    (static_cast<double>(m[1]) * m[10] -
                                     static_cast<double>(m[2]) * m[9]) +
                                static_cast<double>(m[8]) *
                                    (static_cast<double>(m[1]) * m[6] -
                                     static_cast<double>(m[2]) * m[5]);
                            std::fprintf(stderr,
                                "CadRendererGL normal diagnostic "
                                "indices=(%u,%u,%u) det=%.9g "
                                "localFace=(%.6g,%.6g,%.6g) "
                                "localNormal=(%.6g,%.6g,%.6g) "
                                "worldFace=(%.6g,%.6g,%.6g) "
                                "worldNormal=(%.6g,%.6g,%.6g) "
                                "faceDot=%.6g normalDot=%.6g align=%.6g\n",
                                ia, ib, ic, determinant,
                                face[0], face[1], face[2],
                                normal[0], normal[1], normal[2],
                                worldFace[0], worldFace[1], worldFace[2],
                                worldNormal[0], worldNormal[1],
                                worldNormal[2], worldFace.dot(light),
                                worldNormal.dot(light),
                                worldFace.dot(worldNormal));
                        }
                    }
                }

                glue->glUniformMatrix4fvARB(loc.model, 1, GL_FALSE,
                                            inst.transform.data());
                float rgba[4] = {
                    inst.rgba[0] / 255.0f, inst.rgba[1] / 255.0f,
                    inst.rgba[2] / 255.0f, inst.rgba[3] / 255.0f
                };
                glue->glUniform4fvARB(loc.color, 1, rgba);
                if (variant) {
                    uploadProgressivePositionUniforms(
                        glue, loc.encodeScale, loc.decodeScale, loc.minimum,
                        progressive->quantizationAtCut(level),
                        progressive->progressiveQuantizationMinimum,
                        progressive->progressiveQuantizationMaximum);
                }
                if (cadLightDebugRequested()) {
                    static unsigned int uniformValueReportCount = 0;
                    if (uniformValueReportCount++ < 8) {
                        typedef void (APIENTRY * GetUniformivProc)(
                            GLuint, GLint, GLint *);
                        GetUniformivProc getUniform =
                            reinterpret_cast<GetUniformivProc>(
                                SoGLContext_getprocaddress(
                                    glue, "glGetUniformivARB"));
                        GLint storedHasNorm = -999;
                        if (getUniform) {
                            getUniform(activeProgram, loc.hasNormal,
                                       &storedHasNorm);
                        }
                        std::fprintf(stderr,
                            "CadRendererGL shaded uniform values "
                            "hasNorm=%d popActive=%d level=%u getProc=%d\n",
                            storedHasNorm, variant,
                            static_cast<unsigned int>(level),
                            getUniform ? 1 : 0);
                    }
                }

                std::vector<CadTriangleDrawRange> drawRanges;
                if (progressive &&
                        progressive->hasAdaptiveProgressiveClusters()) {
                    SbMatrix model;
                    model.setValue(inst.transform.data());
                    drawRanges.reserve(
                        progressive->progressiveClusters.size());
                    for (const ProgressiveTriangleCluster& cluster :
                            progressive->progressiveClusters) {
                        float clusterMinimum[3];
                        float clusterMaximum[3];
                        executorTransformedBox(
                            cluster.bounds, model,
                            clusterMinimum, clusterMaximum);
                        if (isBoxOutsideExecutorFrustum(
                                clusterMinimum, clusterMaximum, fp))
                            continue;
                        CadTriangleDrawRange active;
                        for (const ProgressiveTriangleClusterRange& range :
                                cluster.ranges) {
                            if (range.activationCut > level)
                                break;
                            if (!active.indexCount) {
                                active.firstIndex = range.firstIndex;
                                active.indexCount = range.indexCount;
                            } else if (static_cast<uint64_t>(
                                    active.firstIndex) +
                                    active.indexCount == range.firstIndex) {
                                active.indexCount = range.indexCount >
                                        UINT32_MAX - active.indexCount ?
                                    UINT32_MAX :
                                    active.indexCount + range.indexCount;
                            } else {
                                drawRanges.push_back(active);
                                active.firstIndex = range.firstIndex;
                                active.indexCount = range.indexCount;
                            }
                        }
                        if (active.indexCount)
                            drawRanges.push_back(active);
                    }
                } else {
                    const GLsizei indexCount = std::min(
                        progressiveTriangleIndexCount(
                            assembly, item.rep.part, inst, t->idxCount),
                        t->idxCount);
                    if (indexCount > 0)
                        drawRanges.push_back({
                            0u, static_cast<uint32_t>(indexCount)});
                }
                bindAndDrawTriRanges(
                    t, glue, locPos, locNorm, hasNorm, drawRanges);
                uint64_t submittedIndices = 0;
                for (const CadTriangleDrawRange& range : drawRanges)
                    submittedIndices = cadSaturatingWorkAdd(
                        submittedIndices, range.indexCount);
                const uint64_t submittedTriangles =
                    submittedIndices / 3;
                renderedTriangleCount =
                    renderedTriangleCount >
                            UINT64_MAX -
                                submittedTriangles ?
                        UINT64_MAX :
                        renderedTriangleCount +
                            submittedTriangles;
                if (geometry && geometry->shaded)
                    cadAccumulateRenderedShadedWork(
                        lastRenderedWork_, *geometry->shaded,
                        level, submittedTriangles, 1, submittedIndices);
            }
            if (interrupted)
                break;
        }

        glue->glUseProgramObjectARB(0);
    }
    lastRenderedTriangleCount_ =
        renderedTriangleCount > UINT64_MAX - lastRenderedTriangleCount_ ?
            UINT64_MAX : lastRenderedTriangleCount_ + renderedTriangleCount;
}

// ---------------------------------------------------------------------------
// Tier-1 compatibility path: retained VBOs with fixed-function arrays
// ---------------------------------------------------------------------------

void CadRendererGL::renderFixedVboLoop(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext* glue,
        const SbMatrix& viewProj,
        const SbMatrix& viewMatrix,
        const SbMatrix& projectionMatrix,
        bool drawWire,
        bool drawShaded)
{
    size_t deadlineWork = 256u;
    uint64_t renderedTriangleCount = 0;
    bool interrupted = renderInterruptedAfter(deadlineWork);
    if (interrupted)
        return;

    glue->glMatrixMode(GL_PROJECTION);
    glue->glPushMatrix();
    glue->glLoadMatrixf(projectionMatrix[0]);
    glue->glMatrixMode(GL_MODELVIEW);
    glue->glPushMatrix();
    glue->glLoadMatrixf(viewMatrix[0]);
    if (caps_.isSoftwareRenderer)
        this->uploadFixedLights(glue);

    const GLboolean wasLighting = glue->glIsEnabled(GL_LIGHTING);
    glue->glDisable(GL_LIGHTING);
    glue->glEnableClientState(GL_VERTEX_ARRAY);
    const ExecutorFrustumPlanes fp =
        extractExecutorFrustumPlanes(viewProj);
    const CadWireRasterState rasterState = captureWireRasterState(
        glue, caps_.hasLineStipple);

    if (drawWire) for (const auto& item : plan.wireItems) {
        if (renderInterruptedAfter(deadlineWork)) {
            interrupted = true;
            break;
        }
        const CadWireGpu *wire = gpuRes_->wireFor(item.rep.part);
        if (!wire) continue;
        const PartGeometry *geometry = assembly.partGeometry(item.rep.part);
        const WireRep *progressive =
            geometry && geometry->wire && geometry->wire->isProgressive() ?
            &*geometry->wire : nullptr;
        if (!wire->sequentialSegments)
            glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wire->segIdxBuf);

        /* The fixed-function renderer must materialize snapped positions on
         * the CPU, but an adaptive mesh generally needs only the clusters
         * intersecting the current view.  Build one deterministic per-cut
         * union for every occurrence sharing this part.  Uploading the whole
         * resident Lucy wire prefix for each zoom cut consumed 54 MB per cut,
         * thrashed the bounded derived-buffer cache, and starved the quiet
         * handoff despite only a few visible clusters. */
        std::vector<std::vector<CadProgressiveGpu::PackedRange>>
            visibleWireRanges(progressive ?
                progressive->progressiveCuts.size() : 0);
        std::vector<std::unordered_set<uint64_t>> visibleWireRangeKeys(
            visibleWireRanges.size());
        if (progressive && progressive->hasAdaptiveProgressiveClusters()) {
            for (uint32_t i = 0; i < item.instanceCount; ++i) {
                if (renderInterruptedAfter(deadlineWork)) {
                    interrupted = true;
                    break;
                }
                const size_t visibleIndex = item.baseInstance + i;
                if (!cadInstanceDrawable(
                        plan, item, visibleIndex, CadDrawChannel::Wire))
                    continue;
                const auto& inst = plan.visibleInstances[visibleIndex];
                if (isBoxOutsideExecutorFrustum(
                        inst.wbMin, inst.wbMax, fp))
                    continue;
                const uint8_t level = executorVisibleProgressiveCut(
                    *progressive, inst, fp,
                    assembly.effectiveProgressiveCut(inst.lodCut));
                if (level >= visibleWireRanges.size())
                    continue;
                SbMatrix model;
                model.setValue(inst.transform.data());
                for (const ProgressiveWireCluster& cluster :
                        progressive->progressiveClusters) {
                    float clusterMinimum[3];
                    float clusterMaximum[3];
                    executorTransformedBox(
                        cluster.bounds, model,
                        clusterMinimum, clusterMaximum);
                    if (isBoxOutsideExecutorFrustum(
                            clusterMinimum, clusterMaximum, fp))
                        continue;
                    for (const ProgressiveWireClusterRange& range :
                            cluster.ranges) {
                        if (range.activationCut > level)
                            break;
                        const uint64_t key =
                            (static_cast<uint64_t>(range.firstSegment) <<
                                32u) |
                            static_cast<uint64_t>(range.segmentCount);
                        if (visibleWireRangeKeys[level].insert(key).second)
                            visibleWireRanges[level].push_back({
                                range.firstSegment, range.segmentCount, 0});
                    }
                }
            }
            for (std::vector<CadProgressiveGpu::PackedRange>& ranges :
                    visibleWireRanges) {
                std::sort(ranges.begin(), ranges.end(),
                    [](const CadProgressiveGpu::PackedRange& left,
                       const CadProgressiveGpu::PackedRange& right) {
                        if (left.sourceFirst != right.sourceFirst)
                            return left.sourceFirst < right.sourceFirst;
                        return left.sourceCount < right.sourceCount;
                    });
                ranges.erase(std::unique(ranges.begin(), ranges.end(),
                    [](const CadProgressiveGpu::PackedRange& left,
                       const CadProgressiveGpu::PackedRange& right) {
                        return left.sourceFirst == right.sourceFirst &&
                            left.sourceCount == right.sourceCount;
                    }), ranges.end());
            }
        }
        if (interrupted)
            break;

        for (uint32_t i = 0; i < item.instanceCount; ++i) {
            if (renderInterruptedAfter(deadlineWork)) {
                interrupted = true;
                break;
            }
            const size_t visibleIndex = item.baseInstance + i;
            if (!cadInstanceDrawable(
                    plan, item, visibleIndex, CadDrawChannel::Wire))
                continue;
            const auto& inst = plan.visibleInstances[visibleIndex];
            if (isBoxOutsideExecutorFrustum(
                    inst.wbMin, inst.wbMax, fp))
                continue;

            SbMatrix model;
            model.setValue(inst.transform.data());
            SbMatrix modelView = model;
            modelView.multRight(viewMatrix);
            glue->glLoadMatrixf(modelView[0]);
            glue->glColor4ub(inst.rgba[0], inst.rgba[1],
                             inst.rgba[2], inst.rgba[3]);
            applyWireRasterStyle(glue, inst, caps_.hasLineStipple);

            GLuint positionBuffer = wire->posBuf;
            GLsizei availableSegmentCount = wire->segCount;
            GLsizei segmentFirst = 0;
            uint8_t level = Obol::ProgressiveCutUnspecified;
            const CadProgressiveGpu *progressiveCut = nullptr;
            if (progressive) {
		level = executorVisibleProgressiveCut(
		    *progressive, inst, fp,
		    assembly.effectiveProgressiveCut(inst.lodCut));
                if (!progressive->quantizationAtCut(level).isExact()) {
                    const std::vector<CadProgressiveGpu::PackedRange>
                        *ranges = progressive->
                            hasAdaptiveProgressiveClusters() &&
                            level < visibleWireRanges.size() ?
                        &visibleWireRanges[level] : nullptr;
                    progressiveCut = ensureProgressiveWireGpu(
                        gpuRes_, item.rep.part, *progressive, level,
                        ranges, glue);
                    if (!progressiveCut) continue;
                    positionBuffer = progressiveCut->posBuf;
                    availableSegmentCount =
                        progressiveCut->vertexCount / 2;
                } else
                    segmentFirst = static_cast<GLsizei>(
                        progressive->segmentFirstAtCut(level));
            }
            glue->glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
            glue->glVertexPointer(3, GL_FLOAT, 3 * sizeof(float), nullptr);
            uint64_t submittedSegments = 0;
            const auto drawSegmentRange = [&](uint32_t first,
                                               uint32_t count) {
                if (!count || first >=
                        static_cast<uint32_t>(availableSegmentCount))
                    return;
                const GLsizei bounded = static_cast<GLsizei>(
                    std::min<uint64_t>(count,
                        static_cast<uint64_t>(availableSegmentCount) - first));
                if (wire->sequentialSegments)
                    glue->glDrawArrays(GL_LINES,
                        static_cast<GLint>(first * 2u), bounded * 2);
                else
                    glue->glDrawElements(GL_LINES, bounded * 2,
                        GL_UNSIGNED_INT,
                        reinterpret_cast<const GLvoid *>(
                            static_cast<uintptr_t>(first) * 2u *
                            sizeof(uint32_t)));
                submittedSegments = cadSaturatingWorkAdd(
                    submittedSegments, static_cast<uint64_t>(bounded));
            };
            if (progressive &&
                    progressive->hasAdaptiveProgressiveClusters()) {
                for (const ProgressiveWireCluster& cluster :
                        progressive->progressiveClusters) {
                    float clusterMinimum[3];
                    float clusterMaximum[3];
                    executorTransformedBox(
                        cluster.bounds, model,
                        clusterMinimum, clusterMaximum);
                    if (isBoxOutsideExecutorFrustum(
                            clusterMinimum, clusterMaximum, fp))
                        continue;
                    if (progressiveCut &&
                            !progressiveCut->packedRanges.empty()) {
                        for (const ProgressiveWireClusterRange& range :
                                cluster.ranges) {
                            if (range.activationCut > level)
                                break;
                            const auto packed = std::lower_bound(
                                progressiveCut->packedRanges.begin(),
                                progressiveCut->packedRanges.end(),
                                range.firstSegment,
                                [](const CadProgressiveGpu::PackedRange& entry,
                                   uint32_t first) {
                                    return entry.sourceFirst < first;
                                });
                            if (packed ==
                                    progressiveCut->packedRanges.end() ||
                                    packed->sourceFirst !=
                                        range.firstSegment ||
                                    packed->sourceCount !=
                                        range.segmentCount)
                                continue;
                            drawSegmentRange(
                                packed->packedFirst, packed->sourceCount);
                        }
                        continue;
                    }
                    uint32_t first = 0;
                    uint32_t count = 0;
                    for (const ProgressiveWireClusterRange& range :
                            cluster.ranges) {
                        if (range.activationCut > level)
                            break;
                        if (!count) {
                            first = range.firstSegment;
                            count = range.segmentCount;
                        } else if (static_cast<uint64_t>(first) + count ==
                                range.firstSegment) {
                            count = range.segmentCount > UINT32_MAX - count ?
                                UINT32_MAX : count + range.segmentCount;
                        } else {
                            drawSegmentRange(first, count);
                            first = range.firstSegment;
                            count = range.segmentCount;
                        }
                    }
                    drawSegmentRange(first, count);
                }
            } else {
                const GLsizei segmentCount = progressiveWireSegmentCount(
                    assembly, item.rep.part, inst, wire->segCount);
                drawSegmentRange(static_cast<uint32_t>(segmentFirst),
                    static_cast<uint32_t>((std::max)(0, segmentCount)));
            }
            cadAccumulateRenderedWireWork(
                lastRenderedWork_, submittedSegments);
        }
        if (interrupted)
            break;
    }

    glue->glDisableClientState(GL_VERTEX_ARRAY);
    restoreWireRasterState(glue, rasterState, caps_.hasLineStipple);

    const GLboolean wasColorMaterial = glue->glIsEnabled(GL_COLOR_MATERIAL);
    GLint wasTwoSidedLighting = GL_FALSE;
    glue->glGetIntegerv(GL_LIGHT_MODEL_TWO_SIDE, &wasTwoSidedLighting);
    if (!interrupted && drawShaded && !plan.shadedItems.empty()) {
        // CAD shading must not depend on the caller enabling GL_LIGHTING.
        glue->glEnable(GL_LIGHTING);
        glue->glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
    }
    glue->glDisable(GL_COLOR_MATERIAL);
    glue->glEnableClientState(GL_VERTEX_ARRAY);
    if (!interrupted && drawShaded) for (const auto& item : plan.shadedItems) {
        if (renderInterruptedAfter(deadlineWork)) {
            interrupted = true;
            break;
        }
        const CadTriGpu *tri = gpuRes_->triFor(item.rep.part);
        if (!tri) continue;
        const PartGeometry *geometry = assembly.partGeometry(item.rep.part);
        const TriMesh *progressive =
            geometry && geometry->shaded &&
            geometry->shaded->isProgressive() ?
            &*geometry->shaded : nullptr;

        /* The compatibility renderer expands meshes without authored normals
         * to triangle-corner positions and flat normals.  For a large mesh
         * intersecting only part of the view, expanding the whole PoP prefix
         * before cluster culling defeats the purpose of view-local LoD and
         * can monopolize the presentation thread for hundreds of
         * milliseconds.  Build one deterministic union of the source ranges
         * visible to every occurrence in this item.  A fully contained
         * occurrence deliberately selects the ordinary full-prefix record;
         * otherwise all occurrences share one compact part/cut VBO. */
        std::vector<std::vector<CadProgressiveGpu::PackedRange>>
            visibleSourceRanges(progressive ?
                progressive->progressiveCuts.size() : 0);
        std::vector<bool> requireFullProgressiveCut(
            visibleSourceRanges.size(), false);
        if (progressive && progressive->hasProgressiveClusters() &&
                progressive->normals.empty()) {
            for (uint32_t i = 0; i < item.instanceCount; ++i) {
                const size_t instanceIndex = item.baseInstance + i;
                if (!cadInstanceDrawable(
                        plan, item, instanceIndex,
                        CadDrawChannel::Shaded))
                    continue;
                const auto& inst = plan.visibleInstances[instanceIndex];
                if (isBoxOutsideExecutorFrustum(
                        inst.wbMin, inst.wbMax, fp))
                    continue;
		const uint8_t instanceLevel = executorVisibleProgressiveCut(
		    *progressive, inst, fp,
		    assembly.effectiveProgressiveCut(inst.lodCut));
                if (!progressive->hasAdaptiveProgressiveClusters() &&
                        isBoxInsideExecutorFrustum(
                            inst.wbMin, inst.wbMax, fp)) {
                    requireFullProgressiveCut[instanceLevel] = true;
                    continue;
                }
                SbMatrix model;
                model.setValue(inst.transform.data());
                for (const ProgressiveTriangleCluster& cluster :
                        progressive->progressiveClusters) {
                    if (cluster.ranges.empty()) continue;
                    float clusterMinimum[3];
                    float clusterMaximum[3];
                    executorTransformedBox(
                        cluster.bounds, model,
                        clusterMinimum, clusterMaximum);
                    if (isBoxOutsideExecutorFrustum(
                            clusterMinimum, clusterMaximum, fp))
                        continue;
                    for (const ProgressiveTriangleClusterRange& range :
                            cluster.ranges) {
                        if (range.activationCut > instanceLevel)
                            break;
                        visibleSourceRanges[instanceLevel].push_back({
                            range.firstIndex, range.indexCount, 0});
                    }
                }
            }
            for (size_t cutIndex = 0;
                    cutIndex < visibleSourceRanges.size(); ++cutIndex) {
                std::vector<CadProgressiveGpu::PackedRange>& ranges =
                    visibleSourceRanges[cutIndex];
                if (requireFullProgressiveCut[cutIndex]) {
                    ranges.clear();
                    continue;
                }
                std::sort(ranges.begin(), ranges.end(),
                    [](const CadProgressiveGpu::PackedRange& left,
                       const CadProgressiveGpu::PackedRange& right) {
                        if (left.sourceFirst != right.sourceFirst)
                            return left.sourceFirst < right.sourceFirst;
                        return left.sourceCount < right.sourceCount;
                    });
                ranges.erase(std::unique(ranges.begin(), ranges.end(),
                    [](const CadProgressiveGpu::PackedRange& left,
                       const CadProgressiveGpu::PackedRange& right) {
                        return left.sourceFirst == right.sourceFirst &&
                            left.sourceCount == right.sourceCount;
                    }), ranges.end());
            }
        }

        bool normalArrayEnabled = false;

        for (uint32_t i = 0; i < item.instanceCount; ++i) {
            if (renderInterruptedAfter(deadlineWork)) {
                interrupted = true;
                break;
            }
            const size_t instanceIndex = item.baseInstance + i;
            if (!cadInstanceDrawable(
                    plan, item, instanceIndex, CadDrawChannel::Shaded))
                continue;
            const auto& inst = plan.visibleInstances[instanceIndex];
            if (isBoxOutsideExecutorFrustum(
                    inst.wbMin, inst.wbMax, fp))
                continue;

            SbMatrix model;
            model.setValue(inst.transform.data());
            SbMatrix modelView = model;
            modelView.multRight(viewMatrix);
            glue->glLoadMatrixf(modelView[0]);
            setImmediateMaterialFromRgba(glue, inst.rgba.data());

            if (cadLightDebugRequested()) {
                static unsigned int fixedLightReportCount = 0;
                if (fixedLightReportCount++ < 16) {
                    GLfloat modelAmbient[4] = {};
                    GLfloat materialAmbient[4] = {};
                    GLfloat materialDiffuse[4] = {};
                    GLfloat materialSpecular[4] = {};
                    GLfloat materialShininess = 0.0f;
                    SoGLContext_glGetFloatv(
                        glue, GL_LIGHT_MODEL_AMBIENT, modelAmbient);
                    SoGLContext_glGetMaterialfv(
                        glue, GL_FRONT, GL_AMBIENT, materialAmbient);
                    SoGLContext_glGetMaterialfv(
                        glue, GL_FRONT, GL_DIFFUSE, materialDiffuse);
                    SoGLContext_glGetMaterialfv(
                        glue, GL_FRONT, GL_SPECULAR, materialSpecular);
                    SoGLContext_glGetMaterialfv(
                        glue, GL_FRONT, GL_SHININESS, &materialShininess);
                    std::fprintf(stderr,
                        "CadRendererGL fixed lighting globalAmbient="
                        "(%.6g,%.6g,%.6g) material={ambient=%.6g "
                        "diffuse=%.6g specular=%.6g shininess=%.6g} "
                        "lights=",
                        modelAmbient[0], modelAmbient[1], modelAmbient[2],
                        materialAmbient[0], materialDiffuse[0],
                        materialSpecular[0], materialShininess);
                    for (int lightIndex = 0; lightIndex < kMaxLights;
                            ++lightIndex) {
                        const GLenum light = static_cast<GLenum>(
                            GL_LIGHT0 + lightIndex);
                        if (!SoGLContext_glIsEnabled(glue, light))
                            continue;
                        GLfloat diffuse[4] = {};
                        GLfloat position[4] = {};
                        SoGLContext_glGetLightfv(
                            glue, light, GL_DIFFUSE, diffuse);
                        SoGLContext_glGetLightfv(
                            glue, light, GL_POSITION, position);
                        std::fprintf(stderr,
                            "%d:{d=%.6g,p=(%.6g,%.6g,%.6g,%.6g)} ",
                            lightIndex, diffuse[0], position[0], position[1],
                            position[2], position[3]);
                    }
                    std::fprintf(stderr, "\n");
                }
            }

            const GLsizei indexCount = progressiveTriangleIndexCount(
                assembly, item.rep.part, inst, tri->idxCount);
            const CadProgressiveGpu *cut = nullptr;
            uint8_t level = Obol::ProgressiveCutUnspecified;
            if (progressive) {
		level = executorVisibleProgressiveCut(
		    *progressive, inst, fp,
		    assembly.effectiveProgressiveCut(inst.lodCut));
                /* A conservative part box can intersect the frustum while
                 * none of its spatial triangle clusters do.  Do not turn
                 * that empty view-local selection into a full-prefix
                 * compatibility upload. */
                if (progressive->hasProgressiveClusters() &&
                        progressive->normals.empty() &&
                        level < visibleSourceRanges.size() &&
                        !requireFullProgressiveCut[level] &&
                        visibleSourceRanges[level].empty())
                    continue;
                if (!progressive->quantizationAtCut(level).isExact() ||
                        tri->normBuf == 0) {
                    bool preparedCut = false;
                    const bool partialCut =
                        level < visibleSourceRanges.size() &&
                        !requireFullProgressiveCut[level] &&
                        !visibleSourceRanges[level].empty();
                    cut = ensureProgressiveTriGpu(
                        gpuRes_, item.rep.part, *progressive, level,
                        partialCut ?
                            &visibleSourceRanges[level] : nullptr,
                        &preparedCut, glue);
                    if (!cut) continue;
                    if (preparedCut)
                        noteRenderPreparation(
                            "fixed-progressive-cut-upload");
                }
            }
            setCadBackfaceCulling(glue,
                cadProgressiveCutCullSafe(
                    item.cullBackfaces, progressive, level));

            const GLuint positionBuffer = cut ? cut->posBuf : tri->posBuf;
            glue->glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);
            glue->glVertexPointer(3, GL_FLOAT, 3 * sizeof(float), nullptr);
            const GLuint normalBuffer =
                cut && cut->normBuf ? cut->normBuf : tri->normBuf;
            if (normalBuffer) {
                if (!normalArrayEnabled) {
                    glue->glEnableClientState(GL_NORMAL_ARRAY);
                    normalArrayEnabled = true;
                }
                glue->glBindBuffer(GL_ARRAY_BUFFER, normalBuffer);
                glue->glNormalPointer(GL_FLOAT, 3 * sizeof(float), nullptr);
            } else if (normalArrayEnabled) {
                glue->glDisableClientState(GL_NORMAL_ARRAY);
                normalArrayEnabled = false;
            }

            const bool nonIndexedCut = cut && !cut->indexed;
            if (nonIndexedCut)
                glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            else
                glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, tri->idxBuf);
            const GLsizei availableIndexCount = nonIndexedCut ?
                std::min(indexCount, cut->vertexCount) :
                std::min(indexCount, tri->idxCount);
            uint64_t submittedIndices = 0;
            const auto submitRange = [&](uint32_t first,
                                         uint32_t requestedCount) {
                if (interrupted || first >=
                        static_cast<uint32_t>(availableIndexCount))
                    return;
                GLsizei remaining = static_cast<GLsizei>(std::min<uint64_t>(
                    requestedCount,
                    static_cast<uint64_t>(availableIndexCount) - first));
                GLsizei submitted = 0;
                while (submitted < remaining) {
                    const GLsizei chunk = caps_.isSoftwareRenderer ?
                        std::min(cadSoftwareTriangleChunkIndices,
                                 remaining - submitted) :
                        remaining - submitted;
                    const GLsizei rangeFirst =
                        static_cast<GLsizei>(first) + submitted;
                    if (nonIndexedCut) {
                        glue->glDrawArrays(
                            GL_TRIANGLES, rangeFirst, chunk);
                    } else {
                        glue->glDrawElements(
                            GL_TRIANGLES, chunk, GL_UNSIGNED_INT,
                            reinterpret_cast<const GLvoid *>(
                                static_cast<uintptr_t>(rangeFirst) *
                                sizeof(uint32_t)));
                    }
                    submitted += chunk;
                    submittedIndices = cadSaturatingWorkAdd(
                        submittedIndices, static_cast<uint64_t>(chunk));
                    if (submitted < remaining &&
                            renderInterruptedAfter(deadlineWork)) {
                        interrupted = true;
                        break;
                    }
                }
            };

            /* A fully contained occurrence remains one draw call.  When a
             * large leaf straddles the view boundary, submit only conservative
             * spatial clusters intersecting the frustum.  Clusters retain one
             * logical PartId and one global PoP cut, so this cannot alter CAD
             * selection, transforms, topology, or crack behavior. */
            const bool clusteredPartial = progressive &&
                progressive->hasProgressiveClusters() &&
                (progressive->hasAdaptiveProgressiveClusters() ||
                 !isBoxInsideExecutorFrustum(
                    inst.wbMin, inst.wbMax, fp));
            if (!clusteredPartial) {
                submitRange(0, static_cast<uint32_t>(availableIndexCount));
            } else {
                for (const ProgressiveTriangleCluster& cluster :
                        progressive->progressiveClusters) {
                    if (interrupted) break;
                    if (cluster.ranges.empty()) continue;
                    float clusterMinimum[3];
                    float clusterMaximum[3];
                    executorTransformedBox(
                        cluster.bounds, model,
                        clusterMinimum, clusterMaximum);
                    if (isBoxOutsideExecutorFrustum(
                            clusterMinimum, clusterMaximum, fp))
                        continue;
                    for (const ProgressiveTriangleClusterRange& range :
                            cluster.ranges) {
                        if (range.activationCut > level)
                            break;
                        const uint64_t rangeEnd =
                            static_cast<uint64_t>(range.firstIndex) +
                            range.indexCount;
                        const uint64_t sourceAvailableIndexCount =
                            nonIndexedCut && !cut->packedRanges.empty() ?
                                static_cast<uint64_t>(indexCount) :
                                static_cast<uint64_t>(
                                    availableIndexCount);
                        if (rangeEnd > sourceAvailableIndexCount)
                            continue;
                        uint32_t first = range.firstIndex;
                        uint32_t count = range.indexCount;
                        if (nonIndexedCut &&
                                !cut->packedRanges.empty()) {
                            const auto packed = std::lower_bound(
                                cut->packedRanges.begin(),
                                cut->packedRanges.end(), first,
                                [](const CadProgressiveGpu::PackedRange& entry,
                                   uint32_t value) {
                                    return entry.sourceFirst < value;
                                });
                            if (packed == cut->packedRanges.end() ||
                                    packed->sourceFirst != first ||
                                    packed->sourceCount != count)
                                continue;
                            first = packed->packedFirst;
                        }
                        submitRange(first, count);
                        if (interrupted) break;
                    }
                }
            }
            const uint64_t submittedTriangles = submittedIndices / 3;
            renderedTriangleCount = cadSaturatingWorkAdd(
                renderedTriangleCount, submittedTriangles);
            if (geometry && geometry->shaded)
                cadAccumulateRenderedShadedWork(
                    lastRenderedWork_, *geometry->shaded,
                    level, submittedTriangles, 1,
                    clusteredPartial ? submittedIndices : UINT64_MAX);
        }
        if (normalArrayEnabled)
            glue->glDisableClientState(GL_NORMAL_ARRAY);
    }
    glue->glDisableClientState(GL_VERTEX_ARRAY);
    if (wasColorMaterial) glue->glEnable(GL_COLOR_MATERIAL);
    else glue->glDisable(GL_COLOR_MATERIAL);
    glue->glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, wasTwoSidedLighting);
    if (wasLighting) glue->glEnable(GL_LIGHTING);
    else glue->glDisable(GL_LIGHTING);

    glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
    glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glue->glMatrixMode(GL_MODELVIEW);
    glue->glPopMatrix();
    glue->glMatrixMode(GL_PROJECTION);
    glue->glPopMatrix();
    glue->glMatrixMode(GL_MODELVIEW);
    lastRenderedTriangleCount_ =
        renderedTriangleCount > UINT64_MAX - lastRenderedTriangleCount_ ?
            UINT64_MAX : lastRenderedTriangleCount_ + renderedTriangleCount;
}

// ---------------------------------------------------------------------------
// Tier-0: immediate-mode fallback (GL 1.1, Mesa 7.x swrast)
// ---------------------------------------------------------------------------

void CadRendererGL::renderImmediateMode(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext*   glue,
        const SbMatrix&      viewProj,
        const SbMatrix&      viewMatrix,
        const SbMatrix&      projectionMatrix,
        bool drawWire,
        bool drawShaded)
{
    size_t deadlineWork = 256u;
    uint64_t renderedTriangleCount = 0;
    bool interrupted = renderInterruptedAfter(deadlineWork);
    if (interrupted)
        return;

    // Keep projection out of GL_MODELVIEW so normal transformation uses only
    // the affine local-to-eye transform.
    glue->glMatrixMode(GL_PROJECTION);
    glue->glPushMatrix();
    glue->glLoadMatrixf(projectionMatrix[0]);

    glue->glMatrixMode(GL_MODELVIEW);
    glue->glPushMatrix();
    glue->glLoadMatrixf(viewMatrix[0]);
    if (caps_.isSoftwareRenderer)
        this->uploadFixedLights(glue);

    // Disable lighting so glColor4f controls the final colour
    GLboolean wasLighting = glue->glIsEnabled(GL_LIGHTING);
    glue->glDisable(GL_LIGHTING);
    const CadWireRasterState rasterState = captureWireRasterState(
        glue, caps_.hasLineStipple);

    // Extract frustum planes for per-instance culling.
    const ExecutorFrustumPlanes fp =
        extractExecutorFrustumPlanes(viewProj);

    // --- Wire pass ---
    if (drawWire) for (const auto& item : plan.wireItems) {
        if (renderInterruptedAfter(deadlineWork)) {
            interrupted = true;
            break;
        }
        const Obol::PartGeometry* geom = assembly.partGeometry(item.rep.part);
        if (!geom || !geom->wire.has_value()) continue;
        const Obol::WireRep& wire = *geom->wire;

        for (uint32_t ii = 0; ii < item.instanceCount; ++ii) {
            if (renderInterruptedAfter(deadlineWork)) {
                interrupted = true;
                break;
            }
            const size_t visibleIndex = item.baseInstance + ii;
            if (!cadInstanceDrawable(
                    plan, item, visibleIndex, CadDrawChannel::Wire))
                continue;
            const auto& inst = plan.visibleInstances[visibleIndex];
            if (isBoxOutsideExecutorFrustum(
                    inst.wbMin, inst.wbMax, fp))
                continue;

            SbMatrix model;
            model.setValue(inst.transform.data());
            SbMatrix modelView = model;
            modelView.multRight(viewMatrix);
            glue->glLoadMatrixf(modelView[0]);

            glue->glColor4ub(inst.rgba[0], inst.rgba[1], inst.rgba[2], inst.rgba[3]);
            applyWireRasterStyle(glue, inst, caps_.hasLineStipple);

	    const uint8_t drawLevel = executorVisibleProgressiveCut(
		wire, inst, fp,
		assembly.effectiveProgressiveCut(inst.lodCut));
	    size_t flatPointCount =
		wire.segmentCountAtCut(drawLevel) * 2;
	    size_t flatPointFirst =
		wire.segmentFirstAtCut(drawLevel) * 2;
            uint64_t submittedFlatSegments = 0;
            const auto drawFlatRange = [&](size_t firstSegment,
                                           size_t segmentCount) {
              const size_t firstPoint = firstSegment * 2u;
              const size_t available = firstPoint <
                      wire.segmentPoints.size() ?
                  (wire.segmentPoints.size() - firstPoint) / 2u : 0;
              segmentCount = std::min(segmentCount, available);
              if (!segmentCount)
                  return;
              const size_t endPoint = firstPoint + segmentCount * 2u;
              size_t point = firstPoint;
              while (point + 1 < endPoint) {
                const size_t chunkEnd = std::min(
                    endPoint, point + 512u);
                const size_t segmentWork = (chunkEnd - point) / 2u;
                if (renderInterruptedAfter(deadlineWork, segmentWork)) {
                    interrupted = true;
                    break;
                }
                glue->glBegin(GL_LINES);
                for (; point + 1 < chunkEnd; point += 2) {
                    const SbVec3f a = wire.isProgressive() ?
                        progressiveSnapPoint(wire.segmentPoints[point],
                            wire.progressiveQuantizationMinimum,
                            wire.progressiveQuantizationMaximum,
                            wire.quantizationAtCut(drawLevel)) :
                        wire.segmentPoints[point];
                    const SbVec3f b = wire.isProgressive() ?
                        progressiveSnapPoint(
                            wire.segmentPoints[point + 1],
                            wire.progressiveQuantizationMinimum,
                            wire.progressiveQuantizationMaximum,
                            wire.quantizationAtCut(drawLevel)) :
                        wire.segmentPoints[point + 1];
                    glue->glVertex3f(a[0], a[1], a[2]);
                    glue->glVertex3f(b[0], b[1], b[2]);
                }
                glue->glEnd();
                submittedFlatSegments = cadSaturatingWorkAdd(
                    submittedFlatSegments, segmentWork);
              }
            };
            if (wire.hasAdaptiveProgressiveClusters()) {
                for (const ProgressiveWireCluster& cluster :
                        wire.progressiveClusters) {
                    float clusterMinimum[3];
                    float clusterMaximum[3];
                    executorTransformedBox(cluster.bounds, model,
                        clusterMinimum, clusterMaximum);
                    if (isBoxOutsideExecutorFrustum(
                            clusterMinimum, clusterMaximum, fp))
                        continue;
                    uint32_t first = 0;
                    uint32_t count = 0;
                    for (const ProgressiveWireClusterRange& range :
                            cluster.ranges) {
                        if (range.activationCut > drawLevel)
                            break;
                        if (!count) {
                            first = range.firstSegment;
                            count = range.segmentCount;
                        } else if (static_cast<uint64_t>(first) + count ==
                                range.firstSegment) {
                            count = range.segmentCount > UINT32_MAX - count ?
                                UINT32_MAX : count + range.segmentCount;
                        } else {
                            drawFlatRange(first, count);
                            first = range.firstSegment;
                            count = range.segmentCount;
                        }
                    }
                    drawFlatRange(first, count);
                    if (interrupted)
                        break;
                }
                flatPointCount = static_cast<size_t>(std::min<uint64_t>(
                    submittedFlatSegments * 2u, SIZE_MAX));
                flatPointFirst = 0;
            } else if (flatPointCount > 0) {
                drawFlatRange(flatPointFirst / 2u,
                    flatPointCount / 2u);
            }

            if (interrupted)
                break;
            uint64_t polylineSegments = 0;
            for (const auto& poly : wire.polylines) {
                if (poly.points.size() < 2) continue;
                polylineSegments = cadSaturatingWorkAdd(
                    polylineSegments,
                    static_cast<uint64_t>(poly.points.size() - 1));
                size_t point = 0;
                while (point < poly.points.size()) {
                    const size_t chunkEnd = std::min(
                        poly.points.size(), point + 256u);
                    if (renderInterruptedAfter(
                            deadlineWork, chunkEnd - point)) {
                        interrupted = true;
                        break;
                    }
                    glue->glBegin(GL_LINE_STRIP);
                    if (point) {
                        const SbVec3f& prior = poly.points[point - 1];
                        glue->glVertex3f(prior[0], prior[1], prior[2]);
                    }
                    for (; point < chunkEnd; ++point) {
                        const SbVec3f& pt = poly.points[point];
                        glue->glVertex3f(pt[0], pt[1], pt[2]);
                    }
                    glue->glEnd();
                }
                if (interrupted)
                    break;
            }
            if (!interrupted)
                cadAccumulateRenderedWireWork(
                    lastRenderedWork_,
                    static_cast<uint64_t>(flatPointCount / 2) +
                        polylineSegments);
        }
        if (interrupted)
            break;
    }

    restoreWireRasterState(glue, rasterState, caps_.hasLineStipple);

    // --- Shaded pass ---
    GLboolean wasColorMaterial = glue->glIsEnabled(GL_COLOR_MATERIAL);
    GLint wasTwoSidedLighting = GL_FALSE;
    glue->glGetIntegerv(GL_LIGHT_MODEL_TWO_SIDE, &wasTwoSidedLighting);
    if (!interrupted && drawShaded && !plan.shadedItems.empty()) {
        // Shaded CAD geometry always uses its normals, regardless of the
        // lighting state inherited from the surrounding scene graph.
        glue->glEnable(GL_LIGHTING);
        glue->glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
    }
    glue->glDisable(GL_COLOR_MATERIAL);
    if (!interrupted && drawShaded) for (const auto& item : plan.shadedItems) {
        if (renderInterruptedAfter(deadlineWork)) {
            interrupted = true;
            break;
        }
        const Obol::PartGeometry* geom = assembly.partGeometry(item.rep.part);
        if (!geom || !geom->shaded.has_value()) continue;
        const Obol::TriMesh& mesh = *geom->shaded;

        const bool hasNorm = !mesh.normals.empty();

        for (uint32_t ii = 0; ii < item.instanceCount; ++ii) {
            if (renderInterruptedAfter(deadlineWork)) {
                interrupted = true;
                break;
            }
            const size_t instanceIndex = item.baseInstance + ii;
            if (!cadInstanceDrawable(
                    plan, item, instanceIndex, CadDrawChannel::Shaded))
                continue;
            const auto& inst = plan.visibleInstances[instanceIndex];
            if (isBoxOutsideExecutorFrustum(
                    inst.wbMin, inst.wbMax, fp))
                continue;

            SbMatrix model;
            model.setValue(inst.transform.data());
            SbMatrix modelView = model;
            modelView.multRight(viewMatrix);
            glue->glLoadMatrixf(modelView[0]);

            setImmediateMaterialFromRgba(glue, inst.rgba.data());

            const std::vector<uint32_t>& drawIdx = mesh.indices;
	    const uint8_t drawLevel = executorVisibleProgressiveCut(
		mesh, inst, fp,
		assembly.effectiveProgressiveCut(inst.lodCut));
	    const size_t drawIndexCount = mesh.indexCountAtCut(drawLevel);
            setCadBackfaceCulling(glue,
                cadProgressiveCutCullSafe(
                    item.cullBackfaces,
                    mesh.isProgressive() ? &mesh : nullptr, drawLevel));

            std::vector<CadTriangleDrawRange> drawRanges;
            if (mesh.hasAdaptiveProgressiveClusters()) {
                drawRanges.reserve(mesh.progressiveClusters.size());
                for (const ProgressiveTriangleCluster& cluster :
                        mesh.progressiveClusters) {
                    float clusterMinimum[3];
                    float clusterMaximum[3];
                    executorTransformedBox(
                        cluster.bounds, model,
                        clusterMinimum, clusterMaximum);
                    if (isBoxOutsideExecutorFrustum(
                            clusterMinimum, clusterMaximum, fp))
                        continue;
                    CadTriangleDrawRange active;
                    for (const ProgressiveTriangleClusterRange& range :
                            cluster.ranges) {
                        if (range.activationCut > drawLevel)
                            break;
                        if (!active.indexCount) {
                            active = {range.firstIndex, range.indexCount};
                        } else if (static_cast<uint64_t>(
                                active.firstIndex) +
                                active.indexCount == range.firstIndex) {
                            active.indexCount = range.indexCount >
                                    UINT32_MAX - active.indexCount ?
                                UINT32_MAX :
                                active.indexCount + range.indexCount;
                        } else {
                            drawRanges.push_back(active);
                            active = {range.firstIndex, range.indexCount};
                        }
                    }
                    if (active.indexCount)
                        drawRanges.push_back(active);
                }
            } else if (drawIndexCount) {
                drawRanges.push_back({0u,
                    static_cast<uint32_t>(std::min<size_t>(
                        drawIndexCount, UINT32_MAX))});
            }
            uint64_t submittedIndices = 0;
            for (const CadTriangleDrawRange& range : drawRanges) {
              size_t triangleOffset = range.firstIndex;
              const size_t rangeEnd = std::min<size_t>(
                  drawIdx.size(), static_cast<uint64_t>(range.firstIndex) +
                      range.indexCount);
              while (triangleOffset + 2 < rangeEnd) {
                const size_t triangleWork = std::min<size_t>(
                    256u, (rangeEnd - triangleOffset) / 3u);
                if (renderInterruptedAfter(deadlineWork, triangleWork)) {
                    interrupted = true;
                    break;
                }
                const size_t chunkEnd =
                    triangleOffset + triangleWork * 3u;
                glue->glBegin(GL_TRIANGLES);
                for (; triangleOffset < chunkEnd; triangleOffset += 3) {
                    SbVec3f triangle[3];
                    for (int k = 0; k < 3; ++k) {
                        uint32_t idx = drawIdx[triangleOffset + k];
                        triangle[k] = mesh.isProgressive() ?
                            progressiveSnapPoint(mesh.positions[idx],
                                mesh.progressiveQuantizationMinimum,
                                mesh.progressiveQuantizationMaximum,
                                mesh.quantizationAtCut(drawLevel)) :
                            mesh.positions[idx];
                    }
                    if (!hasNorm) {
                        SbVec3f faceNormal =
                            (mesh.positions[drawIdx[triangleOffset + 1]] -
                                mesh.positions[drawIdx[triangleOffset]]).cross(
                                mesh.positions[drawIdx[triangleOffset + 2]] -
                                mesh.positions[drawIdx[triangleOffset]]);
                        if (faceNormal.sqrLength() > 0.0f)
                            faceNormal.normalize();
                        else
                            faceNormal.setValue(0.0f, 0.0f, 1.0f);
                        glue->glNormal3f(faceNormal[0], faceNormal[1],
                                         faceNormal[2]);
                    }
                    for (int k = 0; k < 3; ++k) {
                        uint32_t idx = drawIdx[triangleOffset + k];
                        if (hasNorm && idx < mesh.normals.size()) {
                            const auto& n = mesh.normals[idx];
                            glue->glNormal3f(n[0], n[1], n[2]);
                        }
                        const SbVec3f& p = triangle[k];
                        glue->glVertex3f(p[0], p[1], p[2]);
                    }
                }
                glue->glEnd();
                renderedTriangleCount =
                    renderedTriangleCount >
                            UINT64_MAX -
                                static_cast<uint64_t>(triangleWork) ?
                        UINT64_MAX : renderedTriangleCount +
                            static_cast<uint64_t>(triangleWork);
                submittedIndices = cadSaturatingWorkAdd(
                    submittedIndices,
                    static_cast<uint64_t>(triangleWork) * 3u);
              }
              if (interrupted)
                  break;
            }
            if (!interrupted)
                cadAccumulateRenderedShadedWork(
                    lastRenderedWork_, mesh, drawLevel,
                    submittedIndices / 3u, 1, submittedIndices);
        }
        if (interrupted)
            break;
    }
    if (wasColorMaterial) glue->glEnable(GL_COLOR_MATERIAL);
    else glue->glDisable(GL_COLOR_MATERIAL);
    glue->glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, wasTwoSidedLighting);
    if (wasLighting) glue->glEnable(GL_LIGHTING);
    else glue->glDisable(GL_LIGHTING);

    // Restore matrix state
    glue->glMatrixMode(GL_MODELVIEW);
    glue->glPopMatrix();
    glue->glMatrixMode(GL_PROJECTION);
    glue->glPopMatrix();
    glue->glMatrixMode(GL_MODELVIEW);
    lastRenderedTriangleCount_ =
        renderedTriangleCount > UINT64_MAX - lastRenderedTriangleCount_ ?
            UINT64_MAX : lastRenderedTriangleCount_ + renderedTriangleCount;
}

// ---------------------------------------------------------------------------
// Tier-2: instanced rendering (GL 3.1+)
// ---------------------------------------------------------------------------

bool CadRendererGL::rejectIndirect(int status, const char *reason)
{
    lastIndirectStatus_ = status;
    if (configuration_->indirectDebug &&
            reportedIndirectStatus_ != status) {
        std::fprintf(stderr,
            "CadRendererGL indirect rejected status=%d reason=%s\n",
            status, reason ? reason : "unknown");
        reportedIndirectStatus_ = status;
    }
    return false;
}

bool CadRendererGL::submitIndirectPrepared(
        const SoGLContext *glue,
        const SbMatrix& viewProj,
        const SbViewVolume& viewVolume)
{
    if (!glue || !gpuRes_ || !indirectPrepared_.valid)
        return false;
    if (indirectPrepared_.pages.empty()) {
        if (indirectPrepared_.pressureProxyPoints.empty())
            return false;
        lastRenderedWork_ = indirectPrepared_.renderedWork;
        lastIndirectStatus_ = 0;
        reportedIndirectStatus_ = 0;
        return true;
    }

    if (indirectPrepared_.instances.empty())
        return rejectIndirect(9, "empty prepared instance stream");
    if (!gpuRes_->instanceVbo() ||
            gpuRes_->instanceUploadSerial() !=
                indirectPrepared_.instanceUploadSerial) {
        gpuRes_->uploadInstanceData(
            indirectPrepared_.instances.data(),
            static_cast<GLsizeiptr>(
                indirectPrepared_.instances.size() * sizeof(InstVertex)),
            glue);
        indirectPrepared_.instanceUploadSerial =
            gpuRes_->instanceUploadSerial();
    }
    const GLuint instanceVbo = gpuRes_->instanceVbo();
    if (!instanceVbo)
        return rejectIndirect(10, "prepared instance upload");

    for (const IndirectPageWork& work : indirectPrepared_.pages) {
        CadTriangleAtlasPage *page =
            gpuRes_->triangleAtlasPage(work.page);
        if (!page || !page->indirectBuf || !page->indirectCapacity ||
                (work.ordinary.empty() && work.culled.empty()))
            return rejectIndirect(11, "prepared page preflight");

        const bool newVao = !page->vao;
        if (newVao)
            glue->glGenVertexArrays(1, &page->vao);
        glue->glBindVertexArray(page->vao);
        if (newVao) {
            glue->glBindBuffer(GL_ARRAY_BUFFER, page->posBuf);
            glue->glVertexAttribPointerARB(
                0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
            glue->glEnableVertexAttribArrayARB(0);
            glue->glBindBuffer(
                GL_ARRAY_BUFFER,
                page->normBuf ? page->normBuf : page->posBuf);
            glue->glVertexAttribPointerARB(
                1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
            glue->glEnableVertexAttribArrayARB(1);
            glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, page->idxBuf);
        }
        if (newVao || page->instanceVbo != instanceVbo) {
            const GLsizei stride =
                static_cast<GLsizei>(sizeof(InstVertex));
            glue->glBindBuffer(GL_ARRAY_BUFFER, instanceVbo);
            for (GLuint column = 0; column < 4; ++column) {
                const GLuint location = kInstTransformLoc + column;
                const uintptr_t offset =
                    offsetof(InstVertex, transform) +
                    column * 4u * sizeof(float);
                glue->glVertexAttribPointerARB(
                    location, 4, GL_FLOAT, GL_FALSE, stride,
                    reinterpret_cast<const GLvoid *>(offset));
                glue->glEnableVertexAttribArrayARB(location);
                glue->glVertexAttribDivisor(location, 1);
            }
            glue->glVertexAttribPointerARB(
                kInstColorLoc, 4, GL_FLOAT, GL_FALSE, stride,
                reinterpret_cast<const GLvoid *>(
                    offsetof(InstVertex, color)));
            glue->glEnableVertexAttribArrayARB(kInstColorLoc);
            glue->glVertexAttribDivisor(kInstColorLoc, 1);
            glue->glVertexAttribPointerARB(
                kInstPopMinLevelLoc, 4, GL_FLOAT, GL_FALSE, stride,
                reinterpret_cast<const GLvoid *>(
                    offsetof(InstVertex, popMinLevel)));
            glue->glEnableVertexAttribArrayARB(kInstPopMinLevelLoc);
            glue->glVertexAttribDivisor(kInstPopMinLevelLoc, 1);
            glue->glVertexAttribPointerARB(
                kInstPopMaxFlagsLoc, 4, GL_FLOAT, GL_FALSE, stride,
                reinterpret_cast<const GLvoid *>(
                    offsetof(InstVertex, popMaxFlags)));
            glue->glEnableVertexAttribArrayARB(kInstPopMaxFlagsLoc);
            glue->glVertexAttribDivisor(kInstPopMaxFlagsLoc, 1);
            page->instanceVbo = instanceVbo;
        }
        glue->glBindVertexArray(0);
    }
    glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
    glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glue->glUseProgramObjectARB(shaders_.shadedIndirect);
    const GLint viewProjection = glue->glGetUniformLocationARB(
        shaders_.shadedIndirect, "u_viewProj");
    glue->glUniformMatrix4fvARB(
        viewProjection, 1, GL_FALSE, viewProj[0]);
    uploadLights(glue, shaders_.shadedIndirect);
    uploadViewFacing(glue, shaders_.shadedIndirect, viewVolume);

    for (const IndirectPageWork& work : indirectPrepared_.pages) {
        const CadTriangleAtlasPage *page =
            gpuRes_->triangleAtlasPage(work.page);
        if (!page) continue;
        glue->glBindVertexArray(page->vao);
        const auto drawCommands =
            [&](const std::vector<CadDrawElementsIndirectCommand>& commands,
                bool cullBackfaces) {
                setCadBackfaceCulling(glue, cullBackfaces);
                size_t offset = 0;
                while (offset < commands.size()) {
                    const size_t count = std::min<size_t>(
                        commands.size() - offset,
                        page->indirectCapacity);
                    if (!gpuRes_->uploadTriangleAtlasCommands(
                            work.page, commands.data() + offset,
                            count, glue))
                        return false;
                    glue->glBindBuffer(
                        GL_DRAW_INDIRECT_BUFFER, page->indirectBuf);
                    glue->glMultiDrawElementsIndirect(
                        GL_TRIANGLES, GL_UNSIGNED_INT, nullptr,
                        static_cast<GLsizei>(count),
                        sizeof(CadDrawElementsIndirectCommand));
                    offset += count;
                }
                return true;
            };
        if (!drawCommands(work.ordinary, false) ||
                !drawCommands(work.culled, true)) {
            glue->glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
            glue->glBindVertexArray(0);
            glue->glUseProgramObjectARB(0);
            gpuRes_->releaseFlatShaded(glue);
            gpuRes_->releaseStandaloneTriangles(glue);
            lastIndirectStatus_ = 12;
            return true;
        }
    }
    glue->glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    glue->glBindVertexArray(0);
    glue->glUseProgramObjectARB(0);
    gpuRes_->releaseFlatShaded(glue);
    gpuRes_->releaseStandaloneTriangles(glue);
    lastRenderedWork_ = indirectPrepared_.renderedWork;
    lastIndirectStatus_ = 0;
    reportedIndirectStatus_ = 0;
    return true;
}

bool CadRendererGL::patchIndirectPreparedAppend(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext *glue,
        const SbMatrix& viewProj)
{
    size_t deadlineWork = 256u;
    if (renderInterruptedAfter(deadlineWork))
        return false;
    const auto fail = [&](const char *reason) {
        if (configuration_->patchDebug)
            std::fprintf(stderr,
                "CadRendererGL append patch detail reason=%s "
                "prepared_append=%llu plan_append=%llu "
                "source_extent=%zu plan_sources=%zu\n",
                reason ? reason : "unknown",
                static_cast<unsigned long long>(
                    indirectPrepared_.appendRevision),
                static_cast<unsigned long long>(
                    plan.appendRevision),
                indirectPrepared_.instanceIndexBySource.size(),
                plan.visibleInstances.size());
        return false;
    };
    if (indirectPrepared_.appendRevision ==
            plan.appendRevision)
        return true;
    if (!glue || !gpuRes_ ||
            indirectPrepared_.appendRevision <
                plan.appendDeltaFloorRevision)
        return fail("journal-floor");

    std::vector<const CadPlanAppendDelta *> deltas;
    for (const CadPlanAppendDelta& delta :
            plan.appendDeltas) {
        if (renderInterruptedAfter(deadlineWork))
            return false;
        if (delta.revision >
                indirectPrepared_.appendRevision)
            deltas.push_back(&delta);
    }
    if (deltas.empty())
        return fail("empty-journal");

    /*
     * Keep append publication cheap without making the retained submission
     * an unbounded journal replay.  Once the shaded tail has grown by either
     * one exact-frame population or a modest startup quantum, ask the caller
     * for one exact preparation.  The anchor then doubles, so the complete
     * cost over a stream is a geometric series (O(final population)), while
     * stale/tombstoned commands, reverse indices, atlas bindings, and packed
     * instances are periodically cross-checked and compacted together.
     *
     * This boundary is based on structural growth, not elapsed time or frame
     * count, so a fast producer does not cause more work than a slow one and
     * camera interaction never triggers it by itself.
     */
    constexpr size_t minimumAppendGrowth = 4096u;
    size_t appendedCandidateCount = 0;
    for (const CadPlanAppendDelta *delta : deltas) {
        if (renderInterruptedAfter(deadlineWork))
            return false;
        if (!delta ||
                delta->shadedItemCount >
                    std::numeric_limits<size_t>::max() -
                        appendedCandidateCount)
            return fail("candidate-overflow");
        appendedCandidateCount += delta->shadedItemCount;
    }
    const size_t appendAnchor =
        indirectPrepared_.appendPatchAnchorInstanceCount;
    const size_t growthAllowance =
        std::max(minimumAppendGrowth, appendAnchor);
    const size_t packedLimit =
        appendAnchor >
                std::numeric_limits<size_t>::max() - growthAllowance ?
            std::numeric_limits<size_t>::max() :
            appendAnchor + growthAllowance;
    if (indirectPrepared_.instances.size() > packedLimit ||
            appendedCandidateCount >
                packedLimit - indirectPrepared_.instances.size()) {
        if (configuration_->patchDebug)
            std::fprintf(stderr,
                "CadRendererGL retained append patch requests geometric "
                "revalidation anchor=%zu packed=%zu candidates=%zu "
                "limit=%zu\n",
                appendAnchor, indirectPrepared_.instances.size(),
                appendedCandidateCount, packedLimit);
        return false;
    }

    const uint32_t noSlot =
        std::numeric_limits<uint32_t>::max();
    size_t sourceExtent =
        indirectPrepared_.instanceIndexBySource.size();
    const size_t priorPackedInstanceCount =
        indirectPrepared_.instances.size();
    const size_t priorPressureProxyCount =
        indirectPrepared_.pressureProxyPoints.size();
    bool pressureProxyAdded = false;
    const ExecutorFrustumPlanes fp =
        extractExecutorFrustumPlanes(viewProj);
    /*
     * The exact path touches every old part before pressure reclamation.
     * This append path deliberately does not rescan them: preserve that
     * already validated working set and fall back to exact preparation only
     * if free/new atlas capacity cannot admit the tail.
     */
    gpuRes_->deferTriangleAtlasReclamation();

    for (const CadPlanAppendDelta *delta : deltas) {
        if (renderInterruptedAfter(deadlineWork))
            return false;
        if (!delta ||
                delta->visibleBegin != sourceExtent ||
                delta->visibleBegin >
                    plan.visibleInstances.size() ||
                delta->visibleCount >
                    plan.visibleInstances.size() -
                        delta->visibleBegin ||
                delta->partBegin >
                    plan.partBindings.size() ||
                delta->partCount >
                    plan.partBindings.size() -
                        delta->partBegin ||
                delta->shadedItemBegin >
                    plan.shadedItems.size() ||
                delta->shadedItemCount >
                    plan.shadedItems.size() -
                        delta->shadedItemBegin)
            return fail("delta-shape");
        for (const uint32_t retired :
                delta->retiredVisibleIndices) {
            if (renderInterruptedAfter(deadlineWork))
                return false;
            if (retired >= sourceExtent)
                return fail("retired-source-range");
            if (retired <
                    indirectPrepared_.
                        instanceIndexBySource.size() &&
                    indirectPrepared_.
                        instanceIndexBySource[retired] != noSlot)
                return fail("retired-packed-instance");
            if (retired <
                    indirectPrepared_.
                        pressureProxyIndexBySource.size() &&
                    indirectPrepared_.
                        pressureProxyIndexBySource[retired] != noSlot)
                return fail("retired-pressure-proxy");
        }

        const size_t newSourceExtent =
            static_cast<size_t>(delta->visibleBegin) +
            delta->visibleCount;
        indirectPrepared_.instanceIndexBySource.resize(
            newSourceExtent, noSlot);
        indirectPrepared_.pressureProxyIndexBySource.resize(
            newSourceExtent, noSlot);
        indirectPrepared_.partByPlanPartIndex.resize(
            static_cast<size_t>(delta->partBegin) +
                delta->partCount,
            noSlot);

        const size_t shadedEnd =
            static_cast<size_t>(delta->shadedItemBegin) +
            delta->shadedItemCount;
        for (size_t itemIndex =
                delta->shadedItemBegin;
                itemIndex < shadedEnd; ++itemIndex) {
            if (renderInterruptedAfter(deadlineWork))
                return false;
            const CadDrawItem& item =
                plan.shadedItems[itemIndex];
            if (!item.instanceCount)
                continue;
            /*
             * Append-only realization batches group by part.  Unique leaves
             * are the scalable fast path; shared occurrence runs retain the
             * exact builder until a run-level append journal is available.
             */
            if (item.instanceCount != 1u ||
                    item.baseInstance < delta->visibleBegin ||
                    item.baseInstance >= newSourceExtent ||
                    item.partIndex < delta->partBegin ||
                    item.partIndex >=
                        static_cast<size_t>(delta->partBegin) +
                            delta->partCount ||
                    item.partIndex >= plan.partBindings.size())
                return fail("item-shape");
            const uint32_t sourceIndex =
                item.baseInstance;
            const CadVisibleInstance& source =
                plan.visibleInstances[sourceIndex];
            if (source.partIndex != item.partIndex)
                return fail("source-part");
            if (!cadInstanceDrawable(
                    plan, item, sourceIndex, CadDrawChannel::Shaded) ||
                    isBoxOutsideExecutorFrustum(
                        source.wbMin, source.wbMax, fp))
                continue;

            const CadPartBinding& binding =
                plan.partBindings[item.partIndex];
            if (!binding.geometry ||
                    !binding.geometry->shaded)
                return fail("missing-geometry");
            const TriMesh& mesh =
                *binding.geometry->shaded;
            uint8_t level = mesh.isProgressive() ?
                cadResolvedProgressiveCut(
                    assembly.effectiveProgressiveCut(
                        source.lodCut),
                    mesh.progressiveMinimumCut,
                    mesh.progressiveResidentCut) :
                15u;
            const size_t vertexCount = mesh.isProgressive() ?
                mesh.positionCountAtCut(level) :
                mesh.positions.size();
            const size_t indexCount = mesh.isProgressive() ?
                mesh.indexCountAtCut(level) :
                mesh.indices.size();
            if (!vertexCount || !indexCount ||
                    vertexCount >
                        std::numeric_limits<uint32_t>::max() ||
                    indexCount >
                        std::numeric_limits<uint32_t>::max())
                return fail("prefix-count");
            uint32_t coverageVertexCount =
                static_cast<uint32_t>(vertexCount);
            uint32_t coverageIndexCount =
                static_cast<uint32_t>(indexCount);
            if (mesh.isProgressive()) {
                coverageVertexCount = static_cast<uint32_t>(
                    mesh.positionCountAtCut(
                        mesh.progressiveMinimumCut));
                coverageIndexCount = static_cast<uint32_t>(
                    mesh.indexCountAtCut(
                        mesh.progressiveMinimumCut));
            }
            const CadTriangleAtlasPart *atlas =
                gpuRes_->upsertTriangleAtlasPart(
                    binding.part, binding.generation,
                    executorPackedVec3fData(mesh.positions),
                    executorPackedVec3fData(mesh.normals),
                    coverageVertexCount, mesh.indices.data(),
                    coverageIndexCount,
                    mesh.isProgressive(), mesh.progressiveLineage,
                    glue, caps_);
            if (atlas &&
                    (atlas->vertexCount < vertexCount ||
                     atlas->indexCount < indexCount)) {
                const CadTriangleAtlasPart *enriched =
                    gpuRes_->upsertTriangleAtlasPart(
                        binding.part, binding.generation,
                        executorPackedVec3fData(mesh.positions),
                        executorPackedVec3fData(mesh.normals),
                        static_cast<uint32_t>(vertexCount),
                        mesh.indices.data(),
                        static_cast<uint32_t>(indexCount),
                        mesh.isProgressive(),
                        mesh.progressiveLineage, glue, caps_);
                if (enriched)
                    atlas = enriched;
            }
            if (!atlas) {
                /*
                 * Atlas pressure is a normal bounded-memory outcome, not an
                 * append-journal failure.  Exact preparation already turns
                 * an eligible unadmitted occurrence into one aggregate
                 * point.  Do the same here so a stream which has reached its
                 * GPU working-set ceiling does not rebuild the entire scene
                 * for every subsequent publication batch.
                 */
                if (!binding.subpixelProxyEligible ||
                        indirectPrepared_.pressureProxyPoints.size() >=
                            std::numeric_limits<uint32_t>::max())
                    return fail("atlas-admission");
                SbVec3f localCenter(0.0f, 0.0f, 0.0f);
                for (const SbVec3f& corner :
                        binding.subpixelProxyCorners)
                    localCenter += corner;
                localCenter /= 8.0f;
                CadSubpixelProxyPoint replacement;
                replacement.position = transformedFlatPoint(
                    localCenter, source.transform);
                replacement.rgba = source.rgba;
                replacement.instanceId = source.instanceId;
                replacement.flags = source.flags;
                const uint32_t proxyIndex =
                    static_cast<uint32_t>(
                        indirectPrepared_.
                            pressureProxyPoints.size());
                indirectPrepared_.pressureProxyPoints.push_back(
                    replacement);
                indirectPrepared_.
                    pressureProxySourceInstanceIndices.push_back(
                        sourceIndex);
                indirectPrepared_.
                    pressureProxyIndexBySource[sourceIndex] =
                        proxyIndex;
                pressureProxyAdded = true;
                continue;
            }
            while (mesh.isProgressive() &&
                    level > mesh.progressiveMinimumCut &&
                    (mesh.positionCountAtCut(level) >
                         atlas->vertexCount ||
                     mesh.indexCountAtCut(level) >
                         atlas->indexCount))
                --level;
            const size_t residentIndexCount =
                mesh.isProgressive() ?
                    mesh.indexCountAtCut(level) :
                    mesh.indices.size();
            if (!residentIndexCount ||
                    residentIndexCount >
                        std::numeric_limits<uint32_t>::max() ||
                    atlas->vertices.first >
                        static_cast<uint32_t>(
                            std::numeric_limits<int32_t>::max()))
                return fail("resident-count");

            InstVertex target = {};
            std::memcpy(
                target.transform, source.transform.data(),
                16 * sizeof(float));
            target.color[0] = source.rgba[0] / 255.0f;
            target.color[1] = source.rgba[1] / 255.0f;
            target.color[2] = source.rgba[2] / 255.0f;
            target.color[3] = source.rgba[3] / 255.0f;
            const SbVec3f minimum = mesh.isProgressive() ?
                mesh.progressiveQuantizationMinimum :
                SbVec3f(0, 0, 0);
            const SbVec3f maximum = mesh.isProgressive() ?
                mesh.progressiveQuantizationMaximum :
                SbVec3f(0, 0, 0);
            for (int axis = 0; axis < 3; ++axis) {
                target.popMinLevel[axis] = minimum[axis];
                target.popMaxFlags[axis] = maximum[axis];
            }
            target.popMinLevel[3] = packedProgressiveQuantization(
                mesh.isProgressive() ? mesh.quantizationAtCut(level) :
                    ProgressiveQuantization());
            target.popMaxFlags[3] =
                (!mesh.normals.empty() ? 1.0f : 0.0f) +
                (mesh.isProgressive() ? 2.0f : 0.0f);
            const uint32_t packedInstance =
                static_cast<uint32_t>(
                    indirectPrepared_.instances.size());
            indirectPrepared_.instances.push_back(target);
            indirectPrepared_.
                sourceInstanceIndices.push_back(sourceIndex);
            indirectPrepared_.
                instanceIndexBySource[sourceIndex] =
                    packedInstance;

            IndirectPageWork *pageWork = nullptr;
            for (IndirectPageWork& candidate :
                    indirectPrepared_.pages) {
                if (candidate.page == atlas->page) {
                    pageWork = &candidate;
                    break;
                }
            }
            if (!pageWork) {
                IndirectPageWork work;
                work.page = atlas->page;
                indirectPrepared_.pages.push_back(
                    std::move(work));
                pageWork =
                    &indirectPrepared_.pages.back();
            }
            auto& commands = item.cullBackfaces ?
                pageWork->culled : pageWork->ordinary;
            CadDrawElementsIndirectCommand command;
            command.count =
                static_cast<uint32_t>(
                    residentIndexCount);
            command.instanceCount = 1u;
            command.firstIndex =
                atlas->indices.first;
            command.baseVertex =
                static_cast<int32_t>(
                    atlas->vertices.first);
            command.baseInstance =
                packedInstance;
            const uint32_t commandIndex =
                static_cast<uint32_t>(
                    commands.size());
            commands.push_back(command);

            IndirectPreparedPart demand;
            demand.part = binding.part;
            demand.partIndex = item.partIndex;
            demand.generation =
                binding.generation;
            demand.vertexCount = std::min(
                static_cast<uint32_t>(vertexCount),
                atlas->vertexCount);
            demand.indexCount = std::min(
                static_cast<uint32_t>(indexCount),
                atlas->indexCount);
            demand.admissionPressure =
                vertexCount > atlas->vertexCount ||
                indexCount > atlas->indexCount;
            if (demand.admissionPressure)
                ++indirectPrepared_.atlasPressurePartCount;
            demand.page = atlas->page;
            demand.vertexFirst =
                atlas->vertices.first;
            demand.indexFirst =
                atlas->indices.first;
            demand.hasNormals =
                !mesh.normals.empty();
            demand.packedInstance =
                packedInstance;
            demand.commandIndex =
                commandIndex;
            demand.commandCulled =
                item.cullBackfaces;
            indirectPrepared_.
                partByPlanPartIndex[item.partIndex] =
                    static_cast<uint32_t>(
                        indirectPrepared_.parts.size());
            indirectPrepared_.parts.push_back(
                demand);
            indirectPrepared_.renderedTriangleCount +=
                residentIndexCount / 3u;
            cadAccumulateRenderedShadedWork(
                indirectPrepared_.renderedWork, mesh, level,
                static_cast<uint64_t>(residentIndexCount / 3u));
        }
        sourceExtent = newSourceExtent;
    }

    const size_t appendedPackedCount =
        indirectPrepared_.instances.size() -
        priorPackedInstanceCount;
    if (appendedPackedCount) {
        const GLintptr byteOffset =
            static_cast<GLintptr>(
                priorPackedInstanceCount *
                sizeof(InstVertex));
        const GLsizeiptr byteCount =
            static_cast<GLsizeiptr>(
                appendedPackedCount *
                sizeof(InstVertex));
        bool uploaded =
            gpuRes_->instanceVbo() &&
            gpuRes_->instanceUploadSerial() ==
                indirectPrepared_.instanceUploadSerial &&
            gpuRes_->appendInstanceData(
                byteOffset,
                indirectPrepared_.instances.data() +
                    priorPackedInstanceCount,
                byteCount, glue);
        if (!uploaded) {
            gpuRes_->uploadInstanceData(
                indirectPrepared_.instances.data(),
                static_cast<GLsizeiptr>(
                    indirectPrepared_.instances.size() *
                    sizeof(InstVertex)),
                glue);
            uploaded =
                gpuRes_->instanceVbo() != 0;
        }
        if (!uploaded)
            return fail("instance-upload");
        indirectPrepared_.instanceUploadSerial =
            gpuRes_->instanceUploadSerial();
    }
    if (pressureProxyAdded) {
        pressureProxyAppendBaseRevision_ =
            pressureProxyRevision_;
        pressureProxyAppendBegin_ =
            priorPressureProxyCount;
        pressureProxyAppendOnly_ = true;
        ++pressureProxyRevision_;
        if (!pressureProxyRevision_)
            pressureProxyRevision_ = 1;
    }

    indirectPrepared_.appendRevision =
        plan.appendRevision;
    indirectPrepared_.planRevision =
        plan.revision;
    indirectPrepared_.geometryRevision =
        plan.geometryRevision;
    indirectPrepared_.shadedLayoutRevision =
        plan.shadedLayoutRevision;
    indirectPrepared_.subpixelProxyRevision =
        plan.subpixelProxyRevision;
    indirectPrepared_.atlasRevision =
        gpuRes_->triangleAtlasRevision();
    indirectPrepared_.atlasValidationCountdown = 30u;
    indirectPrepared_.atlasAdmissionPressure =
        indirectPrepared_.atlasPressurePartCount > 0 ||
        !indirectPrepared_.pressureProxyPoints.empty();
    return true;
}

bool CadRendererGL::patchIndirectPreparedGeometry(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext *glue)
{
    size_t deadlineWork = 256u;
    if (renderInterruptedAfter(deadlineWork))
        return false;
    const auto fail = [&](const char *reason) {
        if (configuration_->patchDebug)
            std::fprintf(stderr,
                "CadRendererGL geometry patch detail reason=%s\n",
                reason ? reason : "unknown");
        return false;
    };
    if (indirectPrepared_.partGeometryRevision ==
            plan.partGeometryRevision)
        return true;
    if (!glue || !gpuRes_ ||
            indirectPrepared_.partGeometryRevision <
                plan.partGeometryDeltaFloorRevision)
        return fail("journal-floor");

    std::vector<CadPartGeometryRange> changedRanges;
    for (const CadPartGeometryDelta& delta :
            plan.partGeometryDeltas) {
        if (renderInterruptedAfter(deadlineWork))
            return false;
        if (delta.revision <=
                indirectPrepared_.partGeometryRevision)
            continue;
        changedRanges.insert(
            changedRanges.end(),
            delta.ranges.begin(), delta.ranges.end());
    }
    if (changedRanges.empty())
        return fail("empty-journal");
    std::sort(changedRanges.begin(), changedRanges.end(),
        [](const auto& left, const auto& right) {
            if (left.partIndex != right.partIndex)
                return left.partIndex < right.partIndex;
            return left.baseInstance < right.baseInstance;
        });
    changedRanges.erase(
        std::unique(changedRanges.begin(), changedRanges.end(),
            [](const auto& left, const auto& right) {
                return left.partIndex == right.partIndex &&
                    left.baseInstance == right.baseInstance;
            }),
        changedRanges.end());

    const uint32_t noSlot =
        std::numeric_limits<uint32_t>::max();
    const ExecutorFrustumPlanes fp =
        extractExecutorFrustumPlanes(
            indirectPrepared_.viewProj);
    std::vector<uint32_t> changedPackedInstances;
    changedPackedInstances.reserve(changedRanges.size());
    gpuRes_->deferTriangleAtlasReclamation();

    for (const CadPartGeometryRange& range :
            changedRanges) {
        if (renderInterruptedAfter(deadlineWork))
            return false;
        /*
         * Wire/point-only part updates are consumed by their own retained
         * paths and do not alter this shaded indirect submission.
         */
        if (!range.shadedItemCount)
            continue;
        if (range.instanceCount != 1u ||
                range.shadedItemCount != 1u ||
                range.baseInstance >=
                    plan.visibleInstances.size() ||
                range.partIndex >=
                    plan.partBindings.size() ||
                range.shadedItemBegin >=
                    plan.shadedItems.size() ||
                range.partIndex >=
                    indirectPrepared_.
                        partByPlanPartIndex.size())
            return fail("range-shape");
        const uint32_t sourceIndex =
            range.baseInstance;
        const CadVisibleInstance& source =
            plan.visibleInstances[sourceIndex];
        const uint32_t preparedPartIndex =
            indirectPrepared_.
                partByPlanPartIndex[range.partIndex];
        const bool shouldDraw =
            !(source.flags & CadInstanceHidden) &&
            !cadInstanceSubpixelReplaced(
                plan, sourceIndex) &&
            !isBoxOutsideExecutorFrustum(
                source.wbMin, source.wbMax, fp);
        if (!shouldDraw) {
            if (preparedPartIndex != noSlot)
                return fail("demotion");
            continue;
        }
        if (preparedPartIndex == noSlot ||
                preparedPartIndex >=
                    indirectPrepared_.parts.size()) {
            /*
             * An exact admission pass may intentionally represent this
             * occurrence with the aggregate pressure proxy.  A richer PoP
             * generation does not promote it past the scene budget; retain
             * the proxy and consume the geometry journal entry.
             */
            if (sourceIndex <
                    indirectPrepared_.
                        pressureProxyIndexBySource.size() &&
                    indirectPrepared_.
                        pressureProxyIndexBySource[sourceIndex] != noSlot)
                continue;
            return fail("promotion");
        }
        IndirectPreparedPart& demand =
            indirectPrepared_.parts[
                preparedPartIndex];
        const CadPartBinding& binding =
            plan.partBindings[range.partIndex];
        if (!(demand.part == binding.part) ||
                demand.partIndex != range.partIndex ||
                demand.packedInstance == noSlot ||
                demand.packedInstance >=
                    indirectPrepared_.instances.size() ||
                demand.commandIndex == noSlot ||
                !binding.geometry ||
                !binding.geometry->shaded)
            return fail("prepared-binding");
        const TriMesh& mesh =
            *binding.geometry->shaded;
        uint8_t level = mesh.isProgressive() ?
            cadResolvedProgressiveCut(
                assembly.effectiveProgressiveCut(
                    source.lodCut),
                mesh.progressiveMinimumCut,
                mesh.progressiveResidentCut) :
            15u;
        const size_t vertexCount = mesh.isProgressive() ?
            mesh.positionCountAtCut(level) :
            mesh.positions.size();
        const size_t indexCount = mesh.isProgressive() ?
            mesh.indexCountAtCut(level) :
            mesh.indices.size();
        if (!vertexCount || !indexCount ||
                vertexCount >
                    std::numeric_limits<uint32_t>::max() ||
                indexCount >
                    std::numeric_limits<uint32_t>::max())
            return fail("counts");
        const CadTriangleAtlasPart *atlas =
            gpuRes_->upsertTriangleAtlasPart(
                binding.part, binding.generation,
                executorPackedVec3fData(mesh.positions),
                executorPackedVec3fData(mesh.normals),
                static_cast<uint32_t>(vertexCount),
                mesh.indices.data(),
                static_cast<uint32_t>(indexCount),
                mesh.isProgressive(), mesh.progressiveLineage,
                glue, caps_);
        if (!atlas)
            return fail("atlas-admission");
        while (mesh.isProgressive() &&
                level > mesh.progressiveMinimumCut &&
                (mesh.positionCountAtCut(level) >
                     atlas->vertexCount ||
                 mesh.indexCountAtCut(level) >
                     atlas->indexCount))
            --level;
        const size_t residentIndexCount =
            mesh.isProgressive() ?
                mesh.indexCountAtCut(level) :
                mesh.indices.size();
        if (!residentIndexCount ||
                residentIndexCount >
                    std::numeric_limits<uint32_t>::max() ||
                atlas->vertices.first >
                    static_cast<uint32_t>(
                        std::numeric_limits<int32_t>::max()))
            return fail("resident-count");

        IndirectPageWork *oldPageWork = nullptr;
        for (IndirectPageWork& candidate :
                indirectPrepared_.pages) {
            if (candidate.page == demand.page) {
                oldPageWork = &candidate;
                break;
            }
        }
        if (!oldPageWork)
            return fail("old-page");
        auto& oldCommands = demand.commandCulled ?
            oldPageWork->culled :
            oldPageWork->ordinary;
        if (demand.commandIndex >=
                oldCommands.size())
            return fail("old-command-index");
        CadDrawElementsIndirectCommand& oldCommand =
            oldCommands[demand.commandIndex];
        if (oldCommand.instanceCount != 1u ||
                oldCommand.baseInstance !=
                    demand.packedInstance)
            return fail("old-command-shape");
        const uint64_t oldTriangles =
            oldCommand.count / 3u;
        const uint64_t oldPositions = demand.vertexCount;
        const bool oldHasNormals = demand.hasNormals;

        IndirectPageWork *newPageWork = nullptr;
        for (IndirectPageWork& candidate :
                indirectPrepared_.pages) {
            if (candidate.page == atlas->page) {
                newPageWork = &candidate;
                break;
            }
        }
        if (!newPageWork) {
            IndirectPageWork work;
            work.page = atlas->page;
            indirectPrepared_.pages.push_back(
                std::move(work));
            newPageWork =
                &indirectPrepared_.pages.back();
            /*
             * The outer-vector append may have invalidated oldPageWork.
             * Resolve it again before retiring the preceding command.
             */
            oldPageWork = nullptr;
            for (IndirectPageWork& candidate :
                    indirectPrepared_.pages) {
                if (candidate.page == demand.page) {
                    oldPageWork = &candidate;
                    break;
                }
            }
            if (!oldPageWork)
                return fail("old-page-relocation");
        }
        auto& refreshedOldCommands =
            demand.commandCulled ?
                oldPageWork->culled :
                oldPageWork->ordinary;
        if (demand.commandIndex >=
                refreshedOldCommands.size())
            return fail("refreshed-command-index");
        refreshedOldCommands[
            demand.commandIndex].instanceCount = 0u;

        const bool commandCulled =
            plan.shadedItems[
                range.shadedItemBegin].cullBackfaces;
        auto& newCommands = commandCulled ?
            newPageWork->culled :
            newPageWork->ordinary;
        CadDrawElementsIndirectCommand command;
        command.count =
            static_cast<uint32_t>(
                residentIndexCount);
        command.instanceCount = 1u;
        command.firstIndex =
            atlas->indices.first;
        command.baseVertex =
            static_cast<int32_t>(
                atlas->vertices.first);
        command.baseInstance =
            demand.packedInstance;
        const uint32_t commandIndex =
            static_cast<uint32_t>(
                newCommands.size());
        newCommands.push_back(command);

        InstVertex& target =
            indirectPrepared_.instances[
                demand.packedInstance];
        const SbVec3f minimum = mesh.isProgressive() ?
            mesh.progressiveQuantizationMinimum :
            SbVec3f(0, 0, 0);
        const SbVec3f maximum = mesh.isProgressive() ?
            mesh.progressiveQuantizationMaximum :
            SbVec3f(0, 0, 0);
        for (int axis = 0; axis < 3; ++axis) {
            target.popMinLevel[axis] = minimum[axis];
            target.popMaxFlags[axis] = maximum[axis];
        }
        target.popMinLevel[3] = packedProgressiveQuantization(
            mesh.isProgressive() ? mesh.quantizationAtCut(level) :
                ProgressiveQuantization());
        target.popMaxFlags[3] =
            (!mesh.normals.empty() ? 1.0f : 0.0f) +
            (mesh.isProgressive() ? 2.0f : 0.0f);
        changedPackedInstances.push_back(
            demand.packedInstance);

        demand.generation =
            binding.generation;
        const bool admissionPressure =
            vertexCount > atlas->vertexCount ||
            indexCount > atlas->indexCount;
        if (demand.admissionPressure != admissionPressure) {
            if (admissionPressure) {
                ++indirectPrepared_.atlasPressurePartCount;
            } else if (indirectPrepared_.atlasPressurePartCount) {
                --indirectPrepared_.atlasPressurePartCount;
            }
            demand.admissionPressure = admissionPressure;
        }
        demand.vertexCount = std::min(
            static_cast<uint32_t>(vertexCount),
            atlas->vertexCount);
        demand.indexCount = std::min(
            static_cast<uint32_t>(indexCount),
            atlas->indexCount);
        demand.page = atlas->page;
        demand.vertexFirst =
            atlas->vertices.first;
        demand.indexFirst =
            atlas->indices.first;
        demand.hasNormals =
            !mesh.normals.empty();
        demand.commandIndex =
            commandIndex;
        demand.commandCulled =
            commandCulled;
        const uint64_t newTriangles =
            residentIndexCount / 3u;
        indirectPrepared_.renderedTriangleCount =
            oldTriangles <=
                    indirectPrepared_.
                        renderedTriangleCount ?
                indirectPrepared_.
                    renderedTriangleCount -
                    oldTriangles + newTriangles :
                newTriangles;
        cadReplacePreparedShadedWork(
            indirectPrepared_.renderedWork,
            oldTriangles, oldPositions, oldHasNormals,
            newTriangles,
            static_cast<uint64_t>(
                mesh.isProgressive() ?
                    mesh.positionCountAtCut(level) :
                    mesh.positions.size()),
            !mesh.normals.empty());
    }

    indirectPrepared_.pages.erase(
        std::remove_if(
            indirectPrepared_.pages.begin(),
            indirectPrepared_.pages.end(),
            [&](const IndirectPageWork& work) {
                if (!gpuRes_->triangleAtlasPage(
                        work.page))
                    return true;
                const auto active =
                    [](const auto& commands) {
                        return std::any_of(
                            commands.begin(),
                            commands.end(),
                            [](const auto& command) {
                                return command.instanceCount != 0u;
                            });
                    };
                return !active(work.ordinary) &&
                    !active(work.culled);
            }),
        indirectPrepared_.pages.end());

    std::sort(changedPackedInstances.begin(),
        changedPackedInstances.end());
    changedPackedInstances.erase(
        std::unique(changedPackedInstances.begin(),
            changedPackedInstances.end()),
        changedPackedInstances.end());
    bool sparseUpload =
        changedPackedInstances.empty() ||
        (gpuRes_->instanceVbo() &&
         gpuRes_->instanceUploadSerial() ==
             indirectPrepared_.instanceUploadSerial);
    size_t begin = 0;
    while (sparseUpload &&
            begin < changedPackedInstances.size()) {
        if (renderInterruptedAfter(deadlineWork))
            return false;
        size_t end = begin + 1u;
        while (end < changedPackedInstances.size() &&
                changedPackedInstances[end] ==
                    changedPackedInstances[end - 1u] + 1u)
            ++end;
        const uint32_t first =
            changedPackedInstances[begin];
        sparseUpload = gpuRes_->updateInstanceData(
            static_cast<GLintptr>(first) *
                sizeof(InstVertex),
            indirectPrepared_.instances.data() + first,
            static_cast<GLsizeiptr>(end - begin) *
                sizeof(InstVertex),
            glue);
        begin = end;
    }
    if (sparseUpload)
        indirectPrepared_.instanceUploadSerial =
            gpuRes_->instanceUploadSerial();
    else
        indirectPrepared_.instanceUploadSerial = 0;

    indirectPrepared_.partGeometryRevision =
        plan.partGeometryRevision;
    indirectPrepared_.planRevision =
        plan.revision;
    indirectPrepared_.geometryRevision =
        plan.geometryRevision;
    indirectPrepared_.shadedLayoutRevision =
        plan.shadedLayoutRevision;
    indirectPrepared_.subpixelProxyRevision =
        plan.subpixelProxyRevision;
    indirectPrepared_.atlasRevision =
        gpuRes_->triangleAtlasRevision();
    indirectPrepared_.atlasValidationCountdown = 30u;
    indirectPrepared_.atlasAdmissionPressure =
        indirectPrepared_.atlasPressurePartCount > 0 ||
        !indirectPrepared_.pressureProxyPoints.empty();
    return true;
}

bool CadRendererGL::patchIndirectPreparedCeiling(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext *glue)
{
    size_t deadlineWork = 256u;
    if (renderInterruptedAfter(deadlineWork))
        return false;
    const int ceiling =
        assembly.progressiveCutCeiling.getValue();
    if (indirectPrepared_.progressiveCutCeiling ==
            ceiling)
        return true;
    if (!glue || !gpuRes_)
        return false;

    const uint32_t noSlot =
        std::numeric_limits<uint32_t>::max();
    std::vector<uint32_t> changedPackedInstances;
    changedPackedInstances.reserve(
        indirectPrepared_.parts.size());
    for (IndirectPreparedPart& demand :
            indirectPrepared_.parts) {
        if (renderInterruptedAfter(deadlineWork))
            return false;
        if (demand.partIndex >=
                plan.partBindings.size())
            return false;
        const CadPartBinding& binding =
            plan.partBindings[demand.partIndex];
        if (!(binding.part == demand.part) ||
                !binding.geometry ||
                !binding.geometry->shaded)
            return false;
        const TriMesh& mesh =
            *binding.geometry->shaded;
        if (!mesh.isProgressive())
            continue;
        if (demand.packedInstance == noSlot ||
                demand.packedInstance >=
                    indirectPrepared_.instances.size() ||
                demand.packedInstance >=
                    indirectPrepared_.
                        sourceInstanceIndices.size() ||
                demand.commandIndex == noSlot)
            return false;
        const uint32_t sourceIndex =
            indirectPrepared_.
                sourceInstanceIndices[
                    demand.packedInstance];
        if (sourceIndex >=
                plan.visibleInstances.size())
            return false;
        uint8_t level = cadResolvedProgressiveCut(
            assembly.effectiveProgressiveCut(
                plan.visibleInstances[sourceIndex].lodCut),
            mesh.progressiveMinimumCut,
            mesh.progressiveResidentCut);
        const CadTriangleAtlasPart *atlas =
            gpuRes_->triangleAtlasPart(
                binding.part);
        if (!atlas || atlas->page != demand.page ||
                atlas->vertices.first !=
                    demand.vertexFirst ||
                atlas->indices.first !=
                    demand.indexFirst)
            return false;
        const uint32_t requestedVertices = static_cast<uint32_t>(
            mesh.positionCountAtCut(level));
        const uint32_t requestedIndices = static_cast<uint32_t>(
            mesh.indexCountAtCut(level));
        const bool admissionPressure =
            requestedVertices > atlas->vertexCount ||
            requestedIndices > atlas->indexCount;
        if (demand.admissionPressure != admissionPressure) {
            if (admissionPressure) {
                ++indirectPrepared_.atlasPressurePartCount;
            } else if (indirectPrepared_.atlasPressurePartCount) {
                --indirectPrepared_.atlasPressurePartCount;
            }
            demand.admissionPressure = admissionPressure;
        }
        while (level > mesh.progressiveMinimumCut &&
                (mesh.positionCountAtCut(level) >
                     atlas->vertexCount ||
                 mesh.indexCountAtCut(level) >
                     atlas->indexCount))
            --level;
        const size_t commandCount =
            mesh.indexCountAtCut(level);
        if (!commandCount ||
                commandCount >
                    std::numeric_limits<uint32_t>::max())
            return false;
        IndirectPageWork *pageWork = nullptr;
        for (IndirectPageWork& candidate :
                indirectPrepared_.pages) {
            if (candidate.page == demand.page) {
                pageWork = &candidate;
                break;
            }
        }
        if (!pageWork)
            return false;
        auto& commands = demand.commandCulled ?
            pageWork->culled : pageWork->ordinary;
        if (demand.commandIndex >=
                commands.size())
            return false;
        CadDrawElementsIndirectCommand& command =
            commands[demand.commandIndex];
        if (command.instanceCount != 1u ||
                command.baseInstance !=
                    demand.packedInstance)
            return false;
        const uint64_t oldTriangles =
            command.count / 3u;
        const uint64_t newTriangles =
            commandCount / 3u;
        const uint64_t oldPositions = demand.vertexCount;
        const uint64_t newPositions =
            static_cast<uint64_t>(mesh.positionCountAtCut(level));
        indirectPrepared_.renderedTriangleCount =
            oldTriangles <=
                    indirectPrepared_.
                        renderedTriangleCount ?
                indirectPrepared_.
                    renderedTriangleCount -
                    oldTriangles + newTriangles :
                newTriangles;
        cadReplacePreparedShadedWork(
            indirectPrepared_.renderedWork,
            oldTriangles, oldPositions, demand.hasNormals,
            newTriangles, newPositions, demand.hasNormals);
        command.count =
            static_cast<uint32_t>(commandCount);
        indirectPrepared_.instances[
            demand.packedInstance].
                popMinLevel[3] =
                    packedProgressiveQuantization(
                        mesh.quantizationAtCut(level));
        demand.vertexCount =
            static_cast<uint32_t>(
                mesh.positionCountAtCut(level));
        demand.indexCount =
            static_cast<uint32_t>(commandCount);
        changedPackedInstances.push_back(
            demand.packedInstance);
    }

    std::sort(changedPackedInstances.begin(),
        changedPackedInstances.end());
    changedPackedInstances.erase(
        std::unique(changedPackedInstances.begin(),
            changedPackedInstances.end()),
        changedPackedInstances.end());
    bool sparseUpload =
        changedPackedInstances.empty() ||
        (gpuRes_->instanceVbo() &&
         gpuRes_->instanceUploadSerial() ==
             indirectPrepared_.instanceUploadSerial);
    size_t begin = 0;
    while (sparseUpload &&
            begin < changedPackedInstances.size()) {
        if (renderInterruptedAfter(deadlineWork))
            return false;
        size_t end = begin + 1u;
        while (end < changedPackedInstances.size() &&
                changedPackedInstances[end] ==
                    changedPackedInstances[end - 1u] + 1u)
            ++end;
        const uint32_t first =
            changedPackedInstances[begin];
        sparseUpload = gpuRes_->updateInstanceData(
            static_cast<GLintptr>(first) *
                sizeof(InstVertex),
            indirectPrepared_.instances.data() + first,
            static_cast<GLsizeiptr>(end - begin) *
                sizeof(InstVertex),
            glue);
        begin = end;
    }
    if (sparseUpload)
        indirectPrepared_.instanceUploadSerial =
            gpuRes_->instanceUploadSerial();
    else
        indirectPrepared_.instanceUploadSerial = 0;
    indirectPrepared_.progressiveCutCeiling =
        ceiling;
    indirectPrepared_.atlasAdmissionPressure =
        indirectPrepared_.atlasPressurePartCount > 0 ||
        !indirectPrepared_.pressureProxyPoints.empty();
    return true;
}

bool CadRendererGL::patchIndirectPreparedCuts(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext *glue)
{
    size_t deadlineWork = 256u;
    if (renderInterruptedAfter(deadlineWork))
        return false;
    if (indirectPrepared_.shadedLodRevision ==
            plan.shadedLodRevision)
        return true;
    if (!glue || !gpuRes_ ||
            indirectPrepared_.shadedLodRevision <
                plan.shadedLodDeltaFloorRevision)
        return false;

    std::vector<CadShadedLodRange> changedRanges;
    for (const CadShadedLodDelta& delta : plan.shadedLodDeltas) {
        if (renderInterruptedAfter(deadlineWork))
            return false;
        if (delta.revision <=
                indirectPrepared_.shadedLodRevision)
            continue;
        changedRanges.insert(
            changedRanges.end(),
            delta.ranges.begin(), delta.ranges.end());
    }
    if (changedRanges.empty())
        return false;
    std::sort(changedRanges.begin(), changedRanges.end(),
        [](const auto& left, const auto& right) {
            if (left.partIndex != right.partIndex)
                return left.partIndex < right.partIndex;
            return left.baseInstance < right.baseInstance;
        });
    changedRanges.erase(
        std::unique(changedRanges.begin(), changedRanges.end(),
            [](const auto& left, const auto& right) {
                return left.partIndex == right.partIndex &&
                    left.baseInstance == right.baseInstance;
            }),
        changedRanges.end());

    const uint32_t noSlot =
        std::numeric_limits<uint32_t>::max();
    std::vector<uint32_t> changedPackedInstances;
    changedPackedInstances.reserve(changedRanges.size());
    for (const CadShadedLodRange& range : changedRanges) {
        if (renderInterruptedAfter(deadlineWork))
            return false;
        /*
         * The high-asset-count vehicle case overwhelmingly consists of
         * unique leaves.  Their prepared instance and command slots are
         * invariant under a level change.  A shared part may reorder several
         * occurrences across level bins; retain the exact path for that
         * materially different operation until it has its own run journal.
         */
        if (range.instanceCount != 1u ||
                range.baseInstance >=
                    plan.visibleInstances.size() ||
                range.partIndex >= plan.partBindings.size() ||
                range.shadedItemBegin >= plan.shadedItems.size() ||
                range.shadedItemCount != 1u)
            return false;
        const uint32_t sourceIndex = range.baseInstance;
        const CadVisibleInstance& source =
            plan.visibleInstances[sourceIndex];
        if (source.partIndex != range.partIndex)
            return false;

        if (range.partIndex >=
                indirectPrepared_.partByPlanPartIndex.size())
            return false;
        const uint32_t preparedPartIndex =
            indirectPrepared_.
                partByPlanPartIndex[range.partIndex];
        if (preparedPartIndex == noSlot) {
            /*
             * The sole occurrence was outside the prepared view, collapsed
             * into a point, or represented by a pressure proxy.  Its PoP cut
             * cannot affect this retained submission.
             */
            if (sourceIndex <
                    indirectPrepared_.instanceIndexBySource.size() &&
                    indirectPrepared_.
                        instanceIndexBySource[sourceIndex] != noSlot)
                return false;
            continue;
        }
        if (preparedPartIndex >=
                indirectPrepared_.parts.size())
            return false;
        IndirectPreparedPart& demand =
            indirectPrepared_.parts[preparedPartIndex];
        const CadPartBinding& binding =
            plan.partBindings[range.partIndex];
        if (!(demand.part == binding.part) ||
                demand.partIndex != range.partIndex ||
                demand.generation != binding.generation ||
                demand.packedInstance == noSlot ||
                demand.packedInstance >=
                    indirectPrepared_.instances.size() ||
                demand.commandIndex == noSlot ||
                demand.packedInstance >=
                    indirectPrepared_.
                        sourceInstanceIndices.size() ||
                indirectPrepared_.
                    sourceInstanceIndices[demand.packedInstance] !=
                        sourceIndex)
            return false;
        if (!binding.geometry || !binding.geometry->shaded ||
                !binding.geometry->shaded->isProgressive())
            return false;
        const TriMesh& mesh = *binding.geometry->shaded;
        uint8_t level = cadResolvedProgressiveCut(
            assembly.effectiveProgressiveCut(source.lodCut),
            mesh.progressiveMinimumCut,
            mesh.progressiveResidentCut);
        const uint32_t requestedVertices =
            static_cast<uint32_t>(
                mesh.positionCountAtCut(level));
        const uint32_t requestedIndices =
            static_cast<uint32_t>(
                mesh.indexCountAtCut(level));
        if (!requestedVertices || !requestedIndices)
            return false;

        const CadTriangleAtlasPart *atlas =
            gpuRes_->touchTriangleAtlasPart(
                binding.part, binding.generation,
                !mesh.normals.empty(),
                requestedVertices, requestedIndices);
        if (!atlas)
            atlas = gpuRes_->upsertTriangleAtlasPart(
                binding.part, binding.generation,
                executorPackedVec3fData(mesh.positions),
                executorPackedVec3fData(mesh.normals),
                requestedVertices, mesh.indices.data(),
                requestedIndices, true, mesh.progressiveLineage,
                glue, caps_);
        if (!atlas || atlas->page != demand.page ||
                atlas->vertices.first != demand.vertexFirst ||
                atlas->indices.first != demand.indexFirst)
            return false;

        const bool admissionPressure =
            requestedVertices > atlas->vertexCount ||
            requestedIndices > atlas->indexCount;
        if (demand.admissionPressure != admissionPressure) {
            if (admissionPressure) {
                ++indirectPrepared_.atlasPressurePartCount;
            } else if (indirectPrepared_.atlasPressurePartCount) {
                --indirectPrepared_.atlasPressurePartCount;
            }
            demand.admissionPressure = admissionPressure;
        }

        while (level > mesh.progressiveMinimumCut &&
                (mesh.positionCountAtCut(level) >
                     atlas->vertexCount ||
                 mesh.indexCountAtCut(level) >
                     atlas->indexCount))
            --level;
        const size_t commandCount =
            mesh.indexCountAtCut(level);
        if (!commandCount ||
                commandCount >
                    std::numeric_limits<uint32_t>::max())
            return false;

        IndirectPageWork *pageWork = nullptr;
        for (IndirectPageWork& candidate :
                indirectPrepared_.pages) {
            if (candidate.page == demand.page) {
                pageWork = &candidate;
                break;
            }
        }
        if (!pageWork)
            return false;
        auto& commands = demand.commandCulled ?
            pageWork->culled : pageWork->ordinary;
        if (demand.commandIndex >= commands.size())
            return false;
        CadDrawElementsIndirectCommand& command =
            commands[demand.commandIndex];
        if (command.instanceCount != 1u ||
                command.baseInstance !=
                    demand.packedInstance)
            return false;

        const uint64_t oldTriangles =
            static_cast<uint64_t>(command.count / 3u);
        const uint64_t newTriangles =
            static_cast<uint64_t>(commandCount / 3u);
        const uint64_t oldPositions = demand.vertexCount;
        const uint64_t newPositions =
            static_cast<uint64_t>(mesh.positionCountAtCut(level));
        indirectPrepared_.renderedTriangleCount =
            oldTriangles <=
                    indirectPrepared_.renderedTriangleCount ?
                indirectPrepared_.renderedTriangleCount -
                    oldTriangles + newTriangles :
                newTriangles;
        cadReplacePreparedShadedWork(
            indirectPrepared_.renderedWork,
            oldTriangles, oldPositions, demand.hasNormals,
            newTriangles, newPositions, demand.hasNormals);
        command.count =
            static_cast<uint32_t>(commandCount);
        InstVertex& target =
            indirectPrepared_.instances[
                demand.packedInstance];
        target.popMinLevel[3] =
            packedProgressiveQuantization(
                mesh.quantizationAtCut(level));
        demand.vertexCount = std::min(
            requestedVertices, atlas->vertexCount);
        demand.indexCount = std::min(
            requestedIndices, atlas->indexCount);
        changedPackedInstances.push_back(
            demand.packedInstance);
    }

    std::sort(changedPackedInstances.begin(),
        changedPackedInstances.end());
    changedPackedInstances.erase(
        std::unique(changedPackedInstances.begin(),
            changedPackedInstances.end()),
        changedPackedInstances.end());
    bool sparseUpload =
        changedPackedInstances.empty() ||
        (gpuRes_->instanceVbo() &&
         gpuRes_->instanceUploadSerial() ==
             indirectPrepared_.instanceUploadSerial);
    size_t begin = 0;
    while (sparseUpload &&
            begin < changedPackedInstances.size()) {
        if (renderInterruptedAfter(deadlineWork))
            return false;
        size_t end = begin + 1u;
        while (end < changedPackedInstances.size() &&
                changedPackedInstances[end] ==
                    changedPackedInstances[end - 1u] + 1u)
            ++end;
        const uint32_t first =
            changedPackedInstances[begin];
        sparseUpload = gpuRes_->updateInstanceData(
            static_cast<GLintptr>(first) *
                sizeof(InstVertex),
            indirectPrepared_.instances.data() + first,
            static_cast<GLsizeiptr>(end - begin) *
                sizeof(InstVertex),
            glue);
        begin = end;
    }
    if (sparseUpload)
        indirectPrepared_.instanceUploadSerial =
            gpuRes_->instanceUploadSerial();
    else
        indirectPrepared_.instanceUploadSerial = 0;

    indirectPrepared_.shadedLodRevision =
        plan.shadedLodRevision;
    indirectPrepared_.planRevision = plan.revision;
    indirectPrepared_.atlasRevision =
        gpuRes_->triangleAtlasRevision();
    indirectPrepared_.atlasValidationCountdown = 30u;
    indirectPrepared_.atlasAdmissionPressure =
        indirectPrepared_.atlasPressurePartCount > 0 ||
        !indirectPrepared_.pressureProxyPoints.empty();
    return true;
}

bool CadRendererGL::replayIndirectShaded(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext *glue,
        const SbMatrix& viewProj,
        const SbViewVolume& viewVolume)
{
    size_t deadlineWork = 256u;
    if (renderInterruptedAfter(deadlineWork))
        return false;
    const bool cameraChanged = !(indirectPrepared_.viewProj == viewProj);
    const bool subpixelProxyChanged =
        indirectPrepared_.subpixelProxyRevision !=
            plan.subpixelProxyRevision;
    bool mayReplayVisibilityProxyChange = false;
    if (!cameraChanged && subpixelProxyChanged &&
            indirectPrepared_.instanceAttributeRevision !=
                plan.instanceAttributeRevision &&
            indirectPrepared_.instanceAttributeRevision >=
                plan.instanceAttributeDeltaFloorRevision) {
        for (const CadInstanceAttributeDelta& delta :
                plan.instanceAttributeDeltas) {
            if (renderInterruptedAfter(deadlineWork))
                return false;
            if (delta.revision >
                    indirectPrepared_.instanceAttributeRevision &&
                    delta.visibilityChanged) {
                mayReplayVisibilityProxyChange = true;
                break;
            }
        }
    }
    /*
     * Camera motion never makes immutable geometry invalid.  When the view
     * controller has established mesh coverage it asks us to preserve this
     * coherent prepared cut for the whole input burst.  Event-count or timer
     * refreshes injected recurring O(scene) 30-50 ms stalls and made a faster
     * mouse produce worse FPS.  The controller clears the hint for the first
     * quiet frame, or sooner if its bounded coverage pass discovers newly
     * visible geometry which has no mesh presentation.
     */
    const bool mayReuseChangedCamera =
        cameraChanged && assembly.cameraMotionFrameReuse.getValue();
    const bool appendChanged =
        indirectPrepared_.appendRevision !=
            plan.appendRevision;
    const bool mayPatchAppend =
        appendChanged &&
        indirectPrepared_.appendRevision >=
            plan.appendDeltaFloorRevision;
    const bool partGeometryChanged =
        indirectPrepared_.partGeometryRevision !=
            plan.partGeometryRevision;
    const bool mayPatchPartGeometry =
        partGeometryChanged &&
        indirectPrepared_.partGeometryRevision >=
            plan.partGeometryDeltaFloorRevision;
    if (!indirectPrepared_.valid ||
            indirectPrepared_.contextId != glue->contextid ||
            ((indirectPrepared_.geometryRevision !=
                  plan.geometryRevision ||
              indirectPrepared_.shadedLayoutRevision !=
                  plan.shadedLayoutRevision) &&
                !mayPatchAppend &&
                !mayPatchPartGeometry) ||
            (subpixelProxyChanged &&
                !mayReplayVisibilityProxyChange &&
                !mayPatchAppend &&
                !mayPatchPartGeometry) ||
            (cameraChanged && !mayReuseChangedCamera)) {
        if (configuration_->patchDebug)
            std::fprintf(stderr,
                "CadRendererGL retained patch gate exact "
                "valid=%d context=%d geometry=%d layout=%d "
                "subpixel=%d append_changed=%d append_patch=%d "
                "append_rev=%llu/%llu/%llu "
                "geometry_changed=%d geometry_patch=%d "
                "geometry_rev=%llu/%llu/%llu "
                "camera=%d camera_reuse=%d\n",
                indirectPrepared_.valid ? 1 : 0,
                indirectPrepared_.contextId == glue->contextid ? 1 : 0,
                indirectPrepared_.geometryRevision ==
                    plan.geometryRevision ? 1 : 0,
                indirectPrepared_.shadedLayoutRevision ==
                    plan.shadedLayoutRevision ? 1 : 0,
                subpixelProxyChanged ? 1 : 0,
                appendChanged ? 1 : 0,
                mayPatchAppend ? 1 : 0,
                static_cast<unsigned long long>(
                    indirectPrepared_.appendRevision),
                static_cast<unsigned long long>(
                    plan.appendRevision),
                static_cast<unsigned long long>(
                    plan.appendDeltaFloorRevision),
                partGeometryChanged ? 1 : 0,
                mayPatchPartGeometry ? 1 : 0,
                static_cast<unsigned long long>(
                    indirectPrepared_.partGeometryRevision),
                static_cast<unsigned long long>(
                    plan.partGeometryRevision),
                static_cast<unsigned long long>(
                    plan.partGeometryDeltaFloorRevision),
                cameraChanged ? 1 : 0,
                mayReuseChangedCamera ? 1 : 0);
        indirectPrepared_.valid = false;
        return false;
    }
    if (cameraChanged) {
        ++indirectPrepared_.cameraMotionReplayCount;
        indirectPrepared_.viewProj = viewProj;
    }
    const bool appendPatchEnabled =
        configuration_->appendPatch;
    if (appendChanged)
        noteRenderPreparation("retained-append-patch");
    if (appendChanged &&
            (!appendPatchEnabled ||
             !patchIndirectPreparedAppend(
                plan, assembly, glue, viewProj))) {
        if (configuration_->patchDebug)
            std::fprintf(stderr,
                "CadRendererGL retained append patch rejected\n");
        indirectPrepared_.valid = false;
        return false;
    }
    if (renderInterrupted())
        return false;
    const bool geometryPatchEnabled =
        configuration_->geometryPatch;
    if (partGeometryChanged)
        noteRenderPreparation("retained-geometry-patch");
    if (partGeometryChanged &&
            (!geometryPatchEnabled ||
             !patchIndirectPreparedGeometry(
                plan, assembly, glue))) {
        if (configuration_->patchDebug)
            std::fprintf(stderr,
                "CadRendererGL retained geometry patch rejected\n");
        indirectPrepared_.valid = false;
        return false;
    }
    if (renderInterrupted())
        return false;
    const bool ceilingChanged =
        indirectPrepared_.progressiveCutCeiling !=
            assembly.progressiveCutCeiling.getValue();
    if (ceilingChanged)
        noteRenderPreparation("retained-ceiling-patch");
    if (ceilingChanged &&
            !patchIndirectPreparedCeiling(
                plan, assembly, glue)) {
        if (configuration_->patchDebug)
            std::fprintf(stderr,
                "CadRendererGL retained ceiling patch rejected\n");
        indirectPrepared_.valid = false;
        return false;
    }
    if (renderInterrupted())
        return false;
    const bool lodPatchEnabled =
        configuration_->lodPatch;
    const bool lodChanged = indirectPrepared_.shadedLodRevision !=
        plan.shadedLodRevision;
    if (lodChanged)
        noteRenderPreparation("retained-lod-patch");
    if (lodChanged &&
            (!lodPatchEnabled ||
             !patchIndirectPreparedCuts(
                plan, assembly, glue))) {
        if (configuration_->patchDebug)
            std::fprintf(stderr,
                "CadRendererGL retained LoD patch rejected\n");
        indirectPrepared_.valid = false;
        return false;
    }
    if (renderInterrupted())
        return false;

    if (indirectPrepared_.instanceAttributeRevision !=
            plan.instanceAttributeRevision) {
        noteRenderPreparation("retained-attribute-patch");
        if (indirectPrepared_.sourceInstanceIndices.size() !=
                indirectPrepared_.instances.size() ||
                indirectPrepared_.pressureProxySourceInstanceIndices.size() !=
                indirectPrepared_.pressureProxyPoints.size()) {
            indirectPrepared_.valid = false;
            return false;
        }

        std::vector<uint32_t> changedSourceIndices;
        std::vector<uint32_t> visibilityChangedSourceIndices;
        bool sparseAttributes =
            indirectPrepared_.instanceAttributeRevision >=
                plan.instanceAttributeDeltaFloorRevision;
        if (sparseAttributes) {
            for (const CadInstanceAttributeDelta& delta :
                    plan.instanceAttributeDeltas) {
                if (renderInterruptedAfter(deadlineWork))
                    return false;
                if (delta.revision <=
                        indirectPrepared_.instanceAttributeRevision)
                    continue;
                changedSourceIndices.insert(
                    changedSourceIndices.end(),
                    delta.visibleIndices.begin(),
                    delta.visibleIndices.end());
                if (delta.visibilityChanged)
                    visibilityChangedSourceIndices.insert(
                        visibilityChangedSourceIndices.end(),
                        delta.visibleIndices.begin(),
                        delta.visibleIndices.end());
            }
            std::sort(changedSourceIndices.begin(),
                changedSourceIndices.end());
            changedSourceIndices.erase(
                std::unique(changedSourceIndices.begin(),
                    changedSourceIndices.end()),
                changedSourceIndices.end());
            std::sort(visibilityChangedSourceIndices.begin(),
                visibilityChangedSourceIndices.end());
            visibilityChangedSourceIndices.erase(
                std::unique(
                    visibilityChangedSourceIndices.begin(),
                    visibilityChangedSourceIndices.end()),
                visibilityChangedSourceIndices.end());
            if (changedSourceIndices.empty())
                sparseAttributes = false;
        }

        const auto updatePackedColor =
            [&](uint32_t packedIndex, uint32_t sourceIndex) {
            if (packedIndex >= indirectPrepared_.instances.size() ||
                    sourceIndex >= plan.visibleInstances.size())
                return false;
            const CadVisibleInstance& source =
                plan.visibleInstances[sourceIndex];
            InstVertex& target =
                indirectPrepared_.instances[packedIndex];
            target.color[0] = source.rgba[0] / 255.0f;
            target.color[1] = source.rgba[1] / 255.0f;
            target.color[2] = source.rgba[2] / 255.0f;
            target.color[3] = source.rgba[3] / 255.0f;
            const float geometryFlags =
                std::fmod(target.popMaxFlags[3], 4.0f);
            target.popMaxFlags[3] = geometryFlags +
                ((source.flags & CadInstanceHidden) ? 4.0f : 0.0f);
            return true;
        };
        std::vector<uint32_t> changedPackedIndices;
        bool pressureAttributesChanged = false;
        if (sparseAttributes) {
            changedPackedIndices.reserve(changedSourceIndices.size());
            for (const uint32_t sourceIndex : changedSourceIndices) {
                if (renderInterruptedAfter(deadlineWork))
                    return false;
                if (sourceIndex >= plan.visibleInstances.size() ||
                        sourceIndex >=
                            indirectPrepared_.instanceIndexBySource.size() ||
                        sourceIndex >=
                            indirectPrepared_.
                                pressureProxyIndexBySource.size()) {
                    indirectPrepared_.valid = false;
                    return false;
                }
                const uint32_t packedIndex =
                    indirectPrepared_.instanceIndexBySource[sourceIndex];
                const bool hasPacked = packedIndex !=
                    std::numeric_limits<uint32_t>::max();
                if (hasPacked) {
                    if (!updatePackedColor(packedIndex, sourceIndex)) {
                        indirectPrepared_.valid = false;
                        return false;
                    }
                    changedPackedIndices.push_back(packedIndex);
                }
                const uint32_t proxyIndex =
                    indirectPrepared_.
                        pressureProxyIndexBySource[sourceIndex];
                const bool hasProxy = proxyIndex !=
                    std::numeric_limits<uint32_t>::max();
                if (hasProxy) {
                    if (proxyIndex >=
                            indirectPrepared_.pressureProxyPoints.size()) {
                        indirectPrepared_.valid = false;
                        return false;
                    }
                    indirectPrepared_.pressureProxyPoints[proxyIndex].rgba =
                        plan.visibleInstances[sourceIndex].rgba;
                    indirectPrepared_.pressureProxyPoints[proxyIndex].flags =
                        plan.visibleInstances[sourceIndex].flags;
                    pressureAttributesChanged = true;
                }
                if (!hasPacked && !hasProxy &&
                        !(plan.visibleInstances[sourceIndex].flags &
                            CadInstanceHidden) &&
                        !cadInstanceSubpixelReplaced(plan, sourceIndex) &&
                        std::binary_search(
                            visibilityChangedSourceIndices.begin(),
                            visibilityChangedSourceIndices.end(),
                            sourceIndex)) {
                    /*
                     * This occurrence was absent when the prepared view was
                     * built (initially hidden or out of frame).  Showing it
                     * needs one exact visibility/admission pass.
                     */
                    indirectPrepared_.valid = false;
                    return false;
                }
            }
        } else {
            for (size_t i = 0;
                    i < indirectPrepared_.instances.size(); ++i) {
                if (renderInterruptedAfter(deadlineWork))
                    return false;
                const uint32_t sourceIndex =
                    indirectPrepared_.sourceInstanceIndices[i];
                if (!updatePackedColor(
                        static_cast<uint32_t>(i), sourceIndex)) {
                    indirectPrepared_.valid = false;
                    return false;
                }
            }
            for (size_t i = 0;
                    i < indirectPrepared_.pressureProxyPoints.size(); ++i) {
                if (renderInterruptedAfter(deadlineWork))
                    return false;
                const uint32_t sourceIndex =
                    indirectPrepared_.
                        pressureProxySourceInstanceIndices[i];
                if (sourceIndex >= plan.visibleInstances.size()) {
                    indirectPrepared_.valid = false;
                    return false;
                }
                indirectPrepared_.pressureProxyPoints[i].rgba =
                    plan.visibleInstances[sourceIndex].rgba;
                indirectPrepared_.pressureProxyPoints[i].flags =
                    plan.visibleInstances[sourceIndex].flags;
                pressureAttributesChanged = true;
            }
        }
        indirectPrepared_.instanceAttributeRevision =
            plan.instanceAttributeRevision;
        if (mayReplayVisibilityProxyChange)
            indirectPrepared_.subpixelProxyRevision =
                plan.subpixelProxyRevision;
        indirectPrepared_.planRevision = plan.revision;

        bool sparseUpload = sparseAttributes &&
            gpuRes_->instanceVbo() &&
            gpuRes_->instanceUploadSerial() ==
                indirectPrepared_.instanceUploadSerial;
        if (sparseUpload && !changedPackedIndices.empty()) {
            std::sort(changedPackedIndices.begin(),
                changedPackedIndices.end());
            changedPackedIndices.erase(
                std::unique(changedPackedIndices.begin(),
                    changedPackedIndices.end()),
                changedPackedIndices.end());
            size_t begin = 0;
            while (sparseUpload &&
                    begin < changedPackedIndices.size()) {
                if (renderInterruptedAfter(deadlineWork))
                    return false;
                size_t end = begin + 1;
                while (end < changedPackedIndices.size() &&
                        changedPackedIndices[end] ==
                            changedPackedIndices[end - 1] + 1u)
                    ++end;
                const uint32_t first = changedPackedIndices[begin];
                const size_t count = end - begin;
                sparseUpload = gpuRes_->updateInstanceData(
                    static_cast<GLintptr>(first) * sizeof(InstVertex),
                    indirectPrepared_.instances.data() + first,
                    static_cast<GLsizeiptr>(count) *
                        sizeof(InstVertex),
                    glue);
                begin = end;
            }
            if (sparseUpload)
                indirectPrepared_.instanceUploadSerial =
                    gpuRes_->instanceUploadSerial();
        }
        /*
         * A missing/overwritten VBO or an expired delta journal falls back to
         * one complete upload.  Command and atlas state remain reusable.
         */
        if (!sparseUpload)
            indirectPrepared_.instanceUploadSerial = 0;
        if (pressureAttributesChanged) {
            pressureProxyAppendOnly_ = false;
            ++pressureProxyRevision_;
            if (!pressureProxyRevision_)
                pressureProxyRevision_ = 1;
        }
    }

    /*
     * Expensive retained-record audit used by the graphical stress harness.
     * It reconstructs the command/instance contract from the authoritative
     * frame plan without touching GL.  Keeping this behind an environment
     * switch lets us distinguish a CPU journal defect from a GPU buffer/state
     * defect without making ordinary replay O(all visible parts).
     */
    if (configuration_->validateReplay) {
        const uint32_t noSlot =
            std::numeric_limits<uint32_t>::max();
        const auto rejectAudit =
            [&](size_t demandIndex, const char *reason) {
                std::fprintf(stderr,
                    "CadRendererGL retained audit failed demand=%zu "
                    "reason=%s parts=%zu packed=%zu pages=%zu\n",
                    demandIndex, reason ? reason : "unknown",
                    indirectPrepared_.parts.size(),
                    indirectPrepared_.instances.size(),
                    indirectPrepared_.pages.size());
                indirectPrepared_.valid = false;
                return false;
            };
        for (size_t demandIndex = 0;
                demandIndex < indirectPrepared_.parts.size();
                ++demandIndex) {
            if (renderInterruptedAfter(deadlineWork))
                return false;
            const IndirectPreparedPart& demand =
                indirectPrepared_.parts[demandIndex];
            if (demand.partIndex >= plan.partBindings.size())
                return rejectAudit(demandIndex, "part-index");
            const CadPartBinding& binding =
                plan.partBindings[demand.partIndex];
            if (!(binding.part == demand.part) ||
                    binding.generation != demand.generation ||
                    !binding.geometry || !binding.geometry->shaded)
                return rejectAudit(demandIndex, "binding");
            if (demand.packedInstance == noSlot ||
                    demand.packedInstance >=
                        indirectPrepared_.instances.size() ||
                    demand.packedInstance >=
                        indirectPrepared_.sourceInstanceIndices.size())
                return rejectAudit(demandIndex, "packed-instance");
            const uint32_t sourceIndex =
                indirectPrepared_.sourceInstanceIndices[
                    demand.packedInstance];
            if (sourceIndex >= plan.visibleInstances.size())
                return rejectAudit(demandIndex, "source-index");
            const CadVisibleInstance& source =
                plan.visibleInstances[sourceIndex];
            if (source.partIndex != demand.partIndex)
                return rejectAudit(demandIndex, "source-part");
            const InstVertex& instance =
                indirectPrepared_.instances[demand.packedInstance];
            if (std::memcmp(
                    instance.transform, source.transform.data(),
                    16u * sizeof(float)) != 0)
                return rejectAudit(demandIndex, "transform");

            const TriMesh& mesh = *binding.geometry->shaded;
            uint8_t level = mesh.isProgressive() ?
                cadResolvedProgressiveCut(
                    assembly.effectiveProgressiveCut(
                        source.lodCut),
                    mesh.progressiveMinimumCut,
                    mesh.progressiveResidentCut) :
                15u;
            const CadTriangleAtlasPart *atlas =
                gpuRes_->triangleAtlasPart(binding.part);
            if (!atlas || atlas->page != demand.page ||
                    atlas->vertices.first != demand.vertexFirst ||
                    atlas->indices.first != demand.indexFirst)
                return rejectAudit(demandIndex, "atlas");
            while (mesh.isProgressive() &&
                    level > mesh.progressiveMinimumCut &&
                    (mesh.positionCountAtCut(level) >
                         atlas->vertexCount ||
                     mesh.indexCountAtCut(level) >
                         atlas->indexCount))
                --level;
            const uint32_t expectedCount =
                static_cast<uint32_t>(mesh.isProgressive() ?
                    mesh.indexCountAtCut(level) :
                    mesh.indices.size());
            if (!expectedCount ||
                    instance.popMinLevel[3] !=
                        packedProgressiveQuantization(
                            mesh.isProgressive() ?
                                mesh.quantizationAtCut(level) :
                                ProgressiveQuantization()))
                return rejectAudit(demandIndex, "lod-level");
            const SbVec3f expectedMinimum = mesh.isProgressive() ?
                mesh.progressiveQuantizationMinimum :
                SbVec3f(0, 0, 0);
            const SbVec3f expectedMaximum = mesh.isProgressive() ?
                mesh.progressiveQuantizationMaximum :
                SbVec3f(0, 0, 0);
            for (int axis = 0; axis < 3; ++axis) {
                if (instance.popMinLevel[axis] !=
                            expectedMinimum[axis] ||
                        instance.popMaxFlags[axis] !=
                            expectedMaximum[axis])
                    return rejectAudit(
                        demandIndex, "quantization");
            }

            const IndirectPageWork *pageWork = nullptr;
            for (const IndirectPageWork& candidate :
                    indirectPrepared_.pages) {
                if (candidate.page == demand.page) {
                    pageWork = &candidate;
                    break;
                }
            }
            if (!pageWork)
                return rejectAudit(demandIndex, "page-work");
            const auto& commands = demand.commandCulled ?
                pageWork->culled : pageWork->ordinary;
            if (demand.commandIndex >= commands.size())
                return rejectAudit(demandIndex, "command-index");
            const CadDrawElementsIndirectCommand& command =
                commands[demand.commandIndex];
            if (command.count != expectedCount ||
                    command.instanceCount != 1u ||
                    command.firstIndex != demand.indexFirst ||
                    command.baseVertex !=
                        static_cast<int32_t>(demand.vertexFirst) ||
                    command.baseInstance != demand.packedInstance)
                return rejectAudit(demandIndex, "command");
        }
    }

    constexpr uint32_t validationIntervalFrames = 30u;
    const bool validateAtlas =
        indirectPrepared_.atlasRevision !=
            gpuRes_->triangleAtlasRevision() ||
        !indirectPrepared_.atlasValidationCountdown;
    if (validateAtlas) {
        noteRenderPreparation("retained-atlas-validation");
        for (const IndirectPreparedPart& demand : indirectPrepared_.parts) {
            if (renderInterruptedAfter(deadlineWork))
                return false;
            if (demand.partIndex >= plan.partBindings.size()) {
                indirectPrepared_.valid = false;
                return false;
            }
            const CadPartBinding& binding =
                plan.partBindings[demand.partIndex];
            if (!(binding.part == demand.part) ||
                    binding.generation != demand.generation) {
                indirectPrepared_.valid = false;
                return false;
            }
            const CadTriangleAtlasPart *atlas =
                gpuRes_->touchTriangleAtlasPart(
                    demand.part, demand.generation, demand.hasNormals,
                    demand.vertexCount, demand.indexCount);
            if (!atlas || atlas->page != demand.page ||
                    atlas->vertices.first != demand.vertexFirst ||
                    atlas->indices.first != demand.indexFirst) {
                indirectPrepared_.valid = false;
                return false;
            }
        }
        indirectPrepared_.atlasRevision =
            gpuRes_->triangleAtlasRevision();
        indirectPrepared_.atlasValidationCountdown =
            validationIntervalFrames;
    } else {
        --indirectPrepared_.atlasValidationCountdown;
        if (!indirectPrepared_.parts.empty())
            gpuRes_->deferTriangleAtlasMaintenance();
    }

    pressureProxyPointsView_ =
        &indirectPrepared_.pressureProxyPoints;
    atlasAdmissionPressure_ =
        indirectPrepared_.atlasAdmissionPressure;
    lastRenderedTriangleCount_ =
        indirectPrepared_.renderedTriangleCount;
    const bool submitted =
        submitIndirectPrepared(glue, viewProj, viewVolume);
    if (submitted)
        lastRenderUsedPreparedReplay_ = true;
    return submitted;
}

bool CadRendererGL::renderIndirectShaded(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext* glue,
        const SbMatrix& viewProj,
        const SbViewVolume& viewVolume)
{
    if (!glue || !gpuRes_ || !caps_.canUseIndirect() ||
            !shaders_.shadedIndirect || plan.shadedItems.empty() ||
            plan.visibleInstances.empty()) {
        if (gpuRes_ && indirectPreparation_.active)
            gpuRes_->endTriangleAtlasExactPreparation();
        indirectPreparation_ = IndirectPreparationState();
        return rejectIndirect(1, "precondition");
    }
    IndirectPreparationState& build = indirectPreparation_;
    const int progressiveCutCeiling =
        assembly.progressiveCutCeiling.getValue();
    const bool matchingBuild = build.active &&
        build.contextId == glue->contextid &&
        build.planRevision == plan.revision &&
        build.progressiveCutCeiling == progressiveCutCeiling &&
        build.viewProj == viewProj;
    if (build.active && !matchingBuild) {
        gpuRes_->endTriangleAtlasExactPreparation();
        build = IndirectPreparationState();
    }
    const bool replayEnabled = configuration_->replay;
    if (!build.active) {
        if (replayEnabled) {
            if (replayIndirectShaded(
                    plan, assembly, glue, viewProj, viewVolume))
                return true;
            if (renderInterrupted())
                return false;
        } else {
            /* Diagnostic/reference mode: retain the same atlas, shaders,
             * indirect commands, and GPU submission route while preparing
             * the CPU submission exactly for every frame. */
            indirectPrepared_.valid = false;
        }
    }
    using IndirectClock = std::chrono::steady_clock;
    const auto indirectStarted = IndirectClock::now();
    static constexpr size_t guaranteedWorkPerRetry = 4096u;
    size_t workSinceAbortCheck = 0u;
    const auto preparationInterrupted = [&](size_t work = 1u) {
        const size_t remaining = guaranteedWorkPerRetry -
            std::min(guaranteedWorkPerRetry, workSinceAbortCheck);
        if (work < remaining) {
            workSinceAbortCheck += work;
            return false;
        }
        workSinceAbortCheck = 0u;
        return renderInterrupted();
    };
    const auto abandon = [&](int status, const char *reason) {
        gpuRes_->endTriangleAtlasExactPreparation();
        build = IndirectPreparationState();
        indirectPrepared_.valid = false;
        return rejectIndirect(status, reason);
    };

    if (!build.active) {
        indirectPrepared_.valid = false;
        gpuRes_->beginTriangleAtlasExactPreparation();
        build.active = true;
        build.phase = IndirectPreparationPhase::Visibility;
        build.contextId = glue->contextid;
        build.planRevision = plan.revision;
        build.progressiveCutCeiling = progressiveCutCeiling;
        build.viewProj = viewProj;

        indirectVisibleMask_.assign(plan.visibleInstances.size(), 0u);
        indirectVisibleMaximumCut_.assign(plan.partBindings.size(), 0u);
        indirectVisiblePart_.assign(plan.partBindings.size(), 0u);
        indirectVisibleImportance_.assign(plan.partBindings.size(), 0.0);
        indirectVisiblePartIndices_.clear();
        if (indirectVisiblePartIndices_.capacity() <
                plan.partBindings.size())
            indirectVisiblePartIndices_.reserve(plan.partBindings.size());
        indirectFirstVisibleOccurrence_.assign(
            plan.partBindings.size(),
            std::numeric_limits<uint32_t>::max());
        indirectNextVisibleOccurrence_.assign(
            plan.visibleInstances.size(),
            std::numeric_limits<uint32_t>::max());
        indirectRequestedVertexCounts_.assign(
            plan.partBindings.size(), 0u);
        indirectRequestedIndexCounts_.assign(
            plan.partBindings.size(), 0u);
        indirectAtlasBindings_.assign(
            plan.partBindings.size(), nullptr);
    }
    ++build.sliceCount;
    /*
     * The preparation serial is a host-visible forward-progress witness.
     * Once all retained records have been published, Submit retries perform
     * no preparation: they only try to draw the already prepared frame.
     * Counting those retries as preparation caused a deadline livelock on
     * slow renderers.  The host kept granting an unchanged retry instead of
     * lowering the reversible progressive ceiling, even though every retry
     * was spending all of its time in the same draw.
     *
     * A slice which starts before Submit advances at least one bounded unit
     * of retained work before its first abort check (see
     * guaranteedWorkPerRetry above), so it is a valid progress witness.  The
     * first slice which reaches Submit is also counted; this grants one
     * unchanged replay of the newly published record.  If that replay still
     * misses its deadline, its unchanged serial correctly classifies the
     * failure as draw-capacity evidence.
     */
    if (build.phase != IndirectPreparationPhase::Submit)
        noteRenderPreparation("retained-exact-build-slice");

    /*
     * Resolve view visibility before GPU admission.  The assembly plan is a
     * structural/presentation cache and intentionally contains authored
     * visible instances outside this camera's frustum.  Admitting every part
     * here defeated view-aware LoD memory management and made a close zoom
     * retain an entire vehicle.  Subpixel proxy occurrences are likewise
     * owned by the aggregate point channel, never by the mesh atlas.
    */
    const ExecutorFrustumPlanes fp =
        extractExecutorFrustumPlanes(viewProj);
    auto& visibleMask = indirectVisibleMask_;
    auto& visibleMaximumCut = indirectVisibleMaximumCut_;
    auto& visiblePart = indirectVisiblePart_;
    auto& visibleImportance = indirectVisibleImportance_;
    auto& visiblePartIndices = indirectVisiblePartIndices_;
    const uint32_t noOccurrence = std::numeric_limits<uint32_t>::max();
    auto& firstVisibleOccurrence = indirectFirstVisibleOccurrence_;
    auto& nextVisibleOccurrence = indirectNextVisibleOccurrence_;
    if (build.phase == IndirectPreparationPhase::Visibility) {
      while (build.itemCursor < plan.shadedItems.size()) {
        const CadDrawItem& item = plan.shadedItems[build.itemCursor];
        if (!item.instanceCount ||
                item.partIndex >= plan.partBindings.size() ||
                item.baseInstance >= plan.visibleInstances.size() ||
                item.instanceCount > plan.visibleInstances.size() -
                    item.baseInstance) {
            ++build.itemCursor;
            build.occurrenceOffset = 0;
            continue;
        }
        const CadPartBinding& binding =
            plan.partBindings[item.partIndex];
        const PartGeometry *geometry = binding.geometry.get();
        if (!geometry || !geometry->shaded)
            return abandon(
                2, "visible part has no shaded geometry");
        const TriMesh& mesh = *geometry->shaded;
        while (build.occurrenceOffset < item.instanceCount) {
            if (preparationInterrupted())
                return false;
            const size_t visibleIndex = item.baseInstance +
                build.occurrenceOffset++;
            const CadVisibleInstance& instance =
                plan.visibleInstances[visibleIndex];
            if (!cadInstanceDrawable(
                    plan, item, visibleIndex, CadDrawChannel::Shaded) ||
                    isBoxOutsideExecutorFrustum(
                        instance.wbMin, instance.wbMax, fp))
                continue;
            if (!visibleMask[visibleIndex]) {
                visibleMask[visibleIndex] = 1u;
                nextVisibleOccurrence[visibleIndex] =
                    firstVisibleOccurrence[item.partIndex];
                firstVisibleOccurrence[item.partIndex] =
                    static_cast<uint32_t>(visibleIndex);
                ++build.visibleOccurrenceCount;
            }
            const uint8_t requested = mesh.isProgressive() ?
                cadResolvedProgressiveCut(
                    assembly.effectiveProgressiveCut(
                        instance.lodCut),
                    mesh.progressiveMinimumCut,
                    mesh.progressiveResidentCut) :
                Obol::ProgressiveCutUnspecified;
            if (!visiblePart[item.partIndex]) {
                visiblePart[item.partIndex] = 1u;
                visiblePartIndices.push_back(item.partIndex);
            }
            visibleMaximumCut[item.partIndex] =
                std::max(visibleMaximumCut[item.partIndex], requested);
            double importance = executorProjectedBoxImportance(
                instance.wbMin, instance.wbMax, viewProj);
            if (instance.flags & 3u)
                importance *= 16.0;
            visibleImportance[item.partIndex] = std::min(
                1.0e12,
                visibleImportance[item.partIndex] + importance);
        }
        ++build.itemCursor;
        build.occurrenceOffset = 0;
      }
      if (!build.visibleOccurrenceCount) {
        gpuRes_->endTriangleAtlasExactPreparation();
        build = IndirectPreparationState();
        lastIndirectStatus_ = 0;
        return true;
      }
      build.phase = IndirectPreparationPhase::Protection;
      build.partCursor = 0;
    }

    auto& requestedVertexCounts = indirectRequestedVertexCounts_;
    auto& requestedIndexCounts = indirectRequestedIndexCounts_;
    auto& atlasBindings = indirectAtlasBindings_;
    /*
     * Mark every retained consumer before admitting any new allocation.
     * Without this phase, pressure while processing part N could evict part
     * N+1 merely because it had not yet been encountered in this frame,
     * creating a perpetual evict/re-upload cycle at the memory ceiling.
     *
     * The touch also validates generation and retained prefix capacity.  An
     * unchanged frame can therefore reuse the returned bindings directly,
     * avoiding a second full-table upsert pass and a third lookup pass over
     * tens of thousands of unique parts.
     */
    if (build.phase == IndirectPreparationPhase::Protection) {
      while (build.partCursor < visiblePartIndices.size()) {
        if (preparationInterrupted())
            return false;
        const uint32_t partIndex =
            visiblePartIndices[build.partCursor++];
        const CadPartBinding& binding =
            plan.partBindings[partIndex];
        const PartGeometry *geometry = binding.geometry.get();
        if (!geometry || !geometry->shaded)
            return abandon(
                2, "visible part has no shaded geometry");
        const TriMesh& mesh = *geometry->shaded;
        const uint8_t requested = visibleMaximumCut[partIndex];
        const size_t vertexCount = mesh.isProgressive() ?
            mesh.positionCountAtCut(requested) : mesh.positions.size();
        const size_t indexCount = mesh.isProgressive() ?
            mesh.indexCountAtCut(requested) : mesh.indices.size();
        if (!vertexCount || !indexCount ||
                vertexCount > std::numeric_limits<uint32_t>::max() ||
                indexCount > std::numeric_limits<uint32_t>::max())
            return abandon(3, "invalid retained prefix counts");
        requestedVertexCounts[partIndex] =
            static_cast<uint32_t>(vertexCount);
        requestedIndexCounts[partIndex] =
            static_cast<uint32_t>(indexCount);
        const uint64_t vertexStride = mesh.normals.empty() ? 12u : 24u;
        const uint64_t requestedBytes =
            static_cast<uint64_t>(vertexCount) * vertexStride +
            static_cast<uint64_t>(indexCount) * sizeof(uint32_t);
        build.requestedLiveBytes = requestedBytes >
                UINT64_MAX - build.requestedLiveBytes ?
            UINT64_MAX : build.requestedLiveBytes + requestedBytes;
        atlasBindings[partIndex] =
            gpuRes_->touchTriangleAtlasPart(
                binding.part, binding.generation,
                !mesh.normals.empty(),
                requestedVertexCounts[partIndex],
                requestedIndexCounts[partIndex]);
        gpuRes_->protectTriangleAtlasExactPart(binding.part);
      }

    auto& admissionPartIndices = indirectAdmissionPartIndices_;
    admissionPartIndices.assign(
        visiblePartIndices.begin(), visiblePartIndices.end());
    const size_t atlasBudget = gpuRes_->triangleAtlasBudgetBytes();
    const bool likelyMemoryPressure = atlasBudget > 0 &&
        (build.requestedLiveBytes >=
             static_cast<uint64_t>(atlasBudget / 4u) * 3u ||
         gpuRes_->triangleAtlasAllocatedBytes() >=
             atlasBudget / 4u * 3u);
    if (likelyMemoryPressure) {
        std::stable_sort(admissionPartIndices.begin(),
            admissionPartIndices.end(),
            [&](uint32_t left, uint32_t right) {
                return visibleImportance[left] >
                    visibleImportance[right];
            });
    }

    /*
     * Make each unique visible part's richest requested prefix resident.
     * Occurrences at smaller levels select smaller command counts and
     * independent quantization attributes from the same cumulative arrays.
     */
    auto& pressureProxyPoints = indirectPressureProxyPoints_;
    pressureProxyPoints.clear();
    indirectPrepared_.pressureProxySourceInstanceIndices.swap(
        indirectPressureProxySourceInstanceIndices_);
    auto& pressureProxySourceInstanceIndices =
        indirectPressureProxySourceInstanceIndices_;
    pressureProxySourceInstanceIndices.clear();
    if (pressureProxySourceInstanceIndices.capacity() <
            build.visibleOccurrenceCount)
        pressureProxySourceInstanceIndices.reserve(
            build.visibleOccurrenceCount);
    build.phase = IndirectPreparationPhase::Coverage;
    build.partCursor = 0;
    }

    auto& admissionPartIndices = indirectAdmissionPartIndices_;
    auto& pressureProxyPoints = indirectPressureProxyPoints_;
    auto& pressureProxySourceInstanceIndices =
        indirectPressureProxySourceInstanceIndices_;
    const auto beginPressureProxy = [&](uint32_t partIndex) {
        const CadPartBinding& binding = plan.partBindings[partIndex];
        if (!binding.subpixelProxyEligible)
            return false;
        build.proxyPartActive = true;
        build.proxyPartIndex = partIndex;
        build.proxyVisibleIndex = firstVisibleOccurrence[partIndex];
        return true;
    };
    const auto continuePressureProxy = [&]() {
        if (!build.proxyPartActive)
            return true;
        const CadPartBinding& binding =
            plan.partBindings[build.proxyPartIndex];
        SbVec3f localCenter(0.0f, 0.0f, 0.0f);
        for (const SbVec3f& corner : binding.subpixelProxyCorners)
            localCenter += corner;
        localCenter /= 8.0f;
        while (build.proxyVisibleIndex != noOccurrence) {
            if (preparationInterrupted())
                return false;
            const uint32_t visibleIndex = build.proxyVisibleIndex;
            build.proxyVisibleIndex =
                nextVisibleOccurrence[visibleIndex];
            if (!visibleMask[visibleIndex])
                continue;
            const CadVisibleInstance& instance =
                plan.visibleInstances[visibleIndex];
            CadSubpixelProxyPoint replacement;
            replacement.position = transformedFlatPoint(
                localCenter, instance.transform);
            replacement.rgba = instance.rgba;
            replacement.instanceId = instance.instanceId;
            replacement.flags = instance.flags;
            pressureProxyPoints.push_back(replacement);
            pressureProxySourceInstanceIndices.push_back(visibleIndex);
            visibleMask[visibleIndex] = 0u;
            --build.visibleOccurrenceCount;
        }
        visiblePart[build.proxyPartIndex] = 0u;
        build.proxyPartActive = false;
        return true;
    };

    /* Coverage pass: give every progressive part its producer-authored
     * minimum coherent prefix before any part consumes memory on enrichment.
     * Under pressure, screen-prominent and selected occurrences are visited
     * first.  This removes plan-order starvation without compromising the
     * no-holes PoP contract. */
    if (build.phase == IndirectPreparationPhase::Coverage) {
      while (build.partCursor < admissionPartIndices.size()) {
        if (build.proxyPartActive) {
            if (!continuePressureProxy())
                return false;
            ++build.partCursor;
            continue;
        }
        if (preparationInterrupted())
            return false;
        const uint32_t partIndex =
            admissionPartIndices[build.partCursor];
        if (!visiblePart[partIndex] || atlasBindings[partIndex]) {
            ++build.partCursor;
            continue;
        }
        const CadPartBinding& binding = plan.partBindings[partIndex];
        const PartGeometry *geometry = binding.geometry.get();
        if (!geometry || !geometry->shaded)
            return abandon(2, "visible part has no shaded geometry");
        const TriMesh& mesh = *geometry->shaded;
        uint32_t coverageVertexCount = requestedVertexCounts[partIndex];
        uint32_t coverageIndexCount = requestedIndexCounts[partIndex];
        if (mesh.isProgressive()) {
            coverageVertexCount = static_cast<uint32_t>(
                mesh.positionCountAtCut(mesh.progressiveMinimumCut));
            coverageIndexCount = static_cast<uint32_t>(
                mesh.indexCountAtCut(mesh.progressiveMinimumCut));
        }
        const CadTriangleAtlasPart *admitted =
            gpuRes_->upsertTriangleAtlasPart(
                binding.part, binding.generation,
                executorPackedVec3fData(mesh.positions),
                executorPackedVec3fData(mesh.normals),
                coverageVertexCount, mesh.indices.data(),
                coverageIndexCount, mesh.isProgressive(),
                mesh.progressiveLineage, glue, caps_);
        if (!admitted) {
            build.atlasAdmissionPressure = true;
            if (configuration_->indirectDebug)
                std::fprintf(stderr,
                    "CadRendererGL indirect atlas coverage failed "
                    "part=%016llx:%016llx vertices=%zu indices=%zu "
                    "allocated=%zu live=%zu pages=%zu parts=%zu\n",
                    static_cast<unsigned long long>(binding.part.w0),
                    static_cast<unsigned long long>(binding.part.w1),
                    static_cast<size_t>(coverageVertexCount),
                    static_cast<size_t>(coverageIndexCount),
                    gpuRes_->triangleAtlasAllocatedBytes(),
                    gpuRes_->triangleAtlasLiveBytes(),
                    gpuRes_->triangleAtlasPageCount(),
                    gpuRes_->triangleAtlasPartCount());
            if (!beginPressureProxy(partIndex))
                return abandon(4, "triangle atlas coverage");
            continue;
        }
        atlasBindings[partIndex] = admitted;
        gpuRes_->protectTriangleAtlasExactPart(binding.part);
        ++build.partCursor;
      }
      build.phase = IndirectPreparationPhase::Enrichment;
      build.partCursor = 0;
    }

    /* Enrichment pass: grow retained prefixes toward the view request in the
     * same value order.  A failed grow preserves and draws the coherent
     * coverage prefix; it never demotes that part back to a box or point. */
    if (build.phase == IndirectPreparationPhase::Enrichment) {
      while (build.partCursor < admissionPartIndices.size()) {
        if (build.proxyPartActive) {
            if (!continuePressureProxy())
                return false;
            ++build.partCursor;
            continue;
        }
        if (preparationInterrupted())
            return false;
        const uint32_t partIndex =
            admissionPartIndices[build.partCursor];
        if (!visiblePart[partIndex] || !atlasBindings[partIndex]) {
            ++build.partCursor;
            continue;
        }
        const uint32_t vertexCount = requestedVertexCounts[partIndex];
        const uint32_t indexCount = requestedIndexCounts[partIndex];
        const CadTriangleAtlasPart *current = atlasBindings[partIndex];
        if (current->vertexCount >= vertexCount &&
                current->indexCount >= indexCount) {
            ++build.partCursor;
            continue;
        }
        const CadPartBinding& binding = plan.partBindings[partIndex];
        const TriMesh& mesh = *binding.geometry->shaded;
        const CadTriangleAtlasPart *enriched =
            gpuRes_->upsertTriangleAtlasPart(
                binding.part, binding.generation,
                executorPackedVec3fData(mesh.positions),
                executorPackedVec3fData(mesh.normals),
                vertexCount, mesh.indices.data(), indexCount,
                mesh.isProgressive(), mesh.progressiveLineage,
                glue, caps_);
        if (!enriched) {
            build.atlasAdmissionPressure = true;
            enriched = gpuRes_->triangleAtlasPart(binding.part);
        }
        if (!enriched) {
            if (!beginPressureProxy(partIndex))
                return abandon(4, "triangle atlas enrichment");
            atlasBindings[partIndex] = nullptr;
            continue;
        }
        atlasBindings[partIndex] = enriched;
        gpuRes_->protectTriangleAtlasExactPart(binding.part);
        if (enriched->vertexCount < vertexCount ||
                enriched->indexCount < indexCount)
            build.atlasAdmissionPressure = true;
        ++build.partCursor;
      }
      build.phase = IndirectPreparationPhase::CommandSetup;
    }

    /*
     * The preceding prepared frame is no longer replayable once exact
     * preparation begins.  Reuse its per-page command capacities as the
     * scratch target for this build.  Page ids are dense atlas slots, so a
     * vector lookup avoids one red-black-tree allocation per page as well.
     */
    if (build.phase == IndirectPreparationPhase::CommandSetup) {
      if (!indirectPrepared_.pages.empty())
          indirectPageWorkScratch_.swap(indirectPrepared_.pages);
      indirectPrepared_.pages.clear();
      for (IndirectPageWork& work : indirectPageWorkScratch_) {
          work.ordinary.clear();
          work.culled.clear();
      }
      const size_t atlasPageCount = gpuRes_->triangleAtlasPageCount();
      indirectPageWorkSlotByPage_.assign(
          atlasPageCount, std::numeric_limits<uint32_t>::max());
      for (size_t i = 0; i < indirectPageWorkScratch_.size(); ++i) {
          const uint32_t page = indirectPageWorkScratch_[i].page;
          if (page < indirectPageWorkSlotByPage_.size())
              indirectPageWorkSlotByPage_[page] =
                  static_cast<uint32_t>(i);
      }
      const uint32_t noPreparedSlot =
          std::numeric_limits<uint32_t>::max();
      indirectCommandIndexByPart_.assign(
          plan.partBindings.size(), noPreparedSlot);
      indirectCommandCullByPart_.assign(
          plan.partBindings.size(), 0u);
      indirectPackedInstanceByPart_.assign(
          plan.partBindings.size(), noPreparedSlot);
      indirectPrepared_.instances.swap(indirectInstances_);
      indirectInstances_.clear();
      if (indirectInstances_.capacity() < build.visibleOccurrenceCount)
          indirectInstances_.reserve(build.visibleOccurrenceCount);
      indirectPrepared_.sourceInstanceIndices.swap(
          indirectSourceInstanceIndices_);
      indirectSourceInstanceIndices_.clear();
      if (indirectSourceInstanceIndices_.capacity() <
              build.visibleOccurrenceCount)
          indirectSourceInstanceIndices_.reserve(
              build.visibleOccurrenceCount);
      build.itemCursor = 0;
      build.occurrenceOffset = 0;
      build.commandItemActive = false;
      build.phase = IndirectPreparationPhase::Commands;
    }
    const auto pageWorkFor = [&](uint32_t page) ->
            IndirectPageWork& {
        if (page >= indirectPageWorkSlotByPage_.size())
            indirectPageWorkSlotByPage_.resize(
                static_cast<size_t>(page) + 1u,
                std::numeric_limits<uint32_t>::max());
        uint32_t& slot = indirectPageWorkSlotByPage_[page];
        if (slot == std::numeric_limits<uint32_t>::max()) {
            slot = static_cast<uint32_t>(
                indirectPageWorkScratch_.size());
            IndirectPageWork work;
            work.page = page;
            indirectPageWorkScratch_.push_back(std::move(work));
        }
        return indirectPageWorkScratch_[slot];
    };
    const uint32_t noPreparedSlot =
        std::numeric_limits<uint32_t>::max();
    auto& instances = indirectInstances_;
    auto& sourceInstanceIndices = indirectSourceInstanceIndices_;
    /*
     * Protection and admission both return stable element pointers.  Use
     * those direct bindings below instead of performing a second hash-table
     * lookup for every visible part after an insertion.
     */
    if (build.phase == IndirectPreparationPhase::Commands) {
      while (build.itemCursor < plan.shadedItems.size()) {
        const CadDrawItem& item = plan.shadedItems[build.itemCursor];
        if (!build.commandItemActive) {
          if (!item.instanceCount ||
                  item.partIndex >= plan.partBindings.size() ||
                  item.baseInstance >= plan.visibleInstances.size() ||
                  item.instanceCount >
                      plan.visibleInstances.size() - item.baseInstance ||
                  !visiblePart[item.partIndex]) {
              ++build.itemCursor;
              build.occurrenceOffset = 0;
              continue;
          }
          if (instances.size() > std::numeric_limits<uint32_t>::max())
              return abandon(8, "instance stream overflow");
          build.commandBaseInstance =
              static_cast<uint32_t>(instances.size());
          build.commandCut = Obol::ProgressiveCutUnspecified;
          build.commandCount = 0;
          build.commandItemActive = true;
        }
        /*
         * Admission is intentionally view-aware.  A part whose occurrences
         * are all outside the frustum or represented by aggregate proxy
         * points has no atlas binding this frame and contributes no command.
         * Test that expected absence before resolving the binding; treating
         * it as an error made one culled/subpixel part throw the entire scene
         * into the CPU-flattened fallback.
         */
        const PartGeometry *geometry =
            plan.partBindings[item.partIndex].geometry.get();
        const CadTriangleAtlasPart *atlas =
            atlasBindings[item.partIndex];
        if (!geometry || !geometry->shaded || !atlas)
            return abandon(5, "missing admitted atlas binding");
        const TriMesh& mesh = *geometry->shaded;
        const bool progressive = mesh.isProgressive();
        while (build.occurrenceOffset < item.instanceCount) {
            if (preparationInterrupted())
                return false;
            const size_t instanceIndex = item.baseInstance +
                build.occurrenceOffset++;
            if (!cadInstanceDrawable(
                    plan, item, instanceIndex, CadDrawChannel::Shaded) ||
                    !visibleMask[instanceIndex])
                continue;
            const CadVisibleInstance& source =
                plan.visibleInstances[instanceIndex];
            uint8_t level = progressive ?
                cadResolvedProgressiveCut(
                    assembly.effectiveProgressiveCut(source.lodCut),
                    mesh.progressiveMinimumCut,
                    mesh.progressiveResidentCut) :
                Obol::ProgressiveCutUnspecified;
            /*
             * Allocation pressure may deliberately retain a correct coarser
             * cumulative prefix.  Clamp to the richest level wholly resident
             * instead of failing the entire frame into a world-space rebuild.
             */
            while (progressive &&
                    level > mesh.progressiveMinimumCut &&
                    (mesh.positionCountAtCut(level) >
                         atlas->vertexCount ||
                     mesh.indexCountAtCut(level) > atlas->indexCount))
                --level;
            const size_t count = progressive ?
                mesh.indexCountAtCut(level) : mesh.indices.size();
            if (!count || count > atlas->indexCount ||
                    count > std::numeric_limits<uint32_t>::max())
                return abandon(6, "invalid indirect draw count");
            if (build.commandCount && level != build.commandCut)
                return abandon(
                    7, "mixed levels in one draw run");
            build.commandCut = level;
            build.commandCount = count;

            InstVertex target = {};
            std::memcpy(target.transform, source.transform.data(),
                        16 * sizeof(float));
            target.color[0] = source.rgba[0] / 255.0f;
            target.color[1] = source.rgba[1] / 255.0f;
            target.color[2] = source.rgba[2] / 255.0f;
            target.color[3] = source.rgba[3] / 255.0f;
            const SbVec3f minimum = progressive ?
                mesh.progressiveQuantizationMinimum : SbVec3f(0, 0, 0);
            const SbVec3f maximum = progressive ?
                mesh.progressiveQuantizationMaximum : SbVec3f(0, 0, 0);
            for (int axis = 0; axis < 3; ++axis) {
                target.popMinLevel[axis] = minimum[axis];
                target.popMaxFlags[axis] = maximum[axis];
            }
            target.popMinLevel[3] = packedProgressiveQuantization(
                progressive ? mesh.quantizationAtCut(level) :
                    ProgressiveQuantization());
            target.popMaxFlags[3] =
                (!mesh.normals.empty() ? 1.0f : 0.0f) +
                (progressive ? 2.0f : 0.0f) +
                ((source.flags & CadInstanceHidden) ? 4.0f : 0.0f);
            instances.push_back(target);
            sourceInstanceIndices.push_back(
                static_cast<uint32_t>(instanceIndex));
        }
        const uint32_t instanceCount =
            static_cast<uint32_t>(instances.size()) -
                build.commandBaseInstance;
        if (instanceCount && (!build.commandCount ||
                atlas->vertices.first >
                    static_cast<uint32_t>(
                        std::numeric_limits<int32_t>::max())))
            return abandon(8, "invalid atlas command base");

        if (instanceCount) {
          CadDrawElementsIndirectCommand command;
          command.count = static_cast<uint32_t>(build.commandCount);
          command.instanceCount = instanceCount;
          command.firstIndex = atlas->indices.first;
          command.baseVertex = static_cast<int32_t>(atlas->vertices.first);
          command.baseInstance = build.commandBaseInstance;
          build.renderedTriangleCount +=
              static_cast<uint64_t>(command.count / 3u) *
              static_cast<uint64_t>(command.instanceCount);
          cadAccumulateRenderedShadedWork(
              build.renderedWork, mesh, build.commandCut,
              static_cast<uint64_t>(command.count / 3u),
              static_cast<uint64_t>(command.instanceCount));
          IndirectPageWork& work = pageWorkFor(atlas->page);
          auto& commands =
              item.cullBackfaces ? work.culled : work.ordinary;
          if (item.partIndex < indirectCommandIndexByPart_.size()) {
            /*
             * A unique progressive part has exactly one active command and
             * one packed occurrence.  Shared parts intentionally retain the
             * sentinel and use the exact path when their LoD runs change.
             */
            if (item.instanceCount == 1u && instanceCount == 1u &&
                    indirectCommandIndexByPart_[item.partIndex] ==
                        noPreparedSlot) {
                indirectCommandIndexByPart_[item.partIndex] =
                    static_cast<uint32_t>(commands.size());
                indirectCommandCullByPart_[item.partIndex] =
                    item.cullBackfaces ? 1u : 0u;
                indirectPackedInstanceByPart_[item.partIndex] =
                    build.commandBaseInstance;
            } else {
                indirectCommandIndexByPart_[item.partIndex] =
                    noPreparedSlot;
                indirectPackedInstanceByPart_[item.partIndex] =
                    noPreparedSlot;
            }
          }
          commands.push_back(command);
        }
        ++build.itemCursor;
        build.occurrenceOffset = 0;
        build.commandItemActive = false;
      }
      build.phase = IndirectPreparationPhase::Preflight;
      build.pageCursor = 0;
    }
    /*
     * Preflight every page before issuing any draws.  Command streams larger
     * than a page's fixed scratch buffer are submitted in bounded chunks.
     * No recoverable rejection is permitted after the prepared frame becomes
     * visible to replay.
    */
    if (build.phase == IndirectPreparationPhase::Preflight) {
      const bool havePageWork = std::any_of(
          indirectPageWorkScratch_.begin(),
          indirectPageWorkScratch_.end(),
          [](const IndirectPageWork& work) {
              return !work.ordinary.empty() || !work.culled.empty();
          });
      if (!havePageWork && pressureProxyPoints.empty())
          return abandon(9, "empty visible page work");
      while (build.pageCursor < indirectPageWorkScratch_.size()) {
        if (preparationInterrupted())
            return false;
        const IndirectPageWork& work =
            indirectPageWorkScratch_[build.pageCursor++];
        if (work.ordinary.empty() && work.culled.empty())
            continue;
        const CadTriangleAtlasPage *page =
            gpuRes_->triangleAtlasPage(work.page);
        if (!page || !page->indirectBuf || !page->indirectCapacity ||
                (work.ordinary.empty() && work.culled.empty()))
            return abandon(11, "indirect page preflight");
      }
      build.phase = IndirectPreparationPhase::PublishSetup;
    }

    /*
     * Publish an immutable CPU submission record.  A replay still touches
     * every demanded atlas part, which both protects it from reclamation and
     * verifies that generation, prefix capacity, page, and offsets remain
     * valid.  It can then skip visibility resolution, per-occurrence LoD,
     * instance packing, command construction, sorting, and proxy packing.
    */
    IndirectPreparedFrame& prepared = indirectPrepared_;
    if (build.phase == IndirectPreparationPhase::PublishSetup) {
      prepared.valid = false;
      prepared.contextId = glue->contextid;
      prepared.planRevision = plan.revision;
      prepared.geometryRevision = plan.geometryRevision;
      prepared.shadedLayoutRevision = plan.shadedLayoutRevision;
      prepared.shadedLodRevision = plan.shadedLodRevision;
      prepared.appendRevision = plan.appendRevision;
      prepared.partGeometryRevision = plan.partGeometryRevision;
      prepared.instanceAttributeRevision =
          plan.instanceAttributeRevision;
      prepared.subpixelProxyRevision = plan.subpixelProxyRevision;
      prepared.progressiveCutCeiling = progressiveCutCeiling;
      prepared.viewProj = viewProj;
      prepared.renderedTriangleCount = build.renderedTriangleCount;
      prepared.renderedWork = build.renderedWork;
      prepared.instanceUploadSerial = 0;
      prepared.atlasRevision = gpuRes_->triangleAtlasRevision();
      prepared.atlasValidationCountdown = 30u;
      prepared.cameraMotionReplayCount = 0;
      prepared.atlasAdmissionPressure = build.atlasAdmissionPressure;
      prepared.atlasPressurePartCount = 0;

      prepared.parts.clear();
      if (prepared.parts.capacity() < visiblePartIndices.size())
          prepared.parts.reserve(visiblePartIndices.size());
      prepared.partByPlanPartIndex.assign(
          plan.partBindings.size(),
          std::numeric_limits<uint32_t>::max());
      build.partCursor = 0;
      build.phase = IndirectPreparationPhase::PublishParts;
    }
    if (build.phase == IndirectPreparationPhase::PublishParts) {
      while (build.partCursor < visiblePartIndices.size()) {
        if (preparationInterrupted())
            return false;
        const uint32_t partIndex =
            visiblePartIndices[build.partCursor++];
        if (!visiblePart[partIndex])
            continue;
        const CadPartBinding& binding = plan.partBindings[partIndex];
        const PartGeometry *geometry = binding.geometry.get();
        const CadTriangleAtlasPart *atlas = atlasBindings[partIndex];
        if (!geometry || !geometry->shaded || !atlas)
            return abandon(11, "prepared part binding");
        IndirectPreparedPart demand;
        demand.part = binding.part;
        demand.partIndex = partIndex;
        demand.generation = binding.generation;
        /* Replay protects the coherent resident prefix, not an unaffordable
         * richer request.  The frame-level pressure bit asks scene policy to
         * revisit quality after memory/view conditions change without making
         * every stable replay fail validation and rebuild O(scene). */
        demand.vertexCount = std::min(
            requestedVertexCounts[partIndex], atlas->vertexCount);
        demand.indexCount = std::min(
            requestedIndexCounts[partIndex], atlas->indexCount);
        demand.admissionPressure =
            requestedVertexCounts[partIndex] > atlas->vertexCount ||
            requestedIndexCounts[partIndex] > atlas->indexCount;
        if (demand.admissionPressure)
            ++prepared.atlasPressurePartCount;
        demand.page = atlas->page;
        demand.vertexFirst = atlas->vertices.first;
        demand.indexFirst = atlas->indices.first;
        demand.hasNormals = !geometry->shaded->normals.empty();
        if (partIndex < indirectPackedInstanceByPart_.size())
            demand.packedInstance =
                indirectPackedInstanceByPart_[partIndex];
        if (partIndex < indirectCommandIndexByPart_.size())
            demand.commandIndex =
                indirectCommandIndexByPart_[partIndex];
        if (partIndex < indirectCommandCullByPart_.size())
            demand.commandCulled =
                indirectCommandCullByPart_[partIndex] != 0u;
        prepared.partByPlanPartIndex[partIndex] =
            static_cast<uint32_t>(prepared.parts.size());
        prepared.parts.push_back(demand);
      }

    /*
     * Remove atlas holes/stale pages without releasing the vector storage of
     * active pages.  Moving active work into the prepared store transfers
     * capacities wholesale; the next exact build swaps them back.
     */
      indirectPageWorkScratch_.erase(
        std::remove_if(
            indirectPageWorkScratch_.begin(),
            indirectPageWorkScratch_.end(),
            [](const IndirectPageWork& work) {
                return work.ordinary.empty() && work.culled.empty();
            }),
        indirectPageWorkScratch_.end());
      prepared.pages.swap(indirectPageWorkScratch_);
      prepared.instances.swap(instances);
      prepared.sourceInstanceIndices.swap(sourceInstanceIndices);
      prepared.pressureProxyPoints.swap(pressureProxyPoints);
      prepared.pressureProxySourceInstanceIndices.swap(
          pressureProxySourceInstanceIndices);
      prepared.appendPatchAnchorInstanceCount =
          prepared.instances.size();
      prepared.instanceIndexBySource.assign(
          plan.visibleInstances.size(), noPreparedSlot);
      build.reverseCursor = 0;
      build.phase = IndirectPreparationPhase::ReverseInstances;
    }
    if (build.phase == IndirectPreparationPhase::ReverseInstances) {
      while (build.reverseCursor <
              prepared.sourceInstanceIndices.size()) {
        if (preparationInterrupted())
            return false;
        const size_t i = build.reverseCursor++;
        const uint32_t sourceIndex =
            prepared.sourceInstanceIndices[i];
        if (sourceIndex >= prepared.instanceIndexBySource.size())
            return abandon(11, "prepared instance reverse index");
        prepared.instanceIndexBySource[sourceIndex] =
            static_cast<uint32_t>(i);
      }
      prepared.pressureProxyIndexBySource.assign(
          plan.visibleInstances.size(), noPreparedSlot);
      build.reverseCursor = 0;
      build.phase = IndirectPreparationPhase::ReverseProxies;
    }
    if (build.phase == IndirectPreparationPhase::ReverseProxies) {
      while (build.reverseCursor <
              prepared.pressureProxySourceInstanceIndices.size()) {
        if (preparationInterrupted())
            return false;
        const size_t i = build.reverseCursor++;
        const uint32_t sourceIndex =
            prepared.pressureProxySourceInstanceIndices[i];
        if (sourceIndex >=
                prepared.pressureProxyIndexBySource.size())
            return abandon(11, "prepared proxy reverse index");
        prepared.pressureProxyIndexBySource[sourceIndex] =
            static_cast<uint32_t>(i);
      }
      prepared.valid = true;
      pressureProxyAppendOnly_ = false;
      ++pressureProxyRevision_;
      if (!pressureProxyRevision_)
          pressureProxyRevision_ = 1;
      build.phase = IndirectPreparationPhase::Submit;
    }

    pressureProxyPointsView_ = &prepared.pressureProxyPoints;
    atlasAdmissionPressure_ = build.atlasAdmissionPressure ||
        prepared.atlasAdmissionPressure;
    lastRenderedTriangleCount_ = prepared.renderedTriangleCount;
    if (build.phase == IndirectPreparationPhase::Submit) {
      const bool submitted =
          submitIndirectPrepared(glue, viewProj, viewVolume);
      if (!submitted) {
          pressureProxyPointsView_ = nullptr;
          if (renderInterrupted())
              return false;
          return abandon(12, "indirect submission");
      }
      if (lastIndirectStatus_ != 0)
          return abandon(lastIndirectStatus_, "indirect submission");
    }

    const uint32_t completedSlices = build.sliceCount;
    const size_t completedVisibleOccurrences =
        build.visibleOccurrenceCount;
    const size_t completedVisibleParts = visiblePartIndices.size();
    const uint64_t completedTriangles = build.renderedTriangleCount;
    gpuRes_->endTriangleAtlasExactPreparation();
    build = IndirectPreparationState();

    /*
     * The atlas is now the only shaded geometry owner for this context.
     * Retaining the former expanded world-space and per-part triangle VBOs
     * would defeat the unified memory ceiling.
     */
    gpuRes_->releaseFlatShaded(glue);
    gpuRes_->releaseStandaloneTriangles(glue);
    if (lastIndirectStatus_ == 0)
        reportedIndirectStatus_ = 0;
    if (configuration_->renderTiming) {
        const auto completed = IndirectClock::now();
        const auto milliseconds = [](auto begin, auto end) {
            return std::chrono::duration<double, std::milli>(
                end - begin).count();
        };
        const double total =
            milliseconds(indirectStarted, completed);
        if (total >= 10.0)
            std::fprintf(stderr,
                "CadRendererGL exact indirect final-slice=%.3fms "
                "slices=%u "
                "source_instances=%zu visible_instances=%zu "
                "visible_parts=%zu triangles=%llu\n",
                total,
                completedSlices,
                plan.visibleInstances.size(),
                completedVisibleOccurrences,
                completedVisibleParts,
                static_cast<unsigned long long>(
                    completedTriangles));
    }
    if (configuration_->indirectDebug) {
        static uint32_t lastDebugContext = UINT32_MAX;
        static size_t lastDebugParts = 0;
        const size_t parts = gpuRes_->triangleAtlasPartCount();
        if (lastDebugContext != glue->contextid ||
                parts >= lastDebugParts + 4096u) {
            std::fprintf(stderr,
                "CadRendererGL indirect rendered context=%u commands=%zu "
                "pages=%zu parts=%zu allocated=%zu live=%zu\n",
                glue->contextid, plan.shadedItems.size(),
                gpuRes_->triangleAtlasPageCount(), parts,
                gpuRes_->triangleAtlasAllocatedBytes(),
                gpuRes_->triangleAtlasLiveBytes());
            lastDebugContext = glue->contextid;
            lastDebugParts = parts;
        }
    }
    return true;
}

void CadRendererGL::renderInstanced(
        const CadFramePlan& plan,
        const SoCADAssembly& assembly,
        const SoGLContext*   glue,
        const SbMatrix&      viewProj,
        const SbViewVolume&  viewVolume,
        const std::unordered_map<PartId, uint64_t,
                                 std::hash<PartId>>& /*partGenMap*/,
        bool drawWire,
        bool solidWireOnly,
        bool drawShaded)
{
    size_t deadlineWork = 256u;
    bool interrupted = renderInterruptedAfter(deadlineWork);
    if (interrupted)
        return;

    // Build per-instance vertex data (transform + colour)
    const size_t nInst = plan.visibleInstances.size();
    if (nInst == 0) return;
    uint64_t renderedTriangleCount = 0;

    std::vector<InstVertex> instData(nInst);
    for (size_t i = 0; i < nInst; ++i) {
        if (renderInterruptedAfter(deadlineWork))
            return;
        const auto& vi = plan.visibleInstances[i];
        std::memcpy(instData[i].transform, vi.transform.data(), 16 * sizeof(float));
        instData[i].color[0] = vi.rgba[0] / 255.0f;
        instData[i].color[1] = vi.rgba[1] / 255.0f;
        instData[i].color[2] = vi.rgba[2] / 255.0f;
        instData[i].color[3] = vi.rgba[3] / 255.0f;
    }

    gpuRes_->uploadTransientInstanceData(
        instData.data(),
        static_cast<GLsizeiptr>(nInst * sizeof(InstVertex)),
        glue);
    if (renderInterruptedAfter(deadlineWork))
        return;

    const GLuint instVbo = gpuRes_->transientInstanceVbo();
    const float* vp = viewProj[0];
    const GLsizei instStride = static_cast<GLsizei>(sizeof(InstVertex));

    // --- Helper to bind per-instance attributes ---
    //
    // Must be called with the correct VAO already bound (if any).  The
    // baseInstance parameter is the index of the first instance for this draw
    // item in the per-frame instance VBO; it is used as a byte offset so each
    // part reads its own slice of the buffer without needing GL 4.2
    // glDrawElementsInstancedBaseInstance.
    auto bindInstAttribs = [&](uint32_t baseInstance) {
        glue->glBindBuffer(GL_ARRAY_BUFFER, instVbo);

        const GLsizeiptr baseOff =
            static_cast<GLsizeiptr>(baseInstance) * instStride;

        // a_instTransform occupies 4 consecutive attribute locations.
        // We use the fixed layout (kInstTransformLoc..kInstTransformLoc+3).
        for (GLuint col = 0; col < 4; ++col) {
            GLuint aloc = kInstTransformLoc + col;
            const GLvoid* off = reinterpret_cast<const GLvoid*>(
                baseOff +
                static_cast<GLsizeiptr>(offsetof(InstVertex, transform)) +
                static_cast<GLsizeiptr>(col) * 4 * static_cast<GLsizeiptr>(sizeof(float)));
            glue->glVertexAttribPointerARB(aloc, 4, GL_FLOAT, GL_FALSE,
                                           instStride, off);
            glue->glEnableVertexAttribArrayARB(aloc);
            glue->glVertexAttribDivisor(aloc, 1);
        }

        // a_instColor
        {
            GLuint aloc = kInstColorLoc;
            const GLvoid* off = reinterpret_cast<const GLvoid*>(
                baseOff +
                static_cast<GLsizeiptr>(offsetof(InstVertex, color)));
            glue->glVertexAttribPointerARB(aloc, 4, GL_FLOAT, GL_FALSE,
                                           instStride, off);
            glue->glEnableVertexAttribArrayARB(aloc);
            glue->glVertexAttribDivisor(aloc, 1);
        }
        glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
    };

    // Must also be called with the same VAO still bound so the cleanup state
    // is recorded there (resets divisors to 0, disables the attribs).
    auto unbindInstAttribs = [&]() {
        for (GLuint col = 0; col < 4; ++col) {
            glue->glVertexAttribDivisor(kInstTransformLoc + col, 0);
            glue->glDisableVertexAttribArrayARB(kInstTransformLoc + col);
        }
        glue->glVertexAttribDivisor(kInstColorLoc, 0);
        glue->glDisableVertexAttribArrayARB(kInstColorLoc);
    };

    // --- Wire pass ---
    if (drawWire && !plan.wireItems.empty()) {
        const CadWireRasterState rasterState = captureWireRasterState(
            glue, caps_.hasLineStipple);
        struct WireLocations {
            GLint viewProjection = -1;
            GLint position = 0;
            GLint encodeScale = -1;
            GLint decodeScale = -1;
            GLint minimum = -1;
        };
        const GLuint programs[2] = {
            shaders_.wireInst, shaders_.wirePopInst
        };
        WireLocations locations[2];
        for (int variant = 0; variant < 2; ++variant) {
            if (!programs[variant])
                continue;
            locations[variant].viewProjection =
                glue->glGetUniformLocationARB(
                    programs[variant], "u_viewProj");
            locations[variant].position =
                glue->glGetAttribLocationARB(programs[variant], "a_pos");
            if (locations[variant].position < 0)
                locations[variant].position = 0;
        }
        if (programs[1]) {
            locations[1].encodeScale = glue->glGetUniformLocationARB(
                programs[1], "u_popEncodeScale");
            locations[1].decodeScale = glue->glGetUniformLocationARB(
                programs[1], "u_popDecodeScale");
            locations[1].minimum = glue->glGetUniformLocationARB(
                programs[1], "u_popMin");
        }
        GLuint activeProgram = 0;

        for (const auto& item : plan.wireItems) {
            if (renderInterruptedAfter(deadlineWork)) {
                interrupted = true;
                break;
            }
            if (solidWireOnly && item.customWireStyle) continue;
            CadWireGpu* w = gpuRes_->wireFor(item.rep.part);
            if (!w || w->segCount == 0) continue;
            const PartGeometry *geometry =
                assembly.partGeometry(item.rep.part);
            const WireRep *progressive =
                geometry && geometry->wire &&
                geometry->wire->isProgressive() ?
                &*geometry->wire : nullptr;
            uint32_t runStart = 0;
            while (runStart < item.instanceCount) {
                if (renderInterruptedAfter(deadlineWork)) {
                    interrupted = true;
                    break;
                }
                while (runStart < item.instanceCount &&
                    !cadInstanceDrawable(
                        plan, item, item.baseInstance + runStart,
                        CadDrawChannel::Wire)) {
                    if (renderInterruptedAfter(deadlineWork)) {
                        interrupted = true;
                        break;
                    }
                    ++runStart;
                }
                if (interrupted)
                    break;
                if (runStart == item.instanceCount)
                    break;
                const uint32_t baseInstance =
                    item.baseInstance + runStart;
                const CadVisibleInstance& levelInstance =
                    plan.visibleInstances[baseInstance];
                const uint8_t level = progressive ?
                    cadResolvedProgressiveCut(
                        assembly.effectiveProgressiveCut(
                            levelInstance.lodCut),
                        progressive->progressiveMinimumCut,
                        progressive->progressiveResidentCut) :
                    Obol::ProgressiveCutUnspecified;
                uint32_t runEnd = runStart + 1;
                while (runEnd < item.instanceCount &&
                    cadInstanceDrawable(
                        plan, item, item.baseInstance + runEnd,
                        CadDrawChannel::Wire) &&
                    (!progressive ||
                     cadResolvedProgressiveCut(
                        assembly.effectiveProgressiveCut(
                            plan.visibleInstances[
                                item.baseInstance + runEnd].lodCut),
                        progressive->progressiveMinimumCut,
                        progressive->progressiveResidentCut) == level))
                {
                    if (renderInterruptedAfter(deadlineWork)) {
                        interrupted = true;
                        break;
                    }
                    ++runEnd;
                }
                if (interrupted)
                    break;

                const int variant = progressive &&
                    !progressive->quantizationAtCut(level).isExact() ? 1 : 0;
                const WireLocations& loc = locations[variant];
                if (!programs[variant]) {
                    runStart = runEnd;
                    continue;
                }
                if (activeProgram != programs[variant]) {
                    activeProgram = programs[variant];
                    glue->glUseProgramObjectARB(activeProgram);
                    glue->glUniformMatrix4fvARB(
                        loc.viewProjection, 1, GL_FALSE, vp);
                }
                if (variant) {
                    uploadProgressivePositionUniforms(
                        glue, loc.encodeScale, loc.decodeScale, loc.minimum,
                        progressive->quantizationAtCut(level),
                        progressive->progressiveQuantizationMinimum,
                        progressive->progressiveQuantizationMaximum);
                }
                const GLsizei segmentFirst = progressive ?
                    static_cast<GLsizei>(
                        progressive->segmentFirstAtCut(level)) : 0;
                const GLsizei segmentCount = progressive ?
                    static_cast<GLsizei>(
                        progressive->segmentCountAtCut(level)) :
                    w->segCount;
                if (segmentCount <= 0) {
                    runStart = runEnd;
                    continue;
                }
                const auto& styleInst = plan.visibleInstances[baseInstance];
                applyWireRasterStyle(glue, styleInst, caps_.hasLineStipple);
                if (w->vao && glue->glBindVertexArray) {
                    glue->glBindVertexArray(w->vao);
                    if (w->instanceVbo != instVbo ||
                            w->instanceBase != baseInstance) {
                        bindInstAttribs(baseInstance);
                        w->instanceVbo = instVbo;
                        w->instanceBase = baseInstance;
                    }
                } else {
                    glue->glBindBuffer(GL_ARRAY_BUFFER, w->posBuf);
                    glue->glVertexAttribPointerARB(
                                                   static_cast<GLuint>(loc.position), 3,
                                                   GL_FLOAT, GL_FALSE,
                                                   3 * sizeof(float), nullptr);
                    glue->glEnableVertexAttribArrayARB(
                        static_cast<GLuint>(loc.position));
                    if (!w->sequentialSegments)
                        glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, w->segIdxBuf);
                    bindInstAttribs(baseInstance);
                }

                const GLsizei runCount = static_cast<GLsizei>(runEnd - runStart);
                if (w->sequentialSegments) {
                    glue->glDrawArraysInstanced(
                                                GL_LINES, segmentFirst * 2,
                                                segmentCount * 2,
                                                runCount);
                } else {
                    glue->glDrawElementsInstanced(
                                                  GL_LINES, segmentCount * 2,
                                                  GL_UNSIGNED_INT,
                                                  reinterpret_cast<const GLvoid *>(
                                                      static_cast<uintptr_t>(segmentFirst) * 2u *
                                                      sizeof(uint32_t)),
                                                  runCount);
                }
                cadAccumulateRenderedWireWork(
                    lastRenderedWork_,
                    static_cast<uint64_t>(segmentCount),
                    static_cast<uint64_t>(runCount));

                if (w->vao && glue->glBindVertexArray) {
                    glue->glBindVertexArray(0);
                } else {
                    unbindInstAttribs();
                    glue->glDisableVertexAttribArrayARB(
                        static_cast<GLuint>(loc.position));
                    glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
                    if (!w->sequentialSegments)
                        glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
                }
                runStart = runEnd;
            }
            if (interrupted)
                break;
        }

        glue->glUseProgramObjectARB(0);
        restoreWireRasterState(glue, rasterState, caps_.hasLineStipple);
    }
    if (interrupted)
        return;

    // --- Shaded pass ---
    if (drawShaded && !plan.shadedItems.empty()) {
        struct ShadedLocations {
            GLint viewProjection = -1;
            GLint hasNormal = -1;
            GLint position = 0;
            GLint normal = 1;
            GLint encodeScale = -1;
            GLint decodeScale = -1;
            GLint minimum = -1;
        };
        const GLuint programs[2] = {
            shaders_.shadedInst, shaders_.shadedPopInst
        };
        ShadedLocations locations[2];
        for (int variant = 0; variant < 2; ++variant) {
            if (!programs[variant])
                continue;
            locations[variant].viewProjection =
                glue->glGetUniformLocationARB(
                    programs[variant], "u_viewProj");
            locations[variant].hasNormal =
                glue->glGetUniformLocationARB(
                    programs[variant], "u_hasNorm");
            locations[variant].position =
                glue->glGetAttribLocationARB(programs[variant], "a_pos");
            locations[variant].normal =
                glue->glGetAttribLocationARB(programs[variant], "a_norm");
            if (locations[variant].position < 0)
                locations[variant].position = 0;
            if (locations[variant].normal < 0)
                locations[variant].normal = 1;
        }
        if (programs[1]) {
            locations[1].encodeScale = glue->glGetUniformLocationARB(
                programs[1], "u_popEncodeScale");
            locations[1].decodeScale = glue->glGetUniformLocationARB(
                programs[1], "u_popDecodeScale");
            locations[1].minimum = glue->glGetUniformLocationARB(
                programs[1], "u_popMin");
        }
        GLuint activeProgram = 0;
        bool lightsUploaded[2] = {false, false};

        for (const auto& item : plan.shadedItems) {
            if (renderInterruptedAfter(deadlineWork)) {
                interrupted = true;
                break;
            }
            CadTriGpu* t = gpuRes_->triFor(item.rep.part);
            if (!t || t->idxCount == 0) continue;
            size_t levelInstanceIndex = plan.visibleInstances.size();
            for (uint32_t instanceOffset = 0;
                    instanceOffset < item.instanceCount; ++instanceOffset) {
                if (renderInterruptedAfter(deadlineWork)) {
                    interrupted = true;
                    break;
                }
                const size_t candidate =
                    item.baseInstance + instanceOffset;
                if (cadInstanceDrawable(
                        plan, item, candidate, CadDrawChannel::Shaded)) {
                    levelInstanceIndex = candidate;
                    break;
                }
            }
            if (interrupted)
                break;
            if (levelInstanceIndex >= plan.visibleInstances.size())
                continue;
            const PartGeometry *geometry =
                assembly.partGeometry(item.rep.part);
            const TriMesh *progressive =
                geometry && geometry->shaded &&
                geometry->shaded->isProgressive() ?
                &*geometry->shaded : nullptr;
            const CadVisibleInstance& levelInstance =
                plan.visibleInstances[levelInstanceIndex];
            const uint8_t level = progressive ?
                cadResolvedProgressiveCut(
                    assembly.effectiveProgressiveCut(
                        levelInstance.lodCut),
                    progressive->progressiveMinimumCut,
                    progressive->progressiveResidentCut) :
                Obol::ProgressiveCutUnspecified;
            const int variant = progressive &&
                !progressive->quantizationAtCut(level).isExact() ? 1 : 0;
            const ShadedLocations& loc = locations[variant];
            if (!programs[variant])
                continue;
            if (activeProgram != programs[variant]) {
                activeProgram = programs[variant];
                glue->glUseProgramObjectARB(activeProgram);
                glue->glUniformMatrix4fvARB(
                    loc.viewProjection, 1, GL_FALSE, vp);
                if (!lightsUploaded[variant]) {
                    this->uploadLights(glue, activeProgram);
                    this->uploadViewFacing(
                        glue, activeProgram, viewVolume);
                    lightsUploaded[variant] = true;
                }
            }
            if (variant) {
                uploadProgressivePositionUniforms(
                    glue, loc.encodeScale, loc.decodeScale, loc.minimum,
                    progressive->quantizationAtCut(level),
                    progressive->progressiveQuantizationMinimum,
                    progressive->progressiveQuantizationMaximum);
            }
            const GLsizei indexCount = progressiveTriangleIndexCount(
                assembly, item.rep.part, levelInstance, t->idxCount);
            if (indexCount <= 0)
                continue;

            setCadBackfaceCulling(glue,
                cadProgressiveCutCullSafe(
                    item.cullBackfaces, progressive, level));
            glue->glUniform1iARB(
                loc.hasNormal, (t->normBuf != 0) ? 1 : 0);

            const bool retainedVao =
                t->vao && glue->glBindVertexArray;
            if (retainedVao) {
                glue->glBindVertexArray(t->vao);
            } else {
                glue->glBindBuffer(GL_ARRAY_BUFFER, t->posBuf);
                glue->glVertexAttribPointerARB(
                                               static_cast<GLuint>(loc.position), 3,
                                               GL_FLOAT, GL_FALSE,
                                               3 * sizeof(float), nullptr);
                glue->glEnableVertexAttribArrayARB(
                    static_cast<GLuint>(loc.position));
                if (t->normBuf && loc.normal >= 0) {
                    glue->glBindBuffer(GL_ARRAY_BUFFER, t->normBuf);
                    glue->glVertexAttribPointerARB(
                                                   static_cast<GLuint>(loc.normal), 3,
                                                   GL_FLOAT, GL_FALSE,
                                                   3 * sizeof(float), nullptr);
                    glue->glEnableVertexAttribArrayARB(
                        static_cast<GLuint>(loc.normal));
                }
                glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, t->idxBuf);
            }
            uint32_t runStart = 0;
            while (runStart < item.instanceCount) {
                if (renderInterruptedAfter(deadlineWork)) {
                    interrupted = true;
                    break;
                }
                while (runStart < item.instanceCount &&
                        !cadInstanceDrawable(
                            plan, item, item.baseInstance + runStart,
                            CadDrawChannel::Shaded)) {
                    if (renderInterruptedAfter(deadlineWork)) {
                        interrupted = true;
                        break;
                    }
                    ++runStart;
                }
                if (interrupted)
                    break;
                if (runStart == item.instanceCount)
                    break;
                uint32_t runEnd = runStart + 1;
                while (runEnd < item.instanceCount &&
                        cadInstanceDrawable(
                            plan, item, item.baseInstance + runEnd,
                            CadDrawChannel::Shaded)) {
                    if (renderInterruptedAfter(deadlineWork)) {
                        interrupted = true;
                        break;
                    }
                    ++runEnd;
                }
                if (interrupted)
                    break;
                const uint32_t baseInstance =
                    item.baseInstance + runStart;
                if (!retainedVao ||
                        t->instanceVbo != instVbo ||
                        t->instanceBase != baseInstance) {
                    bindInstAttribs(baseInstance);
                    t->instanceVbo = instVbo;
                    t->instanceBase = baseInstance;
                }
                glue->glDrawElementsInstanced(
                    GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr,
                    static_cast<GLsizei>(runEnd - runStart));
                if (geometry && geometry->shaded)
                    cadAccumulateRenderedShadedWork(
                        lastRenderedWork_, *geometry->shaded, level,
                        static_cast<uint64_t>(indexCount / 3),
                        static_cast<uint64_t>(runEnd - runStart));
                renderedTriangleCount +=
                    static_cast<uint64_t>(indexCount / 3) *
                    static_cast<uint64_t>(runEnd - runStart);
                runStart = runEnd;
            }

            if (retainedVao) {
                glue->glBindVertexArray(0);
            } else {
                unbindInstAttribs();
                if (t->normBuf && loc.normal >= 0) {
                    glue->glDisableVertexAttribArrayARB(
                        static_cast<GLuint>(loc.normal));
                }
                glue->glDisableVertexAttribArrayARB(
                    static_cast<GLuint>(loc.position));
                glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
                glue->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            }
            if (interrupted)
                break;
        }

        glue->glUseProgramObjectARB(0);
        lastRenderedTriangleCount_ =
            renderedTriangleCount >
                    UINT64_MAX - lastRenderedTriangleCount_ ?
                UINT64_MAX :
                lastRenderedTriangleCount_ + renderedTriangleCount;
    }
}

} // namespace internal
} // namespace Obol
