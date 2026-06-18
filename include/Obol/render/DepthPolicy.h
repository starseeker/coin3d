#ifndef OBOL_RENDER_DEPTHPOLICY_H
#define OBOL_RENDER_DEPTHPOLICY_H

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
 * @file DepthPolicy.h
 * @brief Depth-buffer policy for auxiliary world-space objects.
 *
 * Controls how an auxiliary object (e.g. a grid overlay, annotation line)
 * interacts with the depth buffer when rendered after CAD geometry.
 */

namespace obol {

/**
 * @brief Depth-buffer policy for world-space auxiliary objects.
 *
 * ### Usage
 * Attach a DepthPolicy to any non-CAD world object that should be rendered
 * after the main CAD pass so the renderer can apply the correct GL state.
 *
 * @code
 *   AuxLineGrid grid;
 *   grid.depthPolicy = obol::DepthPolicy::ALWAYS_VISIBLE;
 * @endcode
 */
enum class DepthPolicy : uint8_t {
    /**
     * Normal depth test (GL_LESS).  The object is occluded by closer CAD
     * geometry, just like any ordinary 3-D object.  This is the default.
     */
    OCCLUDED = 0,

    /**
     * Depth test disabled.  The object is always drawn on top of everything
     * else regardless of depth.  Useful for annotations that must always be
     * visible.
     */
    ALWAYS_VISIBLE = 1,

    /**
     * Two-pass x-ray rendering: first draw with depth test ON (write pixels
     * that pass); then draw the occluded remainder with reduced opacity.
     * The exact effect is renderer-dependent; if not supported the renderer
     * may fall back to ALWAYS_VISIBLE.
     */
    XRAY = 2,
};

} // namespace obol

#endif // OBOL_RENDER_DEPTHPOLICY_H
