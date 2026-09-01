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
 * @brief Retained VBO, fixed-function, and immediate CAD executors.
 *
 * CadRendererGL.cpp owns capability selection, state-boundary guards, resource
 * lifetime, and routing.  This unit owns shared executor utilities and the
 * retained compatibility paths.  Flat, instanced, and indirect strategies
 * compile independently.
 */

#include "CadRendererGL.h"
#include "CadRendererConfiguration.h"
#include "CadRendererGLExecutorUtils.h"
#include "CadResolvedDraw.h"
#include "CadShaderSources.h"

#include <Obol/cad/SoCADAssembly.h>

#include <Inventor/system/gl.h>
#include "glue/glp.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <new>

#ifndef GL_DRAW_INDIRECT_BUFFER
#define GL_DRAW_INDIRECT_BUFFER 0x8F3F
#endif

namespace Obol {
namespace internal {

static_assert(sizeof(SbVec3f) == 3 * sizeof(float),
              "SbVec3f must remain tightly packed for CAD GPU uploads");

const float *
executorPackedVec3fData(const std::vector<SbVec3f>& values)
{
    return values.empty() ? nullptr : values[0].getValue();
}

void
executorAppendPackedPoint(std::vector<float>& packed, const SbVec3f& point)
{
    packed.push_back(point[0]);
    packed.push_back(point[1]);
    packed.push_back(point[2]);
}

void
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

void
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
bool
cadProgressiveCutCullSafe(bool sourceCullSafe, const TriMesh *mesh,
                          uint8_t cut)
{
    return sourceCullSafe &&
        (!mesh || mesh->quantizationAtCut(cut).isExact());
}

static const float kLightDir[3] = { 0.577f, 0.577f, 0.577f };

static void
executorNormalMatrix(const std::array<float, 16>& transform,
                     float (&packed)[9])
{
    cadPackInstanceNormalTransform(transform.data(), packed);
}

void
cadPackInstanceNormalTransform(const float *transform,
                               float *normalTransform)
{
    SbMatrix model;
    model.setValue(transform);
    const SbMatrix normal = model.inverse().transpose();
    for (int row = 0; row < 3; ++row)
        for (int column = 0; column < 3; ++column)
            normalTransform[row * 3 + column] = normal[row][column];
}

ExecutorFrustumPlanes
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

bool
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

void
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

uint8_t
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
        level = cadResolvedProgressiveCut(
            level, mesh.progressiveMinimumCut, resident);
    }
    return level;
}

uint8_t
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
        level = cadResolvedProgressiveCut(
            level, wire.progressiveMinimumCut, resident);
    }
    return level;
}

/* Relative projected area is sufficient for admission ordering: viewport
 * dimensions multiply every candidate by the same constant.  The explicit
 * homogeneous transform preserves the intended orthographic contract (depth
 * has no effect) while perspective naturally favors near visible geometry. */
double
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

CadWireRasterState
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

void
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

void
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

SbVec3f
transformedFlatPoint(const SbVec3f& point,
                     const std::array<float, 16>& matrix)
{
    const float x = point[0];
    const float y = point[1];
    const float z = point[2];
    return SbVec3f(x * matrix[0] + y * matrix[4] + z * matrix[8] + matrix[12],
                   x * matrix[1] + y * matrix[5] + z * matrix[9] + matrix[13],
                   x * matrix[2] + y * matrix[6] + z * matrix[10] + matrix[14]);
}

uint64_t
cadSaturatingWorkAdd(uint64_t left, uint64_t right)
{
    return right > UINT64_MAX - left ? UINT64_MAX : left + right;
}

void
CadRendererGL::configureFixedClientArrays(
        const SoGLContext *glue, bool normals, bool colors)
{
    if (!glue)
        return;

    /* glPushClientAttrib in the outer direct-render guard restores the
     * caller.  This function owns the state *during* a CAD draw.  In
     * particular, software TNL evaluates every enabled array even when the
     * CAD executor never references it directly; a stale Coin pointer can
     * therefore turn a valid large first/count range into an out-of-bounds
     * read. */
    glue->glEnableClientState(GL_VERTEX_ARRAY);
    if (normals)
        glue->glEnableClientState(GL_NORMAL_ARRAY);
    else
        glue->glDisableClientState(GL_NORMAL_ARRAY);
    if (colors)
        glue->glEnableClientState(GL_COLOR_ARRAY);
    else
        glue->glDisableClientState(GL_COLOR_ARRAY);
    glue->glDisableClientState(GL_INDEX_ARRAY);
    glue->glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glue->glDisableClientState(GL_EDGE_FLAG_ARRAY);
}

float
packedProgressiveQuantization(ProgressiveQuantization quantization)
{
    return static_cast<float>(quantization.xBits) +
        17.0f * static_cast<float>(quantization.yBits) +
        289.0f * static_cast<float>(quantization.zBits);
}

static GLsizei progressiveWireSegmentCount(
        const SoCADAssembly& assembly,
        const Obol::CadViewState& viewState, PartId part,
        const CadVisibleInstance& instance, GLsizei residentCount)
{
    const PartGeometry *geometry = assembly.partGeometry(part);
    if (!geometry || !geometry->wire || !geometry->wire->isProgressive())
        return residentCount;
    return static_cast<GLsizei>(geometry->wire->segmentCountAtCut(
        Obol::cadEffectiveProgressiveCut(
            viewState, part, instance.lodCut)));
}

GLsizei progressiveTriangleIndexCount(
        const SoCADAssembly& assembly,
        const Obol::CadViewState& viewState, PartId part,
        const CadVisibleInstance& instance, GLsizei residentCount)
{
    const PartGeometry *geometry = assembly.partGeometry(part);
    if (!geometry || !geometry->shaded ||
            !geometry->shaded->isProgressive())
        return residentCount;
    return static_cast<GLsizei>(geometry->shaded->indexCountAtCut(
        Obol::cadEffectiveProgressiveCut(
            viewState, part, instance.lodCut)));
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

void
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

static const CadProgressiveGpu*
ensureProgressiveWireGpuImpl(
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
            const SbVec3f point = cadProgressiveSnapPoint(
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
ensureProgressiveWireGpu(
        CadGpuResources *resources, PartId part, const WireRep& wire,
        uint8_t level,
        const std::vector<CadProgressiveGpu::PackedRange> *sourceRanges,
        const SoGLContext *glue)
{
    try {
        return ensureProgressiveWireGpuImpl(
            resources, part, wire, level, sourceRanges, glue);
    } catch (const std::bad_alloc &) {
        /* Transient packing is optional retained acceleration.  Preserve an
         * already valid cut when process or backend pressure cannot admit
         * another CPU staging vector; the completed framebuffer remains the
         * visual fallback when no cut has been prepared yet. */
        return resources ?
            resources->progressiveForAny(part, false, level) : nullptr;
    }
}

static const CadProgressiveGpu*
ensureProgressiveTriGpuImpl(
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
            const SbVec3f point = cadProgressiveSnapPoint(
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
                triangle[k] = cadProgressiveSnapPoint(
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

static const CadProgressiveGpu*
ensureProgressiveTriGpu(
        CadGpuResources *resources, PartId part, const TriMesh& mesh,
        uint8_t level,
        const std::vector<CadProgressiveGpu::PackedRange> *sourceRanges,
        bool *prepared, const SoGLContext *glue)
{
    try {
        return ensureProgressiveTriGpuImpl(
            resources, part, mesh, level, sourceRanges, prepared, glue);
    } catch (const std::bad_alloc &) {
        if (prepared) *prepared = false;
        return resources ?
            resources->progressiveForAny(part, true, level) : nullptr;
    }
}

/* OSMesa's fixed-function line path is materially faster than submitting
 * triangle polygons in line mode or issuing every edge through immediate
 * mode.  Retain one snapped copy of the indexed mesh positions for each PoP
 * cut and reuse the ordinary compact edge-index buffer.  Unlike the legacy
 * wire representation this is O(vertices), not six copied positions per
 * triangle, and an unchanged frame is a cache lookup. */
static const CadProgressiveGpu*
ensureProgressiveIndexedWireGpuImpl(
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
        executorAppendPackedPoint(positions, cadProgressiveSnapPoint(
            mesh.positions[index], mesh.progressiveQuantizationMinimum,
            mesh.progressiveQuantizationMaximum, quantization));
    if (prepared) *prepared = true;
    resources->uploadProgressive(
        part, false, level, positions, std::vector<float>(), true,
        signature, std::vector<CadProgressiveGpu::PackedRange>(), glue);
    return resources->progressiveFor(part, false, level, signature);
}

static const CadProgressiveGpu*
ensureProgressiveIndexedWireGpu(
        CadGpuResources *resources, PartId part, const TriMesh& mesh,
        uint8_t level, bool *prepared, const SoGLContext *glue)
{
    try {
        return ensureProgressiveIndexedWireGpuImpl(
            resources, part, mesh, level, prepared, glue);
    } catch (const std::bad_alloc &) {
        if (prepared) *prepared = false;
        return resources ?
            resources->progressiveForAny(part, false, level) : nullptr;
    }
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

static uint8_t
executorMaximumRequestedCut(const CadFramePlan& plan,
                            const Obol::CadViewState& viewState,
                            PartId part)
{
    const auto found = plan.partPresentation.find(part);
    return found == plan.partPresentation.end() ?
        Obol::ProgressiveCutUnspecified :
        Obol::cadMaximumEffectiveProgressiveCut(
            viewState, found->second.maximumRequestedCut);
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
                plan, activeViewState(), item.rep.part), glue);
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
            configureFixedClientArrays(glue, false, false);
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
            wire->triangleEdges() : nullptr;
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
                    effectiveProgressiveCut(
                        binding.part, instance.lodCut)) :
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
                                        cadProgressiveSnapPoint(
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

void
cadAccumulateRenderedShadedWork(Obol::CadRenderedWork& work,
                                const Obol::TriMesh& mesh,
                                uint8_t level, uint64_t triangles,
                                uint64_t occurrences,
                                uint64_t visiblePositions)
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

void
cadAccumulateRenderedWireWork(Obol::CadRenderedWork& work,
                              uint64_t segments,
                              uint64_t occurrences)
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

void
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
			effectiveProgressiveCut(
			    item.rep.part, inst.lodCut)) :
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
                        assembly, activeViewState(), item.rep.part, inst, w->segCount);
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
            GLint normalMatrix = -1;
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
            locations[programIndex].normalMatrix =
                glue->glGetUniformLocationARB(
                    programs[programIndex], "u_normalMatrix");
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
            static std::atomic<unsigned int> uniformLocationReportCount{0};
            if (uniformLocationReportCount.fetch_add(
                    1, std::memory_order_relaxed) < 4) {
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
                static std::atomic<unsigned int> geometryReportCount{0};
                if (geometryReportCount.fetch_add(
                        1, std::memory_order_relaxed) < 32) {
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
			effectiveProgressiveCut(
			    item.rep.part, inst.lodCut)) :
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
                            this->uploadAssemblyTransform(
                                glue, activeProgram);
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
                    static std::atomic<unsigned int> normalReportCount{0};
                    if (normalReportCount.fetch_add(
                            1, std::memory_order_relaxed) < 32) {
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
                if (hasNorm && loc.normalMatrix >= 0) {
                    float normalMatrix[9];
                    executorNormalMatrix(inst.transform, normalMatrix);
                    glue->glUniformMatrix3fvARB(
                        loc.normalMatrix, 1, GL_FALSE, normalMatrix);
                }
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
                    static std::atomic<unsigned int> uniformValueReportCount{0};
                    if (uniformValueReportCount.fetch_add(
                            1, std::memory_order_relaxed) < 8) {
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
                            assembly, activeViewState(), item.rep.part, inst, t->idxCount),
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
    configureFixedClientArrays(glue, false, false);
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
                    effectiveProgressiveCut(
                        item.rep.part, inst.lodCut));
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
		    effectiveProgressiveCut(
			item.rep.part, inst.lodCut));
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
                    assembly, activeViewState(), item.rep.part, inst, wire->segCount);
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
    configureFixedClientArrays(glue, false, false);
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
		    effectiveProgressiveCut(
			item.rep.part, inst.lodCut));
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
                static std::atomic<unsigned int> fixedLightReportCount{0};
                if (fixedLightReportCount.fetch_add(
                        1, std::memory_order_relaxed) < 16) {
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
                assembly, activeViewState(), item.rep.part, inst, tri->idxCount);
            const CadProgressiveGpu *cut = nullptr;
            uint8_t level = Obol::ProgressiveCutUnspecified;
            if (progressive) {
		level = executorVisibleProgressiveCut(
		    *progressive, inst, fp,
		    effectiveProgressiveCut(
			item.rep.part, inst.lodCut));
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
		effectiveProgressiveCut(
		    item.rep.part, inst.lodCut));
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
                        cadProgressiveSnapPoint(wire.segmentPoints[point],
                            wire.progressiveQuantizationMinimum,
                            wire.progressiveQuantizationMaximum,
                            wire.quantizationAtCut(drawLevel)) :
                        wire.segmentPoints[point];
                    const SbVec3f b = wire.isProgressive() ?
                        cadProgressiveSnapPoint(
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
		effectiveProgressiveCut(
		    item.rep.part, inst.lodCut));
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
                            cadProgressiveSnapPoint(mesh.positions[idx],
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

} // namespace internal
} // namespace Obol
