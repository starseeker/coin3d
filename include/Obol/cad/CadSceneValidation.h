#ifndef OBOL_CAD_SCENE_VALIDATION_H
#define OBOL_CAD_SCENE_VALIDATION_H

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

/** @file CadSceneValidation.h @brief Validation for CAD scene mutations. */

#include <Obol/cad/CadSceneRecords.h>

#include <cstddef>
#include <limits>
#include <vector>

namespace Obol {

enum class CadSceneError {
    Valid = 0,
    InvalidInstanceId,
    InvalidPartId,
    NonFiniteTransform,
    NonFiniteStyle,
    InvalidStyle,
    MissingInstance,
    ConflictingUpdate
};

/** Deterministic result from validating one scene mutation or batch. */
struct [[nodiscard]] CadSceneValidation {
    static constexpr size_t NoIndex =
        (std::numeric_limits<size_t>::max)();

    CadSceneError error = CadSceneError::Valid;
    size_t updateIndex = NoIndex;

    bool valid() const noexcept
    {
        return error == CadSceneError::Valid;
    }

    explicit operator bool() const noexcept
    {
        return valid();
    }
};

/** Result from inserting an automatically identified occurrence. */
struct [[nodiscard]] CadInstanceUpdateResult {
    CadSceneValidation validation;
    InstanceId instance;

    explicit operator bool() const noexcept
    {
        return validation.valid();
    }
};

/** Result from inserting an automatically identified occurrence batch. */
struct [[nodiscard]] CadInstanceBatchResult {
    CadSceneValidation validation;
    std::vector<InstanceId> instances;

    explicit operator bool() const noexcept
    {
        return validation.valid();
    }
};

OBOL_DLL_API CadSceneValidation cadValidateInstanceRecord(
    InstanceId instance, const InstanceRecord& record) noexcept;
OBOL_DLL_API CadSceneValidation cadValidateInstanceTransform(
    InstanceId instance, const SbMatrix& transform) noexcept;
OBOL_DLL_API CadSceneValidation cadValidateInstanceStyle(
    InstanceId instance, const InstanceStyle& style) noexcept;
OBOL_DLL_API CadSceneValidation cadValidateInstanceUpdates(
    const std::vector<InstanceUpdate>& updates) noexcept;
OBOL_DLL_API CadSceneValidation cadValidateInstanceStyleUpdates(
    const std::vector<InstanceStyleUpdate>& updates) noexcept;
OBOL_DLL_API CadSceneValidation cadValidateInstanceLodUpdates(
    const std::vector<InstanceLodUpdate>& updates) noexcept;
OBOL_DLL_API const char *cadSceneErrorName(CadSceneError error) noexcept;

} // namespace Obol

#endif // OBOL_CAD_SCENE_VALIDATION_H
