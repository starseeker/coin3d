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

#ifndef OBOL_CAD_RENDERER_GL_EXECUTOR_UTILS_H
#define OBOL_CAD_RENDERER_GL_EXECUTOR_UTILS_H

#include "CadRendererGL.h"

#include <Inventor/system/gl.h>

#include <array>
#include <cstdint>
#include <vector>

namespace Obol {
namespace internal {

struct ExecutorFrustumPlanes {
    float planes[6][4];
};

struct CadWireRasterState {
    GLfloat lineWidth = 1.0f;
    GLboolean stippleEnabled = GL_FALSE;
    GLint stipplePattern = 0xffff;
    GLint stippleFactor = 1;
};

const float *executorPackedVec3fData(
    const std::vector<SbVec3f>& values);
void executorAppendPackedPoint(
    std::vector<float>& packed, const SbVec3f& point);
void setImmediateMaterialFromRgba(
    const SoGLContext *glue, const uint8_t rgba[4]);
void setCadBackfaceCulling(const SoGLContext *glue, bool enabled);
bool cadProgressiveCutCullSafe(
    bool sourceCullSafe, const TriMesh *mesh, uint8_t cut);
ExecutorFrustumPlanes extractExecutorFrustumPlanes(
    const SbMatrix& viewProjection) noexcept;
bool isBoxOutsideExecutorFrustum(
    const float minimum[3], const float maximum[3],
    const ExecutorFrustumPlanes& frustum) noexcept;
void executorTransformedBox(
    const SbBox3f& local, const SbMatrix& transform,
    float minimum[3], float maximum[3]) noexcept;
uint8_t executorVisibleProgressiveCut(
    const TriMesh& mesh, const CadVisibleInstance& instance,
    const ExecutorFrustumPlanes& frustum, uint8_t requested) noexcept;
uint8_t executorVisibleProgressiveCut(
    const WireRep& wire, const CadVisibleInstance& instance,
    const ExecutorFrustumPlanes& frustum, uint8_t requested) noexcept;
double executorProjectedBoxImportance(
    const float minimum[3], const float maximum[3],
    const SbMatrix& viewProjection) noexcept;
CadWireRasterState captureWireRasterState(
    const SoGLContext *glue, bool hasLineStipple);
void applyWireRasterStyle(
    const SoGLContext *glue, const CadVisibleInstance& instance,
    bool hasLineStipple);
void restoreWireRasterState(
    const SoGLContext *glue, const CadWireRasterState& state,
    bool hasLineStipple);
SbVec3f transformedFlatPoint(
    const SbVec3f& point, const std::array<float, 16>& matrix);
uint64_t cadSaturatingWorkAdd(uint64_t left, uint64_t right);
float packedProgressiveQuantization(
    ProgressiveQuantization quantization);
SbVec3f progressiveSnapPoint(
    const SbVec3f& point, const SbVec3f& minimum,
    const SbVec3f& maximum, ProgressiveQuantization quantization);
GLsizei progressiveTriangleIndexCount(
    const SoCADAssembly& assembly, const CadViewState& viewState,
    PartId part, const CadVisibleInstance& instance,
    GLsizei residentCount);
void uploadProgressivePositionUniforms(
    const SoGLContext *glue, GLint encodeScaleLocation,
    GLint decodeScaleLocation, GLint minLocation,
    ProgressiveQuantization quantization,
    const SbVec3f& minimum, const SbVec3f& maximum);
void cadAccumulateRenderedShadedWork(
    Obol::CadRenderedWork& work, const Obol::TriMesh& mesh,
    uint8_t level, uint64_t triangles, uint64_t occurrences = 1,
    uint64_t visiblePositions = UINT64_MAX);
void cadAccumulateRenderedWireWork(
    Obol::CadRenderedWork& work, uint64_t segments,
    uint64_t occurrences = 1);
void cadReplacePreparedShadedWork(
    Obol::CadRenderedWork& work, uint64_t oldTriangles,
    uint64_t oldPositions, bool oldHasNormals,
    uint64_t newTriangles, uint64_t newPositions,
    bool newHasNormals);

} // namespace internal
} // namespace Obol

#endif // OBOL_CAD_RENDERER_GL_EXECUTOR_UTILS_H
