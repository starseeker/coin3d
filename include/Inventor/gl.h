#ifndef OBOL_INVENTOR_GL_H
#define OBOL_INVENTOR_GL_H

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
 * Inventor/gl.h  --  Drop-in replacement for <GL/gl.h> that routes all
 *                    gl*() calls through Obol's GL dispatch layer.
 *
 * PURPOSE
 * -------
 * In single-backend Obol builds (system OpenGL only, or pure OSMesa) raw
 * gl*() calls work fine.  In dual-backend builds (OBOL_BUILD_DUAL_GL),
 * raw gl*() calls are always bound to the system GL library, so they
 * silently go to the wrong backend during OSMesa renders.
 *
 * Including this header instead of the platform <GL/gl.h> causes every
 * gl*() call in that translation unit to be redirected through Obol's
 * dispatch layer via sogl_current_render_glue(), which always returns the
 * SoGLContext for the render pass currently active on this thread.
 *
 * USAGE
 * -----
 * In SoCallback code and custom scene-graph nodes, replace:
 *
 *   #include <GL/gl.h>          // or <OSMesa/gl.h> / <OpenGL/gl.h>
 *
 * with:
 *
 *   #include <Inventor/gl.h>
 *
 * Existing gl*() calls need no further changes; they are redirected at
 * compile time by the macros below.
 *
 * SCOPE
 * -----
 * This header covers the complete core OpenGL 1.0-1.3 feature set that
 * external/osmesa guarantees — the maximum feature set that can be used
 * safely in both system-GL and OSMesa contexts.  This is also the maximum
 * subset that Obol itself relies on in its own rendering paths.
 *
 * LIMITATIONS
 * -----------
 * - The macros are valid only while sogl_current_render_glue() is non-NULL,
 *   i.e. during an active SoGLRenderAction traversal pass.  Calling a
 *   redirected gl*() outside of rendering yields undefined behaviour.
 * - Functions beyond the OSMesa boundary (OpenGL 2.0+ features, system-only
 *   extensions such as glGenBuffers) are NOT redirected.  Using them means
 *   the call goes to the system GL library directly.  In a dual-backend
 *   build this will silently bypass the OSMesa render pipeline.  To
 *   acknowledge this risk, define OBOL_ALLOW_SYSTEM_GL_ONLY before including
 *   the system extension header, e.g.:
 *     #define OBOL_ALLOW_SYSTEM_GL_ONLY 1
 *     #include <GL/glext.h>
 * - This header must not be included inside any Obol internal translation
 *   unit that compiles with SOGL_PREFIX_SET (i.e. gl_osmesa.cpp), as the
 *   macro names would conflict with the internal dispatch wrappers.
 */

/* Pull in GL enum values and types (GLenum, GLfloat, etc.).
   Do NOT define our macros before this include; the system GL header may
   use some of these names internally (e.g. as function parameters). */
#include <Inventor/system/gl.h>

/* Dispatch API: sogl_current_render_glue() and SoGLContext_glXxx(). */
#include <Inventor/system/SoGLDispatch.h>

/*
 * Macro redirects.
 *
 * Each bare gl*() call becomes:
 *   SoGLContext_glXxx(sogl_current_render_glue(), ...)
 *
 * The macros shadow the GLAPI-declared gl* functions from the system GL
 * header.  This shadowing is intentional: at the call site, the macro
 * expansion takes precedence over any function declaration.
 *
 * We undef each name before defining it to silence compiler warnings
 * when an existing #define is already present (e.g. from gl_mangle.h
 * in OBOL_OSMESA_BUILD builds that pull in <OSMesa/gl_mangle.h>).
 */

/* --- Immediate mode geometry --- */
#undef glBegin
#define glBegin(mode)                   SoGLContext_glBegin(sogl_current_render_glue(), (mode))
#undef glEnd
#define glEnd()                         SoGLContext_glEnd(sogl_current_render_glue())
#undef glVertex2f
#define glVertex2f(x,y)                 SoGLContext_glVertex2f(sogl_current_render_glue(), (x),(y))
#undef glVertex2s
#define glVertex2s(x,y)                 SoGLContext_glVertex2s(sogl_current_render_glue(), (x),(y))
#undef glVertex3f
#define glVertex3f(x,y,z)               SoGLContext_glVertex3f(sogl_current_render_glue(), (x),(y),(z))
#undef glVertex3fv
#define glVertex3fv(v)                  SoGLContext_glVertex3fv(sogl_current_render_glue(), (v))
#undef glVertex4fv
#define glVertex4fv(v)                  SoGLContext_glVertex4fv(sogl_current_render_glue(), (v))
#undef glNormal3f
#define glNormal3f(nx,ny,nz)            SoGLContext_glNormal3f(sogl_current_render_glue(), (nx),(ny),(nz))
#undef glNormal3fv
#define glNormal3fv(v)                  SoGLContext_glNormal3fv(sogl_current_render_glue(), (v))
#undef glColor3f
#define glColor3f(r,g,b)                SoGLContext_glColor3f(sogl_current_render_glue(), (r),(g),(b))
#undef glColor3fv
#define glColor3fv(v)                   SoGLContext_glColor3fv(sogl_current_render_glue(), (v))
#undef glColor3ub
#define glColor3ub(r,g,b)               SoGLContext_glColor3ub(sogl_current_render_glue(), (r),(g),(b))
#undef glColor3ubv
#define glColor3ubv(v)                  SoGLContext_glColor3ubv(sogl_current_render_glue(), (v))
#undef glColor4ub
#define glColor4ub(r,g,b,a)             SoGLContext_glColor4ub(sogl_current_render_glue(), (r),(g),(b),(a))
#undef glTexCoord2f
#define glTexCoord2f(s,t)               SoGLContext_glTexCoord2f(sogl_current_render_glue(), (s),(t))
#undef glTexCoord2fv
#define glTexCoord2fv(v)                SoGLContext_glTexCoord2fv(sogl_current_render_glue(), (v))
#undef glTexCoord3f
#define glTexCoord3f(s,t,r)             SoGLContext_glTexCoord3f(sogl_current_render_glue(), (s),(t),(r))
#undef glTexCoord3fv
#define glTexCoord3fv(v)                SoGLContext_glTexCoord3fv(sogl_current_render_glue(), (v))
#undef glTexCoord4fv
#define glTexCoord4fv(v)                SoGLContext_glTexCoord4fv(sogl_current_render_glue(), (v))
#undef glIndexi
#define glIndexi(c)                     SoGLContext_glIndexi(sogl_current_render_glue(), (c))

/* --- Matrix stack --- */
#undef glMatrixMode
#define glMatrixMode(mode)              SoGLContext_glMatrixMode(sogl_current_render_glue(), (mode))
#undef glLoadIdentity
#define glLoadIdentity()                SoGLContext_glLoadIdentity(sogl_current_render_glue())
#undef glLoadMatrixf
#define glLoadMatrixf(m)                SoGLContext_glLoadMatrixf(sogl_current_render_glue(), (m))
#undef glLoadMatrixd
#define glLoadMatrixd(m)                SoGLContext_glLoadMatrixd(sogl_current_render_glue(), (m))
#undef glMultMatrixf
#define glMultMatrixf(m)                SoGLContext_glMultMatrixf(sogl_current_render_glue(), (m))
#undef glPushMatrix
#define glPushMatrix()                  SoGLContext_glPushMatrix(sogl_current_render_glue())
#undef glPopMatrix
#define glPopMatrix()                   SoGLContext_glPopMatrix(sogl_current_render_glue())
#undef glOrtho
#define glOrtho(l,r,b,t,n,f)            SoGLContext_glOrtho(sogl_current_render_glue(), (l),(r),(b),(t),(n),(f))
#undef glFrustum
#define glFrustum(l,r,b,t,n,f)          SoGLContext_glFrustum(sogl_current_render_glue(), (l),(r),(b),(t),(n),(f))
#undef glTranslatef
#define glTranslatef(x,y,z)             SoGLContext_glTranslatef(sogl_current_render_glue(), (x),(y),(z))
#undef glRotatef
#define glRotatef(a,x,y,z)              SoGLContext_glRotatef(sogl_current_render_glue(), (a),(x),(y),(z))
#undef glScalef
#define glScalef(x,y,z)                 SoGLContext_glScalef(sogl_current_render_glue(), (x),(y),(z))

/* --- Render state --- */
#undef glEnable
#define glEnable(cap)                   SoGLContext_glEnable(sogl_current_render_glue(), (cap))
#undef glDisable
#define glDisable(cap)                  SoGLContext_glDisable(sogl_current_render_glue(), (cap))
#undef glIsEnabled
#define glIsEnabled(cap)                SoGLContext_glIsEnabled(sogl_current_render_glue(), (cap))
#undef glPushAttrib
#define glPushAttrib(mask)              SoGLContext_glPushAttrib(sogl_current_render_glue(), (mask))
#undef glPopAttrib
#define glPopAttrib()                   SoGLContext_glPopAttrib(sogl_current_render_glue())
#undef glDepthMask
#define glDepthMask(flag)               SoGLContext_glDepthMask(sogl_current_render_glue(), (flag))
#undef glDepthFunc
#define glDepthFunc(func)               SoGLContext_glDepthFunc(sogl_current_render_glue(), (func))
#undef glDepthRange
#define glDepthRange(n,f)               SoGLContext_glDepthRange(sogl_current_render_glue(), (n),(f))
#undef glBlendFunc
#define glBlendFunc(sf,df)              SoGLContext_glBlendFunc(sogl_current_render_glue(), (sf),(df))
#undef glAlphaFunc
#define glAlphaFunc(func,ref)           SoGLContext_glAlphaFunc(sogl_current_render_glue(), (func),(ref))
#undef glViewport
#define glViewport(x,y,w,h)             SoGLContext_glViewport(sogl_current_render_glue(), (x),(y),(w),(h))
#undef glScissor
#define glScissor(x,y,w,h)              SoGLContext_glScissor(sogl_current_render_glue(), (x),(y),(w),(h))
#undef glClearColor
#define glClearColor(r,g,b,a)           SoGLContext_glClearColor(sogl_current_render_glue(), (r),(g),(b),(a))
#undef glClear
#define glClear(mask)                   SoGLContext_glClear(sogl_current_render_glue(), (mask))
#undef glFlush
#define glFlush()                       SoGLContext_glFlush(sogl_current_render_glue())
#undef glFinish
#define glFinish()                      SoGLContext_glFinish(sogl_current_render_glue())
#undef glGetError
#define glGetError()                    SoGLContext_glGetError(sogl_current_render_glue())
#undef glGetString
#define glGetString(name)               SoGLContext_glGetString(sogl_current_render_glue(), (name))
#undef glGetIntegerv
#define glGetIntegerv(pname,params)     SoGLContext_glGetIntegerv(sogl_current_render_glue(), (pname),(params))
#undef glGetFloatv
#define glGetFloatv(pname,params)       SoGLContext_glGetFloatv(sogl_current_render_glue(), (pname),(params))
#undef glGetBooleanv
#define glGetBooleanv(pname,params)     SoGLContext_glGetBooleanv(sogl_current_render_glue(), (pname),(params))
#undef glColorMask
#define glColorMask(r,g,b,a)            SoGLContext_glColorMask(sogl_current_render_glue(), (r),(g),(b),(a))
#undef glStencilFunc
#define glStencilFunc(f,ref,m)          SoGLContext_glStencilFunc(sogl_current_render_glue(), (f),(ref),(m))
#undef glStencilOp
#define glStencilOp(f,z,p)              SoGLContext_glStencilOp(sogl_current_render_glue(), (f),(z),(p))
#undef glFrontFace
#define glFrontFace(mode)               SoGLContext_glFrontFace(sogl_current_render_glue(), (mode))
#undef glCullFace
#define glCullFace(mode)                SoGLContext_glCullFace(sogl_current_render_glue(), (mode))
#undef glPolygonMode
#define glPolygonMode(face,mode)        SoGLContext_glPolygonMode(sogl_current_render_glue(), (face),(mode))
#undef glPolygonStipple
#define glPolygonStipple(mask)          SoGLContext_glPolygonStipple(sogl_current_render_glue(), (mask))
#undef glLineWidth
#define glLineWidth(w)                  SoGLContext_glLineWidth(sogl_current_render_glue(), (w))
#undef glLineStipple
#define glLineStipple(factor,pattern)   SoGLContext_glLineStipple(sogl_current_render_glue(), (factor),(pattern))
#undef glPointSize
#define glPointSize(sz)                 SoGLContext_glPointSize(sogl_current_render_glue(), (sz))
#undef glClipPlane
#define glClipPlane(plane,eq)           SoGLContext_glClipPlane(sogl_current_render_glue(), (plane),(eq))
#undef glDrawBuffer
#define glDrawBuffer(mode)              SoGLContext_glDrawBuffer(sogl_current_render_glue(), (mode))
#undef glClearIndex
#define glClearIndex(c)                 SoGLContext_glClearIndex(sogl_current_render_glue(), (c))
#undef glClearStencil
#define glClearStencil(s)               SoGLContext_glClearStencil(sogl_current_render_glue(), (s))
#undef glAccum
#define glAccum(op,value)               SoGLContext_glAccum(sogl_current_render_glue(), (op),(value))

/* --- Pixel / image operations --- */
#undef glPixelStorei
#define glPixelStorei(pname,param)      SoGLContext_glPixelStorei(sogl_current_render_glue(), (pname),(param))
#undef glReadPixels
#define glReadPixels(x,y,w,h,f,t,p)     SoGLContext_glReadPixels(sogl_current_render_glue(), (x),(y),(w),(h),(f),(t),(p))
#undef glRasterPos2f
#define glRasterPos2f(x,y)              SoGLContext_glRasterPos2f(sogl_current_render_glue(), (x),(y))
#undef glRasterPos3f
#define glRasterPos3f(x,y,z)            SoGLContext_glRasterPos3f(sogl_current_render_glue(), (x),(y),(z))
#undef glBitmap
#define glBitmap(w,h,xo,yo,xm,ym,bm)    SoGLContext_glBitmap(sogl_current_render_glue(), (w),(h),(xo),(yo),(xm),(ym),(bm))
#undef glDrawPixels
#define glDrawPixels(w,h,f,t,p)         SoGLContext_glDrawPixels(sogl_current_render_glue(), (w),(h),(f),(t),(p))
#undef glPixelTransferf
#define glPixelTransferf(pname,param)   SoGLContext_glPixelTransferf(sogl_current_render_glue(), (pname),(param))
#undef glPixelTransferi
#define glPixelTransferi(pname,param)   SoGLContext_glPixelTransferi(sogl_current_render_glue(), (pname),(param))
#undef glPixelMapfv
#define glPixelMapfv(map,ms,vals)       SoGLContext_glPixelMapfv(sogl_current_render_glue(), (map),(ms),(vals))
#undef glPixelMapuiv
#define glPixelMapuiv(map,ms,vals)      SoGLContext_glPixelMapuiv(sogl_current_render_glue(), (map),(ms),(vals))
#undef glPixelZoom
#define glPixelZoom(xf,yf)              SoGLContext_glPixelZoom(sogl_current_render_glue(), (xf),(yf))

/* --- Lighting / material --- */
#undef glLightf
#define glLightf(light,pname,param)     SoGLContext_glLightf(sogl_current_render_glue(), (light),(pname),(param))
#undef glLightfv
#define glLightfv(light,pname,params)   SoGLContext_glLightfv(sogl_current_render_glue(), (light),(pname),(params))
#undef glLightModeli
#define glLightModeli(pname,param)      SoGLContext_glLightModeli(sogl_current_render_glue(), (pname),(param))
#undef glLightModelfv
#define glLightModelfv(pname,params)    SoGLContext_glLightModelfv(sogl_current_render_glue(), (pname),(params))
#undef glMaterialf
#define glMaterialf(face,pname,param)   SoGLContext_glMaterialf(sogl_current_render_glue(), (face),(pname),(param))
#undef glMaterialfv
#define glMaterialfv(face,pname,params) SoGLContext_glMaterialfv(sogl_current_render_glue(), (face),(pname),(params))
#undef glColorMaterial
#define glColorMaterial(face,mode)      SoGLContext_glColorMaterial(sogl_current_render_glue(), (face),(mode))

/* --- Fog --- */
#undef glFogi
#define glFogi(pname,param)             SoGLContext_glFogi(sogl_current_render_glue(), (pname),(param))
#undef glFogf
#define glFogf(pname,param)             SoGLContext_glFogf(sogl_current_render_glue(), (pname),(param))
#undef glFogfv
#define glFogfv(pname,params)           SoGLContext_glFogfv(sogl_current_render_glue(), (pname),(params))

/* --- Textures --- */
#undef glTexEnvi
#define glTexEnvi(t,pname,param)        SoGLContext_glTexEnvi(sogl_current_render_glue(), (t),(pname),(param))
#undef glTexEnvf
#define glTexEnvf(t,pname,param)        SoGLContext_glTexEnvf(sogl_current_render_glue(), (t),(pname),(param))
#undef glTexEnvfv
#define glTexEnvfv(t,pname,params)      SoGLContext_glTexEnvfv(sogl_current_render_glue(), (t),(pname),(params))
#undef glTexGeni
#define glTexGeni(coord,pname,param)    SoGLContext_glTexGeni(sogl_current_render_glue(), (coord),(pname),(param))
#undef glTexGenf
#define glTexGenf(coord,pname,param)    SoGLContext_glTexGenf(sogl_current_render_glue(), (coord),(pname),(param))
#undef glTexGenfv
#define glTexGenfv(coord,pname,params)  SoGLContext_glTexGenfv(sogl_current_render_glue(), (coord),(pname),(params))
#undef glTexParameteri
#define glTexParameteri(t,pname,param)  SoGLContext_glTexParameteri(sogl_current_render_glue(), (t),(pname),(param))
#undef glTexParameterf
#define glTexParameterf(t,pname,param)  SoGLContext_glTexParameterf(sogl_current_render_glue(), (t),(pname),(param))
#undef glTexImage2D
#define glTexImage2D(t,l,if,w,h,b,f,ty,p) \
    SoGLContext_glTexImage2D(sogl_current_render_glue(), (t),(l),(if),(w),(h),(b),(f),(ty),(p))
#undef glCopyTexImage2D
#define glCopyTexImage2D(t,l,if,x,y,w,h,b) \
    SoGLContext_glCopyTexImage2D(sogl_current_render_glue(), (t),(l),(if),(x),(y),(w),(h),(b))
#undef glCopyTexSubImage2D
#define glCopyTexSubImage2D(t,l,xo,yo,x,y,w,h) \
    SoGLContext_glCopyTexSubImage2D(sogl_current_render_glue(), (t),(l),(xo),(yo),(x),(y),(w),(h))

/* --- Display lists --- */
#undef glNewList
#define glNewList(list,mode)            SoGLContext_glNewList(sogl_current_render_glue(), (list),(mode))
#undef glEndList
#define glEndList()                     SoGLContext_glEndList(sogl_current_render_glue())
#undef glCallList
#define glCallList(list)                SoGLContext_glCallList(sogl_current_render_glue(), (list))
#undef glDeleteLists
#define glDeleteLists(list,range)       SoGLContext_glDeleteLists(sogl_current_render_glue(), (list),(range))
#undef glGenLists
#define glGenLists(range)               SoGLContext_glGenLists(sogl_current_render_glue(), (range))

/* --- Vertex arrays --- */
#undef glDrawArrays
#define glDrawArrays(mode,first,count)  SoGLContext_glDrawArrays(sogl_current_render_glue(), (mode),(first),(count))
#undef glDrawElements
#define glDrawElements(mode,count,type,idx) \
    SoGLContext_glDrawElements(sogl_current_render_glue(), (mode),(count),(type),(idx))

/* --- Full OSMesa GL 1.0-1.3 feature-set macro redirects ---
 * Redirects every core GL function that external/osmesa guarantees
 * through the dispatch layer so callbacks and nodes using Inventor/gl.h
 * work correctly with both system-GL and OSMesa contexts.
 *
 * SCOPE BOUNDARY
 * --------------
 * These macros cover the complete OSMesa feature boundary (GL 1.0-1.3
 * core + GL_ARB_imaging subset).  For features beyond that boundary
 * (system-GL-only extensions, OpenGL 2.0+), you must include the
 * platform GL extension header directly and define
 * OBOL_ALLOW_SYSTEM_GL_ONLY to acknowledge that those calls will not
 * work inside an OSMesa render pass in a dual-backend build. */
#undef glActiveTexture
#define glActiveTexture(texture) SoGLContext_glActiveTexture(sogl_current_render_glue(), (texture))
#undef glArrayElement
#define glArrayElement(i) SoGLContext_glArrayElement(sogl_current_render_glue(), (i))
#undef glBindTexture
#define glBindTexture(target,texture) SoGLContext_glBindTexture(sogl_current_render_glue(), (target),(texture))
#undef glBlendEquation
#define glBlendEquation(mode) SoGLContext_glBlendEquation(sogl_current_render_glue(), (mode))
#undef glClientActiveTexture
#define glClientActiveTexture(texture) SoGLContext_glClientActiveTexture(sogl_current_render_glue(), (texture))
#undef glColorPointer
#define glColorPointer(size,type,stride,pointer) SoGLContext_glColorPointer(sogl_current_render_glue(), (size),(type),(stride),(pointer))
#undef glColorSubTable
#define glColorSubTable(target,start,count,format,type,data) SoGLContext_glColorSubTable(sogl_current_render_glue(), (target),(start),(count),(format),(type),(data))
#undef glColorTable
#define glColorTable(target,internalFormat,width,format,type,table) SoGLContext_glColorTable(sogl_current_render_glue(), (target),(internalFormat),(width),(format),(type),(table))
#undef glCompressedTexImage1D
#define glCompressedTexImage1D(target,level,internalformat,width,border,imageSize,data) SoGLContext_glCompressedTexImage1D(sogl_current_render_glue(), (target),(level),(internalformat),(width),(border),(imageSize),(data))
#undef glCompressedTexImage2D
#define glCompressedTexImage2D(target,level,internalformat,width,height,border,imageSize,data) SoGLContext_glCompressedTexImage2D(sogl_current_render_glue(), (target),(level),(internalformat),(width),(height),(border),(imageSize),(data))
#undef glCompressedTexImage3D
#define glCompressedTexImage3D(target,level,internalformat,width,height,depth,border,imageSize,data) SoGLContext_glCompressedTexImage3D(sogl_current_render_glue(), (target),(level),(internalformat),(width),(height),(depth),(border),(imageSize),(data))
#undef glCompressedTexSubImage1D
#define glCompressedTexSubImage1D(target,level,xoffset,width,format,imageSize,data) SoGLContext_glCompressedTexSubImage1D(sogl_current_render_glue(), (target),(level),(xoffset),(width),(format),(imageSize),(data))
#undef glCompressedTexSubImage2D
#define glCompressedTexSubImage2D(target,level,xoffset,yoffset,width,height,format,imageSize,data) SoGLContext_glCompressedTexSubImage2D(sogl_current_render_glue(), (target),(level),(xoffset),(yoffset),(width),(height),(format),(imageSize),(data))
#undef glCompressedTexSubImage3D
#define glCompressedTexSubImage3D(target,level,xoffset,yoffset,zoffset,width,height,depth,format,imageSize,data) SoGLContext_glCompressedTexSubImage3D(sogl_current_render_glue(), (target),(level),(xoffset),(yoffset),(zoffset),(width),(height),(depth),(format),(imageSize),(data))
#undef glCopyTexSubImage3D
#define glCopyTexSubImage3D(target,level,xoffset,yoffset,zoffset,x,y,width,height) SoGLContext_glCopyTexSubImage3D(sogl_current_render_glue(), (target),(level),(xoffset),(yoffset),(zoffset),(x),(y),(width),(height))
#undef glDeleteTextures
#define glDeleteTextures(n,textures) SoGLContext_glDeleteTextures(sogl_current_render_glue(), (n),(textures))
#undef glDisableClientState
#define glDisableClientState(array) SoGLContext_glDisableClientState(sogl_current_render_glue(), (array))
#undef glDrawRangeElements
#define glDrawRangeElements(mode,start,end,count,type,indices) SoGLContext_glDrawRangeElements(sogl_current_render_glue(), (mode),(start),(end),(count),(type),(indices))
#undef glEnableClientState
#define glEnableClientState(array) SoGLContext_glEnableClientState(sogl_current_render_glue(), (array))
#undef glGenTextures
#define glGenTextures(n,textures) SoGLContext_glGenTextures(sogl_current_render_glue(), (n),(textures))
#undef glGetColorTable
#define glGetColorTable(target,format,type,data) SoGLContext_glGetColorTable(sogl_current_render_glue(), (target),(format),(type),(data))
#undef glGetColorTableParameterfv
#define glGetColorTableParameterfv(target,pname,params) SoGLContext_glGetColorTableParameterfv(sogl_current_render_glue(), (target),(pname),(params))
#undef glGetColorTableParameteriv
#define glGetColorTableParameteriv(target,pname,params) SoGLContext_glGetColorTableParameteriv(sogl_current_render_glue(), (target),(pname),(params))
#undef glGetCompressedTexImage
#define glGetCompressedTexImage(target,level,img) SoGLContext_glGetCompressedTexImage(sogl_current_render_glue(), (target),(level),(img))
#undef glIndexPointer
#define glIndexPointer(type,stride,pointer) SoGLContext_glIndexPointer(sogl_current_render_glue(), (type),(stride),(pointer))
#undef glInterleavedArrays
#define glInterleavedArrays(format,stride,pointer) SoGLContext_glInterleavedArrays(sogl_current_render_glue(), (format),(stride),(pointer))
#undef glMultiTexCoord2f
#define glMultiTexCoord2f(target,s,t) SoGLContext_glMultiTexCoord2f(sogl_current_render_glue(), (target),(s),(t))
#undef glMultiTexCoord2fv
#define glMultiTexCoord2fv(target,v) SoGLContext_glMultiTexCoord2fv(sogl_current_render_glue(), (target),(v))
#undef glMultiTexCoord3fv
#define glMultiTexCoord3fv(target,v) SoGLContext_glMultiTexCoord3fv(sogl_current_render_glue(), (target),(v))
#undef glMultiTexCoord4fv
#define glMultiTexCoord4fv(target,v) SoGLContext_glMultiTexCoord4fv(sogl_current_render_glue(), (target),(v))
#undef glNormalPointer
#define glNormalPointer(type,stride,pointer) SoGLContext_glNormalPointer(sogl_current_render_glue(), (type),(stride),(pointer))
#undef glPolygonOffset
#define glPolygonOffset(factor,units) SoGLContext_glPolygonOffset(sogl_current_render_glue(), (factor),(units))
#undef glPopClientAttrib
#define glPopClientAttrib() SoGLContext_glPopClientAttrib(sogl_current_render_glue())
#undef glPushClientAttrib
#define glPushClientAttrib(mask) SoGLContext_glPushClientAttrib(sogl_current_render_glue(), (mask))
#undef glTexCoordPointer
#define glTexCoordPointer(size,type,stride,pointer) SoGLContext_glTexCoordPointer(sogl_current_render_glue(), (size),(type),(stride),(pointer))
#undef glTexImage3D
#define glTexImage3D(target,level,internalformat,width,height,depth,border,format,type,pixels) SoGLContext_glTexImage3D(sogl_current_render_glue(), (target),(level),(internalformat),(width),(height),(depth),(border),(format),(type),(pixels))
#undef glTexSubImage2D
#define glTexSubImage2D(target,level,xoffset,yoffset,width,height,format,type,pixels) SoGLContext_glTexSubImage2D(sogl_current_render_glue(), (target),(level),(xoffset),(yoffset),(width),(height),(format),(type),(pixels))
#undef glTexSubImage3D
#define glTexSubImage3D(target,level,xoffset,yoffset,zoffset,width,height,depth,format,type,pixels) SoGLContext_glTexSubImage3D(sogl_current_render_glue(), (target),(level),(xoffset),(yoffset),(zoffset),(width),(height),(depth),(format),(type),(pixels))
#undef glVertexPointer
#define glVertexPointer(size,type,stride,pointer) SoGLContext_glVertexPointer(sogl_current_render_glue(), (size),(type),(stride),(pointer))
#undef glAreTexturesResident
#define glAreTexturesResident(n,textures,residences) SoGLContext_glAreTexturesResident(sogl_current_render_glue(), (n),(textures),(residences))
#undef glBlendColor
#define glBlendColor(red,green,blue,alpha) SoGLContext_glBlendColor(sogl_current_render_glue(), (red),(green),(blue),(alpha))
#undef glCallLists
#define glCallLists(n,type,lists) SoGLContext_glCallLists(sogl_current_render_glue(), (n),(type),(lists))
#undef glClearAccum
#define glClearAccum(red,green,blue,alpha) SoGLContext_glClearAccum(sogl_current_render_glue(), (red),(green),(blue),(alpha))
#undef glClearDepth
#define glClearDepth(depth) SoGLContext_glClearDepth(sogl_current_render_glue(), (depth))
#undef glColor3b
#define glColor3b(red,green,blue) SoGLContext_glColor3b(sogl_current_render_glue(), (red),(green),(blue))
#undef glColor3bv
#define glColor3bv(v) SoGLContext_glColor3bv(sogl_current_render_glue(), (v))
#undef glColor3d
#define glColor3d(red,green,blue) SoGLContext_glColor3d(sogl_current_render_glue(), (red),(green),(blue))
#undef glColor3dv
#define glColor3dv(v) SoGLContext_glColor3dv(sogl_current_render_glue(), (v))
#undef glColor3i
#define glColor3i(red,green,blue) SoGLContext_glColor3i(sogl_current_render_glue(), (red),(green),(blue))
#undef glColor3iv
#define glColor3iv(v) SoGLContext_glColor3iv(sogl_current_render_glue(), (v))
#undef glColor3s
#define glColor3s(red,green,blue) SoGLContext_glColor3s(sogl_current_render_glue(), (red),(green),(blue))
#undef glColor3sv
#define glColor3sv(v) SoGLContext_glColor3sv(sogl_current_render_glue(), (v))
#undef glColor3ui
#define glColor3ui(red,green,blue) SoGLContext_glColor3ui(sogl_current_render_glue(), (red),(green),(blue))
#undef glColor3uiv
#define glColor3uiv(v) SoGLContext_glColor3uiv(sogl_current_render_glue(), (v))
#undef glColor3us
#define glColor3us(red,green,blue) SoGLContext_glColor3us(sogl_current_render_glue(), (red),(green),(blue))
#undef glColor3usv
#define glColor3usv(v) SoGLContext_glColor3usv(sogl_current_render_glue(), (v))
#undef glColor4b
#define glColor4b(red,green,blue,alpha) SoGLContext_glColor4b(sogl_current_render_glue(), (red),(green),(blue),(alpha))
#undef glColor4bv
#define glColor4bv(v) SoGLContext_glColor4bv(sogl_current_render_glue(), (v))
#undef glColor4d
#define glColor4d(red,green,blue,alpha) SoGLContext_glColor4d(sogl_current_render_glue(), (red),(green),(blue),(alpha))
#undef glColor4dv
#define glColor4dv(v) SoGLContext_glColor4dv(sogl_current_render_glue(), (v))
#undef glColor4f
#define glColor4f(red,green,blue,alpha) SoGLContext_glColor4f(sogl_current_render_glue(), (red),(green),(blue),(alpha))
#undef glColor4fv
#define glColor4fv(v) SoGLContext_glColor4fv(sogl_current_render_glue(), (v))
#undef glColor4i
#define glColor4i(red,green,blue,alpha) SoGLContext_glColor4i(sogl_current_render_glue(), (red),(green),(blue),(alpha))
#undef glColor4iv
#define glColor4iv(v) SoGLContext_glColor4iv(sogl_current_render_glue(), (v))
#undef glColor4s
#define glColor4s(red,green,blue,alpha) SoGLContext_glColor4s(sogl_current_render_glue(), (red),(green),(blue),(alpha))
#undef glColor4sv
#define glColor4sv(v) SoGLContext_glColor4sv(sogl_current_render_glue(), (v))
#undef glColor4ubv
#define glColor4ubv(v) SoGLContext_glColor4ubv(sogl_current_render_glue(), (v))
#undef glColor4ui
#define glColor4ui(red,green,blue,alpha) SoGLContext_glColor4ui(sogl_current_render_glue(), (red),(green),(blue),(alpha))
#undef glColor4uiv
#define glColor4uiv(v) SoGLContext_glColor4uiv(sogl_current_render_glue(), (v))
#undef glColor4us
#define glColor4us(red,green,blue,alpha) SoGLContext_glColor4us(sogl_current_render_glue(), (red),(green),(blue),(alpha))
#undef glColor4usv
#define glColor4usv(v) SoGLContext_glColor4usv(sogl_current_render_glue(), (v))
#undef glColorTableParameterfv
#define glColorTableParameterfv(target,pname,params) SoGLContext_glColorTableParameterfv(sogl_current_render_glue(), (target),(pname),(params))
#undef glColorTableParameteriv
#define glColorTableParameteriv(target,pname,params) SoGLContext_glColorTableParameteriv(sogl_current_render_glue(), (target),(pname),(params))
#undef glConvolutionFilter1D
#define glConvolutionFilter1D(target,internalformat,width,format,type,image) SoGLContext_glConvolutionFilter1D(sogl_current_render_glue(), (target),(internalformat),(width),(format),(type),(image))
#undef glConvolutionFilter2D
#define glConvolutionFilter2D(target,internalformat,width,height,format,type,image) SoGLContext_glConvolutionFilter2D(sogl_current_render_glue(), (target),(internalformat),(width),(height),(format),(type),(image))
#undef glConvolutionParameterf
#define glConvolutionParameterf(target,pname,params) SoGLContext_glConvolutionParameterf(sogl_current_render_glue(), (target),(pname),(params))
#undef glConvolutionParameterfv
#define glConvolutionParameterfv(target,pname,params) SoGLContext_glConvolutionParameterfv(sogl_current_render_glue(), (target),(pname),(params))
#undef glConvolutionParameteri
#define glConvolutionParameteri(target,pname,params) SoGLContext_glConvolutionParameteri(sogl_current_render_glue(), (target),(pname),(params))
#undef glConvolutionParameteriv
#define glConvolutionParameteriv(target,pname,params) SoGLContext_glConvolutionParameteriv(sogl_current_render_glue(), (target),(pname),(params))
#undef glCopyColorSubTable
#define glCopyColorSubTable(target,start,x,y,width) SoGLContext_glCopyColorSubTable(sogl_current_render_glue(), (target),(start),(x),(y),(width))
#undef glCopyColorTable
#define glCopyColorTable(target,internalformat,x,y,width) SoGLContext_glCopyColorTable(sogl_current_render_glue(), (target),(internalformat),(x),(y),(width))
#undef glCopyConvolutionFilter1D
#define glCopyConvolutionFilter1D(target,internalformat,x,y,width) SoGLContext_glCopyConvolutionFilter1D(sogl_current_render_glue(), (target),(internalformat),(x),(y),(width))
#undef glCopyConvolutionFilter2D
#define glCopyConvolutionFilter2D(target,internalformat,x,y,width,height) SoGLContext_glCopyConvolutionFilter2D(sogl_current_render_glue(), (target),(internalformat),(x),(y),(width),(height))
#undef glCopyPixels
#define glCopyPixels(x,y,width,height,type) SoGLContext_glCopyPixels(sogl_current_render_glue(), (x),(y),(width),(height),(type))
#undef glCopyTexImage1D
#define glCopyTexImage1D(target,level,internalformat,x,y,width,border) SoGLContext_glCopyTexImage1D(sogl_current_render_glue(), (target),(level),(internalformat),(x),(y),(width),(border))
#undef glCopyTexSubImage1D
#define glCopyTexSubImage1D(target,level,xoffset,x,y,width) SoGLContext_glCopyTexSubImage1D(sogl_current_render_glue(), (target),(level),(xoffset),(x),(y),(width))
#undef glEdgeFlag
#define glEdgeFlag(flag) SoGLContext_glEdgeFlag(sogl_current_render_glue(), (flag))
#undef glEdgeFlagPointer
#define glEdgeFlagPointer(stride,ptr) SoGLContext_glEdgeFlagPointer(sogl_current_render_glue(), (stride),(ptr))
#undef glEdgeFlagv
#define glEdgeFlagv(flag) SoGLContext_glEdgeFlagv(sogl_current_render_glue(), (flag))
#undef glEvalCoord1d
#define glEvalCoord1d(u) SoGLContext_glEvalCoord1d(sogl_current_render_glue(), (u))
#undef glEvalCoord1dv
#define glEvalCoord1dv(u) SoGLContext_glEvalCoord1dv(sogl_current_render_glue(), (u))
#undef glEvalCoord1f
#define glEvalCoord1f(u) SoGLContext_glEvalCoord1f(sogl_current_render_glue(), (u))
#undef glEvalCoord1fv
#define glEvalCoord1fv(u) SoGLContext_glEvalCoord1fv(sogl_current_render_glue(), (u))
#undef glEvalCoord2d
#define glEvalCoord2d(u,v) SoGLContext_glEvalCoord2d(sogl_current_render_glue(), (u),(v))
#undef glEvalCoord2dv
#define glEvalCoord2dv(u) SoGLContext_glEvalCoord2dv(sogl_current_render_glue(), (u))
#undef glEvalCoord2f
#define glEvalCoord2f(u,v) SoGLContext_glEvalCoord2f(sogl_current_render_glue(), (u),(v))
#undef glEvalCoord2fv
#define glEvalCoord2fv(u) SoGLContext_glEvalCoord2fv(sogl_current_render_glue(), (u))
#undef glEvalMesh1
#define glEvalMesh1(mode,i1,i2) SoGLContext_glEvalMesh1(sogl_current_render_glue(), (mode),(i1),(i2))
#undef glEvalMesh2
#define glEvalMesh2(mode,i1,i2,j1,j2) SoGLContext_glEvalMesh2(sogl_current_render_glue(), (mode),(i1),(i2),(j1),(j2))
#undef glEvalPoint1
#define glEvalPoint1(i) SoGLContext_glEvalPoint1(sogl_current_render_glue(), (i))
#undef glEvalPoint2
#define glEvalPoint2(i,j) SoGLContext_glEvalPoint2(sogl_current_render_glue(), (i),(j))
#undef glFeedbackBuffer
#define glFeedbackBuffer(size,type,buffer) SoGLContext_glFeedbackBuffer(sogl_current_render_glue(), (size),(type),(buffer))
#undef glFogiv
#define glFogiv(pname,params) SoGLContext_glFogiv(sogl_current_render_glue(), (pname),(params))
#undef glGetClipPlane
#define glGetClipPlane(plane,equation) SoGLContext_glGetClipPlane(sogl_current_render_glue(), (plane),(equation))
#undef glGetConvolutionFilter
#define glGetConvolutionFilter(target,format,type,image) SoGLContext_glGetConvolutionFilter(sogl_current_render_glue(), (target),(format),(type),(image))
#undef glGetConvolutionParameterfv
#define glGetConvolutionParameterfv(target,pname,params) SoGLContext_glGetConvolutionParameterfv(sogl_current_render_glue(), (target),(pname),(params))
#undef glGetConvolutionParameteriv
#define glGetConvolutionParameteriv(target,pname,params) SoGLContext_glGetConvolutionParameteriv(sogl_current_render_glue(), (target),(pname),(params))
#undef glGetDoublev
#define glGetDoublev(pname,params) SoGLContext_glGetDoublev(sogl_current_render_glue(), (pname),(params))
#undef glGetHistogram
#define glGetHistogram(target,reset,format,type,values) SoGLContext_glGetHistogram(sogl_current_render_glue(), (target),(reset),(format),(type),(values))
#undef glGetHistogramParameterfv
#define glGetHistogramParameterfv(target,pname,params) SoGLContext_glGetHistogramParameterfv(sogl_current_render_glue(), (target),(pname),(params))
#undef glGetHistogramParameteriv
#define glGetHistogramParameteriv(target,pname,params) SoGLContext_glGetHistogramParameteriv(sogl_current_render_glue(), (target),(pname),(params))
#undef glGetLightfv
#define glGetLightfv(light,pname,params) SoGLContext_glGetLightfv(sogl_current_render_glue(), (light),(pname),(params))
#undef glGetLightiv
#define glGetLightiv(light,pname,params) SoGLContext_glGetLightiv(sogl_current_render_glue(), (light),(pname),(params))
#undef glGetMapdv
#define glGetMapdv(target,query,v) SoGLContext_glGetMapdv(sogl_current_render_glue(), (target),(query),(v))
#undef glGetMapfv
#define glGetMapfv(target,query,v) SoGLContext_glGetMapfv(sogl_current_render_glue(), (target),(query),(v))
#undef glGetMapiv
#define glGetMapiv(target,query,v) SoGLContext_glGetMapiv(sogl_current_render_glue(), (target),(query),(v))
#undef glGetMaterialfv
#define glGetMaterialfv(face,pname,params) SoGLContext_glGetMaterialfv(sogl_current_render_glue(), (face),(pname),(params))
#undef glGetMaterialiv
#define glGetMaterialiv(face,pname,params) SoGLContext_glGetMaterialiv(sogl_current_render_glue(), (face),(pname),(params))
#undef glGetMinmax
#define glGetMinmax(target,reset,format,types,values) SoGLContext_glGetMinmax(sogl_current_render_glue(), (target),(reset),(format),(types),(values))
#undef glGetMinmaxParameterfv
#define glGetMinmaxParameterfv(target,pname,params) SoGLContext_glGetMinmaxParameterfv(sogl_current_render_glue(), (target),(pname),(params))
#undef glGetMinmaxParameteriv
#define glGetMinmaxParameteriv(target,pname,params) SoGLContext_glGetMinmaxParameteriv(sogl_current_render_glue(), (target),(pname),(params))
#undef glGetPixelMapfv
#define glGetPixelMapfv(map,values) SoGLContext_glGetPixelMapfv(sogl_current_render_glue(), (map),(values))
#undef glGetPixelMapuiv
#define glGetPixelMapuiv(map,values) SoGLContext_glGetPixelMapuiv(sogl_current_render_glue(), (map),(values))
#undef glGetPixelMapusv
#define glGetPixelMapusv(map,values) SoGLContext_glGetPixelMapusv(sogl_current_render_glue(), (map),(values))
#undef glGetPointerv
#define glGetPointerv(pname,params) SoGLContext_glGetPointerv(sogl_current_render_glue(), (pname),(params))
#undef glGetPolygonStipple
#define glGetPolygonStipple(mask) SoGLContext_glGetPolygonStipple(sogl_current_render_glue(), (mask))
#undef glGetSeparableFilter
#define glGetSeparableFilter(target,format,type,row,column,span) SoGLContext_glGetSeparableFilter(sogl_current_render_glue(), (target),(format),(type),(row),(column),(span))
#undef glGetTexEnvfv
#define glGetTexEnvfv(target,pname,params) SoGLContext_glGetTexEnvfv(sogl_current_render_glue(), (target),(pname),(params))
#undef glGetTexEnviv
#define glGetTexEnviv(target,pname,params) SoGLContext_glGetTexEnviv(sogl_current_render_glue(), (target),(pname),(params))
#undef glGetTexGendv
#define glGetTexGendv(coord,pname,params) SoGLContext_glGetTexGendv(sogl_current_render_glue(), (coord),(pname),(params))
#undef glGetTexGenfv
#define glGetTexGenfv(coord,pname,params) SoGLContext_glGetTexGenfv(sogl_current_render_glue(), (coord),(pname),(params))
#undef glGetTexGeniv
#define glGetTexGeniv(coord,pname,params) SoGLContext_glGetTexGeniv(sogl_current_render_glue(), (coord),(pname),(params))
#undef glGetTexImage
#define glGetTexImage(target,level,format,type,pixels) SoGLContext_glGetTexImage(sogl_current_render_glue(), (target),(level),(format),(type),(pixels))
#undef glGetTexLevelParameterfv
#define glGetTexLevelParameterfv(target,level,pname,params) SoGLContext_glGetTexLevelParameterfv(sogl_current_render_glue(), (target),(level),(pname),(params))
#undef glGetTexLevelParameteriv
#define glGetTexLevelParameteriv(target,level,pname,params) SoGLContext_glGetTexLevelParameteriv(sogl_current_render_glue(), (target),(level),(pname),(params))
#undef glGetTexParameterfv
#define glGetTexParameterfv(target,pname,params) SoGLContext_glGetTexParameterfv(sogl_current_render_glue(), (target),(pname),(params))
#undef glGetTexParameteriv
#define glGetTexParameteriv(target,pname,params) SoGLContext_glGetTexParameteriv(sogl_current_render_glue(), (target),(pname),(params))
#undef glHint
#define glHint(target,mode) SoGLContext_glHint(sogl_current_render_glue(), (target),(mode))
#undef glHistogram
#define glHistogram(target,width,internalformat,sink) SoGLContext_glHistogram(sogl_current_render_glue(), (target),(width),(internalformat),(sink))
#undef glIndexMask
#define glIndexMask(mask) SoGLContext_glIndexMask(sogl_current_render_glue(), (mask))
#undef glIndexd
#define glIndexd(c) SoGLContext_glIndexd(sogl_current_render_glue(), (c))
#undef glIndexdv
#define glIndexdv(c) SoGLContext_glIndexdv(sogl_current_render_glue(), (c))
#undef glIndexf
#define glIndexf(c) SoGLContext_glIndexf(sogl_current_render_glue(), (c))
#undef glIndexfv
#define glIndexfv(c) SoGLContext_glIndexfv(sogl_current_render_glue(), (c))
#undef glIndexiv
#define glIndexiv(c) SoGLContext_glIndexiv(sogl_current_render_glue(), (c))
#undef glIndexs
#define glIndexs(c) SoGLContext_glIndexs(sogl_current_render_glue(), (c))
#undef glIndexsv
#define glIndexsv(c) SoGLContext_glIndexsv(sogl_current_render_glue(), (c))
#undef glIndexub
#define glIndexub(c) SoGLContext_glIndexub(sogl_current_render_glue(), (c))
#undef glIndexubv
#define glIndexubv(c) SoGLContext_glIndexubv(sogl_current_render_glue(), (c))
#undef glInitNames
#define glInitNames() SoGLContext_glInitNames(sogl_current_render_glue())
#undef glIsList
#define glIsList(list) SoGLContext_glIsList(sogl_current_render_glue(), (list))
#undef glIsTexture
#define glIsTexture(texture) SoGLContext_glIsTexture(sogl_current_render_glue(), (texture))
#undef glLightModelf
#define glLightModelf(pname,param) SoGLContext_glLightModelf(sogl_current_render_glue(), (pname),(param))
#undef glLightModeliv
#define glLightModeliv(pname,params) SoGLContext_glLightModeliv(sogl_current_render_glue(), (pname),(params))
#undef glLighti
#define glLighti(light,pname,param) SoGLContext_glLighti(sogl_current_render_glue(), (light),(pname),(param))
#undef glLightiv
#define glLightiv(light,pname,params) SoGLContext_glLightiv(sogl_current_render_glue(), (light),(pname),(params))
#undef glListBase
#define glListBase(base) SoGLContext_glListBase(sogl_current_render_glue(), (base))
#undef glLoadName
#define glLoadName(name) SoGLContext_glLoadName(sogl_current_render_glue(), (name))
#undef glLoadTransposeMatrixd
#define glLoadTransposeMatrixd(m) SoGLContext_glLoadTransposeMatrixd(sogl_current_render_glue(), (m))
#undef glLoadTransposeMatrixf
#define glLoadTransposeMatrixf(m) SoGLContext_glLoadTransposeMatrixf(sogl_current_render_glue(), (m))
#undef glLogicOp
#define glLogicOp(opcode) SoGLContext_glLogicOp(sogl_current_render_glue(), (opcode))
#undef glMap1d
#define glMap1d(target,u1,u2,stride,order,points) SoGLContext_glMap1d(sogl_current_render_glue(), (target),(u1),(u2),(stride),(order),(points))
#undef glMap1f
#define glMap1f(target,u1,u2,stride,order,points) SoGLContext_glMap1f(sogl_current_render_glue(), (target),(u1),(u2),(stride),(order),(points))
#undef glMap2d
#define glMap2d(target,u1,u2,ustride,uorder,v1,v2,vstride,vorder,points) SoGLContext_glMap2d(sogl_current_render_glue(), (target),(u1),(u2),(ustride),(uorder),(v1),(v2),(vstride),(vorder),(points))
#undef glMap2f
#define glMap2f(target,u1,u2,ustride,uorder,v1,v2,vstride,vorder,points) SoGLContext_glMap2f(sogl_current_render_glue(), (target),(u1),(u2),(ustride),(uorder),(v1),(v2),(vstride),(vorder),(points))
#undef glMapGrid1d
#define glMapGrid1d(un,u1,u2) SoGLContext_glMapGrid1d(sogl_current_render_glue(), (un),(u1),(u2))
#undef glMapGrid1f
#define glMapGrid1f(un,u1,u2) SoGLContext_glMapGrid1f(sogl_current_render_glue(), (un),(u1),(u2))
#undef glMapGrid2d
#define glMapGrid2d(un,u1,u2,vn,v1,v2) SoGLContext_glMapGrid2d(sogl_current_render_glue(), (un),(u1),(u2),(vn),(v1),(v2))
#undef glMapGrid2f
#define glMapGrid2f(un,u1,u2,vn,v1,v2) SoGLContext_glMapGrid2f(sogl_current_render_glue(), (un),(u1),(u2),(vn),(v1),(v2))
#undef glMateriali
#define glMateriali(face,pname,param) SoGLContext_glMateriali(sogl_current_render_glue(), (face),(pname),(param))
#undef glMaterialiv
#define glMaterialiv(face,pname,params) SoGLContext_glMaterialiv(sogl_current_render_glue(), (face),(pname),(params))
#undef glMinmax
#define glMinmax(target,internalformat,sink) SoGLContext_glMinmax(sogl_current_render_glue(), (target),(internalformat),(sink))
#undef glMultMatrixd
#define glMultMatrixd(m) SoGLContext_glMultMatrixd(sogl_current_render_glue(), (m))
#undef glMultTransposeMatrixd
#define glMultTransposeMatrixd(m) SoGLContext_glMultTransposeMatrixd(sogl_current_render_glue(), (m))
#undef glMultTransposeMatrixf
#define glMultTransposeMatrixf(m) SoGLContext_glMultTransposeMatrixf(sogl_current_render_glue(), (m))
#undef glMultiTexCoord1d
#define glMultiTexCoord1d(target,s) SoGLContext_glMultiTexCoord1d(sogl_current_render_glue(), (target),(s))
#undef glMultiTexCoord1dv
#define glMultiTexCoord1dv(target,v) SoGLContext_glMultiTexCoord1dv(sogl_current_render_glue(), (target),(v))
#undef glMultiTexCoord1f
#define glMultiTexCoord1f(target,s) SoGLContext_glMultiTexCoord1f(sogl_current_render_glue(), (target),(s))
#undef glMultiTexCoord1fv
#define glMultiTexCoord1fv(target,v) SoGLContext_glMultiTexCoord1fv(sogl_current_render_glue(), (target),(v))
#undef glMultiTexCoord1i
#define glMultiTexCoord1i(target,s) SoGLContext_glMultiTexCoord1i(sogl_current_render_glue(), (target),(s))
#undef glMultiTexCoord1iv
#define glMultiTexCoord1iv(target,v) SoGLContext_glMultiTexCoord1iv(sogl_current_render_glue(), (target),(v))
#undef glMultiTexCoord1s
#define glMultiTexCoord1s(target,s) SoGLContext_glMultiTexCoord1s(sogl_current_render_glue(), (target),(s))
#undef glMultiTexCoord1sv
#define glMultiTexCoord1sv(target,v) SoGLContext_glMultiTexCoord1sv(sogl_current_render_glue(), (target),(v))
#undef glMultiTexCoord2d
#define glMultiTexCoord2d(target,s,t) SoGLContext_glMultiTexCoord2d(sogl_current_render_glue(), (target),(s),(t))
#undef glMultiTexCoord2dv
#define glMultiTexCoord2dv(target,v) SoGLContext_glMultiTexCoord2dv(sogl_current_render_glue(), (target),(v))
#undef glMultiTexCoord2i
#define glMultiTexCoord2i(target,s,t) SoGLContext_glMultiTexCoord2i(sogl_current_render_glue(), (target),(s),(t))
#undef glMultiTexCoord2iv
#define glMultiTexCoord2iv(target,v) SoGLContext_glMultiTexCoord2iv(sogl_current_render_glue(), (target),(v))
#undef glMultiTexCoord2s
#define glMultiTexCoord2s(target,s,t) SoGLContext_glMultiTexCoord2s(sogl_current_render_glue(), (target),(s),(t))
#undef glMultiTexCoord2sv
#define glMultiTexCoord2sv(target,v) SoGLContext_glMultiTexCoord2sv(sogl_current_render_glue(), (target),(v))
#undef glMultiTexCoord3d
#define glMultiTexCoord3d(target,s,t,r) SoGLContext_glMultiTexCoord3d(sogl_current_render_glue(), (target),(s),(t),(r))
#undef glMultiTexCoord3dv
#define glMultiTexCoord3dv(target,v) SoGLContext_glMultiTexCoord3dv(sogl_current_render_glue(), (target),(v))
#undef glMultiTexCoord3f
#define glMultiTexCoord3f(target,s,t,r) SoGLContext_glMultiTexCoord3f(sogl_current_render_glue(), (target),(s),(t),(r))
#undef glMultiTexCoord3i
#define glMultiTexCoord3i(target,s,t,r) SoGLContext_glMultiTexCoord3i(sogl_current_render_glue(), (target),(s),(t),(r))
#undef glMultiTexCoord3iv
#define glMultiTexCoord3iv(target,v) SoGLContext_glMultiTexCoord3iv(sogl_current_render_glue(), (target),(v))
#undef glMultiTexCoord3s
#define glMultiTexCoord3s(target,s,t,r) SoGLContext_glMultiTexCoord3s(sogl_current_render_glue(), (target),(s),(t),(r))
#undef glMultiTexCoord3sv
#define glMultiTexCoord3sv(target,v) SoGLContext_glMultiTexCoord3sv(sogl_current_render_glue(), (target),(v))
#undef glMultiTexCoord4d
#define glMultiTexCoord4d(target,s,t,r,q) SoGLContext_glMultiTexCoord4d(sogl_current_render_glue(), (target),(s),(t),(r),(q))
#undef glMultiTexCoord4dv
#define glMultiTexCoord4dv(target,v) SoGLContext_glMultiTexCoord4dv(sogl_current_render_glue(), (target),(v))
#undef glMultiTexCoord4f
#define glMultiTexCoord4f(target,s,t,r,q) SoGLContext_glMultiTexCoord4f(sogl_current_render_glue(), (target),(s),(t),(r),(q))
#undef glMultiTexCoord4i
#define glMultiTexCoord4i(target,s,t,r,q) SoGLContext_glMultiTexCoord4i(sogl_current_render_glue(), (target),(s),(t),(r),(q))
#undef glMultiTexCoord4iv
#define glMultiTexCoord4iv(target,v) SoGLContext_glMultiTexCoord4iv(sogl_current_render_glue(), (target),(v))
#undef glMultiTexCoord4s
#define glMultiTexCoord4s(target,s,t,r,q) SoGLContext_glMultiTexCoord4s(sogl_current_render_glue(), (target),(s),(t),(r),(q))
#undef glMultiTexCoord4sv
#define glMultiTexCoord4sv(target,v) SoGLContext_glMultiTexCoord4sv(sogl_current_render_glue(), (target),(v))
#undef glNormal3b
#define glNormal3b(nx,ny,nz) SoGLContext_glNormal3b(sogl_current_render_glue(), (nx),(ny),(nz))
#undef glNormal3bv
#define glNormal3bv(v) SoGLContext_glNormal3bv(sogl_current_render_glue(), (v))
#undef glNormal3d
#define glNormal3d(nx,ny,nz) SoGLContext_glNormal3d(sogl_current_render_glue(), (nx),(ny),(nz))
#undef glNormal3dv
#define glNormal3dv(v) SoGLContext_glNormal3dv(sogl_current_render_glue(), (v))
#undef glNormal3i
#define glNormal3i(nx,ny,nz) SoGLContext_glNormal3i(sogl_current_render_glue(), (nx),(ny),(nz))
#undef glNormal3iv
#define glNormal3iv(v) SoGLContext_glNormal3iv(sogl_current_render_glue(), (v))
#undef glNormal3s
#define glNormal3s(nx,ny,nz) SoGLContext_glNormal3s(sogl_current_render_glue(), (nx),(ny),(nz))
#undef glNormal3sv
#define glNormal3sv(v) SoGLContext_glNormal3sv(sogl_current_render_glue(), (v))
#undef glPassThrough
#define glPassThrough(token) SoGLContext_glPassThrough(sogl_current_render_glue(), (token))
#undef glPixelMapusv
#define glPixelMapusv(map,mapsize,values) SoGLContext_glPixelMapusv(sogl_current_render_glue(), (map),(mapsize),(values))
#undef glPixelStoref
#define glPixelStoref(pname,param) SoGLContext_glPixelStoref(sogl_current_render_glue(), (pname),(param))
#undef glPopName
#define glPopName() SoGLContext_glPopName(sogl_current_render_glue())
#undef glPrioritizeTextures
#define glPrioritizeTextures(n,textures,priorities) SoGLContext_glPrioritizeTextures(sogl_current_render_glue(), (n),(textures),(priorities))
#undef glPushName
#define glPushName(name) SoGLContext_glPushName(sogl_current_render_glue(), (name))
#undef glRasterPos2d
#define glRasterPos2d(x,y) SoGLContext_glRasterPos2d(sogl_current_render_glue(), (x),(y))
#undef glRasterPos2dv
#define glRasterPos2dv(v) SoGLContext_glRasterPos2dv(sogl_current_render_glue(), (v))
#undef glRasterPos2fv
#define glRasterPos2fv(v) SoGLContext_glRasterPos2fv(sogl_current_render_glue(), (v))
#undef glRasterPos2i
#define glRasterPos2i(x,y) SoGLContext_glRasterPos2i(sogl_current_render_glue(), (x),(y))
#undef glRasterPos2iv
#define glRasterPos2iv(v) SoGLContext_glRasterPos2iv(sogl_current_render_glue(), (v))
#undef glRasterPos2s
#define glRasterPos2s(x,y) SoGLContext_glRasterPos2s(sogl_current_render_glue(), (x),(y))
#undef glRasterPos2sv
#define glRasterPos2sv(v) SoGLContext_glRasterPos2sv(sogl_current_render_glue(), (v))
#undef glRasterPos3d
#define glRasterPos3d(x,y,z) SoGLContext_glRasterPos3d(sogl_current_render_glue(), (x),(y),(z))
#undef glRasterPos3dv
#define glRasterPos3dv(v) SoGLContext_glRasterPos3dv(sogl_current_render_glue(), (v))
#undef glRasterPos3fv
#define glRasterPos3fv(v) SoGLContext_glRasterPos3fv(sogl_current_render_glue(), (v))
#undef glRasterPos3i
#define glRasterPos3i(x,y,z) SoGLContext_glRasterPos3i(sogl_current_render_glue(), (x),(y),(z))
#undef glRasterPos3iv
#define glRasterPos3iv(v) SoGLContext_glRasterPos3iv(sogl_current_render_glue(), (v))
#undef glRasterPos3s
#define glRasterPos3s(x,y,z) SoGLContext_glRasterPos3s(sogl_current_render_glue(), (x),(y),(z))
#undef glRasterPos3sv
#define glRasterPos3sv(v) SoGLContext_glRasterPos3sv(sogl_current_render_glue(), (v))
#undef glRasterPos4d
#define glRasterPos4d(x,y,z,w) SoGLContext_glRasterPos4d(sogl_current_render_glue(), (x),(y),(z),(w))
#undef glRasterPos4dv
#define glRasterPos4dv(v) SoGLContext_glRasterPos4dv(sogl_current_render_glue(), (v))
#undef glRasterPos4f
#define glRasterPos4f(x,y,z,w) SoGLContext_glRasterPos4f(sogl_current_render_glue(), (x),(y),(z),(w))
#undef glRasterPos4fv
#define glRasterPos4fv(v) SoGLContext_glRasterPos4fv(sogl_current_render_glue(), (v))
#undef glRasterPos4i
#define glRasterPos4i(x,y,z,w) SoGLContext_glRasterPos4i(sogl_current_render_glue(), (x),(y),(z),(w))
#undef glRasterPos4iv
#define glRasterPos4iv(v) SoGLContext_glRasterPos4iv(sogl_current_render_glue(), (v))
#undef glRasterPos4s
#define glRasterPos4s(x,y,z,w) SoGLContext_glRasterPos4s(sogl_current_render_glue(), (x),(y),(z),(w))
#undef glRasterPos4sv
#define glRasterPos4sv(v) SoGLContext_glRasterPos4sv(sogl_current_render_glue(), (v))
#undef glReadBuffer
#define glReadBuffer(mode) SoGLContext_glReadBuffer(sogl_current_render_glue(), (mode))
#undef glRectd
#define glRectd(x1,y1,x2,y2) SoGLContext_glRectd(sogl_current_render_glue(), (x1),(y1),(x2),(y2))
#undef glRectdv
#define glRectdv(v1,v2) SoGLContext_glRectdv(sogl_current_render_glue(), (v1),(v2))
#undef glRectf
#define glRectf(x1,y1,x2,y2) SoGLContext_glRectf(sogl_current_render_glue(), (x1),(y1),(x2),(y2))
#undef glRectfv
#define glRectfv(v1,v2) SoGLContext_glRectfv(sogl_current_render_glue(), (v1),(v2))
#undef glRecti
#define glRecti(x1,y1,x2,y2) SoGLContext_glRecti(sogl_current_render_glue(), (x1),(y1),(x2),(y2))
#undef glRectiv
#define glRectiv(v1,v2) SoGLContext_glRectiv(sogl_current_render_glue(), (v1),(v2))
#undef glRects
#define glRects(x1,y1,x2,y2) SoGLContext_glRects(sogl_current_render_glue(), (x1),(y1),(x2),(y2))
#undef glRectsv
#define glRectsv(v1,v2) SoGLContext_glRectsv(sogl_current_render_glue(), (v1),(v2))
#undef glRenderMode
#define glRenderMode(mode) SoGLContext_glRenderMode(sogl_current_render_glue(), (mode))
#undef glResetHistogram
#define glResetHistogram(target) SoGLContext_glResetHistogram(sogl_current_render_glue(), (target))
#undef glResetMinmax
#define glResetMinmax(target) SoGLContext_glResetMinmax(sogl_current_render_glue(), (target))
#undef glRotated
#define glRotated(angle,x,y,z) SoGLContext_glRotated(sogl_current_render_glue(), (angle),(x),(y),(z))
#undef glSampleCoverage
#define glSampleCoverage(value,invert) SoGLContext_glSampleCoverage(sogl_current_render_glue(), (value),(invert))
#undef glScaled
#define glScaled(x,y,z) SoGLContext_glScaled(sogl_current_render_glue(), (x),(y),(z))
#undef glSelectBuffer
#define glSelectBuffer(size,buffer) SoGLContext_glSelectBuffer(sogl_current_render_glue(), (size),(buffer))
#undef glSeparableFilter2D
#define glSeparableFilter2D(target,internalformat,width,height,format,type,row,column) SoGLContext_glSeparableFilter2D(sogl_current_render_glue(), (target),(internalformat),(width),(height),(format),(type),(row),(column))
#undef glShadeModel
#define glShadeModel(mode) SoGLContext_glShadeModel(sogl_current_render_glue(), (mode))
#undef glStencilMask
#define glStencilMask(mask) SoGLContext_glStencilMask(sogl_current_render_glue(), (mask))
#undef glTexCoord1d
#define glTexCoord1d(s) SoGLContext_glTexCoord1d(sogl_current_render_glue(), (s))
#undef glTexCoord1dv
#define glTexCoord1dv(v) SoGLContext_glTexCoord1dv(sogl_current_render_glue(), (v))
#undef glTexCoord1f
#define glTexCoord1f(s) SoGLContext_glTexCoord1f(sogl_current_render_glue(), (s))
#undef glTexCoord1fv
#define glTexCoord1fv(v) SoGLContext_glTexCoord1fv(sogl_current_render_glue(), (v))
#undef glTexCoord1i
#define glTexCoord1i(s) SoGLContext_glTexCoord1i(sogl_current_render_glue(), (s))
#undef glTexCoord1iv
#define glTexCoord1iv(v) SoGLContext_glTexCoord1iv(sogl_current_render_glue(), (v))
#undef glTexCoord1s
#define glTexCoord1s(s) SoGLContext_glTexCoord1s(sogl_current_render_glue(), (s))
#undef glTexCoord1sv
#define glTexCoord1sv(v) SoGLContext_glTexCoord1sv(sogl_current_render_glue(), (v))
#undef glTexCoord2d
#define glTexCoord2d(s,t) SoGLContext_glTexCoord2d(sogl_current_render_glue(), (s),(t))
#undef glTexCoord2dv
#define glTexCoord2dv(v) SoGLContext_glTexCoord2dv(sogl_current_render_glue(), (v))
#undef glTexCoord2i
#define glTexCoord2i(s,t) SoGLContext_glTexCoord2i(sogl_current_render_glue(), (s),(t))
#undef glTexCoord2iv
#define glTexCoord2iv(v) SoGLContext_glTexCoord2iv(sogl_current_render_glue(), (v))
#undef glTexCoord2s
#define glTexCoord2s(s,t) SoGLContext_glTexCoord2s(sogl_current_render_glue(), (s),(t))
#undef glTexCoord2sv
#define glTexCoord2sv(v) SoGLContext_glTexCoord2sv(sogl_current_render_glue(), (v))
#undef glTexCoord3d
#define glTexCoord3d(s,t,r) SoGLContext_glTexCoord3d(sogl_current_render_glue(), (s),(t),(r))
#undef glTexCoord3dv
#define glTexCoord3dv(v) SoGLContext_glTexCoord3dv(sogl_current_render_glue(), (v))
#undef glTexCoord3i
#define glTexCoord3i(s,t,r) SoGLContext_glTexCoord3i(sogl_current_render_glue(), (s),(t),(r))
#undef glTexCoord3iv
#define glTexCoord3iv(v) SoGLContext_glTexCoord3iv(sogl_current_render_glue(), (v))
#undef glTexCoord3s
#define glTexCoord3s(s,t,r) SoGLContext_glTexCoord3s(sogl_current_render_glue(), (s),(t),(r))
#undef glTexCoord3sv
#define glTexCoord3sv(v) SoGLContext_glTexCoord3sv(sogl_current_render_glue(), (v))
#undef glTexCoord4d
#define glTexCoord4d(s,t,r,q) SoGLContext_glTexCoord4d(sogl_current_render_glue(), (s),(t),(r),(q))
#undef glTexCoord4dv
#define glTexCoord4dv(v) SoGLContext_glTexCoord4dv(sogl_current_render_glue(), (v))
#undef glTexCoord4f
#define glTexCoord4f(s,t,r,q) SoGLContext_glTexCoord4f(sogl_current_render_glue(), (s),(t),(r),(q))
#undef glTexCoord4i
#define glTexCoord4i(s,t,r,q) SoGLContext_glTexCoord4i(sogl_current_render_glue(), (s),(t),(r),(q))
#undef glTexCoord4iv
#define glTexCoord4iv(v) SoGLContext_glTexCoord4iv(sogl_current_render_glue(), (v))
#undef glTexCoord4s
#define glTexCoord4s(s,t,r,q) SoGLContext_glTexCoord4s(sogl_current_render_glue(), (s),(t),(r),(q))
#undef glTexCoord4sv
#define glTexCoord4sv(v) SoGLContext_glTexCoord4sv(sogl_current_render_glue(), (v))
#undef glTexEnviv
#define glTexEnviv(target,pname,params) SoGLContext_glTexEnviv(sogl_current_render_glue(), (target),(pname),(params))
#undef glTexGend
#define glTexGend(coord,pname,param) SoGLContext_glTexGend(sogl_current_render_glue(), (coord),(pname),(param))
#undef glTexGendv
#define glTexGendv(coord,pname,params) SoGLContext_glTexGendv(sogl_current_render_glue(), (coord),(pname),(params))
#undef glTexGeniv
#define glTexGeniv(coord,pname,params) SoGLContext_glTexGeniv(sogl_current_render_glue(), (coord),(pname),(params))
#undef glTexImage1D
#define glTexImage1D(target,level,internalFormat,width,border,format,type,pixels) SoGLContext_glTexImage1D(sogl_current_render_glue(), (target),(level),(internalFormat),(width),(border),(format),(type),(pixels))
#undef glTexParameterfv
#define glTexParameterfv(target,pname,params) SoGLContext_glTexParameterfv(sogl_current_render_glue(), (target),(pname),(params))
#undef glTexParameteriv
#define glTexParameteriv(target,pname,params) SoGLContext_glTexParameteriv(sogl_current_render_glue(), (target),(pname),(params))
#undef glTexSubImage1D
#define glTexSubImage1D(target,level,xoffset,width,format,type,pixels) SoGLContext_glTexSubImage1D(sogl_current_render_glue(), (target),(level),(xoffset),(width),(format),(type),(pixels))
#undef glTranslated
#define glTranslated(x,y,z) SoGLContext_glTranslated(sogl_current_render_glue(), (x),(y),(z))
#undef glVertex2d
#define glVertex2d(x,y) SoGLContext_glVertex2d(sogl_current_render_glue(), (x),(y))
#undef glVertex2dv
#define glVertex2dv(v) SoGLContext_glVertex2dv(sogl_current_render_glue(), (v))
#undef glVertex2fv
#define glVertex2fv(v) SoGLContext_glVertex2fv(sogl_current_render_glue(), (v))
#undef glVertex2i
#define glVertex2i(x,y) SoGLContext_glVertex2i(sogl_current_render_glue(), (x),(y))
#undef glVertex2iv
#define glVertex2iv(v) SoGLContext_glVertex2iv(sogl_current_render_glue(), (v))
#undef glVertex2sv
#define glVertex2sv(v) SoGLContext_glVertex2sv(sogl_current_render_glue(), (v))
#undef glVertex3d
#define glVertex3d(x,y,z) SoGLContext_glVertex3d(sogl_current_render_glue(), (x),(y),(z))
#undef glVertex3dv
#define glVertex3dv(v) SoGLContext_glVertex3dv(sogl_current_render_glue(), (v))
#undef glVertex3i
#define glVertex3i(x,y,z) SoGLContext_glVertex3i(sogl_current_render_glue(), (x),(y),(z))
#undef glVertex3iv
#define glVertex3iv(v) SoGLContext_glVertex3iv(sogl_current_render_glue(), (v))
#undef glVertex3s
#define glVertex3s(x,y,z) SoGLContext_glVertex3s(sogl_current_render_glue(), (x),(y),(z))
#undef glVertex3sv
#define glVertex3sv(v) SoGLContext_glVertex3sv(sogl_current_render_glue(), (v))
#undef glVertex4d
#define glVertex4d(x,y,z,w) SoGLContext_glVertex4d(sogl_current_render_glue(), (x),(y),(z),(w))
#undef glVertex4dv
#define glVertex4dv(v) SoGLContext_glVertex4dv(sogl_current_render_glue(), (v))
#undef glVertex4f
#define glVertex4f(x,y,z,w) SoGLContext_glVertex4f(sogl_current_render_glue(), (x),(y),(z),(w))
#undef glVertex4i
#define glVertex4i(x,y,z,w) SoGLContext_glVertex4i(sogl_current_render_glue(), (x),(y),(z),(w))
#undef glVertex4iv
#define glVertex4iv(v) SoGLContext_glVertex4iv(sogl_current_render_glue(), (v))
#undef glVertex4s
#define glVertex4s(x,y,z,w) SoGLContext_glVertex4s(sogl_current_render_glue(), (x),(y),(z),(w))
#undef glVertex4sv
#define glVertex4sv(v) SoGLContext_glVertex4sv(sogl_current_render_glue(), (v))

#endif /* OBOL_INVENTOR_GL_H */
