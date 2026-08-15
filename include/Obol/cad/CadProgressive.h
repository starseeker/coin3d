#ifndef OBOL_CADPROGRESSIVE_H
#define OBOL_CADPROGRESSIVE_H

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
 * @file CadProgressive.h
 * @brief Shared constants for producer-defined progressive geometry cuts.
 */

#include <cstddef>
#include <cstdint>
#include <limits>

namespace Obol {

/** Maximum number of producer-defined progressive cuts in one part. */
static constexpr size_t ProgressiveCutLimit = 64;

/**
 * No particular progressive cut was requested.
 *
 * Consumers resolve this value against the resident interval, which selects
 * the richest currently resident cut without assigning special meaning to
 * any valid cut ordinal.
 */
static constexpr uint8_t ProgressiveCutUnspecified =
    (std::numeric_limits<uint8_t>::max)();

/**
 * Per-axis precision used by one admissible progressive cut.
 *
 * Zero means that the axis is unsnapped.  Producers use it for a zero-extent
 * axis (and for geometry whose coordinates are already an explicit,
 * non-quantized approximation).  Values 1..15 request that many quantization
 * bits; 16 restores the original coordinate.  Consequently a planar or
 * linear terminal cut is exact even though one or two components remain zero.
 */
struct ProgressiveQuantization {
    uint8_t xBits = 0;
    uint8_t yBits = 0;
    uint8_t zBits = 0;

    bool isExact() const noexcept {
        return (xBits == 0 || xBits >= 16) &&
            (yBits == 0 || yBits >= 16) &&
            (zBits == 0 || zBits >= 16);
    }
};

} // namespace Obol

#endif // OBOL_CADPROGRESSIVE_H
