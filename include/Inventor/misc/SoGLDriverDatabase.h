#ifndef SOGLDRIVERDATABASE_H
#define SOGLDRIVERDATABASE_H

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

#include <Inventor/lists/SbList.h>

/* Forward declaration */
struct SoGLContext;

class SbName;

class OBOL_DLL_API SoGLDriverDatabase {
public:
  static SbBool isSupported(const SoGLContext * context, const SbName & feature);

  static void init(void);
};

/**************************************************************************/

// OpenGL features that can't be checked with a single GL_ARB/EXT extension test
#define SO_GL_MULTIDRAW_ELEMENTS    "OBOL_multidraw_elements"
#define SO_GL_POLYGON_OFFSET        "OBOL_polygon_offset"
#define SO_GL_TEXTURE_OBJECT        "OBOL_texture_object"
#define SO_GL_3D_TEXTURES           "OBOL_3d_textures"
#define SO_GL_MULTITEXTURE          "OBOL_multitexture"
#define SO_GL_TEXSUBIMAGE           "OBOL_texsubimage"
#define SO_GL_2D_PROXY_TEXTURES     "OBOL_2d_proxy_textures"
#define SO_GL_TEXTURE_EDGE_CLAMP    "OBOL_texture_edge_clamp"
#define SO_GL_TEXTURE_COMPRESSION   "OBOL_texture_compression"
#define SO_GL_COLOR_TABLES          "OBOL_color_tables"
#define SO_GL_COLOR_SUBTABLES       "OBOL_color_subtables"
#define SO_GL_PALETTED_TEXTURES     "OBOL_paletted_textures"
#define SO_GL_BLEND_EQUATION        "OBOL_blend_equation"
#define SO_GL_VERTEX_ARRAY          "OBOL_vertex_array"
#define SO_GL_NV_VERTEX_ARRAY_RANGE "OBOL_nv_vertex_array_range"
#define SO_GL_VERTEX_BUFFER_OBJECT  "OBOL_vertex_buffer_object"
#define SO_GL_ARB_FRAGMENT_PROGRAM  "OBOL_arb_fragment_program"
#define SO_GL_ARB_VERTEX_PROGRAM    "OBOL_arb_vertex_program"
#define SO_GL_ARB_VERTEX_SHADER     "OBOL_arb_vertex_shader"
#define SO_GL_ARB_SHADER_OBJECT     "OBOL_arb_shader_object"
#define SO_GL_OCCLUSION_QUERY       "OBOL_occlusion_query"
#define SO_GL_FRAMEBUFFER_OBJECT    "OBOL_framebuffer_object"
#define SO_GL_ANISOTROPIC_FILTERING "OBOL_anisotropic_filtering"
#define SO_GL_SORTED_LAYERS_BLEND   "OBOL_sorted_layers_blend"
#define SO_GL_BUMPMAPPING           "OBOL_bumpmapping"
#define SO_GL_VBO_IN_DISPLAYLIST    "OBOL_vbo_in_displaylist"
#define SO_GL_NON_POWER_OF_TWO_TEXTURES "OBOL_non_power_of_two_textures"
#define SO_GL_GENERATE_MIPMAP       "OBOL_generate_mipmap"
#define SO_GL_GLSL_CLIP_VERTEX_HW   "OBOL_GLSL_clip_vertex_hw"
#endif // SOGLDATABASE_H
