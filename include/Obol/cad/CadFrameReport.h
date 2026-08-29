#ifndef OBOL_CAD_FRAME_REPORT_H
#define OBOL_CAD_FRAME_REPORT_H

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

/** @file CadFrameReport.h @brief View-stamped CAD presentation results. */

#include <Obol/cad/CadViewState.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace Obol {

/**
 * Logical work submitted by one completed CAD render.
 *
 * Counts describe the active cut, not richer resident arrays.  The complete
 * view stamp makes a report safe to reject after a camera or policy change.
 */
struct CadRenderedWork {
    CadViewState viewState;
    uint64_t triangleCount = 0;
    uint64_t lineCount = 0;
    uint64_t positionCount = 0;
    uint64_t normalCount = 0;
    uint64_t occurrenceCount = 0;
    bool exact = false;
};

/** Camera-local distribution of unresolved structural LoD proxies. */
struct CadStructuralProxyProjectionHistogram {
    static constexpr size_t BucketCount = 7;

    /* Cumulative counts at projected extents <= 1, 2, 4, 8, 16, 32, 64 px. */
    std::array<uint64_t, BucketCount> cumulativeCount = {};
    uint64_t visibleCount = 0;
    uint64_t revision = 0;
    bool exact = false;
};

/** Exact physical fallback work owned by unresolved structural occurrences. */
struct CadStructuralProxyPresentationWork {
    uint64_t aggregatePointCount = 0;
    uint64_t aggregateBoxCount = 0;
    uint64_t retainedWireBoxCount = 0;
    bool exact = false;
};

/** Exact logical aggregate representations used by the completed CAD frame. */
struct CadAggregateProxyPresentationWork {
    uint64_t pointCount = 0;
    uint64_t axisAlignedBoxCount = 0;
    uint64_t orientedBoxCount = 0;
    bool exact = false;
};

} // namespace Obol

#endif // OBOL_CAD_FRAME_REPORT_H
