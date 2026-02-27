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
  \class SoRaytracingParams SoRaytracingParams.h Inventor/nodes/SoRaytracingParams.h
  \brief Scene graph node carrying rendering hints for CPU raytracing backends.

  \ingroup coin_nodes

  See the class header documentation for full details on the available
  fields and their semantics.  This node has no OpenGL side-effects.
*/

#include <Inventor/nodes/SoRaytracingParams.h>

#include "nodes/SoSubNodeP.h"

SO_NODE_SOURCE(SoRaytracingParams);

/*!
  \copydetails SoNode::initClass(void)
*/
void
SoRaytracingParams::initClass(void)
{
  SO_NODE_INTERNAL_INIT_CLASS(SoRaytracingParams, SO_FROM_OBOL_4_0);
}

/*!
  Constructor.  Sets all fields to their default values.
*/
SoRaytracingParams::SoRaytracingParams(void)
{
  SO_NODE_INTERNAL_CONSTRUCTOR(SoRaytracingParams);

  SO_NODE_ADD_FIELD(shadowsEnabled,         (FALSE));
  SO_NODE_ADD_FIELD(maxReflectionBounces,   (0));
  SO_NODE_ADD_FIELD(samplesPerPixel,        (1));
  SO_NODE_ADD_FIELD(ambientIntensity,       (0.2f));
}

/*!
  Destructor.
*/
SoRaytracingParams::~SoRaytracingParams()
{
}
