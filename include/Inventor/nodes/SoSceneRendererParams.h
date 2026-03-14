#ifndef OBOL_SOSCENERENDERERPARAMS_H
#define OBOL_SOSCENERENDERERPARAMS_H

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

/*!
  \class SoSceneRendererParams SoSceneRendererParams.h Inventor/nodes/SoSceneRendererParams.h
  \brief Scene graph node carrying rendering hints for non-GL rendering backends.

  \ingroup coin_nodes

  \c SoSceneRendererParams is a pure-data scene graph node — it has \b no
  OpenGL side-effects and is silently ignored by \c SoGLRenderAction.
  Raytracing backends (nanort, OSPRay, BRL-CAD librt, …) find this node
  via \c SoSearchAction and read its fields to configure their rendering
  pipeline.

  The fields follow the same "hint" convention used by \c SoComplexity:
  they are optional, and a backend may ignore fields it does not support.

  \b Default field values are chosen to be backward-compatible: a scene
  that already works with the nanort backend will behave identically whether
  or not \c SoSceneRendererParams is present.

  \b Placement: typically inserted near the top of the scene graph, before
  the camera and shapes:
  \code
  SoSeparator * root = new SoSeparator;

  SoSceneRendererParams * rendParams = new SoSceneRendererParams;
  rendParams->shadowsEnabled    = TRUE;
  rendParams->maxReflectionBounces = 1;
  rendParams->samplesPerPixel   = 4;
  root->addChild(rendParams);

  root->addChild(camera);
  root->addChild(light);
  root->addChild(shapes);
  \endcode

  <b>FILE FORMAT/DEFAULTS:</b>
  \code
    SceneRendererParams {
        shadowsEnabled         FALSE
        maxReflectionBounces   0
        samplesPerPixel        1
        ambientIntensity       0.2
    }
  \endcode

  \sa SoSceneRenderAction, SoCallbackAction, docs/BACKEND_SURVEY.md
*/

#include <Inventor/nodes/SoSubNode.h>
#include <Inventor/fields/SoSFBool.h>
#include <Inventor/fields/SoSFInt32.h>
#include <Inventor/fields/SoSFFloat.h>

class OBOL_DLL_API SoSceneRendererParams : public SoNode {
  typedef SoNode inherited;

  SO_NODE_HEADER(SoSceneRendererParams);

public:
  static void initClass(void);
  SoSceneRendererParams(void);

  /*!
    Enable hard-shadow casting via secondary shadow rays.  When \c TRUE
    each illuminated surface point fires one shadow ray per light source;
    if the ray is blocked the light's contribution is suppressed.

    Default: \c FALSE (backward-compatible with earlier nanort renderer).
  */
  SoSFBool  shadowsEnabled;

  /*!
    Maximum number of specular reflection bounce rays per primary ray.
    Set to 0 to disable reflections entirely (mirrors are shaded with
    plain Phong only).  Each additional bounce roughly doubles render time.

    Default: 0 (no reflections).
  */
  SoSFInt32 maxReflectionBounces;

  /*!
    Number of primary rays cast per pixel for anti-aliasing.  The rays
    are distributed with jittered sub-pixel offsets and averaged.
    Values above 1 linearly increase render time.  A value of 4 gives a
    good quality/performance trade-off.

    Default: 1 (no AA; single ray per pixel).
  */
  SoSFInt32 samplesPerPixel;

  /*!
    Uniform ambient fill intensity added to every shaded surface point,
    independent of light sources.  Prevents surfaces from becoming
    completely black when facing away from all lights.

    Range: [0, 1].  Default: 0.2.
  */
  SoSFFloat ambientIntensity;

protected:
  virtual ~SoSceneRendererParams();
};

#endif // !OBOL_SOSCENERENDERERPARAMS_H
