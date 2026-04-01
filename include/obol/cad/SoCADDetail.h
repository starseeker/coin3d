#ifndef OBOL_SOCADDETAIL_H
#define OBOL_SOCADDETAIL_H

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
 * @file SoCADDetail.h
 * @brief SoDetail subclass returned by SoCADAssembly pick operations.
 *
 * When a ray-pick hits an SoCADAssembly node, the SoPickedPoint detail
 * field is set to an SoCADDetail that identifies which instance and part
 * were hit, along with the primitive type (edge, triangle, or bounds) and
 * optional parameterisation.
 */

#include <Inventor/details/SoSubDetail.h>
#include <obol/cad/CadIds.h>

/*!
  \class SoCADDetail SoCADDetail.h obol/cad/SoCADDetail.h
  \brief Pick-result detail for an SoCADAssembly hit.

  \ingroup obol_cad

  SoCADDetail records:
  - The InstanceId of the instance that was hit (stable within a session
    unless transforms or hierarchy are rebuilt).
  - The PartId of the part geometry used by that instance.
  - The primitive type (EDGE, TRIANGLE, or BOUNDS).
  - Optional primitive index and parameterisation along an edge (u ∈ [0,1]).

  \sa SoCADAssembly, SoDetail, obol::CadId128
*/
class OBOL_DLL_API SoCADDetail : public SoDetail {
    typedef SoDetail inherited;
    SO_DETAIL_HEADER(SoCADDetail);

public:
    /** Primitive type that was actually hit. */
    enum PrimType {
        EDGE     = 0,   ///< Hit an edge / wire polyline segment.
        TRIANGLE = 1,   ///< Hit a shaded triangle.
        BOUNDS   = 2,   ///< Hit only the bounding-box proxy (no fine geometry).
    };

    SoCADDetail();
    ~SoCADDetail() override;

    static void initClass();
    SoDetail* copy() const override;

    // --- accessors ---

    /** InstanceId of the assembly instance that was hit. */
    obol::InstanceId getInstanceId() const noexcept { return instanceId_; }

    /** PartId of the geometry part used by this instance. */
    obol::PartId getPartId() const noexcept { return partId_; }

    /** Type of geometry primitive that was hit. */
    PrimType getPrimType() const noexcept { return primType_; }

    /**
     * Index of the polyline (for EDGE) or face (for TRIANGLE) within the
     * part geometry.  Undefined for BOUNDS hits.
     */
    uint32_t getPrimIndex0() const noexcept { return primIndex0_; }

    /**
     * Segment index within the polyline (for EDGE), or unused (for TRIANGLE).
     */
    uint32_t getPrimIndex1() const noexcept { return primIndex1_; }

    /**
     * Parametric position along the hit segment [0, 1] (EDGE only).
     * 0 = start of segment, 1 = end of segment.
     */
    float getU() const noexcept { return u_; }

    // --- mutators ---

    void setInstanceId(obol::InstanceId id)  noexcept { instanceId_ = id; }
    void setPartId    (obol::PartId     id)  noexcept { partId_     = id; }
    void setPrimType  (PrimType         t)   noexcept { primType_   = t;  }
    void setPrimIndex0(uint32_t         idx) noexcept { primIndex0_ = idx; }
    void setPrimIndex1(uint32_t         idx) noexcept { primIndex1_ = idx; }
    void setU         (float            u)   noexcept { u_          = u;   }

private:
    obol::InstanceId instanceId_;
    obol::PartId     partId_;
    PrimType         primType_   = BOUNDS;
    uint32_t         primIndex0_ = 0;
    uint32_t         primIndex1_ = 0;
    float            u_          = 0.0f;
};

#endif // OBOL_SOCADDETAIL_H
