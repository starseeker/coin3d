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
  
  The embedded database contains known issues from legacy and current OpenGL drivers
  that require manual workarounds, including:
  - VBO performance and crash issues on Intel, ATI, and NVIDIA hardware
  - Multitexture problems on older integrated graphics
  - Shader compilation failures on legacy drivers  
  - Framebuffer object limitations on older hardware
  - Texture format support issues across various vendors
  
  These entries represent accumulated knowledge of OpenGL driver quirks that
  cannot be reliably detected through extension queries alone.
*/

#include <Inventor/misc/SoGLDriverDatabase.h>

#include <cstring>
#include <string>

#include "C/CoinTidbits.h"
#include <Inventor/SbName.h>
#include <Inventor/SbString.h>
#include "glue/glp.h"
#include <Inventor/errors/SoDebugError.h>

#include "misc/SbHash.h"
#include "glue/glp.h"


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
// 
// Each entry contains:
// - feature_name: Coin3D feature identifier (e.g., "COIN_vertex_buffer_object")
// - driver: Vendor/renderer/version pattern matching (supports wildcards with '*')
// - status: BROKEN (crashes/fails), SLOW (performance issues), FAST (optimized), DISABLED (force off)
// - comment: Human-readable description of the issue
//
// Patterns support simple wildcards:
// - "*" matches any string
// - "prefix*" matches strings starting with "prefix"
// - Exact string matches work as expected
//
// Based on documented issues in gl.cpp and 2025 OpenGL best practices
static const FeatureOverride EMBEDDED_OVERRIDES[] = {
  // Intel integrated graphics issues
  {"COIN_vertex_buffer_object", {"Intel", "GMA 950", "*"}, FeatureOverride::SLOW, "VBO performance is poor on GMA 950"},
  {"COIN_vertex_buffer_object", {"Intel", "GMA 3150", "*"}, FeatureOverride::SLOW, "VBO performance is poor on GMA 3150"},
  {"COIN_multitexture", {"Intel", "GMA 950", "*"}, FeatureOverride::SLOW, "Multitexture performance is poor on GMA 950"},
  {"COIN_multitexture", {"Intel", "Solano", "*"}, FeatureOverride::BROKEN, "Visual artifacts with multitexture on Intel Solano"},
  {"COIN_2d_proxy_textures", {"Intel", "*", "*"}, FeatureOverride::BROKEN, "Proxy texture implementation incompatible"},
  {"COIN_non_power_of_two_textures", {"Intel", "GMA*", "*"}, FeatureOverride::SLOW, "NPOT textures slow on older Intel integrated"},
  
  // AMD/ATI legacy driver issues  
  {"COIN_vertex_buffer_object", {"ATI Technologies Inc.", "Radeon 9*", "1.*"}, FeatureOverride::BROKEN, "VBO crashes on old ATI Radeon 9xxx drivers"},
  {"COIN_vertex_buffer_object", {"ATI Technologies Inc.", "Radeon 7*", "*"}, FeatureOverride::BROKEN, "VBO implementation broken on Radeon 7xxx series"},
  {"COIN_vbo_in_displaylist", {"ATI Technologies Inc.", "Radeon*", "1.*"}, FeatureOverride::BROKEN, "VBO in display lists crashes on old ATI drivers"},
  {"COIN_vbo_in_displaylist", {"ATI Technologies Inc.", "Radeon*", "2.0*"}, FeatureOverride::BROKEN, "VBO in display lists crashes on ATI Radeon 2.0 drivers"},
  {"COIN_3d_textures", {"ATI Technologies Inc.", "Radeon 7500*", "*"}, FeatureOverride::BROKEN, "3D textures crash on Radeon 7500"},
  {"COIN_arb_vertex_shader", {"ATI Technologies Inc.", "Radeon 9*", "1.*"}, FeatureOverride::BROKEN, "Vertex shader compilation issues on old ATI"},
  {"COIN_GLSL_clip_vertex_hw", {"ATI Technologies Inc.", "Radeon*", "1.*"}, FeatureOverride::BROKEN, "Hardware clip vertex broken on old ATI drivers"},
  {"COIN_non_power_of_two_textures", {"ATI Technologies Inc.", "Radeon 9*", "*"}, FeatureOverride::SLOW, "NPOT textures slow on Radeon 9xxx"},
  
  // NVIDIA driver issues
  {"COIN_vertex_buffer_object", {"NVIDIA Corporation", "*", "1.4.0*"}, FeatureOverride::BROKEN, "VBO broken on NVIDIA 44.96 Linux driver"},
  {"COIN_vertex_buffer_object", {"NVIDIA Corporation", "GeForce4 Go*", "*"}, FeatureOverride::SLOW, "VBO performance poor on GeForce4 Go mobile"},
  {"COIN_vertex_buffer_object", {"NVIDIA Corporation", "GeForce 7950 GX2*", "2.0.2*"}, FeatureOverride::BROKEN, "VBO crashes in offscreen contexts on GeForce 7950 GX2"},
  {"COIN_framebuffer_object", {"NVIDIA Corporation", "GeForce2*", "*"}, FeatureOverride::BROKEN, "FBO not properly supported on GeForce2"},
  {"COIN_framebuffer_object", {"NVIDIA Corporation", "GeForce 256*", "*"}, FeatureOverride::BROKEN, "FBO not supported on GeForce 256"},
  
  // 3Dlabs issues
  {"COIN_vertex_buffer_object", {"3Dlabs", "*", "*"}, FeatureOverride::BROKEN, "VBO implementation fundamentally broken on 3Dlabs hardware"},
  
  // Legacy vendor issues
  {"COIN_texture_edge_clamp", {"Trident*", "*", "*"}, FeatureOverride::BROKEN, "GL_CLAMP_TO_EDGE not supported on Trident cards"},
  {"COIN_multitexture", {"Matrox", "G400", "1.1.3*"}, FeatureOverride::BROKEN, "Multitexture broken on old Matrox G400 drivers"},
  {"COIN_polygon_offset", {"ELSA", "TNT2 Vanta*", "1.1.4*"}, FeatureOverride::BROKEN, "Polygon offset broken on old ELSA TNT2 Vanta"},
  
  // Sun/Oracle graphics issues
  {"COIN_multitexture", {"Sun*", "Expert3D*", "1.2*"}, FeatureOverride::BROKEN, "Dual screen artifacts with multitexture on Sun Expert3D"},
  
  // Mesa software renderer performance issues
  {"COIN_vertex_buffer_object", {"*", "*Mesa*", "*"}, FeatureOverride::SLOW, "VBO slower than vertex arrays in Mesa software renderer"},
  {"COIN_framebuffer_object", {"*", "*Mesa*", "7.*"}, FeatureOverride::SLOW, "FBO performance poor in Mesa 7.x software renderer"},
  {"COIN_multitexture", {"*", "*Mesa*", "6.*"}, FeatureOverride::SLOW, "Multitexture slow in Mesa 6.x software renderer"},
  
  // Generic integrated graphics performance
  {"COIN_vertex_buffer_object", {"*", "*Mobile*", "*"}, FeatureOverride::SLOW, "VBO generally slower on mobile/integrated graphics"},
  {"COIN_anisotropic_filtering", {"Intel", "*", "*"}, FeatureOverride::SLOW, "Anisotropic filtering very slow on Intel integrated"},
  {"COIN_generate_mipmap", {"Intel", "GMA*", "*"}, FeatureOverride::SLOW, "Hardware mipmap generation slow on Intel GMA"}
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

// Class method that provides access to the singleton instance
SoGLDriverDatabaseP *
SoGLDriverDatabase::pimpl(void)
{
  return ::pimpl();
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