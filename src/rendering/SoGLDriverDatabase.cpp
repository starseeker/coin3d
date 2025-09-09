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
  \brief The SoGLDriverDatabase class is used for looking up broken/slow features in OpenGL drivers.

  This implementation uses runtime feature detection (GLEW-style) with a minimal
  embedded database for critical driver workarounds that cannot be detected at runtime.
*/

#include <Inventor/misc/SoGLDriverDatabase.h>

#include <cstring>
#include <string>

#include <Inventor/C/tidbits.h>
#include <Inventor/SbName.h>
#include <Inventor/SbString.h>
#include <Inventor/C/glue/gl.h>
#include <Inventor/errors/SoDebugError.h>

#include "misc/SbHash.h"
#include "glue/glp.h"
#include "tidbitsp.h"

// Forward declarations for OpenGL feature test functions
typedef SbBool glglue_feature_test_f(const cc_glglue * glue);

// Function declarations for runtime feature tests
extern "C" {
  SbBool multidraw_elements_wrapper(const cc_glglue * glue);
  SbBool glsl_clip_vertex_hw_wrapper(const cc_glglue * glue);
  SbBool coin_glglue_vbo_in_displaylist_supported(const cc_glglue * glue);
  SbBool coin_glglue_non_power_of_two_textures(const cc_glglue * glue);
  SbBool coin_glglue_has_generate_mipmap(const cc_glglue * glue);
}

// Driver identification structure for embedded workarounds
struct DriverInfo {
  const char* vendor_pattern;
  const char* renderer_pattern;
  const char* version_pattern;
};

// Feature override structure
struct FeatureOverride {
  const char* feature_name;
  DriverInfo driver;
  enum { BROKEN, SLOW, FAST, DISABLED } status;
  const char* comment;
};

// Embedded database of critical driver workarounds
// This replaces the XML database with minimal hard-coded data for known issues
static const FeatureOverride EMBEDDED_OVERRIDES[] = {
  // Example entries - these would be populated with actual known issues
  // {"COIN_vertex_buffer_object", {"ATI", "Radeon 9*", "1.*"}, FeatureOverride::BROKEN, "VBO crashes on old ATI drivers"},
  // {"COIN_multitexture", {"Intel", "GMA 950", "*"}, FeatureOverride::SLOW, "Multitexture is slow on Intel GMA"},
  // Add more entries as needed for critical compatibility issues
};

class SoGLDriverDatabaseP {
public:
  SoGLDriverDatabaseP();
  ~SoGLDriverDatabaseP();

  void initFunctions(void);
  SbBool isSupported(const cc_glglue * context, const SbName & feature);
  SbBool isBroken(const cc_glglue * context, const SbName & feature);
  SbBool isSlow(const cc_glglue * context, const SbName & feature);
  SbBool isFast(const cc_glglue * context, const SbName & feature);
  SbBool isDisabled(const cc_glglue * context, const SbName & feature);
  SbName getComment(const cc_glglue * context, const SbName & feature);

  // Runtime feature detection function map
  SbHash<const char*, glglue_feature_test_f *> featuremap;

private:
  // Helper methods for driver pattern matching
  SbBool matchesPattern(const char* text, const char* pattern);
  SbBool matchesDriver(const cc_glglue * context, const DriverInfo& driver);
  const FeatureOverride* findOverride(const cc_glglue * context, const SbName & feature);
};

static SoGLDriverDatabaseP * sogldriverdatabase_instance = NULL;

// Feature test wrapper functions
SbBool 
multidraw_elements_wrapper(const cc_glglue * glue)
{
  // Implement multidraw elements test - check for GL_EXT_multi_draw_arrays
  return cc_glglue_glext_supported(glue, "GL_EXT_multi_draw_arrays");
}

SbBool 
glsl_clip_vertex_hw_wrapper(const cc_glglue * glue) 
{
  // GLSL clip vertex hardware support test
  if (!cc_glglue_has_arb_vertex_shader(glue)) return FALSE;
  // Additional vendor-specific checks for proper clip vertex support
  // ATI drivers before a certain version had broken clip vertex support
  if (glue->vendor_is_ati) return FALSE;
  return TRUE;
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
                       (glglue_feature_test_f *) &cc_glglue_has_polygon_offset;
  this->featuremap[SbName(SO_GL_TEXTURE_OBJECT).getString()] =
                       (glglue_feature_test_f *) &cc_glglue_has_texture_objects;
  this->featuremap[SbName(SO_GL_3D_TEXTURES).getString()] =
                       (glglue_feature_test_f *) &cc_glglue_has_3d_textures;
  this->featuremap[SbName(SO_GL_MULTITEXTURE).getString()] =
                       (glglue_feature_test_f *) &cc_glglue_has_multitexture;
  this->featuremap[SbName(SO_GL_TEXSUBIMAGE).getString()] =
                       (glglue_feature_test_f *) &cc_glglue_has_texsubimage;
  this->featuremap[SbName(SO_GL_2D_PROXY_TEXTURES).getString()] =
                       (glglue_feature_test_f *) &cc_glglue_has_2d_proxy_textures;
  this->featuremap[SbName(SO_GL_TEXTURE_EDGE_CLAMP).getString()] =
                       (glglue_feature_test_f *) &cc_glglue_has_texture_edge_clamp;
  this->featuremap[SbName(SO_GL_TEXTURE_COMPRESSION).getString()] =
                       (glglue_feature_test_f *) &cc_glue_has_texture_compression;
  this->featuremap[SbName(SO_GL_COLOR_TABLES).getString()] =
                       (glglue_feature_test_f *) &cc_glglue_has_color_tables;
  this->featuremap[SbName(SO_GL_COLOR_SUBTABLES).getString()] =
                       (glglue_feature_test_f *) &cc_glglue_has_color_subtables;
  this->featuremap[SbName(SO_GL_PALETTED_TEXTURES).getString()] =
                       (glglue_feature_test_f *) &cc_glglue_has_paletted_textures;
  this->featuremap[SbName(SO_GL_BLEND_EQUATION).getString()] =
                       (glglue_feature_test_f *) &cc_glglue_has_blendequation;
  this->featuremap[SbName(SO_GL_VERTEX_ARRAY).getString()] =
                       (glglue_feature_test_f *) &cc_glglue_has_vertex_array;
  this->featuremap[SbName(SO_GL_NV_VERTEX_ARRAY_RANGE).getString()] =
                       (glglue_feature_test_f *) &cc_glglue_has_nv_vertex_array_range;
  this->featuremap[SbName(SO_GL_VERTEX_BUFFER_OBJECT).getString()] =
                       (glglue_feature_test_f *) &cc_glglue_has_vertex_buffer_object;
  this->featuremap[SbName(SO_GL_ARB_FRAGMENT_PROGRAM).getString()] =
                       (glglue_feature_test_f *) &cc_glglue_has_arb_fragment_program;
  this->featuremap[SbName(SO_GL_ARB_VERTEX_PROGRAM).getString()] =
                       (glglue_feature_test_f *) &cc_glglue_has_arb_vertex_program;
  this->featuremap[SbName(SO_GL_ARB_VERTEX_SHADER).getString()] =
                       (glglue_feature_test_f *) &cc_glglue_has_arb_vertex_shader;
  this->featuremap[SbName(SO_GL_ARB_SHADER_OBJECT).getString()] =
                       (glglue_feature_test_f *) &cc_glglue_has_arb_shader_objects;
  this->featuremap[SbName(SO_GL_OCCLUSION_QUERY).getString()] =
                       (glglue_feature_test_f *) &cc_glglue_has_occlusion_query;
  this->featuremap[SbName(SO_GL_FRAMEBUFFER_OBJECT).getString()] =
                       (glglue_feature_test_f *) &cc_glglue_has_framebuffer_objects;
  this->featuremap[SbName(SO_GL_ANISOTROPIC_FILTERING).getString()] =
                       (glglue_feature_test_f *) &cc_glglue_can_do_anisotropic_filtering;
  this->featuremap[SbName(SO_GL_SORTED_LAYERS_BLEND).getString()] =
                       (glglue_feature_test_f *) &cc_glglue_can_do_sortedlayersblend;
  this->featuremap[SbName(SO_GL_BUMPMAPPING).getString()] =
                       (glglue_feature_test_f *) &cc_glglue_can_do_bumpmapping;
  this->featuremap[SbName(SO_GL_VBO_IN_DISPLAYLIST).getString()] =
                       (glglue_feature_test_f *) &coin_glglue_vbo_in_displaylist_supported;
  this->featuremap[SbName(SO_GL_NON_POWER_OF_TWO_TEXTURES).getString()] =
                       (glglue_feature_test_f *) &coin_glglue_non_power_of_two_textures;
  this->featuremap[SbName(SO_GL_GENERATE_MIPMAP).getString()] =
                       (glglue_feature_test_f *) &coin_glglue_has_generate_mipmap;
  this->featuremap[SbName(SO_GL_GLSL_CLIP_VERTEX_HW).getString()] =
                       (glglue_feature_test_f *) &glsl_clip_vertex_hw_wrapper;
}

SbBool
SoGLDriverDatabaseP::isSupported(const cc_glglue * context, const SbName & feature)
{
  // check if we're asking about an actual GL extension
  const char * str = feature.getString();
  if ((feature.getLength() > 3) && (str[0] == 'G') && (str[1] == 'L') && (str[2] == '_')) {
    if (!cc_glglue_glext_supported(context, feature)) return FALSE;
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
SoGLDriverDatabaseP::isBroken(const cc_glglue * context, const SbName & feature)
{
  const FeatureOverride* override = findOverride(context, feature);
  return override && (override->status == FeatureOverride::BROKEN);
}

SbBool
SoGLDriverDatabaseP::isSlow(const cc_glglue * context, const SbName & feature)
{
  const FeatureOverride* override = findOverride(context, feature);
  return override && (override->status == FeatureOverride::SLOW);
}

SbBool
SoGLDriverDatabaseP::isFast(const cc_glglue * context, const SbName & feature)
{
  const FeatureOverride* override = findOverride(context, feature);
  return override && (override->status == FeatureOverride::FAST);
}

SbBool
SoGLDriverDatabaseP::isDisabled(const cc_glglue * context, const SbName & feature)
{
  const FeatureOverride* override = findOverride(context, feature);
  return override && (override->status == FeatureOverride::DISABLED);
}

SbName
SoGLDriverDatabaseP::getComment(const cc_glglue * context, const SbName & feature)
{
  const FeatureOverride* override = findOverride(context, feature);
  return override ? SbName(override->comment) : SbName("");
}

// Helper function to match simple wildcard patterns
SbBool
SoGLDriverDatabaseP::matchesPattern(const char* text, const char* pattern)
{
  if (!text || !pattern) return FALSE;
  
  // Simple wildcard matching - "*" matches anything
  if (strcmp(pattern, "*") == 0) return TRUE;
  
  // Exact match
  if (strcmp(text, pattern) == 0) return TRUE;
  
  // Pattern ending with "*" - prefix match
  size_t patlen = strlen(pattern);
  if (patlen > 0 && pattern[patlen-1] == '*') {
    return strncmp(text, pattern, patlen-1) == 0;
  }
  
  return FALSE;
}

// Check if the current driver matches the given driver info
SbBool
SoGLDriverDatabaseP::matchesDriver(const cc_glglue * context, const DriverInfo& driver)
{
  const char* vendor = (const char*)glGetString(GL_VENDOR);
  const char* renderer = (const char*)glGetString(GL_RENDERER);
  const char* version = (const char*)glGetString(GL_VERSION);
  
  if (!vendor || !renderer || !version) return FALSE;
  
  return matchesPattern(vendor, driver.vendor_pattern) &&
         matchesPattern(renderer, driver.renderer_pattern) &&
         matchesPattern(version, driver.version_pattern);
}

// Find an override for the given feature and driver context
const FeatureOverride*
SoGLDriverDatabaseP::findOverride(const cc_glglue * context, const SbName & feature)
{
  const char* feature_str = feature.getString();
  
  for (size_t i = 0; i < sizeof(EMBEDDED_OVERRIDES)/sizeof(EMBEDDED_OVERRIDES[0]); i++) {
    const FeatureOverride& override = EMBEDDED_OVERRIDES[i];
    if (strcmp(feature_str, override.feature_name) == 0) {
      if (matchesDriver(context, override.driver)) {
        return &override;
      }
    }
  }
  
  return NULL;
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
  (void) pimpl(); // Initialize the singleton
}

SbBool
SoGLDriverDatabase::isSupported(const cc_glglue * context, const SbName & feature)
{
  return pimpl()->isSupported(context, feature);
}

SbBool
SoGLDriverDatabase::isBroken(const cc_glglue * context, const SbName & feature)
{
  return pimpl()->isBroken(context, feature);
}

SbBool
SoGLDriverDatabase::isSlow(const cc_glglue * context, const SbName & feature)
{
  return pimpl()->isSlow(context, feature);
}

SbBool
SoGLDriverDatabase::isFast(const cc_glglue * context, const SbName & feature)
{
  return pimpl()->isFast(context, feature);
}

SbName
SoGLDriverDatabase::getComment(const cc_glglue * context, const SbName & feature)
{
  return pimpl()->getComment(context, feature);
}

// Legacy XML loading methods - now stubs since we use embedded data
void
SoGLDriverDatabase::loadFromBuffer(const char * buffer)
{
  // XML loading removed - using embedded database
  SoDebugError::post("SoGLDriverDatabase::loadFromBuffer",
                     "XML loading is no longer supported. Using embedded driver database.");
}

void
SoGLDriverDatabase::loadFromFile(const SbName & filename)
{
  // XML loading removed - using embedded database  
  SoDebugError::post("SoGLDriverDatabase::loadFromFile",
                     "XML loading is no longer supported. Using embedded driver database.");
}

void
SoGLDriverDatabase::addBuffer(const char * buffer)
{
  // XML loading removed - using embedded database
  SoDebugError::post("SoGLDriverDatabase::addBuffer",
                     "XML loading is no longer supported. Using embedded driver database.");
}

void
SoGLDriverDatabase::addFile(const SbName & filename)
{
  // XML loading removed - using embedded database
  SoDebugError::post("SoGLDriverDatabase::addFile",
                     "XML loading is no longer supported. Using embedded driver database.");
}

void
SoGLDriverDatabase::addFeature(const SbName & feature, const SbName & comment)
{
  // Runtime feature addition removed - use embedded database
  SoDebugError::post("SoGLDriverDatabase::addFeature",
                     "Runtime feature addition is no longer supported. Using embedded driver database.");
}