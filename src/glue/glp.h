#ifndef OBOL_GLUE_GLP_H
#define OBOL_GLUE_GLP_H

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

/* This header consolidates all OpenGL glue functionality that was previously
   exposed in public headers include/Inventor/C/glue/gl.h and src/C/glue/gl.h.
   
   The public API has been moved to internal implementation details to reduce 
   the public API footprint and eliminate code duplication. The functionality
   remains the same but is now accessible only to internal library code.
   
   External applications should use the high-level Coin3D APIs instead of
   directly accessing these low-level OpenGL wrapper functions. */

#include <string>

#ifndef OBOL_INTERNAL
#error this is a private header file
#endif /* ! OBOL_INTERNAL */

#include "config.h"

/* ********************************************************************** */

#include <Inventor/system/gl.h>
#include "base/dict.h"
#include <Inventor/misc/SoState.h>

/* ********************************************************************** */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Apply the optional function-name prefix (for dual-GL builds where both
   system-OpenGL and OSMesa variants are compiled into the same library).
   When SOGL_PREFIX_SET is defined (e.g. in gl_osmesa.cpp), each SoGLContext_*
   declaration below is #define-redirected to its prefixed counterpart
   (e.g. osmesa_SoGLContext_*) so the declarations match the definitions.
   In normal single-backend builds this header is a no-op. */
#include <Inventor/system/sogl_prefix.h>

#if 0 /* to get proper auto-indentation in emacs */
}
#endif /* emacs indentation */

/* Forward declaration of cc_libhandle */
typedef struct cc_libhandle_struct * cc_libhandle;

/* Under Win32, we need to make sure we use the correct calling method
   by using the APIENTRY define for the function signature types (or
   else we'll get weird stack errors). On other platforms, just define
   APIENTRY empty. */
#ifndef APIENTRY
#define APIENTRY
#endif /* !APIENTRY */

/* Our own typedefs for OpenGL functions. Prefixed with OBOL_ to avoid
   namespace collisions. */
typedef void (APIENTRY * OBOL_PFNGLTEXIMAGE3DPROC)(GLenum target,
                                                   GLint level,
                                                   GLenum internalformat,
                                                   GLsizei width,
                                                   GLsizei height,
                                                   GLsizei depth,
                                                   GLint border,
                                                   GLenum format,
                                                   GLenum type,
                                                   const GLvoid * pixels);

typedef void (APIENTRY * OBOL_PFNGLTEXSUBIMAGE3DPROC)(GLenum target,
                                                      GLint level,
                                                      GLint xoffset,
                                                      GLint yoffset,
                                                      GLint zoffset,
                                                      GLsizei width,
                                                      GLsizei height,
                                                      GLsizei depth,
                                                      GLenum format,
                                                      GLenum type,
                                                      const GLvoid * pixels);

typedef void (APIENTRY * OBOL_PFNGLCOPYTEXSUBIMAGE3DPROC)(GLenum target,
                                                          GLint level,
                                                          GLint xoffset,
                                                          GLint yoffset,
                                                          GLint zoffset,
                                                          GLint x,
                                                          GLint y,
                                                          GLsizei width,
                                                          GLsizei height);

/* Core GL 1.0/1.1 functions needed for proper dual-GL dispatch in
   SoSceneTexture2 and other nodes that must never mix backends. */
typedef void (APIENTRY * OBOL_PFNGLTEXIMAGE2DPROC)(GLenum target, GLint level,
                                                   GLint internalformat,
                                                   GLsizei width, GLsizei height,
                                                   GLint border, GLenum format,
                                                   GLenum type,
                                                   const GLvoid * pixels);
typedef void (APIENTRY * OBOL_PFNGLTEXPARAMETERIPROC)(GLenum target,
                                                      GLenum pname, GLint param);
typedef void (APIENTRY * OBOL_PFNGLTEXPARAMETERFPROC)(GLenum target,
                                                      GLenum pname,
                                                      GLfloat param);
typedef void (APIENTRY * OBOL_PFNGLGETINTEGERVPROC)(GLenum pname,
                                                    GLint * params);
typedef void (APIENTRY * OBOL_PFNGLGETFLOATVPROC)(GLenum pname,
                                                   GLfloat * params);
typedef void (APIENTRY * OBOL_PFNGLCLEARCOLORPROC)(GLclampf red,
                                                   GLclampf green,
                                                   GLclampf blue,
                                                   GLclampf alpha);
typedef void (APIENTRY * OBOL_PFNGLCLEARPROC)(GLbitfield mask);
typedef void (APIENTRY * OBOL_PFNGLFLUSHPROC)(void);
typedef void (APIENTRY * OBOL_PFNGLFINISHPROC)(void);
typedef GLenum (APIENTRY * OBOL_PFNGLGETERRORPROC)(void);
typedef const GLubyte * (APIENTRY * OBOL_PFNGLGETSTRINGPROC)(GLenum name);
typedef void (APIENTRY * OBOL_PFNGLENABLEPROC)(GLenum cap);
typedef void (APIENTRY * OBOL_PFNGLDISABLEPROC)(GLenum cap);
typedef GLboolean (APIENTRY * OBOL_PFNGLISENABLEDPROC)(GLenum cap);
typedef void (APIENTRY * OBOL_PFNGLPIXELSTOREIPROC)(GLenum pname, GLint param);
typedef void (APIENTRY * OBOL_PFNGLREADPIXELSPROC)(GLint x, GLint y,
                                                   GLsizei width, GLsizei height,
                                                   GLenum format, GLenum type,
                                                   GLvoid * pixels);
typedef void (APIENTRY * OBOL_PFNGLCOPYTEXSUBIMAGE2DPROC)(GLenum target,
                                                          GLint level,
                                                          GLint xoffset,
                                                          GLint yoffset,
                                                          GLint x, GLint y,
                                                          GLsizei width,
                                                          GLsizei height);

/* Additional core GL 1.0/1.1 dispatch function-pointer types needed
   for full dual-GL dispatch across all rendering code. */
typedef void (APIENTRY * OBOL_PFNGLBEGINPROC)(GLenum mode);
typedef void (APIENTRY * OBOL_PFNGLENDPROC)(void);
typedef void (APIENTRY * OBOL_PFNGLVERTEX2FPROC)(GLfloat x, GLfloat y);
typedef void (APIENTRY * OBOL_PFNGLVERTEX2SPROC)(GLshort x, GLshort y);
typedef void (APIENTRY * OBOL_PFNGLVERTEX3FPROC)(GLfloat x, GLfloat y, GLfloat z);
typedef void (APIENTRY * OBOL_PFNGLVERTEX3FVPROC)(const GLfloat * v);
typedef void (APIENTRY * OBOL_PFNGLVERTEX4FVPROC)(const GLfloat * v);
typedef void (APIENTRY * OBOL_PFNGLNORMAL3FPROC)(GLfloat nx, GLfloat ny, GLfloat nz);
typedef void (APIENTRY * OBOL_PFNGLNORMAL3FVPROC)(const GLfloat * v);
typedef void (APIENTRY * OBOL_PFNGLCOLOR3FPROC)(GLfloat r, GLfloat g, GLfloat b);
typedef void (APIENTRY * OBOL_PFNGLCOLOR3FVPROC)(const GLfloat * v);
typedef void (APIENTRY * OBOL_PFNGLCOLOR3UBPROC)(GLubyte r, GLubyte g, GLubyte b);
typedef void (APIENTRY * OBOL_PFNGLCOLOR3UBVPROC)(const GLubyte * v);
typedef void (APIENTRY * OBOL_PFNGLCOLOR4UBPROC)(GLubyte r, GLubyte g, GLubyte b, GLubyte a);
typedef void (APIENTRY * OBOL_PFNGLTEXCOORD2FPROC)(GLfloat s, GLfloat t);
typedef void (APIENTRY * OBOL_PFNGLTEXCOORD2FVPROC)(const GLfloat * v);
typedef void (APIENTRY * OBOL_PFNGLTEXCOORD3FPROC)(GLfloat s, GLfloat t, GLfloat r);
typedef void (APIENTRY * OBOL_PFNGLTEXCOORD3FVPROC)(const GLfloat * v);
typedef void (APIENTRY * OBOL_PFNGLTEXCOORD4FVPROC)(const GLfloat * v);
typedef void (APIENTRY * OBOL_PFNGLINDEXIPROC)(GLint c);
typedef void (APIENTRY * OBOL_PFNGLMATRIXMODEPROC)(GLenum mode);
typedef void (APIENTRY * OBOL_PFNGLLOADIDENTITYPROC)(void);
typedef void (APIENTRY * OBOL_PFNGLLOADMATRIXFPROC)(const GLfloat * m);
typedef void (APIENTRY * OBOL_PFNGLLOADMATRIXDPROC)(const GLdouble * m);
typedef void (APIENTRY * OBOL_PFNGLMULTMATRIXFPROC)(const GLfloat * m);
typedef void (APIENTRY * OBOL_PFNGLPUSHMATRIXPROC)(void);
typedef void (APIENTRY * OBOL_PFNGLPOPMATRIXPROC)(void);
typedef void (APIENTRY * OBOL_PFNGLORTHOPROC)(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val);
typedef void (APIENTRY * OBOL_PFNGLFRUSTUMPROC)(GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val);
typedef void (APIENTRY * OBOL_PFNGLTRANSLATEFPROC)(GLfloat x, GLfloat y, GLfloat z);
typedef void (APIENTRY * OBOL_PFNGLROTATEFPROC)(GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
typedef void (APIENTRY * OBOL_PFNGLSCALEFPROC)(GLfloat x, GLfloat y, GLfloat z);
typedef void (APIENTRY * OBOL_PFNGLLIGHTFPROC)(GLenum light, GLenum pname, GLfloat param);
typedef void (APIENTRY * OBOL_PFNGLLIGHTFVPROC)(GLenum light, GLenum pname, const GLfloat * params);
typedef void (APIENTRY * OBOL_PFNGLLIGHTMODELIPROC)(GLenum pname, GLint param);
typedef void (APIENTRY * OBOL_PFNGLLIGHTMODELFVPROC)(GLenum pname, const GLfloat * params);
typedef void (APIENTRY * OBOL_PFNGLMATERIALFPROC)(GLenum face, GLenum pname, GLfloat param);
typedef void (APIENTRY * OBOL_PFNGLMATERIALFVPROC)(GLenum face, GLenum pname, const GLfloat * params);
typedef void (APIENTRY * OBOL_PFNGLCOLORMATERIALPROC)(GLenum face, GLenum mode);
typedef void (APIENTRY * OBOL_PFNGLFOGLPROC)(GLenum pname, GLint param);
typedef void (APIENTRY * OBOL_PFNGLFOGFPROC)(GLenum pname, GLfloat param);
typedef void (APIENTRY * OBOL_PFNGLFOGFVPROC)(GLenum pname, const GLfloat * params);
typedef void (APIENTRY * OBOL_PFNGLTEXENVIPROC)(GLenum target, GLenum pname, GLint param);
typedef void (APIENTRY * OBOL_PFNGLTEXENVFPROC)(GLenum target, GLenum pname, GLfloat param);
typedef void (APIENTRY * OBOL_PFNGLTEXENVFVPROC)(GLenum target, GLenum pname, const GLfloat * params);
typedef void (APIENTRY * OBOL_PFNGLTEXGENIPROC)(GLenum coord, GLenum pname, GLint param);
typedef void (APIENTRY * OBOL_PFNGLTEXGENFPROC)(GLenum coord, GLenum pname, GLfloat param);
typedef void (APIENTRY * OBOL_PFNGLTEXGENFVPROC)(GLenum coord, GLenum pname, const GLfloat * params);
typedef void (APIENTRY * OBOL_PFNGLCOPYTEXIMAGE2DPROC)(GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
typedef void (APIENTRY * OBOL_PFNGLRASTERPOS2FPROC)(GLfloat x, GLfloat y);
typedef void (APIENTRY * OBOL_PFNGLRASTERPOS3FPROC)(GLfloat x, GLfloat y, GLfloat z);
typedef void (APIENTRY * OBOL_PFNGLBITMAPPROC)(GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte * bitmap);
typedef void (APIENTRY * OBOL_PFNGLDRAWPIXELSPROC)(GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid * pixels);
typedef void (APIENTRY * OBOL_PFNGLPIXELTRANSFERFPROC)(GLenum pname, GLfloat param);
typedef void (APIENTRY * OBOL_PFNGLPIXELTRANSFERIPROC)(GLenum pname, GLint param);
typedef void (APIENTRY * OBOL_PFNGLPIXELMAPFVPROC)(GLenum map, GLint mapsize, const GLfloat * values);
typedef void (APIENTRY * OBOL_PFNGLPIXELMAPUIVPROC)(GLenum map, GLint mapsize, const GLuint * values);
typedef void (APIENTRY * OBOL_PFNGLPIXELZOOMPROC)(GLfloat xfactor, GLfloat yfactor);
typedef void (APIENTRY * OBOL_PFNGLVIEWPORTPROC)(GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRY * OBOL_PFNGLSCISSORPROC)(GLint x, GLint y, GLsizei width, GLsizei height);
typedef void (APIENTRY * OBOL_PFNGLDEPTHMASKPROC)(GLboolean flag);
typedef void (APIENTRY * OBOL_PFNGLDEPTHFUNCPROC)(GLenum func);
typedef void (APIENTRY * OBOL_PFNGLDEPTHRANGEPROC)(GLclampd near_val, GLclampd far_val);
typedef void (APIENTRY * OBOL_PFNGLSTENCILFUNCPROC)(GLenum func, GLint ref, GLuint mask);
typedef void (APIENTRY * OBOL_PFNGLSTENCILOPPROC)(GLenum fail, GLenum zfail, GLenum zpass);
typedef void (APIENTRY * OBOL_PFNGLBLENDFUNCPROC)(GLenum sfactor, GLenum dfactor);
typedef void (APIENTRY * OBOL_PFNGLALPHAFUNCPROC)(GLenum func, GLclampf ref);
typedef void (APIENTRY * OBOL_PFNGLFRONTFACEPROC)(GLenum mode);
typedef void (APIENTRY * OBOL_PFNGLCULLFACEPROC)(GLenum mode);
typedef void (APIENTRY * OBOL_PFNGLPOLYGONMODEPROC)(GLenum face, GLenum mode);
typedef void (APIENTRY * OBOL_PFNGLPOLYGONSTIPPLEPROC)(const GLubyte * mask);
typedef void (APIENTRY * OBOL_PFNGLLINEWIDTHPROC)(GLfloat width);
typedef void (APIENTRY * OBOL_PFNGLLINESTIPPLEPROC)(GLint factor, GLushort pattern);
typedef void (APIENTRY * OBOL_PFNGLPOINTSIZEPROC)(GLfloat size);
typedef void (APIENTRY * OBOL_PFNGLCOLORMASKPROC)(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
typedef void (APIENTRY * OBOL_PFNGLCLIPPLANEPROC)(GLenum plane, const GLdouble * equation);
typedef void (APIENTRY * OBOL_PFNGLDRAWBUFFERPROC)(GLenum mode);
typedef void (APIENTRY * OBOL_PFNGLCLEARINDEXPROC)(GLfloat c);
typedef void (APIENTRY * OBOL_PFNGLCLEARSTENCILPROC)(GLint s);
typedef void (APIENTRY * OBOL_PFNGLACCUMPROC)(GLenum op, GLfloat value);
typedef void (APIENTRY * OBOL_PFNGLGETBOOLEANVPROC)(GLenum pname, GLboolean * params);
typedef void (APIENTRY * OBOL_PFNGLNEWLISTPROC)(GLuint list, GLenum mode);
typedef void (APIENTRY * OBOL_PFNGLENDLISTPROC)(void);
typedef void (APIENTRY * OBOL_PFNGLCALLLISTPROC)(GLuint list);
typedef void (APIENTRY * OBOL_PFNGLDELETELISTSPROC)(GLuint list, GLsizei range);
typedef GLuint (APIENTRY * OBOL_PFNGLGENLISTSPROC)(GLsizei range);
typedef void (APIENTRY * OBOL_PFNGLPUSHATTRIBPROC)(GLbitfield mask);
typedef void (APIENTRY * OBOL_PFNGLPOPATTRIBPROC)(void);

typedef void (APIENTRY * OBOL_PFNGLPOLYGONOFFSETPROC)(GLfloat factor,
                                                      GLfloat bias);

typedef void (APIENTRY * OBOL_PFNGLBINDTEXTUREPROC)(GLenum target,
                                                    GLuint texture);

typedef void (APIENTRY * OBOL_PFNGLDELETETEXTURESPROC)(GLsizei n,
                                                       const GLuint * textures);

typedef void (APIENTRY * OBOL_PFNGLGENTEXTURESPROC)(GLsizei n,
                                                    GLuint *textures);

typedef void (APIENTRY * OBOL_PFNGLTEXSUBIMAGE2DPROC)(GLenum target,
                                                      GLint level,
                                                      GLint xoffset,
                                                      GLint yoffset,
                                                      GLsizei width,
                                                      GLsizei height,
                                                      GLenum format,
                                                      GLenum type,
                                                      const GLvoid * pixels);

typedef void (APIENTRY * OBOL_PFNGLACTIVETEXTUREPROC)(GLenum texture);
typedef void (APIENTRY * OBOL_PFNGLCLIENTACTIVETEXTUREPROC)(GLenum texture);
typedef void (APIENTRY * OBOL_PFNGLMULTITEXCOORD2FPROC)(GLenum target,
                                                        GLfloat s,
                                                        GLfloat t);
typedef void (APIENTRY * OBOL_PFNGLMULTITEXCOORD2FVPROC)(GLenum target,
                                                         const GLfloat * v);
typedef void (APIENTRY * OBOL_PFNGLMULTITEXCOORD3FVPROC)(GLenum target,
                                                         const GLfloat * v);
typedef void (APIENTRY * OBOL_PFNGLMULTITEXCOORD4FVPROC)(GLenum target,
                                                         const GLfloat * v);

typedef void (APIENTRY * OBOL_PFNGLPUSHCLIENTATTRIBPROC)(GLbitfield mask);
typedef void (APIENTRY * OBOL_PFNGLPOPCLIENTATTRIBPROC)(void);

/* typedefs for texture compression */
typedef void (APIENTRY * OBOL_PFNGLCOMPRESSEDTEXIMAGE3DPROC)(GLenum target,
                                                             GLint level,
                                                             GLenum internalformat,
                                                             GLsizei width,
                                                             GLsizei height,
                                                             GLsizei depth,
                                                             GLint border,
                                                             GLsizei imageSize,
                                                             const GLvoid * data);
typedef void (APIENTRY * OBOL_PFNGLCOMPRESSEDTEXIMAGE2DPROC)(GLenum target,
                                                             GLint level,
                                                             GLenum internalformat,
                                                             GLsizei width,
                                                             GLsizei height,
                                                             GLint border,
                                                             GLsizei imageSize,
                                                             const GLvoid * data);
typedef void (APIENTRY * OBOL_PFNGLCOMPRESSEDTEXIMAGE1DPROC)(GLenum target,
                                                             GLint level,
                                                             GLenum internalformat,
                                                             GLsizei width,
                                                             GLint border,
                                                             GLsizei imageSize,
                                                             const GLvoid * data);
typedef void (APIENTRY * OBOL_PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC)(GLenum target,
                                                                GLint level,
                                                                GLint xoffset,
                                                                GLint yoffset,
                                                                GLint zoffset,
                                                                GLsizei width,
                                                                GLsizei height,
                                                                GLsizei depth,
                                                                GLenum format,
                                                                GLsizei imageSize,
                                                                const GLvoid * data);
typedef void (APIENTRY * OBOL_PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC)(GLenum target,
                                                                GLint level,
                                                                GLint xoffset,
                                                                GLint yoffset,
                                                                GLsizei width,
                                                                GLsizei height,
                                                                GLenum format,
                                                                GLsizei imageSize,
                                                                const GLvoid * data);
typedef void (APIENTRY * OBOL_PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC)(GLenum target,
                                                                GLint level,
                                                                GLint xoffset,
                                                                GLsizei width,
                                                                GLenum format,
                                                                GLsizei imageSize,
                                                                const GLvoid *data);
typedef void (APIENTRY * OBOL_PFNGLGETCOMPRESSEDTEXIMAGEPROC)(GLenum target,
                                                              GLint level,
                                                              void * img);


/* typedefs for palette tetures */
typedef void (APIENTRY * OBOL_PFNGLCOLORTABLEPROC)(GLenum target,
                                                   GLenum internalFormat,
                                                   GLsizei width,
                                                   GLenum format,
                                                   GLenum type,
                                                   const GLvoid * table);
typedef void (APIENTRY * OBOL_PFNGLCOLORSUBTABLEPROC)(GLenum target,
                                                      GLsizei start,
                                                      GLsizei count,
                                                      GLenum format,
                                                      GLenum type,
                                                      const GLvoid * data);
typedef void (APIENTRY * OBOL_PFNGLGETCOLORTABLEPROC)(GLenum target,
                                                      GLenum format,
                                                      GLenum type,
                                                      GLvoid * data);
typedef void (APIENTRY * OBOL_PFNGLGETCOLORTABLEPARAMETERIVPROC)(GLenum target,
                                                                 GLenum pname,
                                                                 GLint *params);
typedef void (APIENTRY * OBOL_PFNGLGETCOLORTABLEPARAMETERFVPROC)(GLenum target,
                                                                 GLenum pname,
                                                                 GLfloat * params);

/* Typedefs for glBlendEquation[EXT]. */
typedef void *(APIENTRY * OBOL_PFNGLBLENDEQUATIONPROC)(GLenum);

/* Typedef for glBlendFuncSeparate */
typedef void *(APIENTRY * OBOL_PFNGLBLENDFUNCSEPARATEPROC)(GLenum, GLenum, GLenum, GLenum);

/* typedefs for OpenGL vertex arrays */
typedef void (APIENTRY * OBOL_PFNGLVERTEXPOINTERPROC)(GLint size, GLenum type, GLsizei stride, const GLvoid * pointer);
typedef void (APIENTRY * OBOL_PFNGLTEXCOORDPOINTERPROC)(GLint size, GLenum type, GLsizei stride, const GLvoid * pointer);
typedef void (APIENTRY * OBOL_PFNGLNORMALPOINTERPROC)(GLenum type, GLsizei stride, const GLvoid *pointer);
typedef void (APIENTRY * OBOL_PNFGLCOLORPOINTERPROC)(GLint size, GLenum type, GLsizei stride, const GLvoid * pointer);
typedef void (APIENTRY * OBOL_PFNGLINDEXPOINTERPROC)(GLenum type, GLsizei stride, const GLvoid * pointer);
typedef void (APIENTRY * OBOL_PFNGLENABLECLIENTSTATEPROC)(GLenum array);
typedef void (APIENTRY * OBOL_PFNGLDISABLECLIENTSTATEPROC)(GLenum array);
typedef void (APIENTRY * OBOL_PFNGLINTERLEAVEDARRAYSPROC)(GLenum format, GLsizei stride, const GLvoid * pointer);
typedef void (APIENTRY * OBOL_PFNGLDRAWARRAYSPROC)(GLenum mode, GLint first, GLsizei count);
typedef void (APIENTRY * OBOL_PFNGLDRAWELEMENTSPROC)(GLenum mode, GLsizei count, GLenum type, const GLvoid * indices);
typedef void (APIENTRY * OBOL_PFNGLDRAWRANGEELEMENTSPROC)(GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid * indices);
typedef void (APIENTRY * OBOL_PFNGLARRAYELEMENTPROC)(GLint i);

typedef void (APIENTRY * OBOL_PFNGLMULTIDRAWARRAYSPROC)(GLenum mode, const GLint * first,
                                                        const GLsizei * count, GLsizei primcount);
typedef void (APIENTRY * OBOL_PFNGLMULTIDRAWELEMENTSPROC)(GLenum mode, const GLsizei * count,
                                                          GLenum type, const GLvoid ** indices, GLsizei primcount);

/* Typedefs for NV_vertex_array_range */
typedef void (APIENTRY * OBOL_PFNGLFLUSHVERTEXARRAYRANGENVPROC)(void);
typedef void (APIENTRY * OBOL_PFNGLVERTEXARRAYRANGENVPROC)(GLsizei size, const GLvoid * pointer);
typedef void * (APIENTRY * OBOL_PFNGLALLOCATEMEMORYNVPROC)(GLsizei size, GLfloat readfreq,
                                                           GLfloat writefreq, GLfloat priority);
typedef void (APIENTRY * OBOL_PFNGLFREEMEMORYNVPROC)(GLvoid * buffer);


/* typedefs for GL_ARB_vertex_buffer_object */
typedef void (APIENTRY * OBOL_PFNGLBINDBUFFERPROC)(GLenum target, GLuint buffer);
typedef void (APIENTRY * OBOL_PFNGLDELETEBUFFERSPROC)(GLsizei n, const GLuint * buffers);
typedef void (APIENTRY * OBOL_PFNGLGENBUFFERSPROC)(GLsizei n, GLuint *buffers);
typedef GLboolean (APIENTRY * OBOL_PFNGLISBUFFERPROC)(GLuint buffer);
typedef void (APIENTRY * OBOL_PFNGLBUFFERDATAPROC)(GLenum target,
                                                   intptr_t size, /* 64 bit on 64 bit systems */
                                                   const GLvoid *data,
                                                   GLenum usage);
typedef void (APIENTRY * OBOL_PFNGLBUFFERSUBDATAPROC)(GLenum target,
                                                      intptr_t offset, /* 64 bit */
                                                      intptr_t size, /* 64 bit */
                                                      const GLvoid * data);
typedef void (APIENTRY * OBOL_PFNGLGETBUFFERSUBDATAPROC)(GLenum target,
                                                         intptr_t offset, /* 64 bit */
                                                         intptr_t size, /* 64 bit */
                                                         GLvoid *data);
typedef GLvoid * (APIENTRY * OBOL_PNFGLMAPBUFFERPROC)(GLenum target, GLenum access);
typedef GLboolean (APIENTRY * OBOL_PFNGLUNMAPBUFFERPROC)(GLenum target);
typedef void (APIENTRY * OBOL_PFNGLGETBUFFERPARAMETERIVPROC)(GLenum target,
                                                             GLenum pname,
                                                             GLint * params);
typedef void (APIENTRY * OBOL_PFNGLGETBUFFERPOINTERVPROC)(GLenum target,
                                                          GLenum pname,
                                                          GLvoid ** params);

/* Typedefs for GL_NV_register_combiners */
typedef void (APIENTRY * OBOL_PFNGLCOMBINERPARAMETERFVNVPROC)(GLenum pname,
                                                              const GLfloat *params);
typedef void (APIENTRY * OBOL_PFNGLCOMBINERPARAMETERIVNVPROC)(GLenum pname,
                                                              const GLint *params);
typedef void (APIENTRY * OBOL_PFNGLCOMBINERPARAMETERFNVPROC)(GLenum pname,
                                                             GLfloat param);
typedef void (APIENTRY * OBOL_PFNGLCOMBINERPARAMETERINVPROC)(GLenum pname,
                                                            GLint param);
typedef void (APIENTRY * OBOL_PFNGLCOMBINERINPUTNVPROC)(GLenum stage,
                                                        GLenum portion,
                                                        GLenum variable,
                                                        GLenum input,
                                                        GLenum mapping,
                                                        GLenum componentUsage);
typedef void (APIENTRY * OBOL_PFNGLCOMBINEROUTPUTNVPROC)(GLenum stage,
                                                         GLenum portion,
                                                         GLenum abOutput,
                                                         GLenum cdOutput,
                                                         GLenum sumOutput,
                                                         GLenum scale,
                                                         GLenum bias,
                                                         GLboolean abDotProduct,
                                                         GLboolean cdDotProduct,
                                                         GLboolean muxSum);
typedef void (APIENTRY * OBOL_PFNGLFINALCOMBINERINPUTNVPROC)(GLenum variable,
                                                             GLenum input,
                                                             GLenum mapping,
                                                             GLenum componentUsage);
typedef void (APIENTRY * OBOL_PFNGLGETCOMBINERINPUTPARAMETERFVNVPROC)(GLenum stage,
                                                                      GLenum portion,
                                                                      GLenum variable,
                                                                      GLenum pname,
                                                                      GLfloat *params);
typedef void (APIENTRY * OBOL_PFNGLGETCOMBINERINPUTPARAMETERIVNVPROC)(GLenum stage,
                                                                      GLenum portion,
                                                                      GLenum variable,
                                                                      GLenum pname,
                                                                      GLint *params);
typedef void (APIENTRY * OBOL_PFNGLGETCOMBINEROUTPUTPARAMETERFVNVPROC)(GLenum stage,
                                                                       GLenum portion,
                                                                       GLenum pname,
                                                                       GLfloat *params);
typedef void (APIENTRY * OBOL_PFNGLGETCOMBINEROUTPUTPARAMETERIVNVPROC)(GLenum stage,
                                                                       GLenum portion,
                                                                       GLenum pname,
                                                                       GLint *params);
typedef void (APIENTRY * OBOL_PFNGLGETFINALCOMBINERINPUTPARAMETERFVNVPROC)(GLenum variable,
                                                                           GLenum pname,
                                                                           GLfloat *params);
typedef void (APIENTRY * OBOL_PFNGLGETFINALCOMBINERINPUTPARAMETERIVNVPROC)(GLenum variable,
                                                                           GLenum pname,
                                                                           GLint *params);
/* Typedefs for GL_ARB_fragment_program */
typedef void (APIENTRY * OBOL_PFNGLPROGRAMSTRINGARBPROC)(GLenum target,
                                                         GLenum format,
                                                         GLsizei len,
                                                         const GLvoid *string);

typedef void (APIENTRY * OBOL_PFNGLBINDPROGRAMARBPROC)(GLenum target,
                                                       GLuint program);

typedef void (APIENTRY * OBOL_PFNGLDELETEPROGRAMSARBPROC)(GLsizei n,
                                                          const GLuint *programs);

typedef void (APIENTRY * OBOL_PFNGLGENPROGRAMSARBPROC)(GLsizei n,
                                                       GLuint *programs);

typedef void (APIENTRY * OBOL_PFNGLPROGRAMENVPARAMETER4DARBPROC)(GLenum target,
                                                                 GLuint index,
                                                                 GLdouble x,
                                                                 GLdouble y,
                                                                 GLdouble z,
                                                                 GLdouble w);
typedef void (APIENTRY * OBOL_PFNGLPROGRAMENVPARAMETER4DVARBPROC)(GLenum target,
                                                                  GLuint index,
                                                                  const GLdouble *params);
typedef void (APIENTRY * OBOL_PFNGLPROGRAMENVPARAMETER4FARBPROC)(GLenum target,
                                                                 GLuint index,
                                                                 GLfloat x,
                                                                 GLfloat y,
                                                                 GLfloat z,
                                                                 GLfloat w);
typedef void (APIENTRY * OBOL_PFNGLPROGRAMENVPARAMETER4FVARBPROC)(GLenum target,
                                                                  GLuint index,
                                                                  const GLfloat *params);
typedef void (APIENTRY * OBOL_PFNGLPROGRAMLOCALPARAMETER4DARBPROC)(GLenum target,
                                                                   GLuint index,
                                                                   GLdouble x,
                                                                   GLdouble y,
                                                                   GLdouble z,
                                                                   GLdouble w);
typedef void (APIENTRY * OBOL_PFNGLPROGRAMLOCALPARAMETER4DVARBPROC)(GLenum target,
                                                                    GLuint index,
                                                                    const GLdouble *params);
typedef void (APIENTRY * OBOL_PFNGLPROGRAMLOCALPARAMETER4FARBPROC)(GLenum target,
                                                                   GLuint index,
                                                                   GLfloat x,
                                                                   GLfloat y,
                                                                   GLfloat z,
                                                                   GLfloat w);
typedef void (APIENTRY * OBOL_PFNGLPROGRAMLOCALPARAMETER4FVARBPROC)(GLenum target,
                                                                    GLuint index,
                                                                    const GLfloat *params);
typedef void (APIENTRY * OBOL_PFNGLGETPROGRAMENVPARAMETERDVARBPROC)(GLenum target,
                                                                    GLuint index,
                                                                    GLdouble *params);
typedef void (APIENTRY * OBOL_PFNGLGETPROGRAMENVPARAMETERFVARBPROC)(GLenum target,
                                                                    GLuint index,
                                                                    GLfloat *params);
typedef void (APIENTRY * OBOL_PFNGLGETPROGRAMLOCALPARAMETERDVARBPROC)(GLenum target,
                                                                      GLuint index,
                                                                      GLdouble *params);
typedef void (APIENTRY * OBOL_PFNGLGETPROGRAMLOCALPARAMETERFVARBPROC)(GLenum target,
                                                                      GLuint index,
                                                                      GLfloat *params);
typedef void (APIENTRY * OBOL_PFNGLGETPROGRAMIVARBPROC)(GLenum target,
                                                        GLenum pname,
                                                        GLint *params);
typedef void (APIENTRY * OBOL_PFNGLGETPROGRAMSTRINGARBPROC)(GLenum target,
                                                            GLenum pname,
                                                            GLvoid *string);
typedef GLboolean (APIENTRY * OBOL_PFNGLISPROGRAMARBPROC)(GLuint program);


/* Typedefs for GL_ARB_vertex_program */
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB1SARBPROC)(GLuint index, GLshort x);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB1FARBPROC)(GLuint index, GLfloat x);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB1DARBPROC)(GLuint index, GLdouble x);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB2SARBPROC)(GLuint index, GLshort x, GLshort y);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB2FARBPROC)(GLuint index, GLfloat x, GLfloat y);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB2DARBPROC)(GLuint index, GLdouble x, GLdouble y);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB3SARBPROC)(GLuint index, GLshort x,
                                                          GLshort y, GLshort z);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB3FARBPROC)(GLuint index, GLfloat x,
                                                          GLfloat y, GLfloat z);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB3DARBPROC)(GLuint index, GLdouble x,
                                                          GLdouble y, GLdouble z);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB4SARBPROC)(GLuint index, GLshort x,
                                                          GLshort y, GLshort z, GLshort w);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB4FARBPROC)(GLuint index, GLfloat x,
                                                          GLfloat y, GLfloat z, GLfloat w);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB4DARBPROC)(GLuint index, GLdouble x,
                                                          GLdouble y, GLdouble z, GLdouble w);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB4NUBARBPROC)(GLuint index, GLubyte x,
                                                            GLubyte y, GLubyte z, GLubyte w);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB1SVARBPROC)(GLuint index, const GLshort *v);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB1FVARBPROC)(GLuint index, const GLfloat *v);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB1DVARBPROC)(GLuint index, const GLdouble *v);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB2SVARBPROC)(GLuint index, const GLshort *v);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB2FVARBPROC)(GLuint index, const GLfloat *v);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB2DVARBPROC)(GLuint index, const GLdouble *v);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB3SVARBPROC)(GLuint index, const GLshort *v);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB3FVARBPROC)(GLuint index, const GLfloat *v);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB3DVARBPROC)(GLuint index, const GLdouble *v);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB4BVARBPROC)(GLuint index, const GLbyte *v);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB4SVARBPROC)(GLuint index, const GLshort *v);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB4IVARBPROC)(GLuint index, const GLint *v);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB4UBVARBPROC)(GLuint index, const GLubyte *v);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB4USVARBPROC)(GLuint index, const GLushort *v);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB4UIVARBPROC)(GLuint index, const GLuint *v);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB4FVARBPROC)(GLuint index, const GLfloat *v);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB4DVARBPROC)(GLuint index, const GLdouble *v);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB4NBVARBPROC)(GLuint index, const GLbyte *v);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB4NSVARBPROC)(GLuint index, const GLshort *v);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB4NIVARBPROC)(GLuint index, const GLint *v);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB4NUBVARBPROC)(GLuint index, const GLubyte *v);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB4NUSVARBPROC)(GLuint index, const GLushort *v);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIB4NUIVARBPROC)(GLuint index, const GLuint *v);
typedef void (APIENTRY * OBOL_PFNGLVERTEXATTRIBPOINTERARBPROC)(GLuint index, GLint size,
                                                               GLenum type, GLboolean normalized,
                                                               GLsizei stride,
                                                               const GLvoid *pointer);
typedef void (APIENTRY * OBOL_PFNGLENABLEVERTEXATTRIBARRAYARBPROC)(GLuint index);
typedef void (APIENTRY * OBOL_PFNGLDISABLEVERTEXATTRIBARRAYARBPROC)(GLuint index);
typedef void (APIENTRY * OBOL_PFNGLGETVERTEXATTRIBDVARBPROC)(GLuint index, GLenum pname,
                                                             GLdouble *params);
typedef void (APIENTRY * OBOL_PFNGLGETVERTEXATTRIBFVARBPROC)(GLuint index, GLenum pname,
                                                             GLfloat *params);
typedef void (APIENTRY * OBOL_PFNGLGETVERTEXATTRIBIVARBPROC)(GLuint index, GLenum pname,
                                                             GLint *params);
typedef void (APIENTRY * OBOL_PFNGLGETVERTEXATTRIBPOINTERVARBPROC)(GLuint index, GLenum pname,
                                                                   GLvoid **pointer);

/* FIXME: according to the GL_ARB_shader_objects doc, these types must
   be at least 8 bits wide and 32 bits wide, respectively. Apart from
   that, there does not seem to be any other limitations on them, so
   these types may not match the actual types used on the platform
   (these were taken from NVIDIA's glext.h for their 32-bit Linux
   drivers). How should this be properly handled? Is there any way at
   all one could possibly pick up these at the correct size in a
   dynamic manner? 20050124 mortene. */
typedef char OBOL_GLchar;
typedef unsigned long OBOL_GLhandle;

/* Typedefs for GL_ARB_vertex_shader */
typedef void (APIENTRY * OBOL_PFNGLBINDATTRIBLOCATIONARBPROC)(OBOL_GLhandle programobj, GLuint index, OBOL_GLchar * name);
typedef int (APIENTRY * OBOL_PFNGLGETATTRIBLOCATIONARBPROC)(OBOL_GLhandle programobj, const OBOL_GLchar * name);
typedef void (APIENTRY * OBOL_PFNGLGETACTIVEATTRIBARBPROC)(OBOL_GLhandle programobj, GLuint index, GLsizei bufSize, GLsizei * length, GLint * size, GLenum * type, OBOL_GLchar * name);


/* Typedefs for shader objects -- GL_ARB_shader_objects */
typedef void (APIENTRY * OBOL_PFNGLPROGRAMPARAMETERIEXT)(OBOL_GLhandle, GLenum, GLenum);

typedef int (APIENTRY * OBOL_PFNGLGETUNIFORMLOCATIONARBPROC)(OBOL_GLhandle,
                                                             const OBOL_GLchar *);
typedef void (APIENTRY * OBOL_PFNGLGETACTIVEUNIFORMARBPROC)(OBOL_GLhandle,
                                                            GLuint index,
                                                            GLsizei maxLength,
                                                            GLsizei * length,
                                                            GLint * size,
                                                            GLenum * type,
                                                            OBOL_GLchar * name);
typedef void (APIENTRY * OBOL_PFNGLUNIFORM1FARBPROC)(GLint location, GLfloat v0);
typedef void (APIENTRY * OBOL_PFNGLUNIFORM2FARBPROC)(GLint location, GLfloat v0, GLfloat v1);
typedef void (APIENTRY * OBOL_PFNGLUNIFORM3FARBPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2);
typedef void (APIENTRY * OBOL_PFNGLUNIFORM4FARBPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef OBOL_GLhandle (APIENTRY * OBOL_PFNGLCREATESHADEROBJECTARBPROC)(GLenum);
typedef void (APIENTRY * OBOL_PFNGLSHADERSOURCEARBPROC)(OBOL_GLhandle, GLsizei, const OBOL_GLchar **, const GLint *);
typedef void (APIENTRY * OBOL_PFNGLCOMPILESHADERARBPROC)(OBOL_GLhandle);
typedef void (APIENTRY * OBOL_PFNGLGETOBJECTPARAMETERIVARBPROC)(OBOL_GLhandle, GLenum, GLint *);
typedef void (APIENTRY * OBOL_PFNGLDELETEOBJECTARBPROC)(OBOL_GLhandle);
typedef void (APIENTRY * OBOL_PFNGLATTACHOBJECTARBPROC)(OBOL_GLhandle, OBOL_GLhandle);
typedef void (APIENTRY * OBOL_PFNGLDETACHOBJECTARBPROC)(OBOL_GLhandle, OBOL_GLhandle);
typedef void (APIENTRY * OBOL_PFNGLGETINFOLOGARBPROC)(OBOL_GLhandle, GLsizei, GLsizei *, OBOL_GLchar *);
typedef void (APIENTRY * OBOL_PFNGLLINKPROGRAMARBPROC)(OBOL_GLhandle);
typedef void (APIENTRY * OBOL_PFNGLUSEPROGRAMOBJECTARBPROC)(OBOL_GLhandle);
typedef OBOL_GLhandle (APIENTRY * OBOL_PFNGLCREATEPROGRAMOBJECTARBPROC)(void);
typedef void (APIENTRY * OBOL_PFNGLUNIFORM1FVARBPROC)(OBOL_GLhandle, GLsizei, const GLfloat *);
typedef void (APIENTRY * OBOL_PFNGLUNIFORM2FVARBPROC)(OBOL_GLhandle, GLsizei, const GLfloat *);
typedef void (APIENTRY * OBOL_PFNGLUNIFORM3FVARBPROC)(OBOL_GLhandle, GLsizei, const GLfloat *);
typedef void (APIENTRY * OBOL_PFNGLUNIFORM4FVARBPROC)(OBOL_GLhandle, GLsizei, const GLfloat *);
typedef void (APIENTRY * OBOL_PFNGLUNIFORM1IARBPROC)(OBOL_GLhandle, const GLint);
typedef void (APIENTRY * OBOL_PFNGLUNIFORM2IARBPROC)(OBOL_GLhandle, const GLint, GLint);
typedef void (APIENTRY * OBOL_PFNGLUNIFORM3IARBPROC)(OBOL_GLhandle, const GLint, GLint, GLint);
typedef void (APIENTRY * OBOL_PFNGLUNIFORM4IARBPROC)(OBOL_GLhandle, const GLint, GLint, GLint, GLint);
typedef void (APIENTRY * OBOL_PFNGLUNIFORM1IVARBPROC)(OBOL_GLhandle, GLsizei, const GLint *);
typedef void (APIENTRY * OBOL_PFNGLUNIFORM2IVARBPROC)(OBOL_GLhandle, GLsizei, const GLint *);
typedef void (APIENTRY * OBOL_PFNGLUNIFORM3IVARBPROC)(OBOL_GLhandle, GLsizei, const GLint *);
typedef void (APIENTRY * OBOL_PFNGLUNIFORM4IVARBPROC)(OBOL_GLhandle, GLsizei, const GLint *);

typedef void (APIENTRY * OBOL_PFNGLUNIFORMMATRIX2FVARBPROC)(OBOL_GLhandle, GLsizei, GLboolean, const GLfloat *);
typedef void (APIENTRY * OBOL_PFNGLUNIFORMMATRIX3FVARBPROC)(OBOL_GLhandle, GLsizei, GLboolean, const GLfloat *);
typedef void (APIENTRY * OBOL_PFNGLUNIFORMMATRIX4FVARBPROC)(OBOL_GLhandle, GLsizei, GLboolean, const GLfloat *);


/* Typedefs for occlusion queries -- GL_ARB_occlusion_query */

typedef void (APIENTRY * OBOL_PFNGLGENQUERIESPROC)(GLsizei n, GLuint * ids);
typedef void (APIENTRY * OBOL_PFNGLDELETEQUERIESPROC)(GLsizei n, const GLuint *ids);
typedef GLboolean (APIENTRY * OBOL_PFNGLISQUERYPROC)(GLuint id);
typedef void (APIENTRY * OBOL_PFNGLBEGINQUERYPROC)(GLenum target, GLuint id);
typedef void (APIENTRY * OBOL_PFNGLENDQUERYPROC)(GLenum target);
typedef void (APIENTRY * OBOL_PFNGLGETQUERYIVPROC)(GLenum target, GLenum pname, GLint * params);
typedef void (APIENTRY * OBOL_PFNGLGETQUERYOBJECTIVPROC)(GLuint id, GLenum pname, GLint * params);
typedef void (APIENTRY * OBOL_PFNGLGETQUERYOBJECTUIVPROC)(GLuint id, GLenum pname, GLuint * params);

/* Typedefs for Framebuffer objects */

typedef void (APIENTRY * OBOL_PFNGLISRENDERBUFFERPROC)(GLuint renderbuffer);
typedef void (APIENTRY * OBOL_PFNGLBINDRENDERBUFFERPROC)(GLenum target, GLuint renderbuffer);
typedef void (APIENTRY * OBOL_PFNGLDELETERENDERBUFFERSPROC)(GLsizei n, const GLuint *renderbuffers);
typedef void (APIENTRY * OBOL_PFNGLGENRENDERBUFFERSPROC)(GLsizei n, GLuint *renderbuffers);
typedef void (APIENTRY * OBOL_PFNGLRENDERBUFFERSTORAGEPROC)(GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
typedef void (APIENTRY * OBOL_PFNGLGETRENDERBUFFERPARAMETERIVPROC)(GLenum target, GLenum pname, GLint *params);
typedef GLboolean (APIENTRY * OBOL_PFNGLISFRAMEBUFFERPROC)(GLuint framebuffer);
typedef void (APIENTRY * OBOL_PFNGLBINDFRAMEBUFFERPROC)(GLenum target, GLuint framebuffer);
typedef void (APIENTRY * OBOL_PFNGLDELETEFRAMEBUFFERSPROC)(GLsizei n, const GLuint *framebuffers);
typedef void (APIENTRY * OBOL_PFNGLGENFRAMEBUFFERSPROC)(GLsizei n, GLuint *framebuffers);
typedef GLenum (APIENTRY * OBOL_PFNGLCHECKFRAMEBUFFERSTATUSPROC)(GLenum target);
typedef void (APIENTRY * OBOL_PFNGLFRAMEBUFFERTEXTURE1DPROC)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (APIENTRY * OBOL_PFNGLFRAMEBUFFERTEXTURE2DPROC)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (APIENTRY * OBOL_PFNGLFRAMEBUFFERTEXTURE3DPROC)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset);
typedef void (APIENTRY * OBOL_PFNGLFRAMEBUFFERRENDERBUFFERPROC)(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
typedef void (APIENTRY * OBOL_PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC)(GLenum target, GLenum attachment, GLenum pname, GLint *params);
typedef void (APIENTRY * OBOL_PFNGLGENERATEMIPMAPPROC)(GLenum target);

/* Typedef for new extension string method */

typedef GLubyte* (APIENTRY * OBOL_PFNGLGETSTRINGIPROC) (GLenum target, GLuint idx);


/* ********************************************************************** */

/* Type specification for GLX info storage structure, embedded within
   the main GL info structure below. */
/* ********************************************************************** */

/* GL info storage structure. An instance will be allocated and
   initialized for each new GL context id. */
struct SoGLContext {

  uint32_t contextid;
  struct { /* OpenGL versioning. */
    unsigned int major, minor, release;
  } version;

  /* OpenGL calls. Will be NULL if not available, otherwise they
     contain a valid function pointer into the OpenGL library. */
  OBOL_PFNGLPOLYGONOFFSETPROC glPolygonOffset;
  OBOL_PFNGLPOLYGONOFFSETPROC glPolygonOffsetEXT;

  /* Core GL 1.0/1.1 dispatch pointers — always non-NULL after init.
     Required so that dual-GL builds (OBOL_BUILD_DUAL_GL) never mix
     system-GL and OSMesa calls within the same render pass. */
  OBOL_PFNGLTEXIMAGE2DPROC      glTexImage2D;
  OBOL_PFNGLTEXPARAMETERIPROC   glTexParameteri;
  OBOL_PFNGLTEXPARAMETERFPROC   glTexParameterf;
  OBOL_PFNGLGETINTEGERVPROC     glGetIntegerv;
  OBOL_PFNGLGETFLOATVPROC       glGetFloatv;
  OBOL_PFNGLCLEARCOLORPROC      glClearColor;
  OBOL_PFNGLCLEARPROC           glClear;
  OBOL_PFNGLFLUSHPROC           glFlush;
  OBOL_PFNGLFINISHPROC          glFinish;
  OBOL_PFNGLGETERRORPROC        glGetError;
  OBOL_PFNGLGETSTRINGPROC       glGetString;
  OBOL_PFNGLENABLEPROC          glEnable;
  OBOL_PFNGLDISABLEPROC         glDisable;
  OBOL_PFNGLISENABLEDPROC       glIsEnabled;
  OBOL_PFNGLPIXELSTOREIPROC     glPixelStorei;
  OBOL_PFNGLREADPIXELSPROC      glReadPixels;
  OBOL_PFNGLCOPYTEXSUBIMAGE2DPROC glCopyTexSubImage2D;

  /* Additional core GL 1.0/1.1 dispatch pointers for full dual-GL support. */
  OBOL_PFNGLBEGINPROC glBegin;
  OBOL_PFNGLENDPROC glEnd;
  OBOL_PFNGLVERTEX2FPROC glVertex2f;
  OBOL_PFNGLVERTEX2SPROC glVertex2s;
  OBOL_PFNGLVERTEX3FPROC glVertex3f;
  OBOL_PFNGLVERTEX3FVPROC glVertex3fv;
  OBOL_PFNGLVERTEX4FVPROC glVertex4fv;
  OBOL_PFNGLNORMAL3FPROC glNormal3f;
  OBOL_PFNGLNORMAL3FVPROC glNormal3fv;
  OBOL_PFNGLCOLOR3FPROC glColor3f;
  OBOL_PFNGLCOLOR3FVPROC glColor3fv;
  OBOL_PFNGLCOLOR3UBPROC glColor3ub;
  OBOL_PFNGLCOLOR3UBVPROC glColor3ubv;
  OBOL_PFNGLCOLOR4UBPROC glColor4ub;
  OBOL_PFNGLTEXCOORD2FPROC glTexCoord2f;
  OBOL_PFNGLTEXCOORD2FVPROC glTexCoord2fv;
  OBOL_PFNGLTEXCOORD3FPROC glTexCoord3f;
  OBOL_PFNGLTEXCOORD3FVPROC glTexCoord3fv;
  OBOL_PFNGLTEXCOORD4FVPROC glTexCoord4fv;
  OBOL_PFNGLINDEXIPROC glIndexi;
  OBOL_PFNGLMATRIXMODEPROC glMatrixMode;
  OBOL_PFNGLLOADIDENTITYPROC glLoadIdentity;
  OBOL_PFNGLLOADMATRIXFPROC glLoadMatrixf;
  OBOL_PFNGLLOADMATRIXDPROC glLoadMatrixd;
  OBOL_PFNGLMULTMATRIXFPROC glMultMatrixf;
  OBOL_PFNGLPUSHMATRIXPROC glPushMatrix;
  OBOL_PFNGLPOPMATRIXPROC glPopMatrix;
  OBOL_PFNGLORTHOPROC glOrtho;
  OBOL_PFNGLFRUSTUMPROC glFrustum;
  OBOL_PFNGLTRANSLATEFPROC glTranslatef;
  OBOL_PFNGLROTATEFPROC glRotatef;
  OBOL_PFNGLSCALEFPROC glScalef;
  OBOL_PFNGLLIGHTFPROC glLightf;
  OBOL_PFNGLLIGHTFVPROC glLightfv;
  OBOL_PFNGLLIGHTMODELIPROC glLightModeli;
  OBOL_PFNGLLIGHTMODELFVPROC glLightModelfv;
  OBOL_PFNGLMATERIALFPROC glMaterialf;
  OBOL_PFNGLMATERIALFVPROC glMaterialfv;
  OBOL_PFNGLCOLORMATERIALPROC glColorMaterial;
  OBOL_PFNGLFOGLPROC glFogi;
  OBOL_PFNGLFOGFPROC glFogf;
  OBOL_PFNGLFOGFVPROC glFogfv;
  OBOL_PFNGLTEXENVIPROC glTexEnvi;
  OBOL_PFNGLTEXENVFPROC glTexEnvf;
  OBOL_PFNGLTEXENVFVPROC glTexEnvfv;
  OBOL_PFNGLTEXGENIPROC glTexGeni;
  OBOL_PFNGLTEXGENFPROC glTexGenf;
  OBOL_PFNGLTEXGENFVPROC glTexGenfv;
  OBOL_PFNGLCOPYTEXIMAGE2DPROC glCopyTexImage2D;
  OBOL_PFNGLRASTERPOS2FPROC glRasterPos2f;
  OBOL_PFNGLRASTERPOS3FPROC glRasterPos3f;
  OBOL_PFNGLBITMAPPROC glBitmap;
  OBOL_PFNGLDRAWPIXELSPROC glDrawPixels;
  OBOL_PFNGLPIXELTRANSFERFPROC glPixelTransferf;
  OBOL_PFNGLPIXELTRANSFERIPROC glPixelTransferi;
  OBOL_PFNGLPIXELMAPFVPROC glPixelMapfv;
  OBOL_PFNGLPIXELMAPUIVPROC glPixelMapuiv;
  OBOL_PFNGLPIXELZOOMPROC glPixelZoom;
  OBOL_PFNGLVIEWPORTPROC glViewport;
  OBOL_PFNGLSCISSORPROC glScissor;
  OBOL_PFNGLDEPTHMASKPROC glDepthMask;
  OBOL_PFNGLDEPTHFUNCPROC glDepthFunc;
  OBOL_PFNGLDEPTHRANGEPROC glDepthRange;
  OBOL_PFNGLSTENCILFUNCPROC glStencilFunc;
  OBOL_PFNGLSTENCILOPPROC glStencilOp;
  OBOL_PFNGLBLENDFUNCPROC glBlendFunc;
  OBOL_PFNGLALPHAFUNCPROC glAlphaFunc;
  OBOL_PFNGLFRONTFACEPROC glFrontFace;
  OBOL_PFNGLCULLFACEPROC glCullFace;
  OBOL_PFNGLPOLYGONMODEPROC glPolygonMode;
  OBOL_PFNGLPOLYGONSTIPPLEPROC glPolygonStipple;
  OBOL_PFNGLLINEWIDTHPROC glLineWidth;
  OBOL_PFNGLLINESTIPPLEPROC glLineStipple;
  OBOL_PFNGLPOINTSIZEPROC glPointSize;
  OBOL_PFNGLCOLORMASKPROC glColorMask;
  OBOL_PFNGLCLIPPLANEPROC glClipPlane;
  OBOL_PFNGLDRAWBUFFERPROC glDrawBuffer;
  OBOL_PFNGLCLEARINDEXPROC glClearIndex;
  OBOL_PFNGLCLEARSTENCILPROC glClearStencil;
  OBOL_PFNGLACCUMPROC glAccum;
  OBOL_PFNGLGETBOOLEANVPROC glGetBooleanv;
  OBOL_PFNGLNEWLISTPROC glNewList;
  OBOL_PFNGLENDLISTPROC glEndList;
  OBOL_PFNGLCALLLISTPROC glCallList;
  OBOL_PFNGLDELETELISTSPROC glDeleteLists;
  OBOL_PFNGLGENLISTSPROC glGenLists;
  OBOL_PFNGLPUSHATTRIBPROC glPushAttrib;
  OBOL_PFNGLPOPATTRIBPROC glPopAttrib;

  OBOL_PFNGLGENTEXTURESPROC glGenTextures;
  OBOL_PFNGLBINDTEXTUREPROC glBindTexture;
  OBOL_PFNGLDELETETEXTURESPROC glDeleteTextures;

  OBOL_PFNGLTEXIMAGE3DPROC glTexImage3D;
  OBOL_PFNGLCOPYTEXSUBIMAGE3DPROC glCopyTexSubImage3D;
  OBOL_PFNGLTEXSUBIMAGE3DPROC glTexSubImage3D;
  OBOL_PFNGLTEXSUBIMAGE2DPROC glTexSubImage2D;

  OBOL_PFNGLACTIVETEXTUREPROC glActiveTexture;
  OBOL_PFNGLCLIENTACTIVETEXTUREPROC glClientActiveTexture;
  OBOL_PFNGLMULTITEXCOORD2FPROC glMultiTexCoord2f;
  OBOL_PFNGLMULTITEXCOORD2FVPROC glMultiTexCoord2fv;
  OBOL_PFNGLMULTITEXCOORD3FVPROC glMultiTexCoord3fv;
  OBOL_PFNGLMULTITEXCOORD4FVPROC glMultiTexCoord4fv;

  OBOL_PFNGLCOLORTABLEPROC glColorTable;
  OBOL_PFNGLCOLORSUBTABLEPROC glColorSubTable;
  OBOL_PFNGLGETCOLORTABLEPROC glGetColorTable;
  OBOL_PFNGLGETCOLORTABLEPARAMETERIVPROC glGetColorTableParameteriv;
  OBOL_PFNGLGETCOLORTABLEPARAMETERFVPROC glGetColorTableParameterfv;

  SbBool supportsPalettedTextures;

  OBOL_PFNGLCOMPRESSEDTEXIMAGE3DPROC glCompressedTexImage3D;
  OBOL_PFNGLCOMPRESSEDTEXIMAGE2DPROC glCompressedTexImage2D;
  OBOL_PFNGLCOMPRESSEDTEXIMAGE1DPROC glCompressedTexImage1D;
  OBOL_PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC glCompressedTexSubImage3D;
  OBOL_PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC glCompressedTexSubImage2D;
  OBOL_PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC glCompressedTexSubImage1D;
  OBOL_PFNGLGETCOMPRESSEDTEXIMAGEPROC glGetCompressedTexImage;

  OBOL_PFNGLBLENDEQUATIONPROC glBlendEquation;
  OBOL_PFNGLBLENDEQUATIONPROC glBlendEquationEXT;

  OBOL_PFNGLBLENDFUNCSEPARATEPROC glBlendFuncSeparate;

  OBOL_PFNGLVERTEXPOINTERPROC glVertexPointer;
  OBOL_PFNGLTEXCOORDPOINTERPROC glTexCoordPointer;
  OBOL_PFNGLNORMALPOINTERPROC glNormalPointer;
  OBOL_PNFGLCOLORPOINTERPROC glColorPointer;
  OBOL_PFNGLINDEXPOINTERPROC glIndexPointer;
  OBOL_PFNGLENABLECLIENTSTATEPROC glEnableClientState;
  OBOL_PFNGLDISABLECLIENTSTATEPROC glDisableClientState;
  OBOL_PFNGLINTERLEAVEDARRAYSPROC glInterleavedArrays;
  OBOL_PFNGLDRAWARRAYSPROC glDrawArrays;
  OBOL_PFNGLDRAWELEMENTSPROC glDrawElements;
  OBOL_PFNGLDRAWRANGEELEMENTSPROC glDrawRangeElements;
  OBOL_PFNGLARRAYELEMENTPROC glArrayElement;

  OBOL_PFNGLMULTIDRAWARRAYSPROC glMultiDrawArrays;
  OBOL_PFNGLMULTIDRAWELEMENTSPROC glMultiDrawElements;

  OBOL_PFNGLVERTEXARRAYRANGENVPROC glVertexArrayRangeNV;
  OBOL_PFNGLFLUSHVERTEXARRAYRANGENVPROC glFlushVertexArrayRangeNV;
  OBOL_PFNGLALLOCATEMEMORYNVPROC glAllocateMemoryNV;
  OBOL_PFNGLFREEMEMORYNVPROC glFreeMemoryNV;

  OBOL_PFNGLBINDBUFFERPROC glBindBuffer;
  OBOL_PFNGLDELETEBUFFERSPROC glDeleteBuffers;
  OBOL_PFNGLGENBUFFERSPROC glGenBuffers;
  OBOL_PFNGLISBUFFERPROC glIsBuffer;
  OBOL_PFNGLBUFFERDATAPROC glBufferData;
  OBOL_PFNGLBUFFERSUBDATAPROC glBufferSubData;
  OBOL_PFNGLGETBUFFERSUBDATAPROC glGetBufferSubData;
  OBOL_PNFGLMAPBUFFERPROC glMapBuffer;
  OBOL_PFNGLUNMAPBUFFERPROC glUnmapBuffer;
  OBOL_PFNGLGETBUFFERPARAMETERIVPROC glGetBufferParameteriv;
  OBOL_PFNGLGETBUFFERPOINTERVPROC glGetBufferPointerv;

  /* NV register combiners */
  OBOL_PFNGLCOMBINERPARAMETERFVNVPROC glCombinerParameterfvNV;
  OBOL_PFNGLCOMBINERPARAMETERIVNVPROC glCombinerParameterivNV;
  OBOL_PFNGLCOMBINERPARAMETERFNVPROC glCombinerParameterfNV;
  OBOL_PFNGLCOMBINERPARAMETERINVPROC glCombinerParameteriNV;
  OBOL_PFNGLCOMBINERINPUTNVPROC glCombinerInputNV;
  OBOL_PFNGLCOMBINEROUTPUTNVPROC glCombinerOutputNV;
  OBOL_PFNGLFINALCOMBINERINPUTNVPROC glFinalCombinerInputNV;
  OBOL_PFNGLGETCOMBINERINPUTPARAMETERFVNVPROC glGetCombinerInputParameterfvNV;
  OBOL_PFNGLGETCOMBINERINPUTPARAMETERIVNVPROC glGetCombinerInputParameterivNV;
  OBOL_PFNGLGETCOMBINEROUTPUTPARAMETERFVNVPROC glGetCombinerOutputParameterfvNV;
  OBOL_PFNGLGETCOMBINEROUTPUTPARAMETERIVNVPROC glGetCombinerOutputParameterivNV;
  OBOL_PFNGLGETFINALCOMBINERINPUTPARAMETERFVNVPROC glGetFinalCombinerInputParameterfvNV;
  OBOL_PFNGLGETFINALCOMBINERINPUTPARAMETERIVNVPROC glGetFinalCombinerInputParameterivNV;

  /* fragment program */
  OBOL_PFNGLPROGRAMSTRINGARBPROC glProgramStringARB;
  OBOL_PFNGLBINDPROGRAMARBPROC glBindProgramARB;
  OBOL_PFNGLDELETEPROGRAMSARBPROC glDeleteProgramsARB;
  OBOL_PFNGLGENPROGRAMSARBPROC glGenProgramsARB;
  OBOL_PFNGLPROGRAMENVPARAMETER4DARBPROC glProgramEnvParameter4dARB;
  OBOL_PFNGLPROGRAMENVPARAMETER4DVARBPROC glProgramEnvParameter4dvARB;
  OBOL_PFNGLPROGRAMENVPARAMETER4FARBPROC glProgramEnvParameter4fARB;
  OBOL_PFNGLPROGRAMENVPARAMETER4FVARBPROC glProgramEnvParameter4fvARB;
  OBOL_PFNGLPROGRAMLOCALPARAMETER4DARBPROC glProgramLocalParameter4dARB;
  OBOL_PFNGLPROGRAMLOCALPARAMETER4DVARBPROC glProgramLocalParameter4dvARB;
  OBOL_PFNGLPROGRAMLOCALPARAMETER4FARBPROC glProgramLocalParameter4fARB;
  OBOL_PFNGLPROGRAMLOCALPARAMETER4FVARBPROC glProgramLocalParameter4fvARB;
  OBOL_PFNGLGETPROGRAMENVPARAMETERDVARBPROC glGetProgramEnvParameterdvARB;
  OBOL_PFNGLGETPROGRAMENVPARAMETERFVARBPROC glGetProgramEnvParameterfvARB;
  OBOL_PFNGLGETPROGRAMLOCALPARAMETERDVARBPROC glGetProgramLocalParameterdvARB;
  OBOL_PFNGLGETPROGRAMLOCALPARAMETERFVARBPROC glGetProgramLocalParameterfvARB;
  OBOL_PFNGLGETPROGRAMIVARBPROC glGetProgramivARB;
  OBOL_PFNGLGETPROGRAMSTRINGARBPROC glGetProgramStringARB;
  OBOL_PFNGLISPROGRAMARBPROC glIsProgramARB;

  /* vertex program */
  OBOL_PFNGLVERTEXATTRIB1SARBPROC glVertexAttrib1sARB;
  OBOL_PFNGLVERTEXATTRIB1FARBPROC glVertexAttrib1fARB;
  OBOL_PFNGLVERTEXATTRIB1DARBPROC glVertexAttrib1dARB;
  OBOL_PFNGLVERTEXATTRIB2SARBPROC glVertexAttrib2sARB;
  OBOL_PFNGLVERTEXATTRIB2FARBPROC glVertexAttrib2fARB;
  OBOL_PFNGLVERTEXATTRIB2DARBPROC glVertexAttrib2dARB;
  OBOL_PFNGLVERTEXATTRIB3SARBPROC glVertexAttrib3sARB;
  OBOL_PFNGLVERTEXATTRIB3FARBPROC glVertexAttrib3fARB;
  OBOL_PFNGLVERTEXATTRIB3DARBPROC glVertexAttrib3dARB;
  OBOL_PFNGLVERTEXATTRIB4SARBPROC glVertexAttrib4sARB;
  OBOL_PFNGLVERTEXATTRIB4FARBPROC glVertexAttrib4fARB;
  OBOL_PFNGLVERTEXATTRIB4DARBPROC glVertexAttrib4dARB;
  OBOL_PFNGLVERTEXATTRIB4NUBARBPROC glVertexAttrib4NubARB;
  OBOL_PFNGLVERTEXATTRIB1SVARBPROC glVertexAttrib1svARB;
  OBOL_PFNGLVERTEXATTRIB1FVARBPROC glVertexAttrib1fvARB;
  OBOL_PFNGLVERTEXATTRIB1DVARBPROC glVertexAttrib1dvARB;
  OBOL_PFNGLVERTEXATTRIB2SVARBPROC glVertexAttrib2svARB;
  OBOL_PFNGLVERTEXATTRIB2FVARBPROC glVertexAttrib2fvARB;
  OBOL_PFNGLVERTEXATTRIB2DVARBPROC glVertexAttrib2dvARB;
  OBOL_PFNGLVERTEXATTRIB3SVARBPROC glVertexAttrib3svARB;
  OBOL_PFNGLVERTEXATTRIB3FVARBPROC glVertexAttrib3fvARB;
  OBOL_PFNGLVERTEXATTRIB3DVARBPROC glVertexAttrib3dvARB;
  OBOL_PFNGLVERTEXATTRIB4BVARBPROC glVertexAttrib4bvARB;
  OBOL_PFNGLVERTEXATTRIB4SVARBPROC glVertexAttrib4svARB;
  OBOL_PFNGLVERTEXATTRIB4IVARBPROC glVertexAttrib4ivARB;
  OBOL_PFNGLVERTEXATTRIB4UBVARBPROC glVertexAttrib4ubvARB;
  OBOL_PFNGLVERTEXATTRIB4USVARBPROC glVertexAttrib4usvARB;
  OBOL_PFNGLVERTEXATTRIB4UIVARBPROC glVertexAttrib4uivARB;
  OBOL_PFNGLVERTEXATTRIB4FVARBPROC glVertexAttrib4fvARB;
  OBOL_PFNGLVERTEXATTRIB4DVARBPROC glVertexAttrib4dvARB;
  OBOL_PFNGLVERTEXATTRIB4NBVARBPROC glVertexAttrib4NbvARB;
  OBOL_PFNGLVERTEXATTRIB4NSVARBPROC glVertexAttrib4NsvARB;
  OBOL_PFNGLVERTEXATTRIB4NIVARBPROC glVertexAttrib4NivARB;
  OBOL_PFNGLVERTEXATTRIB4NUBVARBPROC glVertexAttrib4NubvARB;
  OBOL_PFNGLVERTEXATTRIB4NUSVARBPROC glVertexAttrib4NusvARB;
  OBOL_PFNGLVERTEXATTRIB4NUIVARBPROC glVertexAttrib4NuivARB;
  OBOL_PFNGLVERTEXATTRIBPOINTERARBPROC glVertexAttribPointerARB;
  OBOL_PFNGLENABLEVERTEXATTRIBARRAYARBPROC glEnableVertexAttribArrayARB;
  OBOL_PFNGLDISABLEVERTEXATTRIBARRAYARBPROC glDisableVertexAttribArrayARB;
  OBOL_PFNGLGETVERTEXATTRIBDVARBPROC glGetVertexAttribdvARB;
  OBOL_PFNGLGETVERTEXATTRIBFVARBPROC glGetVertexAttribfvARB;
  OBOL_PFNGLGETVERTEXATTRIBIVARBPROC glGetVertexAttribivARB;
  OBOL_PFNGLGETVERTEXATTRIBPOINTERVARBPROC glGetVertexAttribPointervARB;

  /* vertex shader */
  OBOL_PFNGLBINDATTRIBLOCATIONARBPROC glBindAttribLocationARB;
  OBOL_PFNGLGETACTIVEATTRIBARBPROC glGetActiveAttribARB;
  OBOL_PFNGLGETATTRIBLOCATIONARBPROC glGetAttribLocationARB;

  /* shader objects */
  OBOL_PFNGLPROGRAMPARAMETERIEXT glProgramParameteriEXT;
  OBOL_PFNGLGETUNIFORMLOCATIONARBPROC glGetUniformLocationARB;
  OBOL_PFNGLGETACTIVEUNIFORMARBPROC glGetActiveUniformARB;
  OBOL_PFNGLUNIFORM1FARBPROC glUniform1fARB;
  OBOL_PFNGLUNIFORM2FARBPROC glUniform2fARB;
  OBOL_PFNGLUNIFORM3FARBPROC glUniform3fARB;
  OBOL_PFNGLUNIFORM4FARBPROC glUniform4fARB;
  OBOL_PFNGLCREATESHADEROBJECTARBPROC glCreateShaderObjectARB;
  OBOL_PFNGLSHADERSOURCEARBPROC glShaderSourceARB;
  OBOL_PFNGLCOMPILESHADERARBPROC glCompileShaderARB;
  OBOL_PFNGLGETOBJECTPARAMETERIVARBPROC glGetObjectParameterivARB;
  OBOL_PFNGLDELETEOBJECTARBPROC glDeleteObjectARB;
  OBOL_PFNGLATTACHOBJECTARBPROC glAttachObjectARB;
  OBOL_PFNGLDETACHOBJECTARBPROC glDetachObjectARB;
  OBOL_PFNGLGETINFOLOGARBPROC glGetInfoLogARB;
  OBOL_PFNGLLINKPROGRAMARBPROC glLinkProgramARB;
  OBOL_PFNGLUSEPROGRAMOBJECTARBPROC glUseProgramObjectARB;
  OBOL_PFNGLCREATEPROGRAMOBJECTARBPROC glCreateProgramObjectARB;
  OBOL_PFNGLUNIFORM1FVARBPROC glUniform1fvARB;
  OBOL_PFNGLUNIFORM2FVARBPROC glUniform2fvARB;
  OBOL_PFNGLUNIFORM3FVARBPROC glUniform3fvARB;
  OBOL_PFNGLUNIFORM4FVARBPROC glUniform4fvARB;
  OBOL_PFNGLUNIFORM1IARBPROC glUniform1iARB;
  OBOL_PFNGLUNIFORM2IARBPROC glUniform2iARB;
  OBOL_PFNGLUNIFORM3IARBPROC glUniform3iARB;
  OBOL_PFNGLUNIFORM4IARBPROC glUniform4iARB;
  OBOL_PFNGLUNIFORM1IVARBPROC glUniform1ivARB;
  OBOL_PFNGLUNIFORM2IVARBPROC glUniform2ivARB;
  OBOL_PFNGLUNIFORM3IVARBPROC glUniform3ivARB;
  OBOL_PFNGLUNIFORM4IVARBPROC glUniform4ivARB;
  OBOL_PFNGLUNIFORMMATRIX2FVARBPROC glUniformMatrix2fvARB;
  OBOL_PFNGLUNIFORMMATRIX3FVARBPROC glUniformMatrix3fvARB;
  OBOL_PFNGLUNIFORMMATRIX4FVARBPROC glUniformMatrix4fvARB;

  OBOL_PFNGLPUSHCLIENTATTRIBPROC glPushClientAttrib;
  OBOL_PFNGLPOPCLIENTATTRIBPROC glPopClientAttrib;

  OBOL_PFNGLGENQUERIESPROC glGenQueries;
  OBOL_PFNGLDELETEQUERIESPROC glDeleteQueries;
  OBOL_PFNGLISQUERYPROC glIsQuery;
  OBOL_PFNGLBEGINQUERYPROC glBeginQuery;
  OBOL_PFNGLENDQUERYPROC glEndQuery;
  OBOL_PFNGLGETQUERYIVPROC glGetQueryiv;
  OBOL_PFNGLGETQUERYOBJECTIVPROC glGetQueryObjectiv;
  OBOL_PFNGLGETQUERYOBJECTUIVPROC glGetQueryObjectuiv;

  /* FBO */
  OBOL_PFNGLISRENDERBUFFERPROC glIsRenderbuffer;
  OBOL_PFNGLBINDRENDERBUFFERPROC glBindRenderbuffer;
  OBOL_PFNGLDELETERENDERBUFFERSPROC glDeleteRenderbuffers;
  OBOL_PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers;
  OBOL_PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage;
  OBOL_PFNGLGETRENDERBUFFERPARAMETERIVPROC glGetRenderbufferParameteriv;
  OBOL_PFNGLISFRAMEBUFFERPROC glIsFramebuffer;
  OBOL_PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
  OBOL_PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;
  OBOL_PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
  OBOL_PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus;
  OBOL_PFNGLFRAMEBUFFERTEXTURE1DPROC glFramebufferTexture1D;
  OBOL_PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;
  OBOL_PFNGLFRAMEBUFFERTEXTURE3DPROC glFramebufferTexture3D;
  OBOL_PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer;
  OBOL_PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC glGetFramebufferAttachmentParameteriv;
  OBOL_PFNGLGENERATEMIPMAPPROC glGenerateMipmap;

  /* glGetStringi - part of replacement for obsolete glGetString(GL_EXTENSIONS) in OpenGL 3.0 */
  OBOL_PFNGLGETSTRINGIPROC glGetStringi;

  const char * versionstr;
  const char * vendorstr;
  SbBool vendor_is_SGI;
  SbBool vendor_is_intel;
  SbBool vendor_is_ati;
  SbBool vendor_is_3dlabs;
  const char * rendererstr;
  const char * extensionsstr;
  int maxtextureunits;
  float max_anisotropy;

  /* normalization cube map */
  GLuint normalizationcubemap;
  GLuint specularlookup;

  SbBool can_do_bumpmapping;
  SbBool can_do_sortedlayersblend;
  SbBool can_do_anisotropic_filtering;

  SbBool has_nv_register_combiners;
  SbBool has_ext_texture_rectangle;
  SbBool has_nv_texture_shader;
  SbBool has_depth_texture;
  SbBool has_shadow;
  SbBool has_arb_fragment_program;
  SbBool has_arb_vertex_program;
  SbBool has_arb_shader_objects;
  SbBool has_arb_vertex_shader;
  SbBool has_texture_env_combine;
  SbBool has_fbo;

  SbBool vbo_in_displaylist_ok;
  SbBool non_power_of_two_textures;
  int max_lights;
  float line_width_range[2];
  float point_size_range[2];
  int max_texture_size;
  cc_dict * glextdict;

  cc_libhandle dl_handle;

  /* Per-context manager set at instance creation time by SoGLContext_instance().
     Points to the SoDB::ContextManager that owns this GL context.  Used by
     SoGLContext_getprocaddress() to call the correct backend proc-address
     resolver without consulting the global singleton.  Stored as void* so
     this C-compatible struct does not need to include SoDB.h. */
  void * context_manager;
};

/* ********************************************************************** */

/* Called from SoContextHandler::destructingContext() to be able
   to deallocate the SoGLContext instance. */

void SoGLContext_destruct(uint32_t contextid);

/* ********************************************************************** */

/* Primarily used internally from functions that are badly designed,
   lacking a SoGLContext* argument in the function signature.

   Note: you should try to avoid using this function if possible! */

void * coin_gl_current_context(void);

/* Explicit context retrieval from a state pointer.  Defined in SoGL.cpp;
   delegates to sogl_glue_instance() which derives the context from
   state->getAction()->getCacheContext().  Works at every point in the
   render pipeline, including during SoState construction. */
const SoGLContext * sogl_glue_from_state(const SoState * state);

/* Returns the function pointer for glGetString as seen in the linked GL
   library.  Used by dl.cpp to verify that cc_dl_opengl_handle() opened the
   same GL DLL that the rest of the library uses, without needing to include
   raw OpenGL headers in dl.cpp. */
void * coin_gl_getstring_ptr(void);

/* Thread-local current render glue — set by SoGLRenderAction before each
   traversal pass so that GL element updategl() methods (which lack a state
   pointer) can still dispatch through the correct backend in dual-GL builds. */
const SoGLContext * sogl_current_render_glue(void);
void sogl_set_current_render_glue(const SoGLContext * glue);

/* ********************************************************************** */

/*
 * Needed for a hack in SoVertexShader and SoFramgmentShader
 * Will be removed soon.
 */
const SoGLContext * SoGLContext_instance_from_context_ptr(void * ptr);

/* ********************************************************************** */

/* Scanning for and printing info about current set of glGetError()s. */
unsigned int coin_catch_gl_errors(std::string *);
/* Convert OpenGL glGetError() error code to string. */
const char * coin_glerror_string(GLenum errorcode);

/* ********************************************************************** */

int SoGLContext_debug(void);
int SoGLContext_extension_available(const char * extensions, const char * ext);

int SoGLContext_stencil_bits_hack(void);

/* ********************************************************************** */

/* ********************************************************************** */

/* ARB_shader_objects */
SbBool SoGLContext_has_arb_shader_objects(const SoGLContext * glue);

/* Moved from gl.h and added compressed parameter.
   Original function is deprecated for internal use.
*/
SbBool SoGLContext_is_texture_size_legal(const SoGLContext * glw,
                                         int xsize, int ysize, int zsize,
                                         GLenum internalformat,
                                         GLenum format,
                                         GLenum type,
                                         SbBool mipmap);

GLint SoGLContext_get_internal_texture_format(const SoGLContext * glw,
                                              int numcomponents,
                                              SbBool compress);

GLenum SoGLContext_get_texture_format(const SoGLContext * glw, int numcomponents);
SbBool SoGLContext_vbo_in_displaylist_supported(const SoGLContext * glw);
SbBool SoGLContext_non_power_of_two_textures(const SoGLContext * glue);
SbBool SoGLContext_has_generate_mipmap(const SoGLContext * glue);

/* context creation callback */
typedef void SoGLContext_instance_created_cb(const uint32_t contextid, void * closure);
void SoGLContext_add_instance_created_callback(SoGLContext_instance_created_cb * cb,
                                               void * closure);

cc_libhandle SoGLContext_dl_handle(const SoGLContext * glw);

/* ********************************************************************** */

/* Public API functions moved from include/Inventor/C/glue/gl.h for internal use only */

/* Singleton functions for getting hold of SoGLContext instance for context. */
const SoGLContext * SoGLContext_instance(int contextid);

/* General interface. */
void SoGLContext_glversion(const SoGLContext * glue,
                         unsigned int * major,
                         unsigned int * minor,
                         unsigned int * release);

SbBool SoGLContext_glversion_matches_at_least(const SoGLContext * glue,
                                            unsigned int major,
                                            unsigned int minor,
                                            unsigned int release);

SbBool SoGLContext_glxversion_matches_at_least(const SoGLContext * glue,
                                             int major,
                                             int minor);

SbBool SoGLContext_glext_supported(const SoGLContext * glue, const char * extname);

void * SoGLContext_getprocaddress(const SoGLContext * glue, const char * symname);

SbBool SoGLContext_isdirect(const SoGLContext * w);

/* Wrapped OpenGL 1.1+ features and extensions. */

/* Z-buffer offsetting */
SbBool SoGLContext_has_polygon_offset(const SoGLContext * glue);
enum SoGLContext_Primitives { SoGLContext_FILLED = 1 << 0,
                            SoGLContext_LINES  = 1 << 1,
                            SoGLContext_POINTS = 1 << 2 };
void SoGLContext_glPolygonOffsetEnable(const SoGLContext * glue,
                                     SbBool enable, int m);
void SoGLContext_glPolygonOffset(const SoGLContext * glue,
                               GLfloat factor,
                               GLfloat units);

/* Texture objects */
SbBool SoGLContext_has_texture_objects(const SoGLContext * glue);
void SoGLContext_glGenTextures(const SoGLContext * glue,
                             GLsizei n,
                             GLuint *textures);
void SoGLContext_glBindTexture(const SoGLContext * glue,
                             GLenum target,
                             GLuint texture);
void SoGLContext_glDeleteTextures(const SoGLContext * glue,
                                GLsizei n,
                                const GLuint * textures);

/* 3D textures */
SbBool SoGLContext_has_3d_textures(const SoGLContext * glue);
void SoGLContext_glTexImage3D(const SoGLContext * glue,
                            GLenum target,
                            GLint level,
                            GLenum internalformat,
                            GLsizei width,
                            GLsizei height,
                            GLsizei depth,
                            GLint border,
                            GLenum format,
                            GLenum type,
                            const GLvoid *pixels);
void SoGLContext_glTexSubImage3D(const SoGLContext * glue,
                               GLenum target,
                               GLint level,
                               GLint xoffset,
                               GLint yoffset,
                               GLint zoffset,
                               GLsizei width,
                               GLsizei height,
                               GLsizei depth,
                               GLenum format,
                               GLenum type,
                               const GLvoid * pixels);
void SoGLContext_glCopyTexSubImage3D(const SoGLContext * glue,
                                   GLenum target,
                                   GLint level,
                                   GLint xoffset,
                                   GLint yoffset,
                                   GLint zoffset,
                                   GLint x,
                                   GLint y,
                                   GLsizei width,
                                   GLsizei height);

/* Multi-texturing */
SbBool SoGLContext_has_multitexture(const SoGLContext * glue);
void SoGLContext_glMultiTexCoord2f(const SoGLContext * glue,
                                 GLenum target,
                                 GLfloat s,
                                 GLfloat t);
void SoGLContext_glMultiTexCoord2fv(const SoGLContext * glue,
                                  GLenum target,
                                  const GLfloat * v);
void SoGLContext_glMultiTexCoord3fv(const SoGLContext * glue,
                                  GLenum target,
                                  const GLfloat * v);
void SoGLContext_glMultiTexCoord4fv(const SoGLContext * glue,
                                  GLenum target,
                                  const GLfloat * v);

void SoGLContext_glActiveTexture(const SoGLContext * glue,
                               GLenum texture);
void SoGLContext_glClientActiveTexture(const SoGLContext * glue,
                                     GLenum texture);

/* Sub-texture operations */
SbBool SoGLContext_has_texsubimage(const SoGLContext * glue);
void SoGLContext_glTexSubImage2D(const SoGLContext * glue,
                               GLenum target,
                               GLint level,
                               GLint xoffset,
                               GLint yoffset,
                               GLsizei width,
                               GLsizei height,
                               GLenum format,
                               GLenum type,
                               const GLvoid * pixels);

/* Misc texture operations */
SbBool SoGLContext_has_2d_proxy_textures(const SoGLContext * glue);
SbBool SoGLContext_has_texture_edge_clamp(const SoGLContext * glue);
void SoGLContext_glPushClientAttrib(const SoGLContext * glue, GLbitfield mask);
void SoGLContext_glPopClientAttrib(const SoGLContext * glue);

/* Texture compression */
SbBool cc_glue_has_texture_compression(const SoGLContext * glue);
void SoGLContext_glCompressedTexImage3D(const SoGLContext * glue,
                                      GLenum target, 
                                      GLint level, 
                                      GLenum internalformat, 
                                      GLsizei width, 
                                      GLsizei height, 
                                      GLsizei depth, 
                                      GLint border, 
                                      GLsizei imageSize, 
                                      const GLvoid * data);
void SoGLContext_glCompressedTexImage2D(const SoGLContext * glue,
                                      GLenum target, 
                                      GLint level, 
                                      GLenum internalformat, 
                                      GLsizei width, 
                                      GLsizei height, 
                                      GLint border, 
                                      GLsizei imageSize, 
                                      const GLvoid *data);
void SoGLContext_glCompressedTexImage1D(const SoGLContext * glue,
                                      GLenum target, 
                                      GLint level, 
                                      GLenum internalformat, 
                                      GLsizei width, 
                                      GLint border, 
                                      GLsizei imageSize, 
                                      const GLvoid *data);
void SoGLContext_glCompressedTexSubImage3D(const SoGLContext * glue,
                                         GLenum target, 
                                         GLint level, 
                                         GLint xoffset, 
                                         GLint yoffset, 
                                         GLint zoffset, 
                                         GLsizei width, 
                                         GLsizei height, 
                                         GLsizei depth, 
                                         GLenum format, 
                                         GLsizei imageSize, 
                                         const GLvoid *data);
void SoGLContext_glCompressedTexSubImage2D(const SoGLContext * glue,
                                         GLenum target, 
                                         GLint level, 
                                         GLint xoffset, 
                                         GLint yoffset, 
                                         GLsizei width, 
                                         GLsizei height, 
                                         GLenum format, 
                                         GLsizei imageSize, 
                                         const GLvoid *data);
void SoGLContext_glCompressedTexSubImage1D(const SoGLContext * glue,
                                         GLenum target, 
                                         GLint level, 
                                         GLint xoffset, 
                                         GLsizei width, 
                                         GLenum format, 
                                         GLsizei imageSize, 
                                         const GLvoid *data);
void SoGLContext_glGetCompressedTexImage(const SoGLContext * glue,
                                       GLenum target, 
                                       GLint level, 
                                       void *img);

/* Palette textures */
SbBool SoGLContext_has_color_tables(const SoGLContext * glue);
SbBool SoGLContext_has_color_subtables(const SoGLContext * glue);
SbBool SoGLContext_has_paletted_textures(const SoGLContext * glue);

void SoGLContext_glColorTable(const SoGLContext * glue,
                            GLenum target, 
                            GLenum internalFormat, 
                            GLsizei width, 
                            GLenum format, 
                            GLenum type, 
                            const GLvoid *table);
void SoGLContext_glColorSubTable(const SoGLContext * glue,
                               GLenum target,
                               GLsizei start,
                               GLsizei count,
                               GLenum format,
                               GLenum type,
                               const GLvoid * data);
void SoGLContext_glGetColorTable(const SoGLContext * glue,
                               GLenum target, 
                               GLenum format, 
                               GLenum type, 
                               GLvoid *data);
void SoGLContext_glGetColorTableParameteriv(const SoGLContext * glue,
                                          GLenum target, 
                                          GLenum pname, 
                                          GLint *params);
void SoGLContext_glGetColorTableParameterfv(const SoGLContext * glue,
                                          GLenum target, 
                                          GLenum pname, 
                                          GLfloat *params);

/* Texture blending settings */
SbBool SoGLContext_has_blendequation(const SoGLContext * glue);
void SoGLContext_glBlendEquation(const SoGLContext * glue, GLenum mode);

/* Texture blend separate */
SbBool SoGLContext_has_blendfuncseparate(const SoGLContext * glue);
void SoGLContext_glBlendFuncSeparate(const SoGLContext * glue, 
                                   GLenum srgb, GLenum drgb,
                                   GLenum salpha, GLenum dalpha);

/* OpenGL vertex array */
SbBool SoGLContext_has_vertex_array(const SoGLContext * glue);
void SoGLContext_glVertexPointer(const SoGLContext * glue,
                               GLint size, GLenum type, GLsizei stride, const GLvoid * pointer);
void SoGLContext_glTexCoordPointer(const SoGLContext * glue,
                                 GLint size, GLenum type, GLsizei stride, const GLvoid * pointer);
void SoGLContext_glNormalPointer(const SoGLContext * glue,
                               GLenum type, GLsizei stride, const GLvoid *pointer);
void SoGLContext_glColorPointer(const SoGLContext * glue,
                              GLint size, GLenum type, GLsizei stride, const GLvoid * pointer);
void SoGLContext_glIndexPointer (const SoGLContext * glue,
                               GLenum type, GLsizei stride, const GLvoid * pointer);
void SoGLContext_glEnableClientState(const SoGLContext * glue, GLenum array);
void SoGLContext_glDisableClientState(const SoGLContext * glue, GLenum array);
void SoGLContext_glInterleavedArrays(const SoGLContext * glue, 
                                   GLenum format, GLsizei stride, const GLvoid * pointer);
void SoGLContext_glDrawArrays(const SoGLContext * glue, 
                            GLenum mode, GLint first, GLsizei count);
void SoGLContext_glDrawElements(const SoGLContext * glue, 
                              GLenum mode, GLsizei count, GLenum type, const GLvoid * indices);
void SoGLContext_glDrawRangeElements(const SoGLContext * glue, 
                                   GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid * indices);
void SoGLContext_glArrayElement(const SoGLContext * glue, GLint i);

int SoGLContext_max_texture_units(const SoGLContext * glue);
SbBool SoGLContext_has_multidraw_vertex_arrays(const SoGLContext * glue);

void SoGLContext_glMultiDrawArrays(const SoGLContext * glue, GLenum mode, const GLint * first, 
                                 const GLsizei * count, GLsizei primcount);
void SoGLContext_glMultiDrawElements(const SoGLContext * glue, GLenum mode, const GLsizei * count, 
                                   GLenum type, const GLvoid ** indices, GLsizei primcount);

/* NV_vertex_array_range */
SbBool SoGLContext_has_nv_vertex_array_range(const SoGLContext * glue);
void SoGLContext_glFlushVertexArrayRangeNV(const SoGLContext * glue);
void SoGLContext_glVertexArrayRangeNV(const SoGLContext * glue, GLsizei size, const GLvoid * pointer);
void * SoGLContext_glAllocateMemoryNV(const SoGLContext * glue,
                                    GLsizei size, GLfloat readfreq,
                                    GLfloat writefreq, GLfloat priority);
void SoGLContext_glFreeMemoryNV(const SoGLContext * glue, GLvoid * buffer);

/* ARB_vertex_buffer_object */
SbBool SoGLContext_has_vertex_buffer_object(const SoGLContext * glue);
void SoGLContext_glBindBuffer(const SoGLContext * glue, GLenum target, GLuint buffer);
void SoGLContext_glDeleteBuffers(const SoGLContext * glue, GLsizei n, const GLuint *buffers);
void SoGLContext_glGenBuffers(const SoGLContext * glue, GLsizei n, GLuint *buffers);
GLboolean SoGLContext_glIsBuffer(const SoGLContext * glue, GLuint buffer);
void SoGLContext_glBufferData(const SoGLContext * glue,
                            GLenum target, 
                            intptr_t size, /* 64 bit on 64 bit systems */ 
                            const GLvoid *data, 
                            GLenum usage);
void SoGLContext_glBufferSubData(const SoGLContext * glue,
                               GLenum target, 
                               intptr_t offset, /* 64 bit */ 
                               intptr_t size, /* 64 bit */ 
                               const GLvoid * data);
void SoGLContext_glGetBufferSubData(const SoGLContext * glue,
                                  GLenum target, 
                                  intptr_t offset, /* 64 bit */ 
                                  intptr_t size, /* 64 bit */ 
                                  GLvoid *data);
GLvoid * SoGLContext_glMapBuffer(const SoGLContext * glue,
                               GLenum target, GLenum access);
GLboolean SoGLContext_glUnmapBuffer(const SoGLContext * glue,
                                  GLenum target);
void SoGLContext_glGetBufferParameteriv(const SoGLContext * glue,
                                      GLenum target, 
                                      GLenum pname, 
                                      GLint * params);
void SoGLContext_glGetBufferPointerv(const SoGLContext * glue,
                                   GLenum target, 
                                   GLenum pname, 
                                   GLvoid ** params);

/* GL_ARB_fragment_program */
SbBool SoGLContext_has_arb_fragment_program(const SoGLContext * glue);
void SoGLContext_glProgramString(const SoGLContext * glue, GLenum target, GLenum format, 
                               GLsizei len, const GLvoid *string);
void SoGLContext_glBindProgram(const SoGLContext * glue, GLenum target, 
                             GLuint program);
void SoGLContext_glDeletePrograms(const SoGLContext * glue, GLsizei n, 
                                const GLuint *programs);
void SoGLContext_glGenPrograms(const SoGLContext * glue, GLsizei n, GLuint *programs);
void SoGLContext_glProgramEnvParameter4d(const SoGLContext * glue, GLenum target,
                                       GLuint index, GLdouble x, GLdouble y, 
                                       GLdouble z, GLdouble w);
void SoGLContext_glProgramEnvParameter4dv(const SoGLContext * glue, GLenum target,
                                        GLuint index, const GLdouble *params);
void SoGLContext_glProgramEnvParameter4f(const SoGLContext * glue, GLenum target, 
                                       GLuint index, GLfloat x, 
                                       GLfloat y, GLfloat z, 
                                       GLfloat w);
void SoGLContext_glProgramEnvParameter4fv(const SoGLContext * glue, GLenum target, 
                                        GLuint index, const GLfloat *params);
void SoGLContext_glProgramLocalParameter4d(const SoGLContext * glue, GLenum target, 
                                         GLuint index, GLdouble x, 
                                         GLdouble y, GLdouble z, 
                                         GLdouble w);
void SoGLContext_glProgramLocalParameter4dv(const SoGLContext * glue, GLenum target, 
                                          GLuint index, const GLdouble *params);
void SoGLContext_glProgramLocalParameter4f(const SoGLContext * glue, GLenum target, 
                                         GLuint index, GLfloat x, GLfloat y, 
                                         GLfloat z, GLfloat w);
void SoGLContext_glProgramLocalParameter4fv(const SoGLContext * glue, GLenum target, 
                                          GLuint index, const GLfloat *params);
void SoGLContext_glGetProgramEnvParameterdv(const SoGLContext * glue, GLenum target, 
                                          GLuint index, GLdouble *params);
void SoGLContext_glGetProgramEnvParameterfv(const SoGLContext * glue, GLenum target, 
                                          GLuint index, GLfloat *params);
void SoGLContext_glGetProgramLocalParameterdv(const SoGLContext * glue, GLenum target, 
                                            GLuint index, GLdouble *params);
void SoGLContext_glGetProgramLocalParameterfv(const SoGLContext * glue, GLenum target, 
                                            GLuint index, GLfloat *params);
void SoGLContext_glGetProgramiv(const SoGLContext * glue, GLenum target, 
                              GLenum pname, GLint *params);
void SoGLContext_glGetProgramString(const SoGLContext * glue, GLenum target, 
                                  GLenum pname, GLvoid *string);
SbBool SoGLContext_glIsProgram(const SoGLContext * glue, GLuint program);

/* ARB_vertex_program */
SbBool SoGLContext_has_arb_vertex_program(const SoGLContext * glue);
void SoGLContext_glVertexAttrib1s(const SoGLContext * glue, GLuint index, GLshort x);
void SoGLContext_glVertexAttrib1f(const SoGLContext * glue, GLuint index, GLfloat x);
void SoGLContext_glVertexAttrib1d(const SoGLContext * glue, GLuint index, GLdouble x);
void SoGLContext_glVertexAttrib2s(const SoGLContext * glue, GLuint index, GLshort x, GLshort y);
void SoGLContext_glVertexAttrib2f(const SoGLContext * glue, GLuint index, GLfloat x, GLfloat y);
void SoGLContext_glVertexAttrib2d(const SoGLContext * glue, GLuint index, GLdouble x, GLdouble y);
void SoGLContext_glVertexAttrib3s(const SoGLContext * glue, GLuint index, 
                                GLshort x, GLshort y, GLshort z);
void SoGLContext_glVertexAttrib3f(const SoGLContext * glue, GLuint index, 
                                GLfloat x, GLfloat y, GLfloat z);
void SoGLContext_glVertexAttrib3d(const SoGLContext * glue, GLuint index, 
                                GLdouble x, GLdouble y, GLdouble z);
void SoGLContext_glVertexAttrib4s(const SoGLContext * glue, GLuint index, GLshort x, 
                                GLshort y, GLshort z, GLshort w);
void SoGLContext_glVertexAttrib4f(const SoGLContext * glue, GLuint index, GLfloat x, 
                                GLfloat y, GLfloat z, GLfloat w);
void SoGLContext_glVertexAttrib4d(const SoGLContext * glue, GLuint index, GLdouble x, 
                                GLdouble y, GLdouble z, GLdouble w);
void SoGLContext_glVertexAttrib4Nub(const SoGLContext * glue, GLuint index, GLubyte x, 
                                  GLubyte y, GLubyte z, GLubyte w);
void SoGLContext_glVertexAttrib1sv(const SoGLContext * glue, GLuint index, const GLshort *v);
void SoGLContext_glVertexAttrib1fv(const SoGLContext * glue, GLuint index, const GLfloat *v);
void SoGLContext_glVertexAttrib1dv(const SoGLContext * glue, GLuint index, const GLdouble *v);
void SoGLContext_glVertexAttrib2sv(const SoGLContext * glue, GLuint index, const GLshort *v);
void SoGLContext_glVertexAttrib2fv(const SoGLContext * glue, GLuint index, const GLfloat *v);
void SoGLContext_glVertexAttrib2dv(const SoGLContext * glue, GLuint index, const GLdouble *v);
void SoGLContext_glVertexAttrib3sv(const SoGLContext * glue, GLuint index, const GLshort *v);
void SoGLContext_glVertexAttrib3fv(const SoGLContext * glue, GLuint index, const GLfloat *v);
void SoGLContext_glVertexAttrib3dv(const SoGLContext * glue, GLuint index, const GLdouble *v);
void SoGLContext_glVertexAttrib4bv(const SoGLContext * glue, GLuint index, const GLbyte *v);
void SoGLContext_glVertexAttrib4sv(const SoGLContext * glue, GLuint index, const GLshort *v);
void SoGLContext_glVertexAttrib4iv(const SoGLContext * glue, GLuint index, const GLint *v);
void SoGLContext_glVertexAttrib4ubv(const SoGLContext * glue, GLuint index, const GLubyte *v);
void SoGLContext_glVertexAttrib4usv(const SoGLContext * glue, GLuint index, const GLushort *v);
void SoGLContext_glVertexAttrib4uiv(const SoGLContext * glue, GLuint index, const GLuint *v);
void SoGLContext_glVertexAttrib4fv(const SoGLContext * glue, GLuint index, const GLfloat *v);
void SoGLContext_glVertexAttrib4dv(const SoGLContext * glue, GLuint index, const GLdouble *v);
void SoGLContext_glVertexAttrib4Nbv(const SoGLContext * glue, GLuint index, const GLbyte *v);
void SoGLContext_glVertexAttrib4Nsv(const SoGLContext * glue, GLuint index, const GLshort *v);
void SoGLContext_glVertexAttrib4Niv(const SoGLContext * glue, GLuint index, const GLint *v);
void SoGLContext_glVertexAttrib4Nubv(const SoGLContext * glue, GLuint index, const GLubyte *v);
void SoGLContext_glVertexAttrib4Nusv(const SoGLContext * glue, GLuint index, const GLushort *v);
void SoGLContext_glVertexAttrib4Nuiv(const SoGLContext * glue, GLuint index, const GLuint *v);
void SoGLContext_glVertexAttribPointer(const SoGLContext * glue, GLuint index, GLint size, 
                                     GLenum type, GLboolean normalized, GLsizei stride, 
                                     const GLvoid *pointer);
void SoGLContext_glEnableVertexAttribArray(const SoGLContext * glue, GLuint index);
void SoGLContext_glDisableVertexAttribArray(const SoGLContext * glue, GLuint index);
void SoGLContext_glGetVertexAttribdv(const SoGLContext * glue, GLuint index, GLenum pname, 
                                   GLdouble *params);
void SoGLContext_glGetVertexAttribfv(const SoGLContext * glue, GLuint index, GLenum pname, 
                                   GLfloat *params);
void SoGLContext_glGetVertexAttribiv(const SoGLContext * glue, GLuint index, GLenum pname, 
                                   GLint *params);
void SoGLContext_glGetVertexAttribPointerv(const SoGLContext * glue, GLuint index, GLenum pname, 
                                         GLvoid **pointer);

/* ARB_vertex_shader */
SbBool SoGLContext_has_arb_vertex_shader(const SoGLContext * glue);

/* ARB_occlusion_query */
SbBool SoGLContext_has_occlusion_query(const SoGLContext * glue);
void SoGLContext_glGenQueries(const SoGLContext * glue, 
                            GLsizei n, GLuint * ids);
void SoGLContext_glDeleteQueries(const SoGLContext * glue, 
                               GLsizei n, const GLuint *ids);
GLboolean SoGLContext_glIsQuery(const SoGLContext * glue, 
                            GLuint id);
void SoGLContext_glBeginQuery(const SoGLContext * glue, 
                            GLenum target, GLuint id);
void SoGLContext_glEndQuery(const SoGLContext * glue, 
                          GLenum target);
void SoGLContext_glGetQueryiv(const SoGLContext * glue, 
                            GLenum target, GLenum pname, 
                            GLint * params);
void SoGLContext_glGetQueryObjectiv(const SoGLContext * glue, 
                                  GLuint id, GLenum pname, 
                                  GLint * params);
void SoGLContext_glGetQueryObjectuiv(const SoGLContext * glue, 
                                   GLuint id, GLenum pname, 
                                   GLuint * params);

/* framebuffer_object */
void SoGLContext_glIsRenderbuffer(const SoGLContext * glue, GLuint renderbuffer);
void SoGLContext_glBindRenderbuffer(const SoGLContext * glue, GLenum target, GLuint renderbuffer);
void SoGLContext_glDeleteRenderbuffers(const SoGLContext * glue, GLsizei n, const GLuint *renderbuffers);
void SoGLContext_glGenRenderbuffers(const SoGLContext * glue, GLsizei n, GLuint *renderbuffers);
void SoGLContext_glRenderbufferStorage(const SoGLContext * glue, GLenum target, GLenum internalformat, GLsizei width, GLsizei height);
void SoGLContext_glGetRenderbufferParameteriv(const SoGLContext * glue, GLenum target, GLenum pname, GLint *params);
GLboolean SoGLContext_glIsFramebuffer(const SoGLContext * glue, GLuint framebuffer);
void SoGLContext_glBindFramebuffer(const SoGLContext * glue, GLenum target, GLuint framebuffer);
void SoGLContext_glDeleteFramebuffers(const SoGLContext * glue, GLsizei n, const GLuint *framebuffers);
void SoGLContext_glGenFramebuffers(const SoGLContext * glue, GLsizei n, GLuint *framebuffers);
GLenum SoGLContext_glCheckFramebufferStatus(const SoGLContext * glue, GLenum target);
void SoGLContext_glFramebufferTexture1D(const SoGLContext * glue, GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
void SoGLContext_glFramebufferTexture2D(const SoGLContext * glue, GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
void SoGLContext_glFramebufferTexture3D(const SoGLContext * glue, GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset);
void SoGLContext_glFramebufferRenderbuffer(const SoGLContext * glue, GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer);
void SoGLContext_glGetFramebufferAttachmentParameteriv(const SoGLContext * glue, GLenum target, GLenum attachment, GLenum pname, GLint *params);
void SoGLContext_glGenerateMipmap(const SoGLContext * glue, GLenum target);
SbBool SoGLContext_has_framebuffer_objects(const SoGLContext * glue);

/* Core GL 1.0/1.1 wrappers — always dispatch through the correct backend.
   Use these everywhere a bare gl* call would otherwise mix backends in
   dual-GL (OBOL_BUILD_DUAL_GL) builds. */
void SoGLContext_glTexImage2D(const SoGLContext * glue,
                              GLenum target, GLint level, GLint internalformat,
                              GLsizei width, GLsizei height, GLint border,
                              GLenum format, GLenum type, const GLvoid * pixels);
void SoGLContext_glTexParameteri(const SoGLContext * glue,
                                 GLenum target, GLenum pname, GLint param);
void SoGLContext_glTexParameterf(const SoGLContext * glue,
                                 GLenum target, GLenum pname, GLfloat param);
void SoGLContext_glGetIntegerv(const SoGLContext * glue,
                               GLenum pname, GLint * params);
void SoGLContext_glGetFloatv(const SoGLContext * glue,
                             GLenum pname, GLfloat * params);
void SoGLContext_glClearColor(const SoGLContext * glue,
                              GLclampf red, GLclampf green,
                              GLclampf blue, GLclampf alpha);
void SoGLContext_glClear(const SoGLContext * glue, GLbitfield mask);
void SoGLContext_glFlush(const SoGLContext * glue);
void SoGLContext_glFinish(const SoGLContext * glue);
GLenum SoGLContext_glGetError(const SoGLContext * glue);
const GLubyte * SoGLContext_glGetString(const SoGLContext * glue, GLenum name);
void SoGLContext_glEnable(const SoGLContext * glue, GLenum cap);
void SoGLContext_glDisable(const SoGLContext * glue, GLenum cap);
GLboolean SoGLContext_glIsEnabled(const SoGLContext * glue, GLenum cap);
void SoGLContext_glPixelStorei(const SoGLContext * glue,
                               GLenum pname, GLint param);
void SoGLContext_glReadPixels(const SoGLContext * glue,
                              GLint x, GLint y,
                              GLsizei width, GLsizei height,
                              GLenum format, GLenum type, GLvoid * pixels);
void SoGLContext_glCopyTexSubImage2D(const SoGLContext * glue,
                                     GLenum target, GLint level,
                                     GLint xoffset, GLint yoffset,
                                     GLint x, GLint y,
                                     GLsizei width, GLsizei height);

/* Additional core GL 1.0/1.1 dispatch wrappers. */
void SoGLContext_glBegin(const SoGLContext * glue, GLenum mode);
void SoGLContext_glEnd(const SoGLContext * glue);
void SoGLContext_glVertex2f(const SoGLContext * glue, GLfloat x, GLfloat y);
void SoGLContext_glVertex2s(const SoGLContext * glue, GLshort x, GLshort y);
void SoGLContext_glVertex3f(const SoGLContext * glue, GLfloat x, GLfloat y, GLfloat z);
void SoGLContext_glVertex3fv(const SoGLContext * glue, const GLfloat * v);
void SoGLContext_glVertex4fv(const SoGLContext * glue, const GLfloat * v);
void SoGLContext_glNormal3f(const SoGLContext * glue, GLfloat nx, GLfloat ny, GLfloat nz);
void SoGLContext_glNormal3fv(const SoGLContext * glue, const GLfloat * v);
void SoGLContext_glColor3f(const SoGLContext * glue, GLfloat r, GLfloat g, GLfloat b);
void SoGLContext_glColor3fv(const SoGLContext * glue, const GLfloat * v);
void SoGLContext_glColor3ub(const SoGLContext * glue, GLubyte r, GLubyte g, GLubyte b);
void SoGLContext_glColor3ubv(const SoGLContext * glue, const GLubyte * v);
void SoGLContext_glColor4ub(const SoGLContext * glue, GLubyte r, GLubyte g, GLubyte b, GLubyte a);
void SoGLContext_glTexCoord2f(const SoGLContext * glue, GLfloat s, GLfloat t);
void SoGLContext_glTexCoord2fv(const SoGLContext * glue, const GLfloat * v);
void SoGLContext_glTexCoord3f(const SoGLContext * glue, GLfloat s, GLfloat t, GLfloat r);
void SoGLContext_glTexCoord3fv(const SoGLContext * glue, const GLfloat * v);
void SoGLContext_glTexCoord4fv(const SoGLContext * glue, const GLfloat * v);
void SoGLContext_glIndexi(const SoGLContext * glue, GLint c);
void SoGLContext_glMatrixMode(const SoGLContext * glue, GLenum mode);
void SoGLContext_glLoadIdentity(const SoGLContext * glue);
void SoGLContext_glLoadMatrixf(const SoGLContext * glue, const GLfloat * m);
void SoGLContext_glLoadMatrixd(const SoGLContext * glue, const GLdouble * m);
void SoGLContext_glMultMatrixf(const SoGLContext * glue, const GLfloat * m);
void SoGLContext_glPushMatrix(const SoGLContext * glue);
void SoGLContext_glPopMatrix(const SoGLContext * glue);
void SoGLContext_glOrtho(const SoGLContext * glue, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val);
void SoGLContext_glFrustum(const SoGLContext * glue, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val);
void SoGLContext_glTranslatef(const SoGLContext * glue, GLfloat x, GLfloat y, GLfloat z);
void SoGLContext_glRotatef(const SoGLContext * glue, GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
void SoGLContext_glScalef(const SoGLContext * glue, GLfloat x, GLfloat y, GLfloat z);
void SoGLContext_glLightf(const SoGLContext * glue, GLenum light, GLenum pname, GLfloat param);
void SoGLContext_glLightfv(const SoGLContext * glue, GLenum light, GLenum pname, const GLfloat * params);
void SoGLContext_glLightModeli(const SoGLContext * glue, GLenum pname, GLint param);
void SoGLContext_glLightModelfv(const SoGLContext * glue, GLenum pname, const GLfloat * params);
void SoGLContext_glMaterialf(const SoGLContext * glue, GLenum face, GLenum pname, GLfloat param);
void SoGLContext_glMaterialfv(const SoGLContext * glue, GLenum face, GLenum pname, const GLfloat * params);
void SoGLContext_glColorMaterial(const SoGLContext * glue, GLenum face, GLenum mode);
void SoGLContext_glFogi(const SoGLContext * glue, GLenum pname, GLint param);
void SoGLContext_glFogf(const SoGLContext * glue, GLenum pname, GLfloat param);
void SoGLContext_glFogfv(const SoGLContext * glue, GLenum pname, const GLfloat * params);
void SoGLContext_glTexEnvi(const SoGLContext * glue, GLenum target, GLenum pname, GLint param);
void SoGLContext_glTexEnvf(const SoGLContext * glue, GLenum target, GLenum pname, GLfloat param);
void SoGLContext_glTexEnvfv(const SoGLContext * glue, GLenum target, GLenum pname, const GLfloat * params);
void SoGLContext_glTexGeni(const SoGLContext * glue, GLenum coord, GLenum pname, GLint param);
void SoGLContext_glTexGenf(const SoGLContext * glue, GLenum coord, GLenum pname, GLfloat param);
void SoGLContext_glTexGenfv(const SoGLContext * glue, GLenum coord, GLenum pname, const GLfloat * params);
void SoGLContext_glCopyTexImage2D(const SoGLContext * glue, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
void SoGLContext_glRasterPos2f(const SoGLContext * glue, GLfloat x, GLfloat y);
void SoGLContext_glRasterPos3f(const SoGLContext * glue, GLfloat x, GLfloat y, GLfloat z);
void SoGLContext_glBitmap(const SoGLContext * glue, GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte * bitmap);
void SoGLContext_glDrawPixels(const SoGLContext * glue, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid * pixels);
void SoGLContext_glPixelTransferf(const SoGLContext * glue, GLenum pname, GLfloat param);
void SoGLContext_glPixelTransferi(const SoGLContext * glue, GLenum pname, GLint param);
void SoGLContext_glPixelMapfv(const SoGLContext * glue, GLenum map, GLint mapsize, const GLfloat * values);
void SoGLContext_glPixelMapuiv(const SoGLContext * glue, GLenum map, GLint mapsize, const GLuint * values);
void SoGLContext_glPixelZoom(const SoGLContext * glue, GLfloat xfactor, GLfloat yfactor);
void SoGLContext_glViewport(const SoGLContext * glue, GLint x, GLint y, GLsizei width, GLsizei height);
void SoGLContext_glScissor(const SoGLContext * glue, GLint x, GLint y, GLsizei width, GLsizei height);
void SoGLContext_glDepthMask(const SoGLContext * glue, GLboolean flag);
void SoGLContext_glDepthFunc(const SoGLContext * glue, GLenum func);
void SoGLContext_glDepthRange(const SoGLContext * glue, GLclampd near_val, GLclampd far_val);
void SoGLContext_glStencilFunc(const SoGLContext * glue, GLenum func, GLint ref, GLuint mask);
void SoGLContext_glStencilOp(const SoGLContext * glue, GLenum fail, GLenum zfail, GLenum zpass);
void SoGLContext_glBlendFunc(const SoGLContext * glue, GLenum sfactor, GLenum dfactor);
void SoGLContext_glAlphaFunc(const SoGLContext * glue, GLenum func, GLclampf ref);
void SoGLContext_glFrontFace(const SoGLContext * glue, GLenum mode);
void SoGLContext_glCullFace(const SoGLContext * glue, GLenum mode);
void SoGLContext_glPolygonMode(const SoGLContext * glue, GLenum face, GLenum mode);
void SoGLContext_glPolygonStipple(const SoGLContext * glue, const GLubyte * mask);
void SoGLContext_glLineWidth(const SoGLContext * glue, GLfloat width);
void SoGLContext_glLineStipple(const SoGLContext * glue, GLint factor, GLushort pattern);
void SoGLContext_glPointSize(const SoGLContext * glue, GLfloat size);
void SoGLContext_glColorMask(const SoGLContext * glue, GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
void SoGLContext_glClipPlane(const SoGLContext * glue, GLenum plane, const GLdouble * equation);
void SoGLContext_glDrawBuffer(const SoGLContext * glue, GLenum mode);
void SoGLContext_glClearIndex(const SoGLContext * glue, GLfloat c);
void SoGLContext_glClearStencil(const SoGLContext * glue, GLint s);
void SoGLContext_glAccum(const SoGLContext * glue, GLenum op, GLfloat value);
void SoGLContext_glGetBooleanv(const SoGLContext * glue, GLenum pname, GLboolean * params);
void SoGLContext_glNewList(const SoGLContext * glue, GLuint list, GLenum mode);
void SoGLContext_glEndList(const SoGLContext * glue);
void SoGLContext_glCallList(const SoGLContext * glue, GLuint list);
void SoGLContext_glDeleteLists(const SoGLContext * glue, GLuint list, GLsizei range);
GLuint SoGLContext_glGenLists(const SoGLContext * glue, GLsizei range);
void SoGLContext_glPushAttrib(const SoGLContext * glue, GLbitfield mask);
void SoGLContext_glPopAttrib(const SoGLContext * glue);

/* GL feature queries */
SbBool SoGLContext_can_do_bumpmapping(const SoGLContext * glue);
SbBool SoGLContext_can_do_sortedlayersblend(const SoGLContext * glue);
SbBool SoGLContext_can_do_anisotropic_filtering(const SoGLContext * glue);

/* GL limits */
int SoGLContext_get_max_lights(const SoGLContext * glue);
const float * SoGLContext_get_line_width_range(const SoGLContext * glue);
const float * SoGLContext_get_point_size_range(const SoGLContext * glue);

float SoGLContext_get_max_anisotropy(const SoGLContext * glue);

/* GLX extensions */
void * SoGLContext_glXGetCurrentDisplay(const SoGLContext * w);

/* Offscreen buffer creation */
void SoGLContext_context_max_dimensions(void * mgr, unsigned int * width, unsigned int * height);

void SoGLContext_context_bind_pbuffer(void * ctx);
void SoGLContext_context_release_pbuffer(void * ctx);
SbBool SoGLContext_context_pbuffer_is_bound(void * ctx);
SbBool SoGLContext_context_can_render_to_texture(void * ctx);

const void * SoGLContext_win32_HDC(void * ctx);
void SoGLContext_win32_updateHDCBitmap(void * ctx);

/* -----------------------------------------------------------------------
 * Dual-GL backend registration
 *
 * When building with OBOL_BUILD_DUAL_GL=ON, both system-OpenGL and OSMesa
 * variants of the GL glue layer are compiled into the same library.
 * Applications (or CoinOffscreenGLCanvas) must call this function after
 * assigning a render-context ID to a context that was created via the OSMesa
 * backend.  This allows SoGLContext_instance() to dispatch the context
 * initialisation to the osmesa_SoGLContext_instance() implementation.
 *
 * Both functions are always declared and safe to call; they are no-ops in
 * non-dual-GL builds.
 * --------------------------------------------------------------------- */
void coingl_register_osmesa_context(int contextid);
void coingl_unregister_osmesa_context(int contextid);

/* Per-context-ID manager registry.  Call coingl_register_context_manager()
   after assigning a render-context ID whenever the context was created via a
   SoDB::ContextManager (e.g. inside CoinOffscreenGLCanvas::tryActivateGLContext()).
   SoGLContext_instance() reads this registry to set gi->context_manager so that
   SoGLContext_getprocaddress() can use the backend-specific resolver instead of
   the global singleton.  Both functions are always safe to call.
   The mgr parameter is typed void* here for C compatibility; callers must pass
   a SoDB::ContextManager* (cast to void* if necessary). */
void coingl_register_context_manager(int contextid, void * mgr);
void coingl_unregister_context_manager(int contextid);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* !OBOL_GLUE_GLP_H */
