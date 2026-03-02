#ifndef OBOL_SOGL_DISPATCH_H
#define OBOL_SOGL_DISPATCH_H

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

/*
 * SoGLDispatch.h  --  Public API for Obol's backend-dispatched GL wrappers.
 *
 * PURPOSE
 * -------
 * In single-backend Obol builds (system OpenGL only, or pure OSMesa) raw
 * gl*() calls work fine because there is only one backend.  In dual-backend
 * builds (OBOL_BUILD_DUAL_GL) both system OpenGL and OSMesa contexts coexist
 * in the same process; a raw gl*() call is always routed to the system GL
 * library regardless of which context is currently active, so OSMesa renders
 * receive the wrong calls.
 *
 * This header exposes the dispatch layer so user code (SoCallback handlers,
 * custom nodes, etc.) can make GL calls that are correctly routed to whatever
 * backend owns the active rendering context.
 *
 * USAGE – EXPLICIT FORM
 * ---------------------
 * Obtain a dispatch context and call SoGLContext_glXxx():
 *
 *   #include <Inventor/system/SoGLDispatch.h>
 *   #include <Inventor/actions/SoGLRenderAction.h>
 *
 *   void myCallback(void * data, SoAction * action)
 *   {
 *       if (!action->isOfType(SoGLRenderAction::getClassTypeId())) return;
 *       const SoGLContext * ctx = sogl_glue_from_state(action->getState());
 *       SoGLContext_glMatrixMode(ctx, GL_PROJECTION);
 *       SoGLContext_glPushMatrix(ctx);
 *       SoGLContext_glLoadIdentity(ctx);
 *       SoGLContext_glOrtho(ctx, 0, w, 0, h, -1, 1);
 *       SoGLContext_glBegin(ctx, GL_QUADS);
 *         SoGLContext_glColor3f(ctx, 0.2f, 0.4f, 0.8f);
 *         SoGLContext_glVertex2f(ctx, 0.0f, 0.0f);
 *         // ...
 *       SoGLContext_glEnd(ctx);
 *       SoGLContext_glMatrixMode(ctx, GL_PROJECTION);
 *       SoGLContext_glPopMatrix(ctx);
 *   }
 *
 * USAGE – IMPLICIT FORM (Inventor/gl.h trick header)
 * ---------------------------------------------------
 * For code that already uses bare gl*() calls, include <Inventor/gl.h>
 * instead of <GL/gl.h>.  That header includes SoGLDispatch.h and defines
 * macros that redirect every gl*() call through sogl_current_render_glue(),
 * which holds the active context for the duration of a rendering pass.
 * No other source changes are required.
 */

#include <Inventor/basic.h>
#include <Inventor/system/gl.h>   /* GL types: GLenum, GLfloat, GLsizei … */

/* Forward declarations – SoGLContext is intentionally opaque to callers. */
struct SoGLContext;
class  SoState;

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Context retrieval
 * --------------------------------------------------------------------- */

/**
 * Return the GL dispatch context for the render pass currently active on
 * this thread.  Set by SoGLRenderAction before each traversal pass and
 * cleared afterwards.  Valid inside any SoCallback that is invoked during
 * a GL render action; returns NULL outside of a render pass.
 */
OBOL_DLL_API const SoGLContext * sogl_current_render_glue(void);

/**
 * Derive the GL dispatch context from an SoState pointer.
 * Equivalent to sogl_current_render_glue() but uses an explicit state
 * pointer, which is slightly safer (no dependency on thread-local storage).
 * Typical use: pass action->getState() from your SoCallback.
 */
OBOL_DLL_API const SoGLContext * sogl_glue_from_state(const SoState * state);

/* -----------------------------------------------------------------------
 * Core GL 1.0/1.1 dispatch wrappers
 *
 * Every function routes through the correct backend at runtime.
 * Signature: (const SoGLContext * ctx, <original GL parameters>)
 * --------------------------------------------------------------------- */

/* --- Immediate mode geometry --- */
OBOL_DLL_API void SoGLContext_glBegin(const SoGLContext * glue, GLenum mode);
OBOL_DLL_API void SoGLContext_glEnd(const SoGLContext * glue);
OBOL_DLL_API void SoGLContext_glVertex2f(const SoGLContext * glue, GLfloat x, GLfloat y);
OBOL_DLL_API void SoGLContext_glVertex2s(const SoGLContext * glue, GLshort x, GLshort y);
OBOL_DLL_API void SoGLContext_glVertex3f(const SoGLContext * glue, GLfloat x, GLfloat y, GLfloat z);
OBOL_DLL_API void SoGLContext_glVertex3fv(const SoGLContext * glue, const GLfloat * v);
OBOL_DLL_API void SoGLContext_glVertex4fv(const SoGLContext * glue, const GLfloat * v);
OBOL_DLL_API void SoGLContext_glNormal3f(const SoGLContext * glue, GLfloat nx, GLfloat ny, GLfloat nz);
OBOL_DLL_API void SoGLContext_glNormal3fv(const SoGLContext * glue, const GLfloat * v);
OBOL_DLL_API void SoGLContext_glColor3f(const SoGLContext * glue, GLfloat r, GLfloat g, GLfloat b);
OBOL_DLL_API void SoGLContext_glColor3fv(const SoGLContext * glue, const GLfloat * v);
OBOL_DLL_API void SoGLContext_glColor3ub(const SoGLContext * glue, GLubyte r, GLubyte g, GLubyte b);
OBOL_DLL_API void SoGLContext_glColor3ubv(const SoGLContext * glue, const GLubyte * v);
OBOL_DLL_API void SoGLContext_glColor4ub(const SoGLContext * glue, GLubyte r, GLubyte g, GLubyte b, GLubyte a);
OBOL_DLL_API void SoGLContext_glTexCoord2f(const SoGLContext * glue, GLfloat s, GLfloat t);
OBOL_DLL_API void SoGLContext_glTexCoord2fv(const SoGLContext * glue, const GLfloat * v);
OBOL_DLL_API void SoGLContext_glTexCoord3f(const SoGLContext * glue, GLfloat s, GLfloat t, GLfloat r);
OBOL_DLL_API void SoGLContext_glTexCoord3fv(const SoGLContext * glue, const GLfloat * v);
OBOL_DLL_API void SoGLContext_glTexCoord4fv(const SoGLContext * glue, const GLfloat * v);
OBOL_DLL_API void SoGLContext_glIndexi(const SoGLContext * glue, GLint c);

/* --- Matrix stack --- */
OBOL_DLL_API void SoGLContext_glMatrixMode(const SoGLContext * glue, GLenum mode);
OBOL_DLL_API void SoGLContext_glLoadIdentity(const SoGLContext * glue);
OBOL_DLL_API void SoGLContext_glLoadMatrixf(const SoGLContext * glue, const GLfloat * m);
OBOL_DLL_API void SoGLContext_glLoadMatrixd(const SoGLContext * glue, const GLdouble * m);
OBOL_DLL_API void SoGLContext_glMultMatrixf(const SoGLContext * glue, const GLfloat * m);
OBOL_DLL_API void SoGLContext_glPushMatrix(const SoGLContext * glue);
OBOL_DLL_API void SoGLContext_glPopMatrix(const SoGLContext * glue);
OBOL_DLL_API void SoGLContext_glOrtho(const SoGLContext * glue, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val);
OBOL_DLL_API void SoGLContext_glFrustum(const SoGLContext * glue, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val);
OBOL_DLL_API void SoGLContext_glTranslatef(const SoGLContext * glue, GLfloat x, GLfloat y, GLfloat z);
OBOL_DLL_API void SoGLContext_glRotatef(const SoGLContext * glue, GLfloat angle, GLfloat x, GLfloat y, GLfloat z);
OBOL_DLL_API void SoGLContext_glScalef(const SoGLContext * glue, GLfloat x, GLfloat y, GLfloat z);

/* --- Render state --- */
OBOL_DLL_API void SoGLContext_glEnable(const SoGLContext * glue, GLenum cap);
OBOL_DLL_API void SoGLContext_glDisable(const SoGLContext * glue, GLenum cap);
OBOL_DLL_API GLboolean SoGLContext_glIsEnabled(const SoGLContext * glue, GLenum cap);
OBOL_DLL_API void SoGLContext_glPushAttrib(const SoGLContext * glue, GLbitfield mask);
OBOL_DLL_API void SoGLContext_glPopAttrib(const SoGLContext * glue);
OBOL_DLL_API void SoGLContext_glDepthMask(const SoGLContext * glue, GLboolean flag);
OBOL_DLL_API void SoGLContext_glDepthFunc(const SoGLContext * glue, GLenum func);
OBOL_DLL_API void SoGLContext_glDepthRange(const SoGLContext * glue, GLclampd near_val, GLclampd far_val);
OBOL_DLL_API void SoGLContext_glBlendFunc(const SoGLContext * glue, GLenum sfactor, GLenum dfactor);
OBOL_DLL_API void SoGLContext_glAlphaFunc(const SoGLContext * glue, GLenum func, GLclampf ref);
OBOL_DLL_API void SoGLContext_glViewport(const SoGLContext * glue, GLint x, GLint y, GLsizei width, GLsizei height);
OBOL_DLL_API void SoGLContext_glScissor(const SoGLContext * glue, GLint x, GLint y, GLsizei width, GLsizei height);
OBOL_DLL_API void SoGLContext_glClearColor(const SoGLContext * glue, GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
OBOL_DLL_API void SoGLContext_glClear(const SoGLContext * glue, GLbitfield mask);
OBOL_DLL_API void SoGLContext_glFlush(const SoGLContext * glue);
OBOL_DLL_API void SoGLContext_glFinish(const SoGLContext * glue);
OBOL_DLL_API GLenum SoGLContext_glGetError(const SoGLContext * glue);
OBOL_DLL_API const GLubyte * SoGLContext_glGetString(const SoGLContext * glue, GLenum name);
OBOL_DLL_API void SoGLContext_glGetIntegerv(const SoGLContext * glue, GLenum pname, GLint * params);
OBOL_DLL_API void SoGLContext_glGetFloatv(const SoGLContext * glue, GLenum pname, GLfloat * params);
OBOL_DLL_API void SoGLContext_glGetBooleanv(const SoGLContext * glue, GLenum pname, GLboolean * params);
OBOL_DLL_API void SoGLContext_glColorMask(const SoGLContext * glue, GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
OBOL_DLL_API void SoGLContext_glStencilFunc(const SoGLContext * glue, GLenum func, GLint ref, GLuint mask);
OBOL_DLL_API void SoGLContext_glStencilOp(const SoGLContext * glue, GLenum fail, GLenum zfail, GLenum zpass);
OBOL_DLL_API void SoGLContext_glFrontFace(const SoGLContext * glue, GLenum mode);
OBOL_DLL_API void SoGLContext_glCullFace(const SoGLContext * glue, GLenum mode);
OBOL_DLL_API void SoGLContext_glPolygonMode(const SoGLContext * glue, GLenum face, GLenum mode);
OBOL_DLL_API void SoGLContext_glPolygonStipple(const SoGLContext * glue, const GLubyte * mask);
OBOL_DLL_API void SoGLContext_glLineWidth(const SoGLContext * glue, GLfloat width);
OBOL_DLL_API void SoGLContext_glLineStipple(const SoGLContext * glue, GLint factor, GLushort pattern);
OBOL_DLL_API void SoGLContext_glPointSize(const SoGLContext * glue, GLfloat size);
OBOL_DLL_API void SoGLContext_glClipPlane(const SoGLContext * glue, GLenum plane, const GLdouble * equation);
OBOL_DLL_API void SoGLContext_glDrawBuffer(const SoGLContext * glue, GLenum mode);
OBOL_DLL_API void SoGLContext_glClearIndex(const SoGLContext * glue, GLfloat c);
OBOL_DLL_API void SoGLContext_glClearStencil(const SoGLContext * glue, GLint s);
OBOL_DLL_API void SoGLContext_glAccum(const SoGLContext * glue, GLenum op, GLfloat value);

/* --- Pixel / image operations --- */
OBOL_DLL_API void SoGLContext_glPixelStorei(const SoGLContext * glue, GLenum pname, GLint param);
OBOL_DLL_API void SoGLContext_glReadPixels(const SoGLContext * glue, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid * pixels);
OBOL_DLL_API void SoGLContext_glRasterPos2f(const SoGLContext * glue, GLfloat x, GLfloat y);
OBOL_DLL_API void SoGLContext_glRasterPos3f(const SoGLContext * glue, GLfloat x, GLfloat y, GLfloat z);
OBOL_DLL_API void SoGLContext_glBitmap(const SoGLContext * glue, GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte * bitmap);
OBOL_DLL_API void SoGLContext_glDrawPixels(const SoGLContext * glue, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid * pixels);
OBOL_DLL_API void SoGLContext_glPixelTransferf(const SoGLContext * glue, GLenum pname, GLfloat param);
OBOL_DLL_API void SoGLContext_glPixelTransferi(const SoGLContext * glue, GLenum pname, GLint param);
OBOL_DLL_API void SoGLContext_glPixelMapfv(const SoGLContext * glue, GLenum map, GLint mapsize, const GLfloat * values);
OBOL_DLL_API void SoGLContext_glPixelMapuiv(const SoGLContext * glue, GLenum map, GLint mapsize, const GLuint * values);
OBOL_DLL_API void SoGLContext_glPixelZoom(const SoGLContext * glue, GLfloat xfactor, GLfloat yfactor);

/* --- Lighting / material --- */
OBOL_DLL_API void SoGLContext_glLightf(const SoGLContext * glue, GLenum light, GLenum pname, GLfloat param);
OBOL_DLL_API void SoGLContext_glLightfv(const SoGLContext * glue, GLenum light, GLenum pname, const GLfloat * params);
OBOL_DLL_API void SoGLContext_glLightModeli(const SoGLContext * glue, GLenum pname, GLint param);
OBOL_DLL_API void SoGLContext_glLightModelfv(const SoGLContext * glue, GLenum pname, const GLfloat * params);
OBOL_DLL_API void SoGLContext_glMaterialf(const SoGLContext * glue, GLenum face, GLenum pname, GLfloat param);
OBOL_DLL_API void SoGLContext_glMaterialfv(const SoGLContext * glue, GLenum face, GLenum pname, const GLfloat * params);
OBOL_DLL_API void SoGLContext_glColorMaterial(const SoGLContext * glue, GLenum face, GLenum mode);

/* --- Fog --- */
OBOL_DLL_API void SoGLContext_glFogi(const SoGLContext * glue, GLenum pname, GLint param);
OBOL_DLL_API void SoGLContext_glFogf(const SoGLContext * glue, GLenum pname, GLfloat param);
OBOL_DLL_API void SoGLContext_glFogfv(const SoGLContext * glue, GLenum pname, const GLfloat * params);

/* --- Textures --- */
OBOL_DLL_API void SoGLContext_glTexEnvi(const SoGLContext * glue, GLenum target, GLenum pname, GLint param);
OBOL_DLL_API void SoGLContext_glTexEnvf(const SoGLContext * glue, GLenum target, GLenum pname, GLfloat param);
OBOL_DLL_API void SoGLContext_glTexEnvfv(const SoGLContext * glue, GLenum target, GLenum pname, const GLfloat * params);
OBOL_DLL_API void SoGLContext_glTexGeni(const SoGLContext * glue, GLenum coord, GLenum pname, GLint param);
OBOL_DLL_API void SoGLContext_glTexGenf(const SoGLContext * glue, GLenum coord, GLenum pname, GLfloat param);
OBOL_DLL_API void SoGLContext_glTexGenfv(const SoGLContext * glue, GLenum coord, GLenum pname, const GLfloat * params);
OBOL_DLL_API void SoGLContext_glTexParameteri(const SoGLContext * glue, GLenum target, GLenum pname, GLint param);
OBOL_DLL_API void SoGLContext_glTexParameterf(const SoGLContext * glue, GLenum target, GLenum pname, GLfloat param);
OBOL_DLL_API void SoGLContext_glTexImage2D(const SoGLContext * glue, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid * pixels);
OBOL_DLL_API void SoGLContext_glCopyTexImage2D(const SoGLContext * glue, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border);
OBOL_DLL_API void SoGLContext_glCopyTexSubImage2D(const SoGLContext * glue, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height);

/* --- Display lists --- */
OBOL_DLL_API void  SoGLContext_glNewList(const SoGLContext * glue, GLuint list, GLenum mode);
OBOL_DLL_API void  SoGLContext_glEndList(const SoGLContext * glue);
OBOL_DLL_API void  SoGLContext_glCallList(const SoGLContext * glue, GLuint list);
OBOL_DLL_API void  SoGLContext_glDeleteLists(const SoGLContext * glue, GLuint list, GLsizei range);
OBOL_DLL_API GLuint SoGLContext_glGenLists(const SoGLContext * glue, GLsizei range);

/* --- Vertex arrays --- */
OBOL_DLL_API void SoGLContext_glDrawArrays(const SoGLContext * glue, GLenum mode, GLint first, GLsizei count);
OBOL_DLL_API void SoGLContext_glDrawElements(const SoGLContext * glue, GLenum mode, GLsizei count, GLenum type, const GLvoid * indices);

/* --- Full OSMesa GL 1.0-1.3 dispatch declarations ---
 * Covers all core functions guaranteed by external/osmesa/include/OSMesa/gl.h. */
OBOL_DLL_API void SoGLContext_glActiveTexture(const SoGLContext * glue, GLenum texture);
OBOL_DLL_API void SoGLContext_glArrayElement(const SoGLContext * glue, GLint i);
OBOL_DLL_API void SoGLContext_glBindTexture(const SoGLContext * glue, GLenum target, GLuint texture);
OBOL_DLL_API void SoGLContext_glBlendEquation(const SoGLContext * glue, GLenum mode);
OBOL_DLL_API void SoGLContext_glClientActiveTexture(const SoGLContext * glue, GLenum texture);
OBOL_DLL_API void SoGLContext_glColorPointer(const SoGLContext * glue, GLint size, GLenum type, GLsizei stride, const GLvoid * pointer);
OBOL_DLL_API void SoGLContext_glColorSubTable(const SoGLContext * glue, GLenum target, GLsizei start, GLsizei count, GLenum format, GLenum type, const GLvoid * data);
OBOL_DLL_API void SoGLContext_glColorTable(const SoGLContext * glue, GLenum target, GLenum internalFormat, GLsizei width, GLenum format, GLenum type, const GLvoid *table);
OBOL_DLL_API void SoGLContext_glCompressedTexImage1D(const SoGLContext * glue, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const GLvoid *data);
OBOL_DLL_API void SoGLContext_glCompressedTexImage2D(const SoGLContext * glue, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid *data);
OBOL_DLL_API void SoGLContext_glCompressedTexImage3D(const SoGLContext * glue, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const GLvoid * data);
OBOL_DLL_API void SoGLContext_glCompressedTexSubImage1D(const SoGLContext * glue, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const GLvoid *data);
OBOL_DLL_API void SoGLContext_glCompressedTexSubImage2D(const SoGLContext * glue, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const GLvoid *data);
OBOL_DLL_API void SoGLContext_glCompressedTexSubImage3D(const SoGLContext * glue, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const GLvoid *data);
OBOL_DLL_API void SoGLContext_glCopyTexSubImage3D(const SoGLContext * glue, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height);
OBOL_DLL_API void SoGLContext_glDeleteTextures(const SoGLContext * glue, GLsizei n, const GLuint * textures);
OBOL_DLL_API void SoGLContext_glDisableClientState(const SoGLContext * glue, GLenum array);
OBOL_DLL_API void SoGLContext_glDrawRangeElements(const SoGLContext * glue, GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid * indices);
OBOL_DLL_API void SoGLContext_glEnableClientState(const SoGLContext * glue, GLenum array);
OBOL_DLL_API void SoGLContext_glGenTextures(const SoGLContext * glue, GLsizei n, GLuint *textures);
OBOL_DLL_API void SoGLContext_glGetColorTable(const SoGLContext * glue, GLenum target, GLenum format, GLenum type, GLvoid *data);
OBOL_DLL_API void SoGLContext_glGetColorTableParameterfv(const SoGLContext * glue, GLenum target, GLenum pname, GLfloat *params);
OBOL_DLL_API void SoGLContext_glGetColorTableParameteriv(const SoGLContext * glue, GLenum target, GLenum pname, GLint *params);
OBOL_DLL_API void SoGLContext_glGetCompressedTexImage(const SoGLContext * glue, GLenum target, GLint level, void *img);
OBOL_DLL_API void SoGLContext_glIndexPointer (const SoGLContext * glue, GLenum type, GLsizei stride, const GLvoid * pointer);
OBOL_DLL_API void SoGLContext_glInterleavedArrays(const SoGLContext * glue, GLenum format, GLsizei stride, const GLvoid * pointer);
OBOL_DLL_API void SoGLContext_glMultiTexCoord2f(const SoGLContext * glue, GLenum target, GLfloat s, GLfloat t);
OBOL_DLL_API void SoGLContext_glMultiTexCoord2fv(const SoGLContext * glue, GLenum target, const GLfloat * v);
OBOL_DLL_API void SoGLContext_glMultiTexCoord3fv(const SoGLContext * glue, GLenum target, const GLfloat * v);
OBOL_DLL_API void SoGLContext_glMultiTexCoord4fv(const SoGLContext * glue, GLenum target, const GLfloat * v);
OBOL_DLL_API void SoGLContext_glNormalPointer(const SoGLContext * glue, GLenum type, GLsizei stride, const GLvoid *pointer);
OBOL_DLL_API void SoGLContext_glPolygonOffset(const SoGLContext * glue, GLfloat factor, GLfloat units);
OBOL_DLL_API void SoGLContext_glPopClientAttrib(const SoGLContext * glue);
OBOL_DLL_API void SoGLContext_glPushClientAttrib(const SoGLContext * glue, GLbitfield mask);
OBOL_DLL_API void SoGLContext_glTexCoordPointer(const SoGLContext * glue, GLint size, GLenum type, GLsizei stride, const GLvoid * pointer);
OBOL_DLL_API void SoGLContext_glTexImage3D(const SoGLContext * glue, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels);
OBOL_DLL_API void SoGLContext_glTexSubImage2D(const SoGLContext * glue, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid * pixels);
OBOL_DLL_API void SoGLContext_glTexSubImage3D(const SoGLContext * glue, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid * pixels);
OBOL_DLL_API void SoGLContext_glVertexPointer(const SoGLContext * glue, GLint size, GLenum type, GLsizei stride, const GLvoid * pointer);
OBOL_DLL_API GLboolean SoGLContext_glAreTexturesResident(const SoGLContext * glue, GLsizei n, const GLuint * textures, GLboolean * residences);
OBOL_DLL_API void SoGLContext_glBlendColor(const SoGLContext * glue, GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha);
OBOL_DLL_API void SoGLContext_glCallLists(const SoGLContext * glue, GLsizei n, GLenum type, const GLvoid * lists);
OBOL_DLL_API void SoGLContext_glClearAccum(const SoGLContext * glue, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
OBOL_DLL_API void SoGLContext_glClearDepth(const SoGLContext * glue, GLclampd depth);
OBOL_DLL_API void SoGLContext_glColor3b(const SoGLContext * glue, GLbyte red, GLbyte green, GLbyte blue);
OBOL_DLL_API void SoGLContext_glColor3bv(const SoGLContext * glue, const GLbyte * v);
OBOL_DLL_API void SoGLContext_glColor3d(const SoGLContext * glue, GLdouble red, GLdouble green, GLdouble blue);
OBOL_DLL_API void SoGLContext_glColor3dv(const SoGLContext * glue, const GLdouble * v);
OBOL_DLL_API void SoGLContext_glColor3i(const SoGLContext * glue, GLint red, GLint green, GLint blue);
OBOL_DLL_API void SoGLContext_glColor3iv(const SoGLContext * glue, const GLint * v);
OBOL_DLL_API void SoGLContext_glColor3s(const SoGLContext * glue, GLshort red, GLshort green, GLshort blue);
OBOL_DLL_API void SoGLContext_glColor3sv(const SoGLContext * glue, const GLshort * v);
OBOL_DLL_API void SoGLContext_glColor3ui(const SoGLContext * glue, GLuint red, GLuint green, GLuint blue);
OBOL_DLL_API void SoGLContext_glColor3uiv(const SoGLContext * glue, const GLuint * v);
OBOL_DLL_API void SoGLContext_glColor3us(const SoGLContext * glue, GLushort red, GLushort green, GLushort blue);
OBOL_DLL_API void SoGLContext_glColor3usv(const SoGLContext * glue, const GLushort * v);
OBOL_DLL_API void SoGLContext_glColor4b(const SoGLContext * glue, GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha);
OBOL_DLL_API void SoGLContext_glColor4bv(const SoGLContext * glue, const GLbyte * v);
OBOL_DLL_API void SoGLContext_glColor4d(const SoGLContext * glue, GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha);
OBOL_DLL_API void SoGLContext_glColor4dv(const SoGLContext * glue, const GLdouble * v);
OBOL_DLL_API void SoGLContext_glColor4f(const SoGLContext * glue, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
OBOL_DLL_API void SoGLContext_glColor4fv(const SoGLContext * glue, const GLfloat * v);
OBOL_DLL_API void SoGLContext_glColor4i(const SoGLContext * glue, GLint red, GLint green, GLint blue, GLint alpha);
OBOL_DLL_API void SoGLContext_glColor4iv(const SoGLContext * glue, const GLint * v);
OBOL_DLL_API void SoGLContext_glColor4s(const SoGLContext * glue, GLshort red, GLshort green, GLshort blue, GLshort alpha);
OBOL_DLL_API void SoGLContext_glColor4sv(const SoGLContext * glue, const GLshort * v);
OBOL_DLL_API void SoGLContext_glColor4ubv(const SoGLContext * glue, const GLubyte * v);
OBOL_DLL_API void SoGLContext_glColor4ui(const SoGLContext * glue, GLuint red, GLuint green, GLuint blue, GLuint alpha);
OBOL_DLL_API void SoGLContext_glColor4uiv(const SoGLContext * glue, const GLuint * v);
OBOL_DLL_API void SoGLContext_glColor4us(const SoGLContext * glue, GLushort red, GLushort green, GLushort blue, GLushort alpha);
OBOL_DLL_API void SoGLContext_glColor4usv(const SoGLContext * glue, const GLushort * v);
OBOL_DLL_API void SoGLContext_glColorTableParameterfv(const SoGLContext * glue, GLenum target, GLenum pname, const GLfloat * params);
OBOL_DLL_API void SoGLContext_glColorTableParameteriv(const SoGLContext * glue, GLenum target, GLenum pname, const GLint * params);
OBOL_DLL_API void SoGLContext_glConvolutionFilter1D(const SoGLContext * glue, GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, const GLvoid * image);
OBOL_DLL_API void SoGLContext_glConvolutionFilter2D(const SoGLContext * glue, GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid * image);
OBOL_DLL_API void SoGLContext_glConvolutionParameterf(const SoGLContext * glue, GLenum target, GLenum pname, GLfloat params);
OBOL_DLL_API void SoGLContext_glConvolutionParameterfv(const SoGLContext * glue, GLenum target, GLenum pname, const GLfloat * params);
OBOL_DLL_API void SoGLContext_glConvolutionParameteri(const SoGLContext * glue, GLenum target, GLenum pname, GLint params);
OBOL_DLL_API void SoGLContext_glConvolutionParameteriv(const SoGLContext * glue, GLenum target, GLenum pname, const GLint * params);
OBOL_DLL_API void SoGLContext_glCopyColorSubTable(const SoGLContext * glue, GLenum target, GLsizei start, GLint x, GLint y, GLsizei width);
OBOL_DLL_API void SoGLContext_glCopyColorTable(const SoGLContext * glue, GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width);
OBOL_DLL_API void SoGLContext_glCopyConvolutionFilter1D(const SoGLContext * glue, GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width);
OBOL_DLL_API void SoGLContext_glCopyConvolutionFilter2D(const SoGLContext * glue, GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height);
OBOL_DLL_API void SoGLContext_glCopyPixels(const SoGLContext * glue, GLint x, GLint y, GLsizei width, GLsizei height, GLenum type);
OBOL_DLL_API void SoGLContext_glCopyTexImage1D(const SoGLContext * glue, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border);
OBOL_DLL_API void SoGLContext_glCopyTexSubImage1D(const SoGLContext * glue, GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width);
OBOL_DLL_API void SoGLContext_glEdgeFlag(const SoGLContext * glue, GLboolean flag);
OBOL_DLL_API void SoGLContext_glEdgeFlagPointer(const SoGLContext * glue, GLsizei stride, const GLvoid * ptr);
OBOL_DLL_API void SoGLContext_glEdgeFlagv(const SoGLContext * glue, const GLboolean * flag);
OBOL_DLL_API void SoGLContext_glEvalCoord1d(const SoGLContext * glue, GLdouble u);
OBOL_DLL_API void SoGLContext_glEvalCoord1dv(const SoGLContext * glue, const GLdouble * u);
OBOL_DLL_API void SoGLContext_glEvalCoord1f(const SoGLContext * glue, GLfloat u);
OBOL_DLL_API void SoGLContext_glEvalCoord1fv(const SoGLContext * glue, const GLfloat * u);
OBOL_DLL_API void SoGLContext_glEvalCoord2d(const SoGLContext * glue, GLdouble u, GLdouble v);
OBOL_DLL_API void SoGLContext_glEvalCoord2dv(const SoGLContext * glue, const GLdouble * u);
OBOL_DLL_API void SoGLContext_glEvalCoord2f(const SoGLContext * glue, GLfloat u, GLfloat v);
OBOL_DLL_API void SoGLContext_glEvalCoord2fv(const SoGLContext * glue, const GLfloat * u);
OBOL_DLL_API void SoGLContext_glEvalMesh1(const SoGLContext * glue, GLenum mode, GLint i1, GLint i2);
OBOL_DLL_API void SoGLContext_glEvalMesh2(const SoGLContext * glue, GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2);
OBOL_DLL_API void SoGLContext_glEvalPoint1(const SoGLContext * glue, GLint i);
OBOL_DLL_API void SoGLContext_glEvalPoint2(const SoGLContext * glue, GLint i, GLint j);
OBOL_DLL_API void SoGLContext_glFeedbackBuffer(const SoGLContext * glue, GLsizei size, GLenum type, GLfloat * buffer);
OBOL_DLL_API void SoGLContext_glFogiv(const SoGLContext * glue, GLenum pname, const GLint * params);
OBOL_DLL_API void SoGLContext_glGetClipPlane(const SoGLContext * glue, GLenum plane, GLdouble * equation);
OBOL_DLL_API void SoGLContext_glGetConvolutionFilter(const SoGLContext * glue, GLenum target, GLenum format, GLenum type, GLvoid * image);
OBOL_DLL_API void SoGLContext_glGetConvolutionParameterfv(const SoGLContext * glue, GLenum target, GLenum pname, GLfloat * params);
OBOL_DLL_API void SoGLContext_glGetConvolutionParameteriv(const SoGLContext * glue, GLenum target, GLenum pname, GLint * params);
OBOL_DLL_API void SoGLContext_glGetDoublev(const SoGLContext * glue, GLenum pname, GLdouble * params);
OBOL_DLL_API void SoGLContext_glGetHistogram(const SoGLContext * glue, GLenum target, GLboolean reset, GLenum format, GLenum type, GLvoid * values);
OBOL_DLL_API void SoGLContext_glGetHistogramParameterfv(const SoGLContext * glue, GLenum target, GLenum pname, GLfloat * params);
OBOL_DLL_API void SoGLContext_glGetHistogramParameteriv(const SoGLContext * glue, GLenum target, GLenum pname, GLint * params);
OBOL_DLL_API void SoGLContext_glGetLightfv(const SoGLContext * glue, GLenum light, GLenum pname, GLfloat * params);
OBOL_DLL_API void SoGLContext_glGetLightiv(const SoGLContext * glue, GLenum light, GLenum pname, GLint * params);
OBOL_DLL_API void SoGLContext_glGetMapdv(const SoGLContext * glue, GLenum target, GLenum query, GLdouble * v);
OBOL_DLL_API void SoGLContext_glGetMapfv(const SoGLContext * glue, GLenum target, GLenum query, GLfloat * v);
OBOL_DLL_API void SoGLContext_glGetMapiv(const SoGLContext * glue, GLenum target, GLenum query, GLint * v);
OBOL_DLL_API void SoGLContext_glGetMaterialfv(const SoGLContext * glue, GLenum face, GLenum pname, GLfloat * params);
OBOL_DLL_API void SoGLContext_glGetMaterialiv(const SoGLContext * glue, GLenum face, GLenum pname, GLint * params);
OBOL_DLL_API void SoGLContext_glGetMinmax(const SoGLContext * glue, GLenum target, GLboolean reset, GLenum format, GLenum types, GLvoid * values);
OBOL_DLL_API void SoGLContext_glGetMinmaxParameterfv(const SoGLContext * glue, GLenum target, GLenum pname, GLfloat * params);
OBOL_DLL_API void SoGLContext_glGetMinmaxParameteriv(const SoGLContext * glue, GLenum target, GLenum pname, GLint * params);
OBOL_DLL_API void SoGLContext_glGetPixelMapfv(const SoGLContext * glue, GLenum map, GLfloat * values);
OBOL_DLL_API void SoGLContext_glGetPixelMapuiv(const SoGLContext * glue, GLenum map, GLuint * values);
OBOL_DLL_API void SoGLContext_glGetPixelMapusv(const SoGLContext * glue, GLenum map, GLushort * values);
OBOL_DLL_API void SoGLContext_glGetPointerv(const SoGLContext * glue, GLenum pname, GLvoid ** params);
OBOL_DLL_API void SoGLContext_glGetPolygonStipple(const SoGLContext * glue, GLubyte * mask);
OBOL_DLL_API void SoGLContext_glGetSeparableFilter(const SoGLContext * glue, GLenum target, GLenum format, GLenum type, GLvoid * row, GLvoid * column, GLvoid * span);
OBOL_DLL_API void SoGLContext_glGetTexEnvfv(const SoGLContext * glue, GLenum target, GLenum pname, GLfloat * params);
OBOL_DLL_API void SoGLContext_glGetTexEnviv(const SoGLContext * glue, GLenum target, GLenum pname, GLint * params);
OBOL_DLL_API void SoGLContext_glGetTexGendv(const SoGLContext * glue, GLenum coord, GLenum pname, GLdouble * params);
OBOL_DLL_API void SoGLContext_glGetTexGenfv(const SoGLContext * glue, GLenum coord, GLenum pname, GLfloat * params);
OBOL_DLL_API void SoGLContext_glGetTexGeniv(const SoGLContext * glue, GLenum coord, GLenum pname, GLint * params);
OBOL_DLL_API void SoGLContext_glGetTexImage(const SoGLContext * glue, GLenum target, GLint level, GLenum format, GLenum type, GLvoid * pixels);
OBOL_DLL_API void SoGLContext_glGetTexLevelParameterfv(const SoGLContext * glue, GLenum target, GLint level, GLenum pname, GLfloat * params);
OBOL_DLL_API void SoGLContext_glGetTexLevelParameteriv(const SoGLContext * glue, GLenum target, GLint level, GLenum pname, GLint * params);
OBOL_DLL_API void SoGLContext_glGetTexParameterfv(const SoGLContext * glue, GLenum target, GLenum pname, GLfloat * params);
OBOL_DLL_API void SoGLContext_glGetTexParameteriv(const SoGLContext * glue, GLenum target, GLenum pname, GLint * params);
OBOL_DLL_API void SoGLContext_glHint(const SoGLContext * glue, GLenum target, GLenum mode);
OBOL_DLL_API void SoGLContext_glHistogram(const SoGLContext * glue, GLenum target, GLsizei width, GLenum internalformat, GLboolean sink);
OBOL_DLL_API void SoGLContext_glIndexMask(const SoGLContext * glue, GLuint mask);
OBOL_DLL_API void SoGLContext_glIndexd(const SoGLContext * glue, GLdouble c);
OBOL_DLL_API void SoGLContext_glIndexdv(const SoGLContext * glue, const GLdouble * c);
OBOL_DLL_API void SoGLContext_glIndexf(const SoGLContext * glue, GLfloat c);
OBOL_DLL_API void SoGLContext_glIndexfv(const SoGLContext * glue, const GLfloat * c);
OBOL_DLL_API void SoGLContext_glIndexiv(const SoGLContext * glue, const GLint * c);
OBOL_DLL_API void SoGLContext_glIndexs(const SoGLContext * glue, GLshort c);
OBOL_DLL_API void SoGLContext_glIndexsv(const SoGLContext * glue, const GLshort * c);
OBOL_DLL_API void SoGLContext_glIndexub(const SoGLContext * glue, GLubyte c);
OBOL_DLL_API void SoGLContext_glIndexubv(const SoGLContext * glue, const GLubyte * c);
OBOL_DLL_API void SoGLContext_glInitNames(const SoGLContext * glue);
OBOL_DLL_API GLboolean SoGLContext_glIsList(const SoGLContext * glue, GLuint list);
OBOL_DLL_API GLboolean SoGLContext_glIsTexture(const SoGLContext * glue, GLuint texture);
OBOL_DLL_API void SoGLContext_glLightModelf(const SoGLContext * glue, GLenum pname, GLfloat param);
OBOL_DLL_API void SoGLContext_glLightModeliv(const SoGLContext * glue, GLenum pname, const GLint * params);
OBOL_DLL_API void SoGLContext_glLighti(const SoGLContext * glue, GLenum light, GLenum pname, GLint param);
OBOL_DLL_API void SoGLContext_glLightiv(const SoGLContext * glue, GLenum light, GLenum pname, const GLint * params);
OBOL_DLL_API void SoGLContext_glListBase(const SoGLContext * glue, GLuint base);
OBOL_DLL_API void SoGLContext_glLoadName(const SoGLContext * glue, GLuint name);
OBOL_DLL_API void SoGLContext_glLoadTransposeMatrixd(const SoGLContext * glue, const GLdouble * m);
OBOL_DLL_API void SoGLContext_glLoadTransposeMatrixf(const SoGLContext * glue, const GLfloat * m);
OBOL_DLL_API void SoGLContext_glLogicOp(const SoGLContext * glue, GLenum opcode);
OBOL_DLL_API void SoGLContext_glMap1d(const SoGLContext * glue, GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble * points);
OBOL_DLL_API void SoGLContext_glMap1f(const SoGLContext * glue, GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat * points);
OBOL_DLL_API void SoGLContext_glMap2d(const SoGLContext * glue, GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble * points);
OBOL_DLL_API void SoGLContext_glMap2f(const SoGLContext * glue, GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat * points);
OBOL_DLL_API void SoGLContext_glMapGrid1d(const SoGLContext * glue, GLint un, GLdouble u1, GLdouble u2);
OBOL_DLL_API void SoGLContext_glMapGrid1f(const SoGLContext * glue, GLint un, GLfloat u1, GLfloat u2);
OBOL_DLL_API void SoGLContext_glMapGrid2d(const SoGLContext * glue, GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2);
OBOL_DLL_API void SoGLContext_glMapGrid2f(const SoGLContext * glue, GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2);
OBOL_DLL_API void SoGLContext_glMateriali(const SoGLContext * glue, GLenum face, GLenum pname, GLint param);
OBOL_DLL_API void SoGLContext_glMaterialiv(const SoGLContext * glue, GLenum face, GLenum pname, const GLint * params);
OBOL_DLL_API void SoGLContext_glMinmax(const SoGLContext * glue, GLenum target, GLenum internalformat, GLboolean sink);
OBOL_DLL_API void SoGLContext_glMultMatrixd(const SoGLContext * glue, const GLdouble * m);
OBOL_DLL_API void SoGLContext_glMultTransposeMatrixd(const SoGLContext * glue, const GLdouble * m);
OBOL_DLL_API void SoGLContext_glMultTransposeMatrixf(const SoGLContext * glue, const GLfloat * m);
OBOL_DLL_API void SoGLContext_glMultiTexCoord1d(const SoGLContext * glue, GLenum target, GLdouble s);
OBOL_DLL_API void SoGLContext_glMultiTexCoord1dv(const SoGLContext * glue, GLenum target, const GLdouble * v);
OBOL_DLL_API void SoGLContext_glMultiTexCoord1f(const SoGLContext * glue, GLenum target, GLfloat s);
OBOL_DLL_API void SoGLContext_glMultiTexCoord1fv(const SoGLContext * glue, GLenum target, const GLfloat * v);
OBOL_DLL_API void SoGLContext_glMultiTexCoord1i(const SoGLContext * glue, GLenum target, GLint s);
OBOL_DLL_API void SoGLContext_glMultiTexCoord1iv(const SoGLContext * glue, GLenum target, const GLint * v);
OBOL_DLL_API void SoGLContext_glMultiTexCoord1s(const SoGLContext * glue, GLenum target, GLshort s);
OBOL_DLL_API void SoGLContext_glMultiTexCoord1sv(const SoGLContext * glue, GLenum target, const GLshort * v);
OBOL_DLL_API void SoGLContext_glMultiTexCoord2d(const SoGLContext * glue, GLenum target, GLdouble s, GLdouble t);
OBOL_DLL_API void SoGLContext_glMultiTexCoord2dv(const SoGLContext * glue, GLenum target, const GLdouble * v);
OBOL_DLL_API void SoGLContext_glMultiTexCoord2i(const SoGLContext * glue, GLenum target, GLint s, GLint t);
OBOL_DLL_API void SoGLContext_glMultiTexCoord2iv(const SoGLContext * glue, GLenum target, const GLint * v);
OBOL_DLL_API void SoGLContext_glMultiTexCoord2s(const SoGLContext * glue, GLenum target, GLshort s, GLshort t);
OBOL_DLL_API void SoGLContext_glMultiTexCoord2sv(const SoGLContext * glue, GLenum target, const GLshort * v);
OBOL_DLL_API void SoGLContext_glMultiTexCoord3d(const SoGLContext * glue, GLenum target, GLdouble s, GLdouble t, GLdouble r);
OBOL_DLL_API void SoGLContext_glMultiTexCoord3dv(const SoGLContext * glue, GLenum target, const GLdouble * v);
OBOL_DLL_API void SoGLContext_glMultiTexCoord3f(const SoGLContext * glue, GLenum target, GLfloat s, GLfloat t, GLfloat r);
OBOL_DLL_API void SoGLContext_glMultiTexCoord3i(const SoGLContext * glue, GLenum target, GLint s, GLint t, GLint r);
OBOL_DLL_API void SoGLContext_glMultiTexCoord3iv(const SoGLContext * glue, GLenum target, const GLint * v);
OBOL_DLL_API void SoGLContext_glMultiTexCoord3s(const SoGLContext * glue, GLenum target, GLshort s, GLshort t, GLshort r);
OBOL_DLL_API void SoGLContext_glMultiTexCoord3sv(const SoGLContext * glue, GLenum target, const GLshort * v);
OBOL_DLL_API void SoGLContext_glMultiTexCoord4d(const SoGLContext * glue, GLenum target, GLdouble s, GLdouble t, GLdouble r, GLdouble q);
OBOL_DLL_API void SoGLContext_glMultiTexCoord4dv(const SoGLContext * glue, GLenum target, const GLdouble * v);
OBOL_DLL_API void SoGLContext_glMultiTexCoord4f(const SoGLContext * glue, GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q);
OBOL_DLL_API void SoGLContext_glMultiTexCoord4i(const SoGLContext * glue, GLenum target, GLint s, GLint t, GLint r, GLint q);
OBOL_DLL_API void SoGLContext_glMultiTexCoord4iv(const SoGLContext * glue, GLenum target, const GLint * v);
OBOL_DLL_API void SoGLContext_glMultiTexCoord4s(const SoGLContext * glue, GLenum target, GLshort s, GLshort t, GLshort r, GLshort q);
OBOL_DLL_API void SoGLContext_glMultiTexCoord4sv(const SoGLContext * glue, GLenum target, const GLshort * v);
OBOL_DLL_API void SoGLContext_glNormal3b(const SoGLContext * glue, GLbyte nx, GLbyte ny, GLbyte nz);
OBOL_DLL_API void SoGLContext_glNormal3bv(const SoGLContext * glue, const GLbyte * v);
OBOL_DLL_API void SoGLContext_glNormal3d(const SoGLContext * glue, GLdouble nx, GLdouble ny, GLdouble nz);
OBOL_DLL_API void SoGLContext_glNormal3dv(const SoGLContext * glue, const GLdouble * v);
OBOL_DLL_API void SoGLContext_glNormal3i(const SoGLContext * glue, GLint nx, GLint ny, GLint nz);
OBOL_DLL_API void SoGLContext_glNormal3iv(const SoGLContext * glue, const GLint * v);
OBOL_DLL_API void SoGLContext_glNormal3s(const SoGLContext * glue, GLshort nx, GLshort ny, GLshort nz);
OBOL_DLL_API void SoGLContext_glNormal3sv(const SoGLContext * glue, const GLshort * v);
OBOL_DLL_API void SoGLContext_glPassThrough(const SoGLContext * glue, GLfloat token);
OBOL_DLL_API void SoGLContext_glPixelMapusv(const SoGLContext * glue, GLenum map, GLsizei mapsize, const GLushort * values);
OBOL_DLL_API void SoGLContext_glPixelStoref(const SoGLContext * glue, GLenum pname, GLfloat param);
OBOL_DLL_API void SoGLContext_glPopName(const SoGLContext * glue);
OBOL_DLL_API void SoGLContext_glPrioritizeTextures(const SoGLContext * glue, GLsizei n, const GLuint * textures, const GLclampf * priorities);
OBOL_DLL_API void SoGLContext_glPushName(const SoGLContext * glue, GLuint name);
OBOL_DLL_API void SoGLContext_glRasterPos2d(const SoGLContext * glue, GLdouble x, GLdouble y);
OBOL_DLL_API void SoGLContext_glRasterPos2dv(const SoGLContext * glue, const GLdouble * v);
OBOL_DLL_API void SoGLContext_glRasterPos2fv(const SoGLContext * glue, const GLfloat * v);
OBOL_DLL_API void SoGLContext_glRasterPos2i(const SoGLContext * glue, GLint x, GLint y);
OBOL_DLL_API void SoGLContext_glRasterPos2iv(const SoGLContext * glue, const GLint * v);
OBOL_DLL_API void SoGLContext_glRasterPos2s(const SoGLContext * glue, GLshort x, GLshort y);
OBOL_DLL_API void SoGLContext_glRasterPos2sv(const SoGLContext * glue, const GLshort * v);
OBOL_DLL_API void SoGLContext_glRasterPos3d(const SoGLContext * glue, GLdouble x, GLdouble y, GLdouble z);
OBOL_DLL_API void SoGLContext_glRasterPos3dv(const SoGLContext * glue, const GLdouble * v);
OBOL_DLL_API void SoGLContext_glRasterPos3fv(const SoGLContext * glue, const GLfloat * v);
OBOL_DLL_API void SoGLContext_glRasterPos3i(const SoGLContext * glue, GLint x, GLint y, GLint z);
OBOL_DLL_API void SoGLContext_glRasterPos3iv(const SoGLContext * glue, const GLint * v);
OBOL_DLL_API void SoGLContext_glRasterPos3s(const SoGLContext * glue, GLshort x, GLshort y, GLshort z);
OBOL_DLL_API void SoGLContext_glRasterPos3sv(const SoGLContext * glue, const GLshort * v);
OBOL_DLL_API void SoGLContext_glRasterPos4d(const SoGLContext * glue, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
OBOL_DLL_API void SoGLContext_glRasterPos4dv(const SoGLContext * glue, const GLdouble * v);
OBOL_DLL_API void SoGLContext_glRasterPos4f(const SoGLContext * glue, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
OBOL_DLL_API void SoGLContext_glRasterPos4fv(const SoGLContext * glue, const GLfloat * v);
OBOL_DLL_API void SoGLContext_glRasterPos4i(const SoGLContext * glue, GLint x, GLint y, GLint z, GLint w);
OBOL_DLL_API void SoGLContext_glRasterPos4iv(const SoGLContext * glue, const GLint * v);
OBOL_DLL_API void SoGLContext_glRasterPos4s(const SoGLContext * glue, GLshort x, GLshort y, GLshort z, GLshort w);
OBOL_DLL_API void SoGLContext_glRasterPos4sv(const SoGLContext * glue, const GLshort * v);
OBOL_DLL_API void SoGLContext_glReadBuffer(const SoGLContext * glue, GLenum mode);
OBOL_DLL_API void SoGLContext_glRectd(const SoGLContext * glue, GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2);
OBOL_DLL_API void SoGLContext_glRectdv(const SoGLContext * glue, const GLdouble * v1, const GLdouble * v2);
OBOL_DLL_API void SoGLContext_glRectf(const SoGLContext * glue, GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2);
OBOL_DLL_API void SoGLContext_glRectfv(const SoGLContext * glue, const GLfloat * v1, const GLfloat * v2);
OBOL_DLL_API void SoGLContext_glRecti(const SoGLContext * glue, GLint x1, GLint y1, GLint x2, GLint y2);
OBOL_DLL_API void SoGLContext_glRectiv(const SoGLContext * glue, const GLint * v1, const GLint * v2);
OBOL_DLL_API void SoGLContext_glRects(const SoGLContext * glue, GLshort x1, GLshort y1, GLshort x2, GLshort y2);
OBOL_DLL_API void SoGLContext_glRectsv(const SoGLContext * glue, const GLshort * v1, const GLshort * v2);
OBOL_DLL_API GLint SoGLContext_glRenderMode(const SoGLContext * glue, GLenum mode);
OBOL_DLL_API void SoGLContext_glResetHistogram(const SoGLContext * glue, GLenum target);
OBOL_DLL_API void SoGLContext_glResetMinmax(const SoGLContext * glue, GLenum target);
OBOL_DLL_API void SoGLContext_glRotated(const SoGLContext * glue, GLdouble angle, GLdouble x, GLdouble y, GLdouble z);
OBOL_DLL_API void SoGLContext_glSampleCoverage(const SoGLContext * glue, GLclampf value, GLboolean invert);
OBOL_DLL_API void SoGLContext_glScaled(const SoGLContext * glue, GLdouble x, GLdouble y, GLdouble z);
OBOL_DLL_API void SoGLContext_glSelectBuffer(const SoGLContext * glue, GLsizei size, GLuint * buffer);
OBOL_DLL_API void SoGLContext_glSeparableFilter2D(const SoGLContext * glue, GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid * row, const GLvoid * column);
OBOL_DLL_API void SoGLContext_glShadeModel(const SoGLContext * glue, GLenum mode);
OBOL_DLL_API void SoGLContext_glStencilMask(const SoGLContext * glue, GLuint mask);
OBOL_DLL_API void SoGLContext_glTexCoord1d(const SoGLContext * glue, GLdouble s);
OBOL_DLL_API void SoGLContext_glTexCoord1dv(const SoGLContext * glue, const GLdouble * v);
OBOL_DLL_API void SoGLContext_glTexCoord1f(const SoGLContext * glue, GLfloat s);
OBOL_DLL_API void SoGLContext_glTexCoord1fv(const SoGLContext * glue, const GLfloat * v);
OBOL_DLL_API void SoGLContext_glTexCoord1i(const SoGLContext * glue, GLint s);
OBOL_DLL_API void SoGLContext_glTexCoord1iv(const SoGLContext * glue, const GLint * v);
OBOL_DLL_API void SoGLContext_glTexCoord1s(const SoGLContext * glue, GLshort s);
OBOL_DLL_API void SoGLContext_glTexCoord1sv(const SoGLContext * glue, const GLshort * v);
OBOL_DLL_API void SoGLContext_glTexCoord2d(const SoGLContext * glue, GLdouble s, GLdouble t);
OBOL_DLL_API void SoGLContext_glTexCoord2dv(const SoGLContext * glue, const GLdouble * v);
OBOL_DLL_API void SoGLContext_glTexCoord2i(const SoGLContext * glue, GLint s, GLint t);
OBOL_DLL_API void SoGLContext_glTexCoord2iv(const SoGLContext * glue, const GLint * v);
OBOL_DLL_API void SoGLContext_glTexCoord2s(const SoGLContext * glue, GLshort s, GLshort t);
OBOL_DLL_API void SoGLContext_glTexCoord2sv(const SoGLContext * glue, const GLshort * v);
OBOL_DLL_API void SoGLContext_glTexCoord3d(const SoGLContext * glue, GLdouble s, GLdouble t, GLdouble r);
OBOL_DLL_API void SoGLContext_glTexCoord3dv(const SoGLContext * glue, const GLdouble * v);
OBOL_DLL_API void SoGLContext_glTexCoord3i(const SoGLContext * glue, GLint s, GLint t, GLint r);
OBOL_DLL_API void SoGLContext_glTexCoord3iv(const SoGLContext * glue, const GLint * v);
OBOL_DLL_API void SoGLContext_glTexCoord3s(const SoGLContext * glue, GLshort s, GLshort t, GLshort r);
OBOL_DLL_API void SoGLContext_glTexCoord3sv(const SoGLContext * glue, const GLshort * v);
OBOL_DLL_API void SoGLContext_glTexCoord4d(const SoGLContext * glue, GLdouble s, GLdouble t, GLdouble r, GLdouble q);
OBOL_DLL_API void SoGLContext_glTexCoord4dv(const SoGLContext * glue, const GLdouble * v);
OBOL_DLL_API void SoGLContext_glTexCoord4f(const SoGLContext * glue, GLfloat s, GLfloat t, GLfloat r, GLfloat q);
OBOL_DLL_API void SoGLContext_glTexCoord4i(const SoGLContext * glue, GLint s, GLint t, GLint r, GLint q);
OBOL_DLL_API void SoGLContext_glTexCoord4iv(const SoGLContext * glue, const GLint * v);
OBOL_DLL_API void SoGLContext_glTexCoord4s(const SoGLContext * glue, GLshort s, GLshort t, GLshort r, GLshort q);
OBOL_DLL_API void SoGLContext_glTexCoord4sv(const SoGLContext * glue, const GLshort * v);
OBOL_DLL_API void SoGLContext_glTexEnviv(const SoGLContext * glue, GLenum target, GLenum pname, const GLint * params);
OBOL_DLL_API void SoGLContext_glTexGend(const SoGLContext * glue, GLenum coord, GLenum pname, GLdouble param);
OBOL_DLL_API void SoGLContext_glTexGendv(const SoGLContext * glue, GLenum coord, GLenum pname, const GLdouble * params);
OBOL_DLL_API void SoGLContext_glTexGeniv(const SoGLContext * glue, GLenum coord, GLenum pname, const GLint * params);
OBOL_DLL_API void SoGLContext_glTexImage1D(const SoGLContext * glue, GLenum target, GLint level, GLint internalFormat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid * pixels);
OBOL_DLL_API void SoGLContext_glTexParameterfv(const SoGLContext * glue, GLenum target, GLenum pname, const GLfloat * params);
OBOL_DLL_API void SoGLContext_glTexParameteriv(const SoGLContext * glue, GLenum target, GLenum pname, const GLint * params);
OBOL_DLL_API void SoGLContext_glTexSubImage1D(const SoGLContext * glue, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid * pixels);
OBOL_DLL_API void SoGLContext_glTranslated(const SoGLContext * glue, GLdouble x, GLdouble y, GLdouble z);
OBOL_DLL_API void SoGLContext_glVertex2d(const SoGLContext * glue, GLdouble x, GLdouble y);
OBOL_DLL_API void SoGLContext_glVertex2dv(const SoGLContext * glue, const GLdouble * v);
OBOL_DLL_API void SoGLContext_glVertex2fv(const SoGLContext * glue, const GLfloat * v);
OBOL_DLL_API void SoGLContext_glVertex2i(const SoGLContext * glue, GLint x, GLint y);
OBOL_DLL_API void SoGLContext_glVertex2iv(const SoGLContext * glue, const GLint * v);
OBOL_DLL_API void SoGLContext_glVertex2sv(const SoGLContext * glue, const GLshort * v);
OBOL_DLL_API void SoGLContext_glVertex3d(const SoGLContext * glue, GLdouble x, GLdouble y, GLdouble z);
OBOL_DLL_API void SoGLContext_glVertex3dv(const SoGLContext * glue, const GLdouble * v);
OBOL_DLL_API void SoGLContext_glVertex3i(const SoGLContext * glue, GLint x, GLint y, GLint z);
OBOL_DLL_API void SoGLContext_glVertex3iv(const SoGLContext * glue, const GLint * v);
OBOL_DLL_API void SoGLContext_glVertex3s(const SoGLContext * glue, GLshort x, GLshort y, GLshort z);
OBOL_DLL_API void SoGLContext_glVertex3sv(const SoGLContext * glue, const GLshort * v);
OBOL_DLL_API void SoGLContext_glVertex4d(const SoGLContext * glue, GLdouble x, GLdouble y, GLdouble z, GLdouble w);
OBOL_DLL_API void SoGLContext_glVertex4dv(const SoGLContext * glue, const GLdouble * v);
OBOL_DLL_API void SoGLContext_glVertex4f(const SoGLContext * glue, GLfloat x, GLfloat y, GLfloat z, GLfloat w);
OBOL_DLL_API void SoGLContext_glVertex4i(const SoGLContext * glue, GLint x, GLint y, GLint z, GLint w);
OBOL_DLL_API void SoGLContext_glVertex4iv(const SoGLContext * glue, const GLint * v);
OBOL_DLL_API void SoGLContext_glVertex4s(const SoGLContext * glue, GLshort x, GLshort y, GLshort z, GLshort w);
OBOL_DLL_API void SoGLContext_glVertex4sv(const SoGLContext * glue, const GLshort * v);

#ifdef __cplusplus
}
#endif

#endif /* OBOL_SOGL_DISPATCH_H */
