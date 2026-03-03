#ifndef OBOL_SOVIEWPORT_H
#define OBOL_SOVIEWPORT_H

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
 * @file SoViewport.h
 * @brief Single-viewport manager that pairs a scene graph with an independent
 *        camera and viewport region.
 *
 * SoViewport is the single-view counterpart to SoQuadViewport.  It manages
 * one camera, one scene, one viewport region, and one background colour,
 * and can render the combined scene into an SoOffscreenRenderer or route
 * events through SoHandleEventAction.
 *
 * SoViewport can be used stand-alone or as the fundamental tile managed by
 * SoQuadViewport (which composes four SoViewport instances in a 2×2 grid).
 *
 * Level-of-Detail (SoLOD / SoLevelOfDetail) nodes in the shared scene are
 * resolved based on this viewport's camera position, so each viewport that
 * holds the same scene graph independently selects the appropriate detail
 * level for its camera.
 *
 * Typical usage (standalone):
 * @code
 *   SoViewport vp;
 *   vp.setWindowSize(SbVec2s(800, 600));
 *   vp.setSceneGraph(myScene);        // scene without a camera node
 *   vp.setCamera(myCamera);
 *   vp.viewAll();
 *
 *   SoOffscreenRenderer ren(vp.getViewportRegion());
 *   vp.render(&ren);
 * @endcode
 *
 * @see SoQuadViewport
 */

#include <Inventor/SbVec2s.h>
#include <Inventor/SbColor.h>
#include <Inventor/SbViewportRegion.h>

class SoCamera;
class SoNode;
class SoSeparator;
class SoOffscreenRenderer;
class SoEvent;
class SoHandleEventAction;

class OBOL_DLL_API SoViewport {
public:
    SoViewport();
    ~SoViewport();

    // ---- Scene graph -------------------------------------------------------

    /**
     * Set the scene graph rendered by this viewport.
     * The scene should NOT contain a camera; the camera is supplied via
     * setCamera().  Passing NULL removes the current scene.
     */
    void    setSceneGraph(SoNode * scene);
    SoNode* getSceneGraph() const;

    // ---- Viewport region ---------------------------------------------------

    /**
     * Set the full viewport region (window size + sub-viewport).
     * This is the low-level setter used by SoQuadViewport to assign each
     * tile a sub-region of the full window.  For standalone use,
     * setWindowSize() is the simpler convenience wrapper.
     */
    void                    setViewportRegion(const SbViewportRegion & region);
    const SbViewportRegion& getViewportRegion() const;

    /**
     * Convenience: set the viewport to cover the entire window of @a size.
     * Equivalent to calling setViewportRegion() with a region whose window
     * size and viewport size are both @a size.
     */
    void    setWindowSize(const SbVec2s & size);
    SbVec2s getWindowSize() const;

    // ---- Camera ------------------------------------------------------------

    /**
     * Set the camera for this viewport.
     * The previous camera (if any) is removed and unreffed.  The new camera
     * is ref'd and inserted as the first child of the internal scene root so
     * it is applied before the shared geometry scene.
     * Passing NULL removes the current camera without replacing it.
     */
    void      setCamera(SoCamera * camera);
    SoCamera* getCamera() const;

    // ---- View-all ----------------------------------------------------------

    /**
     * Adjust the camera so the entire scene fits in the view.
     * Does nothing if no camera or no scene has been set.
     */
    void viewAll();

    // ---- Background colour -------------------------------------------------

    void           setBackgroundColor(const SbColor & color);
    const SbColor& getBackgroundColor() const;

    // ---- Rendering ---------------------------------------------------------

    /**
     * Render the viewport scene into @a renderer.
     * The renderer's viewport region is updated to match this viewport's
     * region, and the background colour is applied before rendering.
     *
     * @return TRUE if the render succeeded, FALSE otherwise.
     */
    SbBool render(SoOffscreenRenderer * renderer);

    // ---- Event routing -----------------------------------------------------

    /**
     * Dispatch @a event through SoHandleEventAction over this viewport's
     * scene graph.
     * @return TRUE if the event was handled by some node in the scene.
     */
    SbBool processEvent(const SoEvent * event);

    // ---- Internal scene root -----------------------------------------------

    /**
     * Return the internal SoSeparator root for this viewport.
     * The root contains the camera (if set) followed by the scene graph (if
     * set).  Advanced users may pass this root directly to SoGLRenderAction
     * together with getViewportRegion() for real-time sub-viewport rendering
     * inside an existing GL context.
     */
    SoSeparator* getRoot() const;

private:
    SoViewport(const SoViewport &);            // not copyable
    SoViewport & operator=(const SoViewport &);

    SoSeparator *         root_;      // [camera (opt)] [scene (opt)]
    SoCamera *            camera_;    // owned (ref'd), may be NULL
    SoNode *              scene_;     // ref'd, may be NULL
    SbViewportRegion      viewport_;
    SbColor               bgColor_;
    SoHandleEventAction * heAction_;  // lazily created
};

#endif // !OBOL_SOVIEWPORT_H
