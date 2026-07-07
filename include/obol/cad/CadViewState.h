#ifndef OBOL_CAD_VIEW_STATE_H
#define OBOL_CAD_VIEW_STATE_H

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
 * @file CadViewState.h
 * @brief Generic per-view render policy for compiled CAD assemblies.
 */

#include <cstdint>

namespace obol {

/**
 * @brief View-local LoD policy.
 *
 * INHERIT leaves the current traversal state's LoD policy unchanged. If no
 * earlier view state exists, LoD is disabled. Applications should set ENABLED
 * or DISABLED explicitly from each view controller when they need predictable
 * per-view behavior.
 */
enum class CadLodMode : int {
    INHERIT  = -1,
    DISABLED = 0,
    ENABLED  = 1
};

/**
 * @brief Traversal-time state supplied by a view.
 *
 * This struct is deliberately generic: applications map their own model,
 * view, and selection concepts onto these fields before rendering.
 */
struct CadViewState {
    uint64_t   viewId = 0;
    CadLodMode lodMode = CadLodMode::DISABLED;
    float      lodScale = 1.0f;
    bool       selectedFullDetail = true;
};

/**
 * @brief Resolved render options consumed by the CAD renderer.
 */
struct CadRenderState {
    uint64_t viewId = 0;
    bool     lodEnabled = false;
    float    lodScale = 1.0f;
    bool     selectedFullDetail = true;
};

inline CadRenderState
resolveCadRenderState(const CadViewState& viewState)
{
    CadRenderState render;
    render.viewId = viewState.viewId;
    render.lodEnabled = viewState.lodMode == CadLodMode::ENABLED;
    render.lodScale = viewState.lodScale > 0.0f ? viewState.lodScale : 1.0f;
    render.selectedFullDetail = viewState.selectedFullDetail;
    return render;
}

} // namespace obol

#endif // OBOL_CAD_VIEW_STATE_H
