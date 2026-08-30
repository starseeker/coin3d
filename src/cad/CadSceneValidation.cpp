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

#include <Obol/cad/CadSceneValidation.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

bool
sceneFinite(float value) noexcept
{
    return std::isfinite(value);
}

bool
sceneFinite(const SbColor4f& color) noexcept
{
    return sceneFinite(color[0]) && sceneFinite(color[1]) &&
        sceneFinite(color[2]) && sceneFinite(color[3]);
}

bool
sceneFinite(const SbMatrix& matrix) noexcept
{
    for (int row = 0; row != 4; ++row)
        for (int column = 0; column != 4; ++column)
            if (!sceneFinite(matrix[row][column]))
                return false;
    return true;
}

bool
sceneAffine(const SbMatrix& matrix) noexcept
{
    return matrix[0][3] == 0.0f && matrix[1][3] == 0.0f &&
        matrix[2][3] == 0.0f && matrix[3][3] == 1.0f;
}

bool
sceneInvertible(const SbMatrix& matrix) noexcept
{
    double maximum = 0.0;
    for (int row = 0; row != 3; ++row)
        for (int column = 0; column != 3; ++column)
            maximum = (std::max)(maximum,
                std::abs(static_cast<double>(matrix[row][column])));
    if (maximum == 0.0)
        return false;
    const double determinant =
        static_cast<double>(matrix[0][0]) *
            (static_cast<double>(matrix[1][1]) * matrix[2][2] -
             static_cast<double>(matrix[1][2]) * matrix[2][1]) -
        static_cast<double>(matrix[0][1]) *
            (static_cast<double>(matrix[1][0]) * matrix[2][2] -
             static_cast<double>(matrix[1][2]) * matrix[2][0]) +
        static_cast<double>(matrix[0][2]) *
            (static_cast<double>(matrix[1][0]) * matrix[2][1] -
             static_cast<double>(matrix[1][1]) * matrix[2][0]);
    const double scale = maximum * maximum * maximum;
    return std::abs(determinant) >
        16.0 * std::numeric_limits<float>::epsilon() * scale;
}

Obol::CadSceneValidation
failure(Obol::CadSceneError error) noexcept
{
    Obol::CadSceneValidation result;
    result.error = error;
    return result;
}

} // namespace

namespace Obol {

CadSceneValidation
cadValidateInstanceStyle(InstanceId instance,
    const InstanceStyle& style) noexcept
{
    if (!instance.isValid())
        return failure(CadSceneError::InvalidInstanceId);
    if (!sceneFinite(style.color) || !sceneFinite(style.lineWidth))
        return failure(CadSceneError::NonFiniteStyle);
    if (style.lineWidth <= 0.0f || style.linePatternFactor == 0)
        return failure(CadSceneError::InvalidStyle);
    return CadSceneValidation();
}

CadSceneValidation
cadValidateInstanceTransform(InstanceId instance,
    const SbMatrix& transform) noexcept
{
    if (!instance.isValid())
        return failure(CadSceneError::InvalidInstanceId);
    if (!sceneFinite(transform))
        return failure(CadSceneError::NonFiniteTransform);
    if (!sceneAffine(transform) || !sceneInvertible(transform))
        return failure(CadSceneError::InvalidTransform);
    return CadSceneValidation();
}

CadSceneValidation
cadValidateInstanceRecord(InstanceId instance,
    const InstanceRecord& record) noexcept
{
    CadSceneValidation validation =
        cadValidateInstanceTransform(instance, record.localToRoot);
    if (!validation)
        return validation;
    if (!record.part.isValid())
        return failure(CadSceneError::InvalidPartId);
    return cadValidateInstanceStyle(instance, record.style);
}

CadSceneValidation
cadValidateInstanceUpdates(
    const std::vector<InstanceUpdate>& updates) noexcept
{
    for (size_t i = 0; i < updates.size(); ++i) {
        CadSceneValidation validation = cadValidateInstanceRecord(
            updates[i].instance, updates[i].record);
        if (!validation) {
            validation.updateIndex = i;
            return validation;
        }
    }
    return CadSceneValidation();
}

CadSceneValidation
cadValidateInstanceStyleUpdates(
    const std::vector<InstanceStyleUpdate>& updates) noexcept
{
    for (size_t i = 0; i < updates.size(); ++i) {
        CadSceneValidation validation = cadValidateInstanceStyle(
            updates[i].instance, updates[i].style);
        if (!validation) {
            validation.updateIndex = i;
            return validation;
        }
    }
    return CadSceneValidation();
}

CadSceneValidation
cadValidateInstanceLodUpdates(
    const std::vector<InstanceLodUpdate>& updates) noexcept
{
    for (size_t i = 0; i < updates.size(); ++i) {
        if (updates[i].instance.isValid())
            continue;
        CadSceneValidation validation =
            failure(CadSceneError::InvalidInstanceId);
        validation.updateIndex = i;
        return validation;
    }
    return CadSceneValidation();
}

const char *
cadSceneErrorName(CadSceneError error) noexcept
{
    switch (error) {
    case CadSceneError::Valid:
        return "valid";
    case CadSceneError::InvalidInstanceId:
        return "invalid-instance-id";
    case CadSceneError::InvalidPartId:
        return "invalid-part-id";
    case CadSceneError::NonFiniteTransform:
        return "non-finite-transform";
    case CadSceneError::NonFiniteStyle:
        return "non-finite-style";
    case CadSceneError::InvalidStyle:
        return "invalid-style";
    case CadSceneError::MissingInstance:
        return "missing-instance";
    case CadSceneError::ConflictingUpdate:
        return "conflicting-update";
    case CadSceneError::InvalidTransform:
        return "invalid-transform";
    }
    return "unknown";
}

} // namespace Obol
