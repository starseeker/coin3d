#ifndef OBOL_CAD_GEOMETRY_VALIDATION_H
#define OBOL_CAD_GEOMETRY_VALIDATION_H

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
 * @file CadGeometryValidation.h
 * @brief Deterministic validation results for retained CAD geometry.
 */

#include <Inventor/SbBasic.h>

#include <cstddef>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace Obol {

struct PartGeometry;
struct PartGeometryBuilder;
struct PartUpdate;

enum class CadGeometryError {
    Valid = 0,
    InvalidPartId,
    NullGeometry,
    NonFiniteValue,
    InvalidAttributeCount,
    InvalidPrimitiveCount,
    InvalidVertexIndex,
    InvalidProgressiveInterval,
    InvalidProgressiveCut,
    InvalidProgressiveOrder,
    InvalidClusterLayout,
    InvalidClusterRange,
    NonConservativeBounds,
    InvalidSubpixelProxy,
    InvalidAggregateProxy
};

/** Complete, allocation-free result from validating one geometry update. */
struct [[nodiscard]] CadGeometryValidation {
    static constexpr size_t NoIndex =
        (std::numeric_limits<size_t>::max)();

    CadGeometryError error = CadGeometryError::Valid;
    size_t updateIndex = NoIndex;
    size_t elementIndex = NoIndex;

    bool valid() const noexcept
    {
        return error == CadGeometryError::Valid;
    }

    explicit operator bool() const noexcept
    {
        return valid();
    }
};

struct CadGeometryAdmission;

/**
 * Validate and admit one immutable geometry snapshot.
 *
 * Call this at the producer boundary, preferably on the worker which builds
 * or reads the snapshot.  Publication through PartUpdate is then an
 * O(1) ownership transfer regardless of geometry size or view count.
 */
OBOL_DLL_API CadGeometryAdmission cadAdmitPartGeometry(
    PartGeometryBuilder geometry);

/**
 * Immutable geometry which has passed every renderer-visible invariant.
 *
 * The token is cheap to copy between views and presentation batches.  Its
 * constructor is deliberately private: shared geometry enters a CAD scene
 * only through cadAdmitPartGeometry(), so a renderer never has to rescan a
 * large PoP generation on the traversal thread.
 */
class ValidatedPartGeometry {
public:
    ValidatedPartGeometry() noexcept = default;

    /**
     * Retain an admitted snapshot.
     *
     * PartGeometry has no public constructor, so every non-null pointer of
     * this type already carries the admission guarantee.  This constructor
     * lets caches restore the lightweight publication token without
     * rescanning immutable arrays.
     */
    explicit ValidatedPartGeometry(
        std::shared_ptr<const PartGeometry> geometry) noexcept :
        geometry_(std::move(geometry))
    {
    }

    const PartGeometry *get() const noexcept
    {
        return geometry_.get();
    }

    const std::shared_ptr<const PartGeometry>& shared() const noexcept
    {
        return geometry_;
    }

    explicit operator bool() const noexcept
    {
        return static_cast<bool>(geometry_);
    }

private:
    std::shared_ptr<const PartGeometry> geometry_;

    friend struct CadGeometryAdmission;
    friend OBOL_DLL_API CadGeometryAdmission cadAdmitPartGeometry(
        PartGeometryBuilder geometry);
};

/** Validation result and, on success, its reusable immutable token. */
struct [[nodiscard]] CadGeometryAdmission {
    CadGeometryValidation validation;
    ValidatedPartGeometry geometry;

    explicit operator bool() const noexcept
    {
        return validation.valid();
    }
};

/** Validate a mutable geometry builder without consuming it. */
OBOL_DLL_API CadGeometryValidation
cadValidatePartGeometry(const PartGeometryBuilder& geometry) noexcept;

/** Validate a complete admitted shared-geometry batch without mutation. */
OBOL_DLL_API CadGeometryValidation cadValidatePartUpdates(
    const std::vector<PartUpdate>& updates) noexcept;

/** Stable diagnostic name for a validation result. */
OBOL_DLL_API const char *
cadGeometryErrorName(CadGeometryError error) noexcept;

} // namespace Obol

#endif // OBOL_CAD_GEOMETRY_VALIDATION_H
