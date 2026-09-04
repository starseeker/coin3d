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
 * @file CadRendererGLFlat.cpp
 * @brief Flattened CAD wire and shaded batch executors.
 *
 * CadRendererGL.cpp owns capability selection, state-boundary guards, resource
 * lifetime, and routing.  This unit owns camera-local flattened atlas
 * construction and submission.  Retained, instanced, and indirect execution
 * remain separate strategies.
 */

#include "CadRendererGL.h"
#include "CadRendererConfiguration.h"
#include "CadRendererGLExecutorUtils.h"
#include "CadProgressiveUtils.h"
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
#include <new>

#ifndef GL_DRAW_INDIRECT_BUFFER
#define GL_DRAW_INDIRECT_BUFFER 0x8F3F
#endif

namespace Obol {
namespace internal {
namespace {

/* Keep every independently committed flattened range below the software
 * executor's cancellation quantum.  A source-authored adaptive cluster range
 * may contain millions of triangles; treating it as one atomic atlas unit
 * prevents any deadline-bounded replay from making durable progress. */
constexpr size_t flatSoftwarePrimitiveChunk = 8u * 1024u;
constexpr size_t flatShadedRangeIndexChunk =
    flatSoftwarePrimitiveChunk * 3u;

enum class FlatShadedAtlasRangeResult {
    Complete,
    Interrupted,
    Invalid
};

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

static uint32_t flatRgbaKey(const CadVisibleInstance& inst)
{
    return static_cast<uint32_t>(inst.rgba[0]) |
           (static_cast<uint32_t>(inst.rgba[1]) << 8) |
           (static_cast<uint32_t>(inst.rgba[2]) << 16) |
           (static_cast<uint32_t>(inst.rgba[3]) << 24);
}

} // namespace

bool CadRendererGL::renderFlatWire(
        const CadFramePlan& plan,
        const SoGLContext* glue,
        const SbMatrix& viewProj)
{
    /* A flattened software-wire batch is optional acceleration, never a
     * correctness boundary.  A large visible scene can exhaust a constrained
     * address space while assembling its transient world-space atlas; fall
     * back to retained per-part rendering instead of unwinding through Coin's
     * render callback. */
    try {
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
                effectiveProgressiveCut(
                    binding.part, instance.lodCut);
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

            Occurrence occurrence;
            occurrence.wire = &wire;
            occurrence.instance = &instance;
            occurrence.rangeKey = CadFlatWireRangeKey{
                instance.instanceId, item.rep.part, level,
                binding.generation, instance.transform, 0, 0};
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
                    cadProgressiveSnapPoint(wire.segmentPoints[p],
                        wire.progressiveQuantizationMinimum,
                        wire.progressiveQuantizationMaximum,
                        wire.quantizationAtCut(occurrence.cut)) :
                    wire.segmentPoints[p];
                const SbVec3f b = wire.isProgressive() ?
                    cadProgressiveSnapPoint(wire.segmentPoints[p + 1],
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
        configureFixedClientArrays(glue, false, false);
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
    bool drawCompleted = true;
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
        if (fixedFunction) {
            if (!drawSoftwareFlatRanges(
                    glue, GL_LINES, group.first, group.count,
                    group.firsts, group.counts, 2)) {
                drawCompleted = false;
                break;
            }
        } else if (group.firsts.empty()) {
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
    if (drawCompleted) {
        lastRenderedWork_.lineCount = cadSaturatingWorkAdd(
            lastRenderedWork_.lineCount,
            static_cast<uint64_t>(visibleVertexCount / 2u));
        lastRenderedWork_.positionCount = cadSaturatingWorkAdd(
            lastRenderedWork_.positionCount,
            static_cast<uint64_t>(visibleVertexCount));
        lastRenderedWork_.occurrenceCount = cadSaturatingWorkAdd(
            lastRenderedWork_.occurrenceCount,
            static_cast<uint64_t>(occurrences.size()));
    }
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
    return drawCompleted;
    } catch (const std::bad_alloc &) {
        return false;
    }
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

bool
CadRendererGL::drawSoftwareFlatRanges(
        const SoGLContext *glue, GLenum mode,
        GLint defaultFirst, GLsizei defaultCount,
        const std::vector<GLint>& firsts,
        const std::vector<GLsizei>& counts, GLsizei primitiveSize)
{
    /* A software draw call is synchronous.  Bound the time during which the
     * host cannot cancel a static quality frame while preserving primitive
     * boundaries and the atomic completed-frame presentation contract. */
    /* On the scalar software path, a 64K-triangle call can remain inside the
     * rasterizer for longer than the host's complete interactive-frame
     * budget.  Eight-kiloprimitive calls keep abort polling comfortably below
     * the default 40 ms endpoint while leaving enough work per call to make
     * dispatch overhead negligible beside software lighting and fill. */
    constexpr GLsizei primitivesPerChunk =
        static_cast<GLsizei>(flatSoftwarePrimitiveChunk);
    if (!glue || primitiveSize <= 0 ||
            primitivesPerChunk >
                std::numeric_limits<GLsizei>::max() / primitiveSize)
        return false;
    const GLsizei maximumVertices = primitivesPerChunk * primitiveSize;
    const auto drawRange = [&](GLint rangeFirst, GLsizei rangeCount) {
        GLsizei submitted = 0;
        while (submitted < rangeCount) {
            GLsizei chunk = std::min(
                maximumVertices, rangeCount - submitted);
            chunk -= chunk % primitiveSize;
            if (chunk <= 0)
                return false;
            glue->glDrawArrays(mode, rangeFirst + submitted, chunk);
            submitted += chunk;
            if (submitted < rangeCount && renderInterrupted())
                return false;
        }
        return true;
    };

    if (firsts.empty())
        return drawRange(defaultFirst, defaultCount);
    if (firsts.size() != counts.size())
        return false;
    for (size_t range = 0; range < firsts.size(); ++range) {
        if (!drawRange(firsts[range], counts[range]))
            return false;
        if (range + 1 < firsts.size() && renderInterrupted())
            return false;
    }
    return true;
}

CadRendererGL::FlatShadedPlanningResult
CadRendererGL::planFlatShadedOccurrences(
        const CadFramePlan& plan,
        const SoGLContext *glue,
        const SbMatrix& viewProj,
        size_t maxVertexCount,
        size_t& deadlineWork)
{
    const int progressiveCutCeiling =
        activeViewState().progressiveCutCeiling;
    const float progressiveCutNextFraction =
        activeViewState().progressiveCutNextFraction;
    const auto targetMatches = [&](const auto& target) {
        if (target.kind !=
                Obol::CadPresentationPreparationKind::FlatShadedPlanning ||
                target.viewId != activeViewState().viewId ||
                target.contextId != glue->contextid ||
                target.planRevision != plan.revision ||
                target.geometryRevision != plan.geometryRevision ||
                target.progressiveCutCeiling != progressiveCutCeiling)
            return false;
        uint32_t fractionBits = 0;
        std::memcpy(&fractionBits, &progressiveCutNextFraction,
            sizeof(fractionBits));
        if (target.progressiveCutNextFractionBits != fractionBits)
            return false;
        for (size_t i = 0; i < target.viewProjectionBits.size(); ++i) {
            uint32_t matrixBits = 0;
            std::memcpy(&matrixBits, viewProj[0] + i,
                sizeof(matrixBits));
            if (target.viewProjectionBits[i] != matrixBits)
                return false;
        }
        return true;
    };
    const auto saturatedAdd = [](uint64_t left, uint64_t right) {
        return left <= std::numeric_limits<uint64_t>::max() - right ?
            left + right : std::numeric_limits<uint64_t>::max();
    };
    const auto unitsPerInstance = [&](const CadDrawItem& item) {
        if (item.partIndex >= plan.partBindings.size())
            return uint64_t(1);
        const CadPartBinding& binding = plan.partBindings[item.partIndex];
        if (!binding.geometry || !binding.geometry->shaded)
            return uint64_t(1);
        const Obol::TriMesh& mesh = *binding.geometry->shaded;
        if (!mesh.hasAdaptiveProgressiveClusters())
            return uint64_t(1);
        uint64_t units = 0;
        for (const ProgressiveTriangleCluster& cluster :
                mesh.progressiveClusters) {
            units = saturatedAdd(units,
                (std::max)(uint64_t(1),
                    static_cast<uint64_t>(cluster.ranges.size())));
        }
        return (std::max)(uint64_t(1), units);
    };
    const auto reservedBytes = [&]() {
        const uint64_t capacity = static_cast<uint64_t>(
            flatShadedPlanning_.occurrences.capacity());
        return capacity <= std::numeric_limits<uint64_t>::max() /
                sizeof(FlatShadedOccurrence) ?
            capacity * sizeof(FlatShadedOccurrence) :
            std::numeric_limits<uint64_t>::max();
    };
    const auto publish = [&](Obol::CadPresentationPreparationState state) {
        publishPreparation(
            flatShadedPlanning_.target, state,
            flatShadedPlanning_.totalUnits,
            flatShadedPlanning_.completedUnits,
            reservedBytes());
    };

    if (!flatShadedPlanning_.valid ||
            !targetMatches(flatShadedPlanning_.target)) {
        std::vector<FlatShadedOccurrence> retainedOccurrences;
        retainedOccurrences.swap(flatShadedPlanning_.occurrences);
        flatShadedPlanning_ = FlatShadedPlanningState();
        flatShadedPlanning_.occurrences.swap(retainedOccurrences);
        flatShadedPlanning_.occurrences.clear();
        flatShadedPlanning_.valid = true;
        flatShadedPlanning_.target = preparationTarget(
            Obol::CadPresentationPreparationKind::FlatShadedPlanning,
            glue->contextid, plan.revision, plan.geometryRevision,
            progressiveCutCeiling, progressiveCutNextFraction, viewProj);
        for (const CadDrawItem& item : plan.shadedItems) {
            const uint64_t perInstance = unitsPerInstance(item);
            const uint64_t count = item.instanceCount;
            const uint64_t itemUnits = count && perInstance >
                    std::numeric_limits<uint64_t>::max() / count ?
                std::numeric_limits<uint64_t>::max() :
                count * perInstance;
            flatShadedPlanning_.totalUnits = saturatedAdd(
                flatShadedPlanning_.totalUnits, itemUnits);
        }
        flatShadedPlanning_.occurrences.reserve(
            plan.visibleInstances.size());
        publish(Obol::CadPresentationPreparationState::Preparing);
    }
    if (flatShadedPlanning_.complete)
        return FlatShadedPlanningResult::Complete;

    const ExecutorFrustumPlanes fp =
        extractExecutorFrustumPlanes(viewProj);
    const uint64_t completedAtEntry =
        flatShadedPlanning_.completedUnits;

    const auto advanceUnits = [&](uint64_t units) {
        flatShadedPlanning_.completedUnits = (std::min)(
            flatShadedPlanning_.totalUnits,
            saturatedAdd(flatShadedPlanning_.completedUnits, units));
        publish(Obol::CadPresentationPreparationState::Preparing);
    };

    while (flatShadedPlanning_.itemCursor < plan.shadedItems.size()) {
        const CadDrawItem& item =
            plan.shadedItems[flatShadedPlanning_.itemCursor];
        if (!flatShadedPlanning_.itemUnitsPerInstance)
            flatShadedPlanning_.itemUnitsPerInstance =
                unitsPerInstance(item);
        const uint64_t instanceUnits =
            flatShadedPlanning_.itemUnitsPerInstance;
        if (item.partIndex >= plan.partBindings.size() ||
                !plan.partBindings[item.partIndex].geometry ||
                !plan.partBindings[item.partIndex].geometry->shaded) {
            const uint64_t remaining = item.instanceCount -
                flatShadedPlanning_.instanceOffset;
            advanceUnits(remaining >
                    std::numeric_limits<uint64_t>::max() / instanceUnits ?
                std::numeric_limits<uint64_t>::max() :
                remaining * instanceUnits);
            ++flatShadedPlanning_.itemCursor;
            flatShadedPlanning_.itemUnitsPerInstance = 0;
            flatShadedPlanning_.instanceOffset = 0;
            continue;
        }
        const CadPartBinding& binding =
            plan.partBindings[item.partIndex];
        const Obol::TriMesh& mesh = *binding.geometry->shaded;
        if (flatShadedPlanning_.instanceOffset >= item.instanceCount) {
            ++flatShadedPlanning_.itemCursor;
            flatShadedPlanning_.itemUnitsPerInstance = 0;
            flatShadedPlanning_.instanceOffset = 0;
            continue;
        }

        const size_t visibleIndex = static_cast<size_t>(item.baseInstance) +
            flatShadedPlanning_.instanceOffset;
        if (!flatShadedPlanning_.instanceActive) {
            if (visibleIndex >= plan.visibleInstances.size() ||
                    !cadInstanceDrawable(
                        plan, item, visibleIndex, CadDrawChannel::Shaded) ||
                    isBoxOutsideExecutorFrustum(
                        plan.visibleInstances[visibleIndex].wbMin,
                        plan.visibleInstances[visibleIndex].wbMax, fp)) {
                advanceUnits(instanceUnits);
                ++flatShadedPlanning_.instanceOffset;
                if (renderInterruptedAfter(deadlineWork)) {
                    noteRenderPreparation("flat-shaded-planning");
                    return FlatShadedPlanningResult::Interrupted;
                }
                continue;
            }

            const CadVisibleInstance& instance =
                plan.visibleInstances[visibleIndex];
            const uint8_t requested = effectiveProgressiveCut(
                binding.part, instance.lodCut);
            flatShadedPlanning_.instanceCut = mesh.isProgressive() ?
                executorVisibleProgressiveCut(
                    mesh, instance, fp, requested) :
                Obol::ProgressiveCutUnspecified;
            flatShadedPlanning_.instanceSourceRevision =
                mesh.isProgressive() &&
                    mesh.progressiveLineage ?
                mesh.progressiveLineage : binding.generation;
            flatShadedPlanning_.instanceOccurrenceBegin =
                flatShadedPlanning_.occurrences.size();
            flatShadedPlanning_.instanceModel.setValue(
                instance.transform.data());
            flatShadedPlanning_.instanceActive = true;
        }

        const CadVisibleInstance& instance =
            plan.visibleInstances[visibleIndex];
        const auto appendOccurrence = [&](size_t firstIndex,
                                           size_t indexCount) {
            if (indexCount < 3)
                return true;
            if (firstIndex >= mesh.indices.size() ||
                    indexCount > mesh.indices.size() - firstIndex ||
                    indexCount > maxVertexCount ||
                    flatShadedPlanning_.currentVertexCount >
                        maxVertexCount - indexCount)
                return false;
            const size_t drawableIndexCount = indexCount - indexCount % 3u;
            flatShadedPlanning_.currentVertexCount += drawableIndexCount;
            for (size_t offset = 0; offset < drawableIndexCount;) {
                const size_t chunkIndexCount = (std::min)(
                    flatShadedRangeIndexChunk,
                    drawableIndexCount - offset);
                const size_t chunkFirstIndex = firstIndex + offset;
                FlatShadedOccurrence occurrence;
                occurrence.partIndex = item.partIndex;
                occurrence.visibleIndex = visibleIndex;
                occurrence.firstIndex = chunkFirstIndex;
                occurrence.indexCount = chunkIndexCount;
                occurrence.cut = flatShadedPlanning_.instanceCut;
                occurrence.rangeKey = CadFlatShadedRangeKey{
                    instance.instanceId, item.rep.part, occurrence.cut,
                    flatShadedPlanning_.instanceSourceRevision,
                    instance.transform,
                    static_cast<uint64_t>(chunkFirstIndex),
                    static_cast<uint64_t>(chunkIndexCount)};
                occurrence.cullBackfaces = cadProgressiveCutCullSafe(
                    item.cullBackfaces, &mesh, occurrence.cut);
                /* A non-exact PoP cut is deliberately not culled because
                 * quantization can temporarily expose a triangle's back
                 * face.  Its lighting must follow that displayed-surface
                 * contract rather than the exact source mesh's orientation;
                 * otherwise the fixed-function software path renders those
                 * newly visible faces nearly black while the GLSL path
                 * correctly flips them. */
                occurrence.twoSidedLighting =
                    !occurrence.cullBackfaces;
                occurrence.styleKey =
                    static_cast<uint64_t>(flatRgbaKey(instance)) |
                    (static_cast<uint64_t>(
                        occurrence.cullBackfaces) << 32) |
                    (static_cast<uint64_t>(
                        occurrence.twoSidedLighting) << 33);
                if (flatShadedPlanning_.occurrences.empty())
                    flatShadedPlanning_.firstStyleKey = occurrence.styleKey;
                else if (occurrence.styleKey !=
                        flatShadedPlanning_.firstStyleKey)
                    flatShadedPlanning_.uniformStyle = false;
                flatShadedPlanning_.occurrences.push_back(occurrence);
                offset += chunkIndexCount;
            }
            return true;
        };

        bool instanceComplete = false;
        if (mesh.hasAdaptiveProgressiveClusters()) {
            if (flatShadedPlanning_.clusterCursor >=
                    mesh.progressiveClusters.size()) {
                instanceComplete = true;
            } else {
                const ProgressiveTriangleCluster& cluster =
                    mesh.progressiveClusters[
                        flatShadedPlanning_.clusterCursor];
                const uint64_t clusterUnits = (std::max)(
                    uint64_t(1),
                    static_cast<uint64_t>(cluster.ranges.size()));
                if (flatShadedPlanning_.rangeCursor == 0) {
                    float minimum[3];
                    float maximum[3];
                    executorTransformedBox(
                        cluster.bounds,
                        flatShadedPlanning_.instanceModel,
                        minimum, maximum);
                    if (isBoxOutsideExecutorFrustum(
                            minimum, maximum, fp)) {
                        advanceUnits(clusterUnits);
                        ++flatShadedPlanning_.clusterCursor;
                        if (renderInterruptedAfter(deadlineWork)) {
                            noteRenderPreparation(
                                "flat-shaded-planning");
                            return FlatShadedPlanningResult::Interrupted;
                        }
                        continue;
                    }
                }
                if (cluster.ranges.empty()) {
                    advanceUnits(1);
                    ++flatShadedPlanning_.clusterCursor;
                } else if (flatShadedPlanning_.rangeCursor <
                        cluster.ranges.size()) {
                    const ProgressiveTriangleClusterRange& range =
                        cluster.ranges[flatShadedPlanning_.rangeCursor];
                    if (range.activationCut <=
                            flatShadedPlanning_.instanceCut &&
                            !appendOccurrence(
                                range.firstIndex, range.indexCount)) {
                        publish(Obol::CadPresentationPreparationState::Failed);
                        flatShadedPlanning_.valid = false;
                        return FlatShadedPlanningResult::Failed;
                    }
                    ++flatShadedPlanning_.rangeCursor;
                    advanceUnits(1);
                    if (range.activationCut >
                            flatShadedPlanning_.instanceCut) {
                        advanceUnits(cluster.ranges.size() -
                            flatShadedPlanning_.rangeCursor);
                        flatShadedPlanning_.rangeCursor =
                            cluster.ranges.size();
                    }
                }
                if (flatShadedPlanning_.rangeCursor >=
                        cluster.ranges.size()) {
                    flatShadedPlanning_.rangeCursor = 0;
                    ++flatShadedPlanning_.clusterCursor;
                }
            }
        } else {
            const size_t indexCount = mesh.isProgressive() ?
                mesh.indexCountAtCut(flatShadedPlanning_.instanceCut) :
                mesh.indices.size();
            if (!appendOccurrence(0, indexCount)) {
                publish(Obol::CadPresentationPreparationState::Failed);
                flatShadedPlanning_.valid = false;
                return FlatShadedPlanningResult::Failed;
            }
            advanceUnits(1);
            instanceComplete = true;
        }

        if (instanceComplete) {
            if (flatShadedPlanning_.occurrences.size() !=
                    flatShadedPlanning_.instanceOccurrenceBegin)
                ++flatShadedPlanning_.semanticOccurrenceCount;
            if (mesh.isProgressive()) {
                flatShadedPlanning_.hasProgressiveOccurrence = true;
                if (!mesh.hasAdaptiveProgressiveClusters() &&
                        flatShadedPlanning_.instanceCut <
                            mesh.progressiveResidentCut) {
                    const size_t residentVertices =
                        mesh.indexCountAtCut(mesh.progressiveResidentCut);
                    if (residentVertices >= maxVertexCount ||
                            flatShadedPlanning_.futureRangeVertexCount >
                                maxVertexCount - residentVertices) {
                        flatShadedPlanning_.futureRangeVertexCount =
                            maxVertexCount;
                    } else {
                        flatShadedPlanning_.futureRangeVertexCount +=
                            residentVertices;
                    }
                }
            }
            flatShadedPlanning_.instanceActive = false;
            flatShadedPlanning_.clusterCursor = 0;
            flatShadedPlanning_.rangeCursor = 0;
            ++flatShadedPlanning_.instanceOffset;
        }
        if (renderInterruptedAfter(deadlineWork)) {
            if (flatShadedPlanning_.completedUnits != completedAtEntry)
                noteRenderPreparation("flat-shaded-planning");
            return FlatShadedPlanningResult::Interrupted;
        }
    }

    if (!flatShadedPlanning_.uniformStyle) {
        std::sort(flatShadedPlanning_.occurrences.begin(),
            flatShadedPlanning_.occurrences.end(),
            [](const FlatShadedOccurrence& left,
                    const FlatShadedOccurrence& right) {
                if (left.styleKey != right.styleKey)
                    return left.styleKey < right.styleKey;
                return left.rangeKey.instance < right.rangeKey.instance;
            });
        if (renderInterrupted()) {
            if (flatShadedPlanning_.completedUnits != completedAtEntry)
                noteRenderPreparation("flat-shaded-planning");
            return FlatShadedPlanningResult::Interrupted;
        }
    }
    flatShadedPlanning_.complete = true;
    publish(Obol::CadPresentationPreparationState::Complete);
    if (flatShadedPlanning_.completedUnits != completedAtEntry)
        noteRenderPreparation("flat-shaded-planning");
    return FlatShadedPlanningResult::Complete;
}

bool CadRendererGL::renderFlatShaded(
        const CadFramePlan& plan,
        const SoGLContext* glue,
        const SbMatrix& viewProj,
        const SbMatrix& viewMatrix,
        const SbMatrix& projectionMatrix,
        bool depthOnly)
{
    constexpr size_t maxVertexBytes = 512u * 1024u * 1024u;
    constexpr size_t floatsPerVertex = 6;
    size_t deadlineWork = 256u;
    const int progressiveCutCeiling =
        activeViewState().progressiveCutCeiling;
    const float progressiveCutNextFraction =
        activeViewState().progressiveCutNextFraction;
    const size_t maxVertexCount =
        maxVertexBytes / (floatsPerVertex * sizeof(float));
    const FlatShadedPlanningResult planningResult =
        planFlatShadedOccurrences(
            plan, glue, viewProj, maxVertexCount, deadlineWork);
    if (planningResult != FlatShadedPlanningResult::Complete)
        return false;
    const std::vector<FlatShadedOccurrence>& occurrences =
        flatShadedPlanning_.occurrences;
    const size_t currentVertexCount =
        flatShadedPlanning_.currentVertexCount;
    const size_t futureRangeVertexCount =
        flatShadedPlanning_.futureRangeVertexCount;
    const size_t semanticOccurrenceCount =
        flatShadedPlanning_.semanticOccurrenceCount;
    const bool hasProgressiveOccurrence =
        flatShadedPlanning_.hasProgressiveOccurrence;

    auto buildAtlasRange = [&](
            const FlatShadedOccurrence& occurrence, GLint baseVertex,
            std::vector<float>& positions,
            std::vector<float>& normals,
            std::unordered_map<CadFlatShadedRangeKey,
                               CadFlatShadedRange,
                               CadFlatShadedRangeKeyHash>& ranges) {
        positions.clear();
        normals.clear();
        positions.reserve(occurrence.indexCount * 3);
        normals.reserve(occurrence.indexCount * 3);
        ranges.clear();
        {
            if (renderInterruptedAfter(deadlineWork))
                return FlatShadedAtlasRangeResult::Interrupted;
            if (occurrence.partIndex >= plan.partBindings.size() ||
                    occurrence.visibleIndex >=
                        plan.visibleInstances.size())
                return FlatShadedAtlasRangeResult::Invalid;
            const CadPartBinding& binding =
                plan.partBindings[occurrence.partIndex];
            if (!binding.geometry || !binding.geometry->shaded)
                return FlatShadedAtlasRangeResult::Invalid;
            const Obol::TriMesh& mesh = *binding.geometry->shaded;
            const CadVisibleInstance& instance =
                plan.visibleInstances[occurrence.visibleIndex];
            const bool hasVertexNormals =
                mesh.normals.size() == mesh.positions.size();
            SbMatrix transform;
            transform.setValue(instance.transform.data());
            const SbMatrix normalMatrix = transform.inverse().transpose();
            const GLint first = baseVertex +
                static_cast<GLint>(positions.size() / 3);
            const size_t rangeEnd =
                occurrence.firstIndex + occurrence.indexCount;
            for (size_t t = occurrence.firstIndex;
                    t + 2 < rangeEnd; t += 3) {
                if (renderInterruptedAfter(deadlineWork))
                    return FlatShadedAtlasRangeResult::Interrupted;
                const uint32_t ia = mesh.indices[t];
                const uint32_t ib = mesh.indices[t + 1];
                const uint32_t ic = mesh.indices[t + 2];
                if (ia >= mesh.positions.size() ||
                        ib >= mesh.positions.size() ||
                        ic >= mesh.positions.size())
                    return FlatShadedAtlasRangeResult::Invalid;
                const uint32_t indices[3] = {ia, ib, ic};
                SbVec3f triangle[3];
                SbVec3f sourceTriangle[3];
                for (size_t vertex = 0; vertex < 3; ++vertex) {
                    const SbVec3f sourcePoint =
                        mesh.positions[indices[vertex]];
                    SbVec3f point = sourcePoint;
                    if (mesh.isProgressive()) {
                        point = cadProgressiveSnapPoint(
                            point, mesh.progressiveQuantizationMinimum,
                            mesh.progressiveQuantizationMaximum,
                            mesh.quantizationAtCut(occurrence.cut));
                    }
                    triangle[vertex] =
                        transformedFlatPoint(point, instance.transform);
                    sourceTriangle[vertex] = transformedFlatPoint(
                        sourcePoint, instance.transform);
                }
                const SbVec3f faceNormal = cadProgressiveSurfaceNormal(
                    triangle, sourceTriangle);
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
        return FlatShadedAtlasRangeResult::Complete;
    };

    bool rebuild = !gpuRes_->flatShaded().posBuf ||
                   !gpuRes_->flatShaded().normBuf;
    std::vector<float> positions;
    std::vector<float> normals;
    std::unordered_map<CadFlatShadedRangeKey, CadFlatShadedRange,
                       CadFlatShadedRangeKeyHash> newRanges;
    size_t missingRangeCount = 0;

    if (!rebuild) {
        size_t missingVertexCount = 0;
        const CadFlatShadedGpu& flat = gpuRes_->flatShaded();
        for (const FlatShadedOccurrence& occurrence : occurrences) {
            if (renderInterruptedAfter(deadlineWork))
                return false;
            if (flat.ranges.find(occurrence.rangeKey) != flat.ranges.end())
                continue;
            ++missingRangeCount;
            if (missingVertexCount >
                    maxVertexCount - occurrence.indexCount)
                return false;
            missingVertexCount += occurrence.indexCount;
        }
        if (missingVertexCount) {
            const size_t availableVertexCount =
                flat.capacityVertexCount > flat.vertexCount ?
                    static_cast<size_t>(
                        flat.capacityVertexCount - flat.vertexCount) : 0u;
            if (missingVertexCount > availableVertexCount) {
                /* The retained level history has filled its reserve.  Compact
                 * to just the cuts needed by this view, then leave fresh
                 * headroom for later view changes. */
                rebuild = true;
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

    Obol::CadPresentationPreparationTarget preparation;
    uint64_t completedPreparationUnits = 0;
    const bool preparationNeeded = rebuild || missingRangeCount != 0;
    if (preparationNeeded) {
        preparation = preparationTarget(
            Obol::CadPresentationPreparationKind::FlatShadedAtlas,
            glue->contextid, plan.revision, plan.geometryRevision,
            progressiveCutCeiling,
            progressiveCutNextFraction, viewProj);
        const bool resuming = presentationPreparation_.state ==
                Obol::CadPresentationPreparationState::Preparing &&
            presentationPreparation_.target == preparation;
        /* A retry resumes the range-granular atlas already published by the
         * preceding slice.  Rebuilding the same exact target would discard
         * its finite progress and recreate the deadline livelock this
         * certificate is designed to exclude. */
        if (resuming && gpuRes_->flatShaded().posBuf &&
                gpuRes_->flatShaded().normBuf)
            rebuild = false;
        if (!rebuild)
            completedPreparationUnits =
                static_cast<uint64_t>(occurrences.size() -
                    missingRangeCount);
        publishPreparation(
            preparation,
            Obol::CadPresentationPreparationState::Preparing,
            static_cast<uint64_t>(occurrences.size()),
            completedPreparationUnits,
            static_cast<uint64_t>(
                gpuRes_->flatShaded().capacityVertexCount) *
                    floatsPerVertex * sizeof(float));
    }

    if (rebuild) {
        if (occurrences.empty()) {
            if (preparationNeeded)
                publishPreparation(
                    preparation,
                    Obol::CadPresentationPreparationState::Complete,
                    0, 0, 0);
            return true;
        }
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
        const size_t growthLimit = currentVertexCount <=
                maxVertexCount / progressiveGrowthReserve ?
            currentVertexCount * progressiveGrowthReserve : maxVertexCount;
        const size_t terminalHeadroom = hasProgressiveOccurrence &&
                currentVertexCount <= maxVertexCount / 2u ?
            currentVertexCount * 2u : currentVertexCount;
        const size_t futureReserve = futureRangeVertexCount <=
                maxVertexCount / 2u ?
            futureRangeVertexCount * 2u : maxVertexCount;
        const size_t futureHeadroom = futureReserve <=
                maxVertexCount - currentVertexCount ?
            currentVertexCount + futureReserve : maxVertexCount;
        const size_t reserveVertexCount = std::min(
            growthLimit, std::max(terminalHeadroom, futureHeadroom));

        /* Build and publish complete occurrence ranges one at a time.  A
         * software traversal may exhaust its deadline while preparing a rich
         * PoP cut.  Keeping all ranges in local vectors discarded every
         * completed page on that edge, so the next frame restarted at page
         * zero and the host misclassified repeated preparation as an
         * unaffordable draw.  The atlas is immutable at range granularity;
         * committing each complete range makes retries resumable without
         * exposing a partial frame. */
        bool freshAtlas = true;
        for (const FlatShadedOccurrence& occurrence : occurrences) {
            if (renderInterruptedAfter(deadlineWork))
                return false;
            const CadFlatShadedGpu& current = gpuRes_->flatShaded();
            if (!freshAtlas &&
                    current.ranges.find(occurrence.rangeKey) !=
                        current.ranges.end())
                continue;
            const GLint baseVertex = freshAtlas ? 0 : current.vertexCount;
            const FlatShadedAtlasRangeResult buildResult = buildAtlasRange(
                occurrence, baseVertex, positions, normals, newRanges);
            if (buildResult == FlatShadedAtlasRangeResult::Interrupted)
                return false;
            if (buildResult == FlatShadedAtlasRangeResult::Invalid) {
                publishPreparation(
                    preparation,
                    Obol::CadPresentationPreparationState::Failed,
                    static_cast<uint64_t>(occurrences.size()),
                    completedPreparationUnits,
                    static_cast<uint64_t>(reserveVertexCount) *
                        floatsPerVertex * sizeof(float));
                return false;
            }
            if (freshAtlas) {
                bool uploadSucceeded = gpuRes_->uploadFlatShaded(
                    plan.revision, plan.geometryRevision,
                    positions, normals,
                    std::vector<CadFlatShadedGroup>(), newRanges,
                    static_cast<GLsizei>(reserveVertexCount), glue, caps_);
                if (!uploadSucceeded &&
                        reserveVertexCount != currentVertexCount) {
                    /* Growth headroom is an optimization, not admission.  A
                     * constrained software context may fit the complete
                     * current view but not its speculative 16x reserve. */
                    uploadSucceeded = gpuRes_->uploadFlatShaded(
                        plan.revision, plan.geometryRevision,
                        positions, normals,
                        std::vector<CadFlatShadedGroup>(), newRanges,
                        static_cast<GLsizei>(currentVertexCount), glue,
                        caps_);
                }
                if (!uploadSucceeded)
                {
                    publishPreparation(
                        preparation,
                        Obol::CadPresentationPreparationState::Constrained,
                        static_cast<uint64_t>(occurrences.size()),
                        completedPreparationUnits,
                        static_cast<uint64_t>(reserveVertexCount) *
                            floatsPerVertex * sizeof(float));
                    return false;
                }
                const CadFlatShadedGpu& uploaded = gpuRes_->flatShaded();
                if (!uploaded.posBuf || !uploaded.normBuf ||
                        uploaded.ranges.find(occurrence.rangeKey) ==
                            uploaded.ranges.end())
                    return false;
                freshAtlas = false;
            } else if (!gpuRes_->appendFlatShaded(
                    positions, normals, newRanges, glue)) {
                publishPreparation(
                    preparation,
                    Obol::CadPresentationPreparationState::Constrained,
                    static_cast<uint64_t>(occurrences.size()),
                    completedPreparationUnits,
                    static_cast<uint64_t>(reserveVertexCount) *
                        floatsPerVertex * sizeof(float));
                return false;
            }
            ++completedPreparationUnits;
            publishPreparation(
                preparation,
                Obol::CadPresentationPreparationState::Preparing,
                static_cast<uint64_t>(occurrences.size()),
                completedPreparationUnits,
                static_cast<uint64_t>(reserveVertexCount) *
                    floatsPerVertex * sizeof(float));
            noteRenderPreparation("flat-shaded-atlas-range");
        }
    } else {
        /* Existing ranges remain directly reusable.  Append only missing
         * view/cut records and commit each one before the next deadline poll,
         * preserving the same resumable guarantee as a fresh atlas build. */
        for (const FlatShadedOccurrence& occurrence : occurrences) {
            if (renderInterruptedAfter(deadlineWork))
                return false;
            const CadFlatShadedGpu& current = gpuRes_->flatShaded();
            if (current.ranges.find(occurrence.rangeKey) !=
                    current.ranges.end())
                continue;
            const FlatShadedAtlasRangeResult buildResult = buildAtlasRange(
                occurrence, current.vertexCount,
                positions, normals, newRanges);
            if (buildResult == FlatShadedAtlasRangeResult::Interrupted)
                return false;
            if (buildResult == FlatShadedAtlasRangeResult::Invalid) {
                publishPreparation(
                    preparation,
                    Obol::CadPresentationPreparationState::Failed,
                    static_cast<uint64_t>(occurrences.size()),
                    completedPreparationUnits,
                    static_cast<uint64_t>(current.capacityVertexCount) *
                        floatsPerVertex * sizeof(float));
                return false;
            }
            if (!gpuRes_->appendFlatShaded(
                    positions, normals, newRanges, glue)) {
                publishPreparation(
                    preparation,
                    Obol::CadPresentationPreparationState::Constrained,
                    static_cast<uint64_t>(occurrences.size()),
                    completedPreparationUnits,
                    static_cast<uint64_t>(current.capacityVertexCount) *
                        floatsPerVertex * sizeof(float));
                return false;
            }
            ++completedPreparationUnits;
            publishPreparation(
                preparation,
                Obol::CadPresentationPreparationState::Preparing,
                static_cast<uint64_t>(occurrences.size()),
                completedPreparationUnits,
                static_cast<uint64_t>(current.capacityVertexCount) *
                    floatsPerVertex * sizeof(float));
            noteRenderPreparation("flat-shaded-atlas-range");
        }
    }

    if (preparationNeeded)
        publishPreparation(
            preparation,
            Obol::CadPresentationPreparationState::Complete,
            static_cast<uint64_t>(occurrences.size()),
            static_cast<uint64_t>(occurrences.size()),
            presentationPreparation_.reservedBytes);

    std::vector<CadFlatShadedGroup> groups;
    uint64_t activeStyle = 0;
    bool haveGroup = false;
    const CadFlatShadedGpu& atlas = gpuRes_->flatShaded();
    for (const FlatShadedOccurrence& occurrence : occurrences) {
        if (renderInterruptedAfter(deadlineWork))
            return false;
        const auto rangeIt = atlas.ranges.find(occurrence.rangeKey);
        if (rangeIt == atlas.ranges.end())
            return false;
        if (!haveGroup || occurrence.styleKey != activeStyle) {
            if (occurrence.visibleIndex >= plan.visibleInstances.size())
                return false;
            const CadVisibleInstance& instance =
                plan.visibleInstances[occurrence.visibleIndex];
            CadFlatShadedGroup group;
            std::copy(instance.rgba.begin(), instance.rgba.end(), group.rgba);
            group.cullBackfaces = occurrence.cullBackfaces;
            group.twoSidedLighting = occurrence.twoSidedLighting;
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
        static_cast<uint64_t>(semanticOccurrenceCount));

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
        configureFixedClientArrays(glue, !depthOnly, false);
        glue->glBindBuffer(GL_ARRAY_BUFFER, flat.posBuf);
        glue->glVertexPointer(3, GL_FLOAT, 3 * sizeof(float), nullptr);
        if (!depthOnly) {
            glue->glBindBuffer(GL_ARRAY_BUFFER, flat.normBuf);
            glue->glNormalPointer(GL_FLOAT, 3 * sizeof(float), nullptr);
        }
        bool drawCompleted = true;
        for (const CadFlatShadedGroup& group : flat.groups) {
            setCadBackfaceCulling(glue, group.cullBackfaces);
            glue->glLightModeli(GL_LIGHT_MODEL_TWO_SIDE,
                group.twoSidedLighting ? GL_TRUE : GL_FALSE);
            if (!depthOnly) setImmediateMaterialFromRgba(glue, group.rgba);
            if (!drawSoftwareFlatRanges(
                    glue, GL_TRIANGLES, group.first, group.count,
                    group.firsts, group.counts, 3)) {
                drawCompleted = false;
                break;
            }
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
        return drawCompleted;
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
        configureFixedClientArrays(glue, false, false);
        glue->glBindBuffer(GL_ARRAY_BUFFER, flat.posBuf);
        glue->glVertexPointer(3, GL_FLOAT, 3 * sizeof(float), nullptr);
        bool drawCompleted = true;
        for (const CadFlatShadedGroup& group : flat.groups) {
            setCadBackfaceCulling(glue, group.cullBackfaces);
            glue->glColor4ub(group.rgba[0], group.rgba[1],
                             group.rgba[2], group.rgba[3]);
            if (!drawSoftwareFlatRanges(
                    glue, GL_TRIANGLES, group.first, group.count,
                    group.firsts, group.counts, 3)) {
                drawCompleted = false;
                break;
            }
        }
        glue->glDisableClientState(GL_VERTEX_ARRAY);
        glue->glBindBuffer(GL_ARRAY_BUFFER, 0);
        if (wasLighting) glue->glEnable(GL_LIGHTING);
        glue->glMatrixMode(GL_MODELVIEW);
        glue->glPopMatrix();
        glue->glMatrixMode(GL_PROJECTION);
        glue->glPopMatrix();
        glue->glMatrixMode(GL_MODELVIEW);
        if (!drawCompleted) {
            glue->glPolygonMode(
                GL_FRONT, static_cast<GLenum>(polygonMode[0]));
            glue->glPolygonMode(
                GL_BACK, static_cast<GLenum>(polygonMode[1]));
            return false;
        }
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

} // namespace internal
} // namespace Obol
