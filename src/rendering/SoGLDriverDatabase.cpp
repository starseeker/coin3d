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
  \class SoGLDriverDatabase SoGLDriverDatabase.h Inventor/misc/SoGLDriverDatabase.h
  \brief The SoGLDriverDatabase class is used for looking up broken/disabled features in OpenGL drivers.

  This implementation uses runtime feature detection with a feature function map.
  Each named feature is mapped to a test function that queries the active GL context.
  For GL extension names (starting with "GL_"), the extension string is queried directly.

  A driver workaround database (isBroken/isDisabled) is also available for cases
  that cannot be reliably detected through extension queries alone.  As of 2026
  there are no active entries: the pre-2010 hardware workarounds that were
  previously maintained here have been removed because that hardware is no longer
  in any realistic use.
*/

#include <Inventor/misc/SoGLDriverDatabase.h>

#include "CoinTidbits.h"
#include <Inventor/SbName.h>
#include <Inventor/SbString.h>
#include "glue/glp.h"
#include <Inventor/errors/SoDebugError.h>

#include "misc/SbHash.h"
#include "glue/glp.h"


// Forward declarations for OpenGL feature test functions
typedef SbBool glglue_feature_test_f(const SoGLContext * glue);

// Function declarations for runtime feature tests (defined locally in this file)
extern "C" {
  SbBool multidraw_elements_wrapper(const SoGLContext * glue);
  SbBool glsl_clip_vertex_hw_wrapper(const SoGLContext * glue);
}

class SoGLDriverDatabaseP {
public:
  SoGLDriverDatabaseP();
  ~SoGLDriverDatabaseP();

  void initFunctions(void);
  SbBool isSupported(const SoGLContext * context, const SbName & feature);
  SbBool isBroken(const SoGLContext * context, const SbName & feature);
  SbBool isDisabled(const SoGLContext * context, const SbName & feature);

  // Runtime feature detection function map
  SbHash<const char*, glglue_feature_test_f *> featuremap;
};

static SoGLDriverDatabaseP * sogldriverdatabase_instance = NULL;

// Feature test wrapper functions
SbBool 
multidraw_elements_wrapper(const SoGLContext * glue)
{
  return SoGLContext_has_multidraw_vertex_arrays(glue);
}

SbBool 
glsl_clip_vertex_hw_wrapper(const SoGLContext * glue) 
{
  return SoGLContext_has_arb_vertex_shader(glue);
}

SoGLDriverDatabaseP::SoGLDriverDatabaseP()
{
  this->initFunctions();
}

SoGLDriverDatabaseP::~SoGLDriverDatabaseP()
{
}

/*
  Initialize the feature detection function map.
  This maps feature names to runtime detection functions.
*/
void
SoGLDriverDatabaseP::initFunctions(void)
{
  this->featuremap[SbName(SO_GL_MULTIDRAW_ELEMENTS).getString()] =
                       (glglue_feature_test_f *) &multidraw_elements_wrapper;
  this->featuremap[SbName(SO_GL_POLYGON_OFFSET).getString()] =
                       (glglue_feature_test_f *) &SoGLContext_has_polygon_offset;
  this->featuremap[SbName(SO_GL_TEXTURE_OBJECT).getString()] =
                       (glglue_feature_test_f *) &SoGLContext_has_texture_objects;
  this->featuremap[SbName(SO_GL_3D_TEXTURES).getString()] =
                       (glglue_feature_test_f *) &SoGLContext_has_3d_textures;
  this->featuremap[SbName(SO_GL_MULTITEXTURE).getString()] =
                       (glglue_feature_test_f *) &SoGLContext_has_multitexture;
  this->featuremap[SbName(SO_GL_TEXSUBIMAGE).getString()] =
                       (glglue_feature_test_f *) &SoGLContext_has_texsubimage;
  this->featuremap[SbName(SO_GL_2D_PROXY_TEXTURES).getString()] =
                       (glglue_feature_test_f *) &SoGLContext_has_2d_proxy_textures;
  this->featuremap[SbName(SO_GL_TEXTURE_EDGE_CLAMP).getString()] =
                       (glglue_feature_test_f *) &SoGLContext_has_texture_edge_clamp;
  this->featuremap[SbName(SO_GL_TEXTURE_COMPRESSION).getString()] =
                       (glglue_feature_test_f *) &cc_glue_has_texture_compression;
  this->featuremap[SbName(SO_GL_COLOR_TABLES).getString()] =
                       (glglue_feature_test_f *) &SoGLContext_has_color_tables;
  this->featuremap[SbName(SO_GL_COLOR_SUBTABLES).getString()] =
                       (glglue_feature_test_f *) &SoGLContext_has_color_subtables;
  this->featuremap[SbName(SO_GL_PALETTED_TEXTURES).getString()] =
                       (glglue_feature_test_f *) &SoGLContext_has_paletted_textures;
  this->featuremap[SbName(SO_GL_BLEND_EQUATION).getString()] =
                       (glglue_feature_test_f *) &SoGLContext_has_blendequation;
  this->featuremap[SbName(SO_GL_VERTEX_ARRAY).getString()] =
                       (glglue_feature_test_f *) &SoGLContext_has_vertex_array;
  this->featuremap[SbName(SO_GL_NV_VERTEX_ARRAY_RANGE).getString()] =
                       (glglue_feature_test_f *) &SoGLContext_has_nv_vertex_array_range;
  this->featuremap[SbName(SO_GL_VERTEX_BUFFER_OBJECT).getString()] =
                       (glglue_feature_test_f *) &SoGLContext_has_vertex_buffer_object;
  this->featuremap[SbName(SO_GL_ARB_FRAGMENT_PROGRAM).getString()] =
                       (glglue_feature_test_f *) &SoGLContext_has_arb_fragment_program;
  this->featuremap[SbName(SO_GL_ARB_VERTEX_PROGRAM).getString()] =
                       (glglue_feature_test_f *) &SoGLContext_has_arb_vertex_program;
  this->featuremap[SbName(SO_GL_ARB_VERTEX_SHADER).getString()] =
                       (glglue_feature_test_f *) &SoGLContext_has_arb_vertex_shader;
  this->featuremap[SbName(SO_GL_ARB_SHADER_OBJECT).getString()] =
                       (glglue_feature_test_f *) &SoGLContext_has_arb_shader_objects;
  this->featuremap[SbName(SO_GL_OCCLUSION_QUERY).getString()] =
                       (glglue_feature_test_f *) &SoGLContext_has_occlusion_query;
  this->featuremap[SbName(SO_GL_FRAMEBUFFER_OBJECT).getString()] =
                       (glglue_feature_test_f *) &SoGLContext_has_framebuffer_objects;
  this->featuremap[SbName(SO_GL_ANISOTROPIC_FILTERING).getString()] =
                       (glglue_feature_test_f *) &SoGLContext_can_do_anisotropic_filtering;
  this->featuremap[SbName(SO_GL_SORTED_LAYERS_BLEND).getString()] =
                       (glglue_feature_test_f *) &SoGLContext_can_do_sortedlayersblend;
  this->featuremap[SbName(SO_GL_BUMPMAPPING).getString()] =
                       (glglue_feature_test_f *) &SoGLContext_can_do_bumpmapping;
  this->featuremap[SbName(SO_GL_VBO_IN_DISPLAYLIST).getString()] =
                       (glglue_feature_test_f *) &SoGLContext_vbo_in_displaylist_supported;
  this->featuremap[SbName(SO_GL_NON_POWER_OF_TWO_TEXTURES).getString()] =
                       (glglue_feature_test_f *) &SoGLContext_non_power_of_two_textures;
  this->featuremap[SbName(SO_GL_GENERATE_MIPMAP).getString()] =
                       (glglue_feature_test_f *) &SoGLContext_has_generate_mipmap;
  this->featuremap[SbName(SO_GL_GLSL_CLIP_VERTEX_HW).getString()] =
                       (glglue_feature_test_f *) &glsl_clip_vertex_hw_wrapper;
}

SbBool
SoGLDriverDatabaseP::isSupported(const SoGLContext * context, const SbName & feature)
{
  // check if we're asking about an actual GL extension
  const char * str = feature.getString();
  if ((feature.getLength() > 3) && (str[0] == 'G') && (str[1] == 'L') && (str[2] == '_')) {
    if (!SoGLContext_glext_supported(context, feature)) return FALSE;
  }
  else { // check our lookup table
    SbHash<const char*, glglue_feature_test_f *>::const_iterator iter = this->featuremap.find(feature.getString());
    if (iter!=this->featuremap.const_end()) {
      glglue_feature_test_f * testfunc = iter->obj;
      if (!testfunc(context)) return FALSE;
    }
    else {
      SoDebugError::post("SoGLDriverDatabase::isSupported",
                         "Unknown feature '%s'.", feature.getString());
    }
  }
  return !(this->isBroken(context, feature) || this->isDisabled(context, feature));
}

SbBool
SoGLDriverDatabaseP::isBroken(const SoGLContext * /*context*/, const SbName & /*feature*/)
{
  // No active driver workarounds as of 2026.
  // Pre-2010 hardware workarounds have been removed as obsolete.
  return FALSE;
}

SbBool
SoGLDriverDatabaseP::isDisabled(const SoGLContext * /*context*/, const SbName & /*feature*/)
{
  return FALSE;
}

static SoGLDriverDatabaseP *
pimpl(void)
{
  if (sogldriverdatabase_instance == NULL) {
    sogldriverdatabase_instance = new SoGLDriverDatabaseP;
  }
  return sogldriverdatabase_instance;
}

// Public API implementation

void
SoGLDriverDatabase::init(void)
{
  (void) ::pimpl(); // Initialize the singleton
}

SbBool
SoGLDriverDatabase::isSupported(const SoGLContext * context, const SbName & feature)
{
  return ::pimpl()->isSupported(context, feature);
}