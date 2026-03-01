/*
 * sogl_prefix.h  --  BRL-CAD-style function-name prefix mechanism for the
 *                    Obol GL-layer (SoGLContext_* functions).
 *
 * Motivation
 * ----------
 * When building with OBOL_BUILD_DUAL_GL=ON, both the system-OpenGL variant
 * and the OSMesa variant of the Obol GL glue layer (src/glue/gl.cpp) are
 * compiled into the same shared library.  Without name decoration the two
 * sets of C linkage functions would collide at link time.
 *
 * The pattern mirrors BRL-CAD's approach for embedding a private zlib:
 *
 *   #define OBOL_OSMESA_PREFIX 1
 *   #define OBOL_OSMESA_PREFIX_STR osmesa_
 *   ...
 *   #define SoGLContext_instance  SOGL_ADD_PREFIX(SoGLContext_instance)
 *   ...
 *
 * Usage
 * -----
 * In normal (non-dual) builds this header is a no-op.
 *
 * When compiling the OSMesa compilation unit (src/glue/gl_osmesa.cpp) the
 * unit defines SOGL_PREFIX_SET before including glp.h, which causes every
 * SoGLContext_* symbol to be decorated with the osmesa_ prefix.  The same
 * define is seen when any caller includes glp.h inside that translation
 * unit, so declarations and definitions match automatically.
 *
 * A thin OBOL_BUILD_DUAL_GL dispatch wrapper (also in gl.cpp) keeps the
 * undecorated API working at runtime by checking the context backend type
 * and forwarding to either the sysgl_ or osmesa_ implementation.
 */

#ifndef OBOL_SOGL_PREFIX_H
#define OBOL_SOGL_PREFIX_H

#ifdef SOGL_PREFIX_SET

/* Allow a user-configurable prefix string, defaulting to "osmesa_". */
#ifndef SOGL_PREFIX_STR
#  define SOGL_PREFIX_STR osmesa_
#endif

#define SOGL_CONCAT2(a, b) a ## b
#define SOGL_CONCAT(a, b)  SOGL_CONCAT2(a, b)
#define SOGL_ADD_PREFIX(name) SOGL_CONCAT(SOGL_PREFIX_STR, name)

/* -----------------------------------------------------------------------
 * SoGLContext lifecycle / query
 * --------------------------------------------------------------------- */
#define SoGLContext_instance                       SOGL_ADD_PREFIX(SoGLContext_instance)
#define SoGLContext_instance_from_context_ptr      SOGL_ADD_PREFIX(SoGLContext_instance_from_context_ptr)
#define SoGLContext_destruct                       SOGL_ADD_PREFIX(SoGLContext_destruct)
#define SoGLContext_debug                          SOGL_ADD_PREFIX(SoGLContext_debug)
#define SoGLContext_extension_available            SOGL_ADD_PREFIX(SoGLContext_extension_available)
#define SoGLContext_stencil_bits_hack              SOGL_ADD_PREFIX(SoGLContext_stencil_bits_hack)
#define SoGLContext_dl_handle                      SOGL_ADD_PREFIX(SoGLContext_dl_handle)
#define SoGLContext_add_instance_created_callback  SOGL_ADD_PREFIX(SoGLContext_add_instance_created_callback)

/* -----------------------------------------------------------------------
 * GL version / extension queries
 * --------------------------------------------------------------------- */
#define SoGLContext_glversion                      SOGL_ADD_PREFIX(SoGLContext_glversion)
#define SoGLContext_glversion_matches_at_least     SOGL_ADD_PREFIX(SoGLContext_glversion_matches_at_least)
#define SoGLContext_glxversion_matches_at_least    SOGL_ADD_PREFIX(SoGLContext_glxversion_matches_at_least)
#define SoGLContext_glext_supported                SOGL_ADD_PREFIX(SoGLContext_glext_supported)
#define SoGLContext_getprocaddress                 SOGL_ADD_PREFIX(SoGLContext_getprocaddress)
#define SoGLContext_isdirect                       SOGL_ADD_PREFIX(SoGLContext_isdirect)

/* -----------------------------------------------------------------------
 * Capabilities
 * --------------------------------------------------------------------- */
#define SoGLContext_has_polygon_offset             SOGL_ADD_PREFIX(SoGLContext_has_polygon_offset)
#define SoGLContext_has_texture_objects            SOGL_ADD_PREFIX(SoGLContext_has_texture_objects)
#define SoGLContext_has_3d_textures                SOGL_ADD_PREFIX(SoGLContext_has_3d_textures)
#define SoGLContext_has_multitexture               SOGL_ADD_PREFIX(SoGLContext_has_multitexture)
#define SoGLContext_has_texsubimage                SOGL_ADD_PREFIX(SoGLContext_has_texsubimage)
#define SoGLContext_has_2d_proxy_textures          SOGL_ADD_PREFIX(SoGLContext_has_2d_proxy_textures)
#define SoGLContext_has_texture_edge_clamp         SOGL_ADD_PREFIX(SoGLContext_has_texture_edge_clamp)
#define SoGLContext_has_color_tables               SOGL_ADD_PREFIX(SoGLContext_has_color_tables)
#define SoGLContext_has_color_subtables            SOGL_ADD_PREFIX(SoGLContext_has_color_subtables)
#define SoGLContext_has_paletted_textures          SOGL_ADD_PREFIX(SoGLContext_has_paletted_textures)
#define SoGLContext_has_blendequation              SOGL_ADD_PREFIX(SoGLContext_has_blendequation)
#define SoGLContext_has_blendfuncseparate          SOGL_ADD_PREFIX(SoGLContext_has_blendfuncseparate)
#define SoGLContext_has_vertex_array               SOGL_ADD_PREFIX(SoGLContext_has_vertex_array)
#define SoGLContext_has_multidraw_vertex_arrays    SOGL_ADD_PREFIX(SoGLContext_has_multidraw_vertex_arrays)
#define SoGLContext_has_nv_vertex_array_range      SOGL_ADD_PREFIX(SoGLContext_has_nv_vertex_array_range)
#define SoGLContext_has_vertex_buffer_object       SOGL_ADD_PREFIX(SoGLContext_has_vertex_buffer_object)
#define SoGLContext_has_arb_fragment_program       SOGL_ADD_PREFIX(SoGLContext_has_arb_fragment_program)
#define SoGLContext_has_arb_vertex_program         SOGL_ADD_PREFIX(SoGLContext_has_arb_vertex_program)
#define SoGLContext_has_arb_vertex_shader          SOGL_ADD_PREFIX(SoGLContext_has_arb_vertex_shader)
#define SoGLContext_has_arb_shader_objects         SOGL_ADD_PREFIX(SoGLContext_has_arb_shader_objects)
#define SoGLContext_has_occlusion_query            SOGL_ADD_PREFIX(SoGLContext_has_occlusion_query)
#define SoGLContext_has_framebuffer_objects        SOGL_ADD_PREFIX(SoGLContext_has_framebuffer_objects)
#define SoGLContext_has_generate_mipmap            SOGL_ADD_PREFIX(SoGLContext_has_generate_mipmap)
#define SoGLContext_can_do_bumpmapping             SOGL_ADD_PREFIX(SoGLContext_can_do_bumpmapping)
#define SoGLContext_can_do_sortedlayersblend       SOGL_ADD_PREFIX(SoGLContext_can_do_sortedlayersblend)
#define SoGLContext_can_do_anisotropic_filtering   SOGL_ADD_PREFIX(SoGLContext_can_do_anisotropic_filtering)

/* -----------------------------------------------------------------------
 * Texture format helpers
 * --------------------------------------------------------------------- */
#define SoGLContext_get_internal_texture_format    SOGL_ADD_PREFIX(SoGLContext_get_internal_texture_format)
#define SoGLContext_get_texture_format             SOGL_ADD_PREFIX(SoGLContext_get_texture_format)
#define SoGLContext_is_texture_size_legal          SOGL_ADD_PREFIX(SoGLContext_is_texture_size_legal)
#define SoGLContext_non_power_of_two_textures      SOGL_ADD_PREFIX(SoGLContext_non_power_of_two_textures)
#define SoGLContext_vbo_in_displaylist_supported   SOGL_ADD_PREFIX(SoGLContext_vbo_in_displaylist_supported)

/* -----------------------------------------------------------------------
 * Limits / capabilities
 * --------------------------------------------------------------------- */
#define SoGLContext_get_max_lights                 SOGL_ADD_PREFIX(SoGLContext_get_max_lights)
#define SoGLContext_get_line_width_range           SOGL_ADD_PREFIX(SoGLContext_get_line_width_range)
#define SoGLContext_get_point_size_range           SOGL_ADD_PREFIX(SoGLContext_get_point_size_range)
#define SoGLContext_get_max_anisotropy             SOGL_ADD_PREFIX(SoGLContext_get_max_anisotropy)
#define SoGLContext_max_texture_units              SOGL_ADD_PREFIX(SoGLContext_max_texture_units)

/* -----------------------------------------------------------------------
 * GL function wrappers (polygon offset)
 * --------------------------------------------------------------------- */
#define SoGLContext_glPolygonOffsetEnable          SOGL_ADD_PREFIX(SoGLContext_glPolygonOffsetEnable)
#define SoGLContext_glPolygonOffset                SOGL_ADD_PREFIX(SoGLContext_glPolygonOffset)

/* -----------------------------------------------------------------------
 * GL function wrappers (textures)
 * --------------------------------------------------------------------- */
#define SoGLContext_glGenTextures                  SOGL_ADD_PREFIX(SoGLContext_glGenTextures)
#define SoGLContext_glBindTexture                  SOGL_ADD_PREFIX(SoGLContext_glBindTexture)
#define SoGLContext_glDeleteTextures               SOGL_ADD_PREFIX(SoGLContext_glDeleteTextures)
#define SoGLContext_glTexImage3D                   SOGL_ADD_PREFIX(SoGLContext_glTexImage3D)
#define SoGLContext_glTexSubImage3D                SOGL_ADD_PREFIX(SoGLContext_glTexSubImage3D)
#define SoGLContext_glCopyTexSubImage3D            SOGL_ADD_PREFIX(SoGLContext_glCopyTexSubImage3D)
#define SoGLContext_glTexSubImage2D                SOGL_ADD_PREFIX(SoGLContext_glTexSubImage2D)
#define SoGLContext_glActiveTexture                SOGL_ADD_PREFIX(SoGLContext_glActiveTexture)
#define SoGLContext_glClientActiveTexture          SOGL_ADD_PREFIX(SoGLContext_glClientActiveTexture)

/* -----------------------------------------------------------------------
 * GL function wrappers (NV register combiners / texture shaders)
 * --------------------------------------------------------------------- */
#define SoGLContext_has_nv_register_combiners      SOGL_ADD_PREFIX(SoGLContext_has_nv_register_combiners)
#define SoGLContext_glCombinerParameterfvNV        SOGL_ADD_PREFIX(SoGLContext_glCombinerParameterfvNV)
#define SoGLContext_glCombinerParameterivNV        SOGL_ADD_PREFIX(SoGLContext_glCombinerParameterivNV)
#define SoGLContext_glCombinerParameterfNV         SOGL_ADD_PREFIX(SoGLContext_glCombinerParameterfNV)
#define SoGLContext_glCombinerParameteriNV         SOGL_ADD_PREFIX(SoGLContext_glCombinerParameteriNV)
#define SoGLContext_glCombinerInputNV              SOGL_ADD_PREFIX(SoGLContext_glCombinerInputNV)
#define SoGLContext_glCombinerOutputNV             SOGL_ADD_PREFIX(SoGLContext_glCombinerOutputNV)
#define SoGLContext_glFinalCombinerInputNV         SOGL_ADD_PREFIX(SoGLContext_glFinalCombinerInputNV)
#define SoGLContext_glGetCombinerInputParameterfvNV  SOGL_ADD_PREFIX(SoGLContext_glGetCombinerInputParameterfvNV)
#define SoGLContext_glGetCombinerInputParameterivNV  SOGL_ADD_PREFIX(SoGLContext_glGetCombinerInputParameterivNV)
#define SoGLContext_glGetCombinerOutputParameterfvNV SOGL_ADD_PREFIX(SoGLContext_glGetCombinerOutputParameterfvNV)
#define SoGLContext_glGetCombinerOutputParameterivNV SOGL_ADD_PREFIX(SoGLContext_glGetCombinerOutputParameterivNV)
#define SoGLContext_glGetFinalCombinerInputParameterfvNV SOGL_ADD_PREFIX(SoGLContext_glGetFinalCombinerInputParameterfvNV)
#define SoGLContext_glGetFinalCombinerInputParameterivNV SOGL_ADD_PREFIX(SoGLContext_glGetFinalCombinerInputParameterivNV)
#define SoGLContext_has_nv_texture_rectangle       SOGL_ADD_PREFIX(SoGLContext_has_nv_texture_rectangle)
#define SoGLContext_has_nv_texture_shader          SOGL_ADD_PREFIX(SoGLContext_has_nv_texture_shader)
#define SoGLContext_has_ext_texture_rectangle      SOGL_ADD_PREFIX(SoGLContext_has_ext_texture_rectangle)
#define SoGLContext_has_texture_env_combine        SOGL_ADD_PREFIX(SoGLContext_has_texture_env_combine)
#define SoGLContext_has_arb_depth_texture          SOGL_ADD_PREFIX(SoGLContext_has_arb_depth_texture)
#define SoGLContext_has_arb_shadow                 SOGL_ADD_PREFIX(SoGLContext_has_arb_shadow)

/* -----------------------------------------------------------------------
 * ARB program (fragment/vertex) functions
 * --------------------------------------------------------------------- */
#define SoGLContext_glProgramString                SOGL_ADD_PREFIX(SoGLContext_glProgramString)
#define SoGLContext_glProgramEnvParameter4d        SOGL_ADD_PREFIX(SoGLContext_glProgramEnvParameter4d)
#define SoGLContext_glProgramEnvParameter4dv       SOGL_ADD_PREFIX(SoGLContext_glProgramEnvParameter4dv)
#define SoGLContext_glProgramEnvParameter4f        SOGL_ADD_PREFIX(SoGLContext_glProgramEnvParameter4f)
#define SoGLContext_glProgramEnvParameter4fv       SOGL_ADD_PREFIX(SoGLContext_glProgramEnvParameter4fv)

/* -----------------------------------------------------------------------
 * Miscellaneous SoGLContext helpers
 * --------------------------------------------------------------------- */
#define SoGLContext_get_contextid                  SOGL_ADD_PREFIX(SoGLContext_get_contextid)

/* -----------------------------------------------------------------------
 * cc_glue texture compression helpers
 * --------------------------------------------------------------------- */
#define cc_glue_has_texture_compression            SOGL_ADD_PREFIX(cc_glue_has_texture_compression)
#define cc_glue_has_texture_compression_2d         SOGL_ADD_PREFIX(cc_glue_has_texture_compression_2d)
#define cc_glue_has_texture_compression_3d         SOGL_ADD_PREFIX(cc_glue_has_texture_compression_3d)

/* -----------------------------------------------------------------------
 * coin_catch_gl_errors helper
 * --------------------------------------------------------------------- */
#define coin_catch_gl_errors                       SOGL_ADD_PREFIX(coin_catch_gl_errors)

/* -----------------------------------------------------------------------
 * GL function wrappers (multitexture coords)
 * --------------------------------------------------------------------- */
#define SoGLContext_glMultiTexCoord2f              SOGL_ADD_PREFIX(SoGLContext_glMultiTexCoord2f)
#define SoGLContext_glMultiTexCoord2fv             SOGL_ADD_PREFIX(SoGLContext_glMultiTexCoord2fv)
#define SoGLContext_glMultiTexCoord3fv             SOGL_ADD_PREFIX(SoGLContext_glMultiTexCoord3fv)
#define SoGLContext_glMultiTexCoord4fv             SOGL_ADD_PREFIX(SoGLContext_glMultiTexCoord4fv)

/* -----------------------------------------------------------------------
 * GL function wrappers (client attrib)
 * --------------------------------------------------------------------- */
#define SoGLContext_glPushClientAttrib             SOGL_ADD_PREFIX(SoGLContext_glPushClientAttrib)
#define SoGLContext_glPopClientAttrib              SOGL_ADD_PREFIX(SoGLContext_glPopClientAttrib)

/* -----------------------------------------------------------------------
 * GL function wrappers (compressed textures)
 * --------------------------------------------------------------------- */
#define SoGLContext_glCompressedTexImage3D         SOGL_ADD_PREFIX(SoGLContext_glCompressedTexImage3D)
#define SoGLContext_glCompressedTexImage2D         SOGL_ADD_PREFIX(SoGLContext_glCompressedTexImage2D)
#define SoGLContext_glCompressedTexImage1D         SOGL_ADD_PREFIX(SoGLContext_glCompressedTexImage1D)
#define SoGLContext_glCompressedTexSubImage3D      SOGL_ADD_PREFIX(SoGLContext_glCompressedTexSubImage3D)
#define SoGLContext_glCompressedTexSubImage2D      SOGL_ADD_PREFIX(SoGLContext_glCompressedTexSubImage2D)
#define SoGLContext_glCompressedTexSubImage1D      SOGL_ADD_PREFIX(SoGLContext_glCompressedTexSubImage1D)
#define SoGLContext_glGetCompressedTexImage        SOGL_ADD_PREFIX(SoGLContext_glGetCompressedTexImage)

/* -----------------------------------------------------------------------
 * GL function wrappers (color tables)
 * --------------------------------------------------------------------- */
#define SoGLContext_glColorTable                   SOGL_ADD_PREFIX(SoGLContext_glColorTable)
#define SoGLContext_glColorSubTable                SOGL_ADD_PREFIX(SoGLContext_glColorSubTable)
#define SoGLContext_glGetColorTable                SOGL_ADD_PREFIX(SoGLContext_glGetColorTable)
#define SoGLContext_glGetColorTableParameteriv     SOGL_ADD_PREFIX(SoGLContext_glGetColorTableParameteriv)
#define SoGLContext_glGetColorTableParameterfv     SOGL_ADD_PREFIX(SoGLContext_glGetColorTableParameterfv)

/* -----------------------------------------------------------------------
 * GL function wrappers (blending)
 * --------------------------------------------------------------------- */
#define SoGLContext_glBlendEquation                SOGL_ADD_PREFIX(SoGLContext_glBlendEquation)
#define SoGLContext_glBlendFuncSeparate            SOGL_ADD_PREFIX(SoGLContext_glBlendFuncSeparate)

/* -----------------------------------------------------------------------
 * GL function wrappers (vertex arrays)
 * --------------------------------------------------------------------- */
#define SoGLContext_glVertexPointer                SOGL_ADD_PREFIX(SoGLContext_glVertexPointer)
#define SoGLContext_glTexCoordPointer              SOGL_ADD_PREFIX(SoGLContext_glTexCoordPointer)
#define SoGLContext_glNormalPointer                SOGL_ADD_PREFIX(SoGLContext_glNormalPointer)
#define SoGLContext_glColorPointer                 SOGL_ADD_PREFIX(SoGLContext_glColorPointer)
#define SoGLContext_glIndexPointer                 SOGL_ADD_PREFIX(SoGLContext_glIndexPointer)
#define SoGLContext_glEnableClientState            SOGL_ADD_PREFIX(SoGLContext_glEnableClientState)
#define SoGLContext_glDisableClientState           SOGL_ADD_PREFIX(SoGLContext_glDisableClientState)
#define SoGLContext_glInterleavedArrays            SOGL_ADD_PREFIX(SoGLContext_glInterleavedArrays)
#define SoGLContext_glDrawArrays                   SOGL_ADD_PREFIX(SoGLContext_glDrawArrays)
#define SoGLContext_glDrawElements                 SOGL_ADD_PREFIX(SoGLContext_glDrawElements)
#define SoGLContext_glDrawRangeElements            SOGL_ADD_PREFIX(SoGLContext_glDrawRangeElements)
#define SoGLContext_glArrayElement                 SOGL_ADD_PREFIX(SoGLContext_glArrayElement)
#define SoGLContext_glMultiDrawArrays              SOGL_ADD_PREFIX(SoGLContext_glMultiDrawArrays)
#define SoGLContext_glMultiDrawElements            SOGL_ADD_PREFIX(SoGLContext_glMultiDrawElements)

/* -----------------------------------------------------------------------
 * GL function wrappers (NV vertex array range)
 * --------------------------------------------------------------------- */
#define SoGLContext_glFlushVertexArrayRangeNV      SOGL_ADD_PREFIX(SoGLContext_glFlushVertexArrayRangeNV)
#define SoGLContext_glVertexArrayRangeNV           SOGL_ADD_PREFIX(SoGLContext_glVertexArrayRangeNV)
#define SoGLContext_glAllocateMemoryNV             SOGL_ADD_PREFIX(SoGLContext_glAllocateMemoryNV)
#define SoGLContext_glFreeMemoryNV                 SOGL_ADD_PREFIX(SoGLContext_glFreeMemoryNV)

/* -----------------------------------------------------------------------
 * GL function wrappers (VBO / buffer objects)
 * --------------------------------------------------------------------- */
#define SoGLContext_glBindBuffer                   SOGL_ADD_PREFIX(SoGLContext_glBindBuffer)
#define SoGLContext_glDeleteBuffers                SOGL_ADD_PREFIX(SoGLContext_glDeleteBuffers)
#define SoGLContext_glGenBuffers                   SOGL_ADD_PREFIX(SoGLContext_glGenBuffers)
#define SoGLContext_glIsBuffer                     SOGL_ADD_PREFIX(SoGLContext_glIsBuffer)
#define SoGLContext_glBufferData                   SOGL_ADD_PREFIX(SoGLContext_glBufferData)
#define SoGLContext_glBufferSubData                SOGL_ADD_PREFIX(SoGLContext_glBufferSubData)
#define SoGLContext_glGetBufferSubData             SOGL_ADD_PREFIX(SoGLContext_glGetBufferSubData)
#define SoGLContext_glMapBuffer                    SOGL_ADD_PREFIX(SoGLContext_glMapBuffer)
#define SoGLContext_glUnmapBuffer                  SOGL_ADD_PREFIX(SoGLContext_glUnmapBuffer)
#define SoGLContext_glGetBufferParameteriv         SOGL_ADD_PREFIX(SoGLContext_glGetBufferParameteriv)
#define SoGLContext_glGetBufferPointerv            SOGL_ADD_PREFIX(SoGLContext_glGetBufferPointerv)

/* -----------------------------------------------------------------------
 * GL function wrappers (ARB programs / shaders)
 * --------------------------------------------------------------------- */
#define SoGLContext_glBindProgram                  SOGL_ADD_PREFIX(SoGLContext_glBindProgram)
#define SoGLContext_glDeletePrograms               SOGL_ADD_PREFIX(SoGLContext_glDeletePrograms)
#define SoGLContext_glGenPrograms                  SOGL_ADD_PREFIX(SoGLContext_glGenPrograms)
#define SoGLContext_glProgramLocalParameter4d      SOGL_ADD_PREFIX(SoGLContext_glProgramLocalParameter4d)
#define SoGLContext_glProgramLocalParameter4dv     SOGL_ADD_PREFIX(SoGLContext_glProgramLocalParameter4dv)
#define SoGLContext_glProgramLocalParameter4f      SOGL_ADD_PREFIX(SoGLContext_glProgramLocalParameter4f)
#define SoGLContext_glProgramLocalParameter4fv     SOGL_ADD_PREFIX(SoGLContext_glProgramLocalParameter4fv)
#define SoGLContext_glGetProgramEnvParameterdv     SOGL_ADD_PREFIX(SoGLContext_glGetProgramEnvParameterdv)
#define SoGLContext_glGetProgramEnvParameterfv     SOGL_ADD_PREFIX(SoGLContext_glGetProgramEnvParameterfv)
#define SoGLContext_glGetProgramLocalParameterdv   SOGL_ADD_PREFIX(SoGLContext_glGetProgramLocalParameterdv)
#define SoGLContext_glGetProgramLocalParameterfv   SOGL_ADD_PREFIX(SoGLContext_glGetProgramLocalParameterfv)
#define SoGLContext_glGetProgramiv                 SOGL_ADD_PREFIX(SoGLContext_glGetProgramiv)
#define SoGLContext_glGetProgramString             SOGL_ADD_PREFIX(SoGLContext_glGetProgramString)
#define SoGLContext_glIsProgram                    SOGL_ADD_PREFIX(SoGLContext_glIsProgram)

/* -----------------------------------------------------------------------
 * GL function wrappers (vertex attribs)
 * --------------------------------------------------------------------- */
#define SoGLContext_glVertexAttrib1s               SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib1s)
#define SoGLContext_glVertexAttrib1f               SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib1f)
#define SoGLContext_glVertexAttrib1d               SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib1d)
#define SoGLContext_glVertexAttrib2s               SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib2s)
#define SoGLContext_glVertexAttrib2f               SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib2f)
#define SoGLContext_glVertexAttrib2d               SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib2d)
#define SoGLContext_glVertexAttrib3s               SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib3s)
#define SoGLContext_glVertexAttrib3f               SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib3f)
#define SoGLContext_glVertexAttrib3d               SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib3d)
#define SoGLContext_glVertexAttrib4s               SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib4s)
#define SoGLContext_glVertexAttrib4f               SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib4f)
#define SoGLContext_glVertexAttrib4d               SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib4d)
#define SoGLContext_glVertexAttrib4Nub             SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib4Nub)
#define SoGLContext_glVertexAttrib1sv              SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib1sv)
#define SoGLContext_glVertexAttrib1fv              SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib1fv)
#define SoGLContext_glVertexAttrib1dv              SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib1dv)
#define SoGLContext_glVertexAttrib2sv              SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib2sv)
#define SoGLContext_glVertexAttrib2fv              SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib2fv)
#define SoGLContext_glVertexAttrib2dv              SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib2dv)
#define SoGLContext_glVertexAttrib3sv              SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib3sv)
#define SoGLContext_glVertexAttrib3fv              SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib3fv)
#define SoGLContext_glVertexAttrib3dv              SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib3dv)
#define SoGLContext_glVertexAttrib4bv              SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib4bv)
#define SoGLContext_glVertexAttrib4sv              SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib4sv)
#define SoGLContext_glVertexAttrib4iv              SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib4iv)
#define SoGLContext_glVertexAttrib4ubv             SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib4ubv)
#define SoGLContext_glVertexAttrib4usv             SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib4usv)
#define SoGLContext_glVertexAttrib4uiv             SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib4uiv)
#define SoGLContext_glVertexAttrib4fv              SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib4fv)
#define SoGLContext_glVertexAttrib4dv              SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib4dv)
#define SoGLContext_glVertexAttrib4Nbv             SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib4Nbv)
#define SoGLContext_glVertexAttrib4Nsv             SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib4Nsv)
#define SoGLContext_glVertexAttrib4Niv             SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib4Niv)
#define SoGLContext_glVertexAttrib4Nubv            SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib4Nubv)
#define SoGLContext_glVertexAttrib4Nusv            SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib4Nusv)
#define SoGLContext_glVertexAttrib4Nuiv            SOGL_ADD_PREFIX(SoGLContext_glVertexAttrib4Nuiv)
#define SoGLContext_glVertexAttribPointer          SOGL_ADD_PREFIX(SoGLContext_glVertexAttribPointer)
#define SoGLContext_glEnableVertexAttribArray      SOGL_ADD_PREFIX(SoGLContext_glEnableVertexAttribArray)
#define SoGLContext_glDisableVertexAttribArray     SOGL_ADD_PREFIX(SoGLContext_glDisableVertexAttribArray)
#define SoGLContext_glGetVertexAttribdv            SOGL_ADD_PREFIX(SoGLContext_glGetVertexAttribdv)
#define SoGLContext_glGetVertexAttribfv            SOGL_ADD_PREFIX(SoGLContext_glGetVertexAttribfv)
#define SoGLContext_glGetVertexAttribiv            SOGL_ADD_PREFIX(SoGLContext_glGetVertexAttribiv)
#define SoGLContext_glGetVertexAttribPointerv      SOGL_ADD_PREFIX(SoGLContext_glGetVertexAttribPointerv)

/* -----------------------------------------------------------------------
 * GL function wrappers (occlusion queries)
 * --------------------------------------------------------------------- */
#define SoGLContext_glGenQueries                   SOGL_ADD_PREFIX(SoGLContext_glGenQueries)
#define SoGLContext_glDeleteQueries                SOGL_ADD_PREFIX(SoGLContext_glDeleteQueries)
#define SoGLContext_glIsQuery                      SOGL_ADD_PREFIX(SoGLContext_glIsQuery)
#define SoGLContext_glBeginQuery                   SOGL_ADD_PREFIX(SoGLContext_glBeginQuery)
#define SoGLContext_glEndQuery                     SOGL_ADD_PREFIX(SoGLContext_glEndQuery)
#define SoGLContext_glGetQueryiv                   SOGL_ADD_PREFIX(SoGLContext_glGetQueryiv)
#define SoGLContext_glGetQueryObjectiv             SOGL_ADD_PREFIX(SoGLContext_glGetQueryObjectiv)
#define SoGLContext_glGetQueryObjectuiv            SOGL_ADD_PREFIX(SoGLContext_glGetQueryObjectuiv)

/* -----------------------------------------------------------------------
 * GL function wrappers (framebuffer objects)
 * --------------------------------------------------------------------- */
#define SoGLContext_glIsRenderbuffer               SOGL_ADD_PREFIX(SoGLContext_glIsRenderbuffer)
#define SoGLContext_glBindRenderbuffer             SOGL_ADD_PREFIX(SoGLContext_glBindRenderbuffer)
#define SoGLContext_glDeleteRenderbuffers          SOGL_ADD_PREFIX(SoGLContext_glDeleteRenderbuffers)
#define SoGLContext_glGenRenderbuffers             SOGL_ADD_PREFIX(SoGLContext_glGenRenderbuffers)
#define SoGLContext_glRenderbufferStorage          SOGL_ADD_PREFIX(SoGLContext_glRenderbufferStorage)
#define SoGLContext_glGetRenderbufferParameteriv   SOGL_ADD_PREFIX(SoGLContext_glGetRenderbufferParameteriv)
#define SoGLContext_glIsFramebuffer                SOGL_ADD_PREFIX(SoGLContext_glIsFramebuffer)
#define SoGLContext_glBindFramebuffer              SOGL_ADD_PREFIX(SoGLContext_glBindFramebuffer)
#define SoGLContext_glDeleteFramebuffers           SOGL_ADD_PREFIX(SoGLContext_glDeleteFramebuffers)
#define SoGLContext_glGenFramebuffers              SOGL_ADD_PREFIX(SoGLContext_glGenFramebuffers)
#define SoGLContext_glCheckFramebufferStatus       SOGL_ADD_PREFIX(SoGLContext_glCheckFramebufferStatus)
#define SoGLContext_glFramebufferTexture1D         SOGL_ADD_PREFIX(SoGLContext_glFramebufferTexture1D)
#define SoGLContext_glFramebufferTexture2D         SOGL_ADD_PREFIX(SoGLContext_glFramebufferTexture2D)
#define SoGLContext_glFramebufferTexture3D         SOGL_ADD_PREFIX(SoGLContext_glFramebufferTexture3D)
#define SoGLContext_glFramebufferRenderbuffer      SOGL_ADD_PREFIX(SoGLContext_glFramebufferRenderbuffer)
#define SoGLContext_glGetFramebufferAttachmentParameteriv SOGL_ADD_PREFIX(SoGLContext_glGetFramebufferAttachmentParameteriv)
#define SoGLContext_glGenerateMipmap               SOGL_ADD_PREFIX(SoGLContext_glGenerateMipmap)

/* -----------------------------------------------------------------------
 * GL function wrappers (core GL 1.0/1.1 — must never mix backends)
 * --------------------------------------------------------------------- */
#define SoGLContext_glTexImage2D                   SOGL_ADD_PREFIX(SoGLContext_glTexImage2D)
#define SoGLContext_glTexParameteri                SOGL_ADD_PREFIX(SoGLContext_glTexParameteri)
#define SoGLContext_glTexParameterf                SOGL_ADD_PREFIX(SoGLContext_glTexParameterf)
#define SoGLContext_glGetIntegerv                  SOGL_ADD_PREFIX(SoGLContext_glGetIntegerv)
#define SoGLContext_glGetFloatv                    SOGL_ADD_PREFIX(SoGLContext_glGetFloatv)
#define SoGLContext_glClearColor                   SOGL_ADD_PREFIX(SoGLContext_glClearColor)
#define SoGLContext_glClear                        SOGL_ADD_PREFIX(SoGLContext_glClear)
#define SoGLContext_glFlush                        SOGL_ADD_PREFIX(SoGLContext_glFlush)
#define SoGLContext_glEnable                       SOGL_ADD_PREFIX(SoGLContext_glEnable)
#define SoGLContext_glDisable                      SOGL_ADD_PREFIX(SoGLContext_glDisable)
#define SoGLContext_glPixelStorei                  SOGL_ADD_PREFIX(SoGLContext_glPixelStorei)
#define SoGLContext_glReadPixels                   SOGL_ADD_PREFIX(SoGLContext_glReadPixels)
#define SoGLContext_glCopyTexSubImage2D            SOGL_ADD_PREFIX(SoGLContext_glCopyTexSubImage2D)

#define SoGLContext_glBegin                        SOGL_ADD_PREFIX(SoGLContext_glBegin)
#define SoGLContext_glEnd                          SOGL_ADD_PREFIX(SoGLContext_glEnd)
#define SoGLContext_glVertex2f                     SOGL_ADD_PREFIX(SoGLContext_glVertex2f)
#define SoGLContext_glVertex2s                     SOGL_ADD_PREFIX(SoGLContext_glVertex2s)
#define SoGLContext_glVertex3f                     SOGL_ADD_PREFIX(SoGLContext_glVertex3f)
#define SoGLContext_glVertex3fv                    SOGL_ADD_PREFIX(SoGLContext_glVertex3fv)
#define SoGLContext_glVertex4fv                    SOGL_ADD_PREFIX(SoGLContext_glVertex4fv)
#define SoGLContext_glNormal3f                     SOGL_ADD_PREFIX(SoGLContext_glNormal3f)
#define SoGLContext_glNormal3fv                    SOGL_ADD_PREFIX(SoGLContext_glNormal3fv)
#define SoGLContext_glColor3f                      SOGL_ADD_PREFIX(SoGLContext_glColor3f)
#define SoGLContext_glColor3fv                     SOGL_ADD_PREFIX(SoGLContext_glColor3fv)
#define SoGLContext_glColor3ub                     SOGL_ADD_PREFIX(SoGLContext_glColor3ub)
#define SoGLContext_glColor3ubv                    SOGL_ADD_PREFIX(SoGLContext_glColor3ubv)
#define SoGLContext_glColor4ub                     SOGL_ADD_PREFIX(SoGLContext_glColor4ub)
#define SoGLContext_glTexCoord2f                   SOGL_ADD_PREFIX(SoGLContext_glTexCoord2f)
#define SoGLContext_glTexCoord2fv                  SOGL_ADD_PREFIX(SoGLContext_glTexCoord2fv)
#define SoGLContext_glTexCoord3f                   SOGL_ADD_PREFIX(SoGLContext_glTexCoord3f)
#define SoGLContext_glTexCoord3fv                  SOGL_ADD_PREFIX(SoGLContext_glTexCoord3fv)
#define SoGLContext_glTexCoord4fv                  SOGL_ADD_PREFIX(SoGLContext_glTexCoord4fv)
#define SoGLContext_glIndexi                       SOGL_ADD_PREFIX(SoGLContext_glIndexi)
#define SoGLContext_glMatrixMode                   SOGL_ADD_PREFIX(SoGLContext_glMatrixMode)
#define SoGLContext_glLoadIdentity                 SOGL_ADD_PREFIX(SoGLContext_glLoadIdentity)
#define SoGLContext_glLoadMatrixf                  SOGL_ADD_PREFIX(SoGLContext_glLoadMatrixf)
#define SoGLContext_glLoadMatrixd                  SOGL_ADD_PREFIX(SoGLContext_glLoadMatrixd)
#define SoGLContext_glMultMatrixf                  SOGL_ADD_PREFIX(SoGLContext_glMultMatrixf)
#define SoGLContext_glPushMatrix                   SOGL_ADD_PREFIX(SoGLContext_glPushMatrix)
#define SoGLContext_glPopMatrix                    SOGL_ADD_PREFIX(SoGLContext_glPopMatrix)
#define SoGLContext_glOrtho                        SOGL_ADD_PREFIX(SoGLContext_glOrtho)
#define SoGLContext_glFrustum                      SOGL_ADD_PREFIX(SoGLContext_glFrustum)
#define SoGLContext_glTranslatef                   SOGL_ADD_PREFIX(SoGLContext_glTranslatef)
#define SoGLContext_glRotatef                      SOGL_ADD_PREFIX(SoGLContext_glRotatef)
#define SoGLContext_glScalef                       SOGL_ADD_PREFIX(SoGLContext_glScalef)
#define SoGLContext_glLightf                       SOGL_ADD_PREFIX(SoGLContext_glLightf)
#define SoGLContext_glLightfv                      SOGL_ADD_PREFIX(SoGLContext_glLightfv)
#define SoGLContext_glLightModeli                  SOGL_ADD_PREFIX(SoGLContext_glLightModeli)
#define SoGLContext_glLightModelfv                 SOGL_ADD_PREFIX(SoGLContext_glLightModelfv)
#define SoGLContext_glMaterialf                    SOGL_ADD_PREFIX(SoGLContext_glMaterialf)
#define SoGLContext_glMaterialfv                   SOGL_ADD_PREFIX(SoGLContext_glMaterialfv)
#define SoGLContext_glColorMaterial                SOGL_ADD_PREFIX(SoGLContext_glColorMaterial)
#define SoGLContext_glFogi                         SOGL_ADD_PREFIX(SoGLContext_glFogi)
#define SoGLContext_glFogf                         SOGL_ADD_PREFIX(SoGLContext_glFogf)
#define SoGLContext_glFogfv                        SOGL_ADD_PREFIX(SoGLContext_glFogfv)
#define SoGLContext_glTexEnvi                      SOGL_ADD_PREFIX(SoGLContext_glTexEnvi)
#define SoGLContext_glTexEnvf                      SOGL_ADD_PREFIX(SoGLContext_glTexEnvf)
#define SoGLContext_glTexEnvfv                     SOGL_ADD_PREFIX(SoGLContext_glTexEnvfv)
#define SoGLContext_glTexGeni                      SOGL_ADD_PREFIX(SoGLContext_glTexGeni)
#define SoGLContext_glTexGenf                      SOGL_ADD_PREFIX(SoGLContext_glTexGenf)
#define SoGLContext_glTexGenfv                     SOGL_ADD_PREFIX(SoGLContext_glTexGenfv)
#define SoGLContext_glCopyTexImage2D               SOGL_ADD_PREFIX(SoGLContext_glCopyTexImage2D)
#define SoGLContext_glRasterPos2f                  SOGL_ADD_PREFIX(SoGLContext_glRasterPos2f)
#define SoGLContext_glRasterPos3f                  SOGL_ADD_PREFIX(SoGLContext_glRasterPos3f)
#define SoGLContext_glBitmap                       SOGL_ADD_PREFIX(SoGLContext_glBitmap)
#define SoGLContext_glDrawPixels                   SOGL_ADD_PREFIX(SoGLContext_glDrawPixels)
#define SoGLContext_glPixelTransferf               SOGL_ADD_PREFIX(SoGLContext_glPixelTransferf)
#define SoGLContext_glPixelTransferi               SOGL_ADD_PREFIX(SoGLContext_glPixelTransferi)
#define SoGLContext_glPixelMapfv                   SOGL_ADD_PREFIX(SoGLContext_glPixelMapfv)
#define SoGLContext_glPixelMapuiv                  SOGL_ADD_PREFIX(SoGLContext_glPixelMapuiv)
#define SoGLContext_glPixelZoom                    SOGL_ADD_PREFIX(SoGLContext_glPixelZoom)
#define SoGLContext_glViewport                     SOGL_ADD_PREFIX(SoGLContext_glViewport)
#define SoGLContext_glScissor                      SOGL_ADD_PREFIX(SoGLContext_glScissor)
#define SoGLContext_glDepthMask                    SOGL_ADD_PREFIX(SoGLContext_glDepthMask)
#define SoGLContext_glDepthFunc                    SOGL_ADD_PREFIX(SoGLContext_glDepthFunc)
#define SoGLContext_glDepthRange                   SOGL_ADD_PREFIX(SoGLContext_glDepthRange)
#define SoGLContext_glStencilFunc                  SOGL_ADD_PREFIX(SoGLContext_glStencilFunc)
#define SoGLContext_glStencilOp                    SOGL_ADD_PREFIX(SoGLContext_glStencilOp)
#define SoGLContext_glBlendFunc                    SOGL_ADD_PREFIX(SoGLContext_glBlendFunc)
#define SoGLContext_glAlphaFunc                    SOGL_ADD_PREFIX(SoGLContext_glAlphaFunc)
#define SoGLContext_glFrontFace                    SOGL_ADD_PREFIX(SoGLContext_glFrontFace)
#define SoGLContext_glCullFace                     SOGL_ADD_PREFIX(SoGLContext_glCullFace)
#define SoGLContext_glPolygonMode                  SOGL_ADD_PREFIX(SoGLContext_glPolygonMode)
#define SoGLContext_glPolygonStipple               SOGL_ADD_PREFIX(SoGLContext_glPolygonStipple)
#define SoGLContext_glLineWidth                    SOGL_ADD_PREFIX(SoGLContext_glLineWidth)
#define SoGLContext_glLineStipple                  SOGL_ADD_PREFIX(SoGLContext_glLineStipple)
#define SoGLContext_glPointSize                    SOGL_ADD_PREFIX(SoGLContext_glPointSize)
#define SoGLContext_glColorMask                    SOGL_ADD_PREFIX(SoGLContext_glColorMask)
#define SoGLContext_glClipPlane                    SOGL_ADD_PREFIX(SoGLContext_glClipPlane)
#define SoGLContext_glDrawBuffer                   SOGL_ADD_PREFIX(SoGLContext_glDrawBuffer)
#define SoGLContext_glClearIndex                   SOGL_ADD_PREFIX(SoGLContext_glClearIndex)
#define SoGLContext_glClearStencil                 SOGL_ADD_PREFIX(SoGLContext_glClearStencil)
#define SoGLContext_glAccum                        SOGL_ADD_PREFIX(SoGLContext_glAccum)
#define SoGLContext_glGetBooleanv                  SOGL_ADD_PREFIX(SoGLContext_glGetBooleanv)
#define SoGLContext_glNewList                      SOGL_ADD_PREFIX(SoGLContext_glNewList)
#define SoGLContext_glEndList                      SOGL_ADD_PREFIX(SoGLContext_glEndList)
#define SoGLContext_glCallList                     SOGL_ADD_PREFIX(SoGLContext_glCallList)
#define SoGLContext_glDeleteLists                  SOGL_ADD_PREFIX(SoGLContext_glDeleteLists)
#define SoGLContext_glPushAttrib                   SOGL_ADD_PREFIX(SoGLContext_glPushAttrib)
#define SoGLContext_glPopAttrib                    SOGL_ADD_PREFIX(SoGLContext_glPopAttrib)

/* -----------------------------------------------------------------------
 * GL context management helpers
 * --------------------------------------------------------------------- */
#define SoGLContext_glXGetCurrentDisplay           SOGL_ADD_PREFIX(SoGLContext_glXGetCurrentDisplay)
#define SoGLContext_context_max_dimensions         SOGL_ADD_PREFIX(SoGLContext_context_max_dimensions)
#define SoGLContext_context_create_offscreen       SOGL_ADD_PREFIX(SoGLContext_context_create_offscreen)
#define SoGLContext_context_make_current           SOGL_ADD_PREFIX(SoGLContext_context_make_current)
#define SoGLContext_context_reinstate_previous     SOGL_ADD_PREFIX(SoGLContext_context_reinstate_previous)
#define SoGLContext_context_destruct               SOGL_ADD_PREFIX(SoGLContext_context_destruct)
#define SoGLContext_context_bind_pbuffer           SOGL_ADD_PREFIX(SoGLContext_context_bind_pbuffer)
#define SoGLContext_context_release_pbuffer        SOGL_ADD_PREFIX(SoGLContext_context_release_pbuffer)
#define SoGLContext_context_pbuffer_is_bound       SOGL_ADD_PREFIX(SoGLContext_context_pbuffer_is_bound)
#define SoGLContext_context_can_render_to_texture  SOGL_ADD_PREFIX(SoGLContext_context_can_render_to_texture)
#define SoGLContext_win32_HDC                      SOGL_ADD_PREFIX(SoGLContext_win32_HDC)
#define SoGLContext_win32_updateHDCBitmap          SOGL_ADD_PREFIX(SoGLContext_win32_updateHDCBitmap)

/* -----------------------------------------------------------------------
 * coin_gl helper functions
 * --------------------------------------------------------------------- */
#define coin_gl_current_context                    SOGL_ADD_PREFIX(coin_gl_current_context)
#define coin_glerror_string                        SOGL_ADD_PREFIX(coin_glerror_string)
#define sogl_current_render_glue                   SOGL_ADD_PREFIX(sogl_current_render_glue)
#define sogl_set_current_render_glue               SOGL_ADD_PREFIX(sogl_set_current_render_glue)

#endif /* SOGL_PREFIX_SET */

#endif /* OBOL_SOGL_PREFIX_H */
