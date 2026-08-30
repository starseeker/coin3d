#ifndef OBOL_CAD_SCENE_RECORDS_H
#define OBOL_CAD_SCENE_RECORDS_H

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

/** @file CadSceneRecords.h @brief CAD part and occurrence update records. */

#include <Inventor/SbColor4f.h>
#include <Inventor/SbMatrix.h>
#include <Inventor/SbVec3f.h>

#include <Obol/cad/CadGeometry.h>
#include <Obol/cad/CadGeometryValidation.h>
#include <Obol/cad/CadIds.h>
#include <Obol/cad/CadProgressive.h>

#include <cstdint>
#include <string>

namespace Obol {

struct InstanceStyle {
    bool hasColorOverride = false;
    SbColor4f color = SbColor4f(0.8f, 0.8f, 0.8f, 1.0f);
    float lineWidth = 1.0f;
    uint16_t linePattern = 0xffffu;
    uint16_t linePatternFactor = 1u;
};

/** One retained occurrence of shared part geometry. */
struct InstanceRecord {
    PartId part;
    /* Finite, invertible affine transform.  Projective and singular matrices
     * are rejected because retained bounds, normal transforms and picking
     * all consume one affine inverse contract. */
    SbMatrix localToRoot = SbMatrix::identity();

    /* Stable automatic-ID inputs.  Explicit-ID updates ignore them. */
    InstanceId parent;
    std::string childName;
    uint32_t occurrenceIndex = 0;
    uint8_t boolOp = 0;

    uint8_t lodCut = ProgressiveCutUnspecified;
    bool lodStructuralProxy = false;
    InstanceStyle style;
};

/** Zero-copy update carrying a previously validated immutable snapshot. */
struct PartUpdate {
    PartId part;
    ValidatedPartGeometry geometry;

    /* When true, the assembly verifies equal conservative bounds and can
     * retain occurrence bounds and its instance BVH across replacement. */
    bool preservesBounds = false;
};

struct InstanceUpdate {
    InstanceId instance;
    InstanceRecord record;
};

struct InstanceLodUpdate {
    InstanceId instance;
    uint8_t lodCut = ProgressiveCutUnspecified;
};

struct InstanceStyleUpdate {
    InstanceId instance;
    InstanceStyle style;
};

/** Renderer-domain hit identity, suitable for application detail adapters. */
struct CadPickDetailRecord {
    enum PrimitiveKind {
        EDGE = 0,
        TRIANGLE = 1,
        BOUNDS = 2,
        POINT = 3,
    };

    InstanceId instance;
    PartId part;
    SbVec3f point = SbVec3f(0.0f, 0.0f, 0.0f);
    PrimitiveKind primitiveKind = BOUNDS;
    uint32_t primIndex0 = 0;
    uint32_t primIndex1 = 0;
    float u = 0.0f;
};

} // namespace Obol

#endif // OBOL_CAD_SCENE_RECORDS_H
