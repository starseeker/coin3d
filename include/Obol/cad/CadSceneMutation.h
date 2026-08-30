#ifndef OBOL_CAD_SCENE_MUTATION_H
#define OBOL_CAD_SCENE_MUTATION_H

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

/** @file CadSceneMutation.h @brief Sparse retained CAD scene transactions. */

#include <Obol/cad/CadGeometryValidation.h>
#include <Obol/cad/CadSceneRecords.h>
#include <Obol/cad/CadSceneValidation.h>

#include <vector>

namespace Obol {

/**
 * One sparse, owner-thread mutation of a retained CAD scene.
 *
 * A transaction may update and remove different records, but the same part
 * or instance may not be both updated and removed.  Style and LoD updates
 * target the post-upsert instance population.  Geometry and arrays remain
 * shared through their admitted immutable tokens.
 */
struct CadSceneMutation {
    std::vector<PartUpdate> parts;
    std::vector<InstanceUpdate> instances;
    std::vector<InstanceStyleUpdate> styles;
    std::vector<InstanceLodUpdate> cuts;
    std::vector<InstanceId> removedInstances;
    std::vector<PartId> removedParts;

    bool empty() const noexcept
    {
        return parts.empty() && instances.empty() && styles.empty() &&
            cuts.empty() && removedInstances.empty() && removedParts.empty();
    }
};

/** Field which rejected a sparse transaction. */
enum class CadSceneMutationDomain {
    NoDomain = 0,
    Parts,
    Instances,
    Styles,
    Cuts,
    RemovedInstances,
    RemovedParts,
    /** Validation or transactional staging could not allocate memory. */
    ResourceUnavailable
};

/**
 * Complete sparse-mutation validation result.
 *
 * A rejected transaction leaves the preceding scene unchanged.  The
 * validation index refers to the vector named by @ref domain.  Geometry
 * and scene carry the reason; in particular, duplicate or contradictory
 * records report CadSceneError::ConflictingUpdate without losing their field.
 * ResourceUnavailable has no vector index and reports allocation failure
 * while validating, staging, or applying the sparse transaction.
 */
struct [[nodiscard]] CadSceneMutationResult {
    CadSceneMutationDomain domain = CadSceneMutationDomain::NoDomain;
    CadGeometryValidation geometry;
    CadSceneValidation scene;

    bool valid() const noexcept
    {
        return domain == CadSceneMutationDomain::NoDomain;
    }

    explicit operator bool() const noexcept
    {
        return valid();
    }
};

OBOL_DLL_API const char *cadSceneMutationDomainName(
    CadSceneMutationDomain domain) noexcept;

} // namespace Obol

#endif // OBOL_CAD_SCENE_MUTATION_H
