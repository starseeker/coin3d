#ifndef OBOL_SOQUADVIEWPORT_H
#define OBOL_SOQUADVIEWPORT_H

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
 * @file SoQuadViewport.h
 * @brief Four-quadrant viewport manager for split-view 3D display.
 *
 * SoQuadViewport partitions a window into a 2×2 grid of four independent
 * SoViewport tiles that all render the same shared scene graph.  Each tile
 * is a full SoViewport instance, so every per-quadrant operation is simply
 * delegated to the underlying SoViewport API.
 *
 * One quadrant is designated as "active" at any time; events routed through
 * processEvent() are forwarded to that quadrant's SoViewport.
 *
 * Level-of-Detail (SoLOD / SoLevelOfDetail) nodes in the shared scene are
 * automatically resolved per-quadrant based on each quadrant's camera
 * position, so a zoomed-in view renders fine detail while a zoomed-out view
 * renders coarser geometry — all from the same shared scene graph.
 *
 * Typical usage:
 * @code
 *   SoQuadViewport qv;
 *   qv.setWindowSize(SbVec2s(800, 600));
 *   qv.setSceneGraph(myGeometryScene);      // scene without cameras
 *
 *   qv.setCamera(SoQuadViewport::TOP_LEFT,     frontCam);
 *   qv.setCamera(SoQuadViewport::TOP_RIGHT,    sideCam);
 *   qv.setCamera(SoQuadViewport::BOTTOM_LEFT,  topCam);
 *   qv.setCamera(SoQuadViewport::BOTTOM_RIGHT, perspCam);
 *   qv.viewAllQuadrants();
 *
 *   // Access the underlying SoViewport for a quadrant directly:
 *   SoViewport * tile = qv.getViewport(SoQuadViewport::TOP_LEFT);
 *   SbVec2s qs = qv.getQuadrantSize();
 *   SoOffscreenRenderer ren(SbViewportRegion(qs[0], qs[1]));
 *   tile->render(&ren);
 * @endcode
 *
 * @see SoViewport
 */

#include <Inventor/SbVec2s.h>
#include <Inventor/SbColor.h>
#include <Inventor/SbViewportRegion.h>
#include <Inventor/SoViewport.h>

class SoCamera;
class SoNode;
class SoOffscreenRenderer;
class SoEvent;

class OBOL_DLL_API SoQuadViewport {
public:
    /** Total number of quadrants managed by this class. */
    static const int NUM_QUADS = 4;

    /**
     * Symbolic indices for the four quadrants.
     *
     * Layout (OpenGL origin at bottom-left):
     * @verbatim
     *  ┌──────────┬──────────┐
     *  │ TOP_LEFT │TOP_RIGHT │
     *  ├──────────┼──────────┤
     *  │ BOT_LEFT │BOT_RIGHT │
     *  └──────────┴──────────┘
     * @endverbatim
     */
    enum QuadIndex {
        TOP_LEFT     = 0,
        TOP_RIGHT    = 1,
        BOTTOM_LEFT  = 2,
        BOTTOM_RIGHT = 3
    };

    SoQuadViewport();
    ~SoQuadViewport();

    // ---- Scene graph -------------------------------------------------------

    /**
     * Set the shared scene graph rendered by all four quadrants.
     * The scene should NOT contain cameras; cameras are supplied per-quadrant
     * via setCamera().  The same SoNode is assigned to all four underlying
     * SoViewport instances; Inventor's multi-parent reference counting keeps
     * it alive as long as any viewport holds it.
     * Passing NULL removes the current scene from all quadrants.
     */
    void    setSceneGraph(SoNode * scene);
    SoNode* getSceneGraph() const;

    // ---- Window / viewport -------------------------------------------------

    /**
     * Set the full window size.  The four quadrant viewports are recomputed
     * as equal floor(W/2) × floor(H/2) tiles arranged in a 2×2 grid.
     */
    void    setWindowSize(const SbVec2s & size);
    SbVec2s getWindowSize() const;

    /** Returns the pixel size of one quadrant tile (floor(W/2) × floor(H/2)). */
    SbVec2s getQuadrantSize() const;

    /**
     * Returns the SbViewportRegion for quadrant @a quad.
     * Delegates to getViewport(quad)->getViewportRegion().
     */
    const SbViewportRegion& getViewportRegion(int quad) const;

    // ---- Per-quadrant SoViewport access ------------------------------------

    /**
     * Return the SoViewport that manages quadrant @a quad.
     * Through the returned pointer the full SoViewport API is available:
     * setCamera(), viewAll(), render(), processEvent(), etc.
     * Returns NULL for out-of-range indices.
     */
    SoViewport *       getViewport(int quad);
    const SoViewport * getViewport(int quad) const;

    // ---- Cameras (convenience wrappers around getViewport()->setCamera()) --

    void      setCamera(int quad, SoCamera * camera);
    SoCamera* getCamera(int quad) const;

    // ---- Active quadrant ---------------------------------------------------

    /**
     * Select the active quadrant (0–3).  Events routed through processEvent()
     * are forwarded to getViewport(activeQuad)->processEvent().
     */
    void setActiveQuadrant(int quad);
    int  getActiveQuadrant() const;

    // ---- View-all (convenience wrappers) -----------------------------------

    /** Call viewAll() on quadrant @a quad. */
    void viewAll(int quad);

    /** Call viewAll() on every quadrant. */
    void viewAllQuadrants();

    // ---- Background colours (convenience wrappers) -------------------------

    void           setBackgroundColor(int quad, const SbColor & color);
    const SbColor& getBackgroundColor(int quad) const;

    // ---- Borders -----------------------------------------------------------

    /**
     * Set the pixel width of the dividing lines drawn between quadrants in
     * the composite image produced by writeCompositeToRGB().
     * A value of 0 (the default) suppresses border drawing entirely.
     * The border straddles the mathematical division point: half the pixels
     * fall in each adjacent quadrant's area.
     */
    void setBorderWidth(int pixelWidth);
    int  getBorderWidth() const;

    /**
     * Set the colour of the quadrant border lines.
     * Default: white (1, 1, 1).
     */
    void          setBorderColor(const SbColor & color);
    const SbColor& getBorderColor() const;

    // ---- Rendering (convenience wrapper) -----------------------------------

    /**
     * Render quadrant @a quad using the supplied SoOffscreenRenderer.
     * Delegates to getViewport(quad)->render(renderer).
     */
    SbBool renderQuadrant(int quad, SoOffscreenRenderer * renderer);

    /**
     * Render all four quadrants into a composite full-window image and write
     * it to an SGI RGB file.
     *
     * @a quadRenderer is used for per-quadrant rendering and must have been
     * initialised with an appropriate context manager.  Its viewport region
     * is temporarily modified to the quadrant size (getQuadrantSize()) during
     * each per-quadrant render call.
     *
     * The composite buffer is assembled from the four quadrant renders placed
     * in their correct 2×2 tile positions.  If getBorderWidth() > 0 the
     * horizontal and vertical dividing lines are then painted over the
     * composite in getBorderColor().
     *
     * The output file is in SGI RGB format (same as
     * SoOffscreenRenderer::writeToRGB()), suitable for conversion to PNG with
     * the rgb_to_png tool.
     *
     * @return TRUE on success, FALSE if any render or file-write step failed.
     */
    SbBool writeCompositeToRGB(const char * filename,
                               SoOffscreenRenderer * quadRenderer);

    // ---- Event routing -----------------------------------------------------

    /**
     * Dispatch @a event to the active quadrant's SoViewport.
     * @return TRUE if the event was handled.
     */
    SbBool processEvent(const SoEvent * event);

private:
    SoQuadViewport(const SoQuadViewport &);
    SoQuadViewport & operator=(const SoQuadViewport &);

    void updateViewports();
    void applyBordersToBuffer(unsigned char * buf, int W, int H) const;

    SoViewport   tiles_[NUM_QUADS];  // the four underlying viewports
    SoNode *     scene_;             // shared scene (ref'd), may be NULL
    SbVec2s      windowSize_;
    int          activeQuad_;
    int          borderWidth_;       // 0 = no border (default)
    SbColor      borderColor_;       // default white
};

#endif // !OBOL_SOQUADVIEWPORT_H
