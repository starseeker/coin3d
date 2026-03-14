/**************************************************************************\
 * src/glue/gl_stub.cpp
 *
 * No-op stub implementations of the GL dispatch layer (SoGLContext_* and
 * related helpers) for OBOL_NO_OPENGL builds.
 *
 * When OBOL_NO_OPENGL is set this file replaces src/glue/gl.cpp entirely.
 * All GL dispatch functions return safe "empty" values; all void functions
 * are no-ops.  The actual rendering is delegated exclusively to custom
 * context drivers (nanort, embree, vulkan, ...) via SoDB::ContextManager.
 *
 * NOTE: sogl_current_render_glue(), sogl_set_current_render_glue(),
 * sogl_glue_instance(), and sogl_glue_from_state() are defined in
 * src/rendering/SoGL.cpp (compiled in all builds), so they are NOT
 * repeated here.
 *
 * This file is ONLY compiled when OBOL_NO_OPENGL=ON.
\**************************************************************************/

#ifdef OBOL_NO_OPENGL  /* guard: only compile in no-GL builds */

#define OBOL_INTERNAL

#include "config.h"
#include "glue/glp.h"   /* includes SoGLDispatch.h and gl_stub.h types */
#include "glue/dlp.h"   /* coin_gl_getstring_ptr */
#include <Inventor/SbBasic.h>

/* Callback typedef used by SoGLContext_add_instance_created_callback */
typedef void SoGLContext_instance_created_cb(uint32_t contextid, void * closure);

// 600 stub implementations (functions already in SoGL.cpp excluded)
void SoGLContext_glBegin(const SoGLContext * glue, GLenum mode)
{
  (void)glue;
  (void)mode;
}

void SoGLContext_glEnd(const SoGLContext * glue)
{
  (void)glue;
}

void SoGLContext_glVertex2f(const SoGLContext * glue, GLfloat x, GLfloat y)
{
  (void)glue;
  (void)x;
  (void)y;
}

void SoGLContext_glVertex2s(const SoGLContext * glue, GLshort x, GLshort y)
{
  (void)glue;
  (void)x;
  (void)y;
}

void SoGLContext_glVertex3f(const SoGLContext * glue, GLfloat x, GLfloat y, GLfloat z)
{
  (void)glue;
  (void)x;
  (void)y;
  (void)z;
}

void SoGLContext_glVertex3fv(const SoGLContext * glue, const GLfloat * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glVertex4fv(const SoGLContext * glue, const GLfloat * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glNormal3f(const SoGLContext * glue, GLfloat nx, GLfloat ny, GLfloat nz)
{
  (void)glue;
  (void)nx;
  (void)ny;
  (void)nz;
}

void SoGLContext_glNormal3fv(const SoGLContext * glue, const GLfloat * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glColor3f(const SoGLContext * glue, GLfloat r, GLfloat g, GLfloat b)
{
  (void)glue;
  (void)r;
  (void)g;
  (void)b;
}

void SoGLContext_glColor3fv(const SoGLContext * glue, const GLfloat * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glColor3ub(const SoGLContext * glue, GLubyte r, GLubyte g, GLubyte b)
{
  (void)glue;
  (void)r;
  (void)g;
  (void)b;
}

void SoGLContext_glColor3ubv(const SoGLContext * glue, const GLubyte * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glColor4ub(const SoGLContext * glue, GLubyte r, GLubyte g, GLubyte b, GLubyte a)
{
  (void)glue;
  (void)r;
  (void)g;
  (void)b;
  (void)a;
}

void SoGLContext_glTexCoord2f(const SoGLContext * glue, GLfloat s, GLfloat t)
{
  (void)glue;
  (void)s;
  (void)t;
}

void SoGLContext_glTexCoord2fv(const SoGLContext * glue, const GLfloat * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glTexCoord3f(const SoGLContext * glue, GLfloat s, GLfloat t, GLfloat r)
{
  (void)glue;
  (void)s;
  (void)t;
  (void)r;
}

void SoGLContext_glTexCoord3fv(const SoGLContext * glue, const GLfloat * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glTexCoord4fv(const SoGLContext * glue, const GLfloat * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glIndexi(const SoGLContext * glue, GLint c)
{
  (void)glue;
  (void)c;
}

void SoGLContext_glMatrixMode(const SoGLContext * glue, GLenum mode)
{
  (void)glue;
  (void)mode;
}

void SoGLContext_glLoadIdentity(const SoGLContext * glue)
{
  (void)glue;
}

void SoGLContext_glLoadMatrixf(const SoGLContext * glue, const GLfloat * m)
{
  (void)glue;
  (void)m;
}

void SoGLContext_glLoadMatrixd(const SoGLContext * glue, const GLdouble * m)
{
  (void)glue;
  (void)m;
}

void SoGLContext_glMultMatrixf(const SoGLContext * glue, const GLfloat * m)
{
  (void)glue;
  (void)m;
}

void SoGLContext_glPushMatrix(const SoGLContext * glue)
{
  (void)glue;
}

void SoGLContext_glPopMatrix(const SoGLContext * glue)
{
  (void)glue;
}

void SoGLContext_glOrtho(const SoGLContext * glue, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val)
{
  (void)glue;
  (void)left;
  (void)right;
  (void)bottom;
  (void)top;
  (void)near_val;
  (void)far_val;
}

void SoGLContext_glFrustum(const SoGLContext * glue, GLdouble left, GLdouble right, GLdouble bottom, GLdouble top, GLdouble near_val, GLdouble far_val)
{
  (void)glue;
  (void)left;
  (void)right;
  (void)bottom;
  (void)top;
  (void)near_val;
  (void)far_val;
}

void SoGLContext_glTranslatef(const SoGLContext * glue, GLfloat x, GLfloat y, GLfloat z)
{
  (void)glue;
  (void)x;
  (void)y;
  (void)z;
}

void SoGLContext_glRotatef(const SoGLContext * glue, GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
  (void)glue;
  (void)angle;
  (void)x;
  (void)y;
  (void)z;
}

void SoGLContext_glScalef(const SoGLContext * glue, GLfloat x, GLfloat y, GLfloat z)
{
  (void)glue;
  (void)x;
  (void)y;
  (void)z;
}

void SoGLContext_glEnable(const SoGLContext * glue, GLenum cap)
{
  (void)glue;
  (void)cap;
}

void SoGLContext_glDisable(const SoGLContext * glue, GLenum cap)
{
  (void)glue;
  (void)cap;
}

GLboolean SoGLContext_glIsEnabled(const SoGLContext * glue, GLenum cap)
{
  (void)glue;
  (void)cap;
  return 0 /* FALSE */;
}

void SoGLContext_glPushAttrib(const SoGLContext * glue, GLbitfield mask)
{
  (void)glue;
  (void)mask;
}

void SoGLContext_glPopAttrib(const SoGLContext * glue)
{
  (void)glue;
}

void SoGLContext_glDepthMask(const SoGLContext * glue, GLboolean flag)
{
  (void)glue;
  (void)flag;
}

void SoGLContext_glDepthFunc(const SoGLContext * glue, GLenum func)
{
  (void)glue;
  (void)func;
}

void SoGLContext_glDepthRange(const SoGLContext * glue, GLclampd near_val, GLclampd far_val)
{
  (void)glue;
  (void)near_val;
  (void)far_val;
}

void SoGLContext_glBlendFunc(const SoGLContext * glue, GLenum sfactor, GLenum dfactor)
{
  (void)glue;
  (void)sfactor;
  (void)dfactor;
}

void SoGLContext_glAlphaFunc(const SoGLContext * glue, GLenum func, GLclampf ref)
{
  (void)glue;
  (void)func;
  (void)ref;
}

void SoGLContext_glViewport(const SoGLContext * glue, GLint x, GLint y, GLsizei width, GLsizei height)
{
  (void)glue;
  (void)x;
  (void)y;
  (void)width;
  (void)height;
}

void SoGLContext_glScissor(const SoGLContext * glue, GLint x, GLint y, GLsizei width, GLsizei height)
{
  (void)glue;
  (void)x;
  (void)y;
  (void)width;
  (void)height;
}

void SoGLContext_glClearColor(const SoGLContext * glue, GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
  (void)glue;
  (void)red;
  (void)green;
  (void)blue;
  (void)alpha;
}

void SoGLContext_glClear(const SoGLContext * glue, GLbitfield mask)
{
  (void)glue;
  (void)mask;
}

void SoGLContext_glFlush(const SoGLContext * glue)
{
  (void)glue;
}

void SoGLContext_glFinish(const SoGLContext * glue)
{
  (void)glue;
}

GLenum SoGLContext_glGetError(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* GL_NO_ERROR */;
}

const GLubyte * SoGLContext_glGetString(const SoGLContext * glue, GLenum name)
{
  (void)glue;
  (void)name;
  return nullptr;
}

void SoGLContext_glGetIntegerv(const SoGLContext * glue, GLenum pname, GLint * params)
{
  (void)glue;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetFloatv(const SoGLContext * glue, GLenum pname, GLfloat * params)
{
  (void)glue;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetBooleanv(const SoGLContext * glue, GLenum pname, GLboolean * params)
{
  (void)glue;
  (void)pname;
  (void)params;
}

void SoGLContext_glColorMask(const SoGLContext * glue, GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
  (void)glue;
  (void)red;
  (void)green;
  (void)blue;
  (void)alpha;
}

void SoGLContext_glStencilFunc(const SoGLContext * glue, GLenum func, GLint ref, GLuint mask)
{
  (void)glue;
  (void)func;
  (void)ref;
  (void)mask;
}

void SoGLContext_glStencilOp(const SoGLContext * glue, GLenum fail, GLenum zfail, GLenum zpass)
{
  (void)glue;
  (void)fail;
  (void)zfail;
  (void)zpass;
}

void SoGLContext_glFrontFace(const SoGLContext * glue, GLenum mode)
{
  (void)glue;
  (void)mode;
}

void SoGLContext_glCullFace(const SoGLContext * glue, GLenum mode)
{
  (void)glue;
  (void)mode;
}

void SoGLContext_glPolygonMode(const SoGLContext * glue, GLenum face, GLenum mode)
{
  (void)glue;
  (void)face;
  (void)mode;
}

void SoGLContext_glPolygonStipple(const SoGLContext * glue, const GLubyte * mask)
{
  (void)glue;
  (void)mask;
}

void SoGLContext_glLineWidth(const SoGLContext * glue, GLfloat width)
{
  (void)glue;
  (void)width;
}

void SoGLContext_glLineStipple(const SoGLContext * glue, GLint factor, GLushort pattern)
{
  (void)glue;
  (void)factor;
  (void)pattern;
}

void SoGLContext_glPointSize(const SoGLContext * glue, GLfloat size)
{
  (void)glue;
  (void)size;
}

void SoGLContext_glClipPlane(const SoGLContext * glue, GLenum plane, const GLdouble * equation)
{
  (void)glue;
  (void)plane;
  (void)equation;
}

void SoGLContext_glDrawBuffer(const SoGLContext * glue, GLenum mode)
{
  (void)glue;
  (void)mode;
}

void SoGLContext_glClearIndex(const SoGLContext * glue, GLfloat c)
{
  (void)glue;
  (void)c;
}

void SoGLContext_glClearStencil(const SoGLContext * glue, GLint s)
{
  (void)glue;
  (void)s;
}

void SoGLContext_glAccum(const SoGLContext * glue, GLenum op, GLfloat value)
{
  (void)glue;
  (void)op;
  (void)value;
}

void SoGLContext_glPixelStorei(const SoGLContext * glue, GLenum pname, GLint param)
{
  (void)glue;
  (void)pname;
  (void)param;
}

void SoGLContext_glReadPixels(const SoGLContext * glue, GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, GLvoid * pixels)
{
  (void)glue;
  (void)x;
  (void)y;
  (void)width;
  (void)height;
  (void)format;
  (void)type;
  (void)pixels;
}

void SoGLContext_glRasterPos2f(const SoGLContext * glue, GLfloat x, GLfloat y)
{
  (void)glue;
  (void)x;
  (void)y;
}

void SoGLContext_glRasterPos3f(const SoGLContext * glue, GLfloat x, GLfloat y, GLfloat z)
{
  (void)glue;
  (void)x;
  (void)y;
  (void)z;
}

void SoGLContext_glBitmap(const SoGLContext * glue, GLsizei width, GLsizei height, GLfloat xorig, GLfloat yorig, GLfloat xmove, GLfloat ymove, const GLubyte * bitmap)
{
  (void)glue;
  (void)width;
  (void)height;
  (void)xorig;
  (void)yorig;
  (void)xmove;
  (void)ymove;
  (void)bitmap;
}

void SoGLContext_glDrawPixels(const SoGLContext * glue, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid * pixels)
{
  (void)glue;
  (void)width;
  (void)height;
  (void)format;
  (void)type;
  (void)pixels;
}

void SoGLContext_glPixelTransferf(const SoGLContext * glue, GLenum pname, GLfloat param)
{
  (void)glue;
  (void)pname;
  (void)param;
}

void SoGLContext_glPixelTransferi(const SoGLContext * glue, GLenum pname, GLint param)
{
  (void)glue;
  (void)pname;
  (void)param;
}

void SoGLContext_glPixelMapfv(const SoGLContext * glue, GLenum map, GLint mapsize, const GLfloat * values)
{
  (void)glue;
  (void)map;
  (void)mapsize;
  (void)values;
}

void SoGLContext_glPixelMapuiv(const SoGLContext * glue, GLenum map, GLint mapsize, const GLuint * values)
{
  (void)glue;
  (void)map;
  (void)mapsize;
  (void)values;
}

void SoGLContext_glPixelZoom(const SoGLContext * glue, GLfloat xfactor, GLfloat yfactor)
{
  (void)glue;
  (void)xfactor;
  (void)yfactor;
}

void SoGLContext_glLightf(const SoGLContext * glue, GLenum light, GLenum pname, GLfloat param)
{
  (void)glue;
  (void)light;
  (void)pname;
  (void)param;
}

void SoGLContext_glLightfv(const SoGLContext * glue, GLenum light, GLenum pname, const GLfloat * params)
{
  (void)glue;
  (void)light;
  (void)pname;
  (void)params;
}

void SoGLContext_glLightModeli(const SoGLContext * glue, GLenum pname, GLint param)
{
  (void)glue;
  (void)pname;
  (void)param;
}

void SoGLContext_glLightModelfv(const SoGLContext * glue, GLenum pname, const GLfloat * params)
{
  (void)glue;
  (void)pname;
  (void)params;
}

void SoGLContext_glMaterialf(const SoGLContext * glue, GLenum face, GLenum pname, GLfloat param)
{
  (void)glue;
  (void)face;
  (void)pname;
  (void)param;
}

void SoGLContext_glMaterialfv(const SoGLContext * glue, GLenum face, GLenum pname, const GLfloat * params)
{
  (void)glue;
  (void)face;
  (void)pname;
  (void)params;
}

void SoGLContext_glColorMaterial(const SoGLContext * glue, GLenum face, GLenum mode)
{
  (void)glue;
  (void)face;
  (void)mode;
}

void SoGLContext_glFogi(const SoGLContext * glue, GLenum pname, GLint param)
{
  (void)glue;
  (void)pname;
  (void)param;
}

void SoGLContext_glFogf(const SoGLContext * glue, GLenum pname, GLfloat param)
{
  (void)glue;
  (void)pname;
  (void)param;
}

void SoGLContext_glFogfv(const SoGLContext * glue, GLenum pname, const GLfloat * params)
{
  (void)glue;
  (void)pname;
  (void)params;
}

void SoGLContext_glTexEnvi(const SoGLContext * glue, GLenum target, GLenum pname, GLint param)
{
  (void)glue;
  (void)target;
  (void)pname;
  (void)param;
}

void SoGLContext_glTexEnvf(const SoGLContext * glue, GLenum target, GLenum pname, GLfloat param)
{
  (void)glue;
  (void)target;
  (void)pname;
  (void)param;
}

void SoGLContext_glTexEnvfv(const SoGLContext * glue, GLenum target, GLenum pname, const GLfloat * params)
{
  (void)glue;
  (void)target;
  (void)pname;
  (void)params;
}

void SoGLContext_glTexGeni(const SoGLContext * glue, GLenum coord, GLenum pname, GLint param)
{
  (void)glue;
  (void)coord;
  (void)pname;
  (void)param;
}

void SoGLContext_glTexGenf(const SoGLContext * glue, GLenum coord, GLenum pname, GLfloat param)
{
  (void)glue;
  (void)coord;
  (void)pname;
  (void)param;
}

void SoGLContext_glTexGenfv(const SoGLContext * glue, GLenum coord, GLenum pname, const GLfloat * params)
{
  (void)glue;
  (void)coord;
  (void)pname;
  (void)params;
}

void SoGLContext_glTexParameteri(const SoGLContext * glue, GLenum target, GLenum pname, GLint param)
{
  (void)glue;
  (void)target;
  (void)pname;
  (void)param;
}

void SoGLContext_glTexParameterf(const SoGLContext * glue, GLenum target, GLenum pname, GLfloat param)
{
  (void)glue;
  (void)target;
  (void)pname;
  (void)param;
}

void SoGLContext_glTexImage2D(const SoGLContext * glue, GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid * pixels)
{
  (void)glue;
  (void)target;
  (void)level;
  (void)internalformat;
  (void)width;
  (void)height;
  (void)border;
  (void)format;
  (void)type;
  (void)pixels;
}

void SoGLContext_glCopyTexImage2D(const SoGLContext * glue, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height, GLint border)
{
  (void)glue;
  (void)target;
  (void)level;
  (void)internalformat;
  (void)x;
  (void)y;
  (void)width;
  (void)height;
  (void)border;
}

void SoGLContext_glCopyTexSubImage2D(const SoGLContext * glue, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
  (void)glue;
  (void)target;
  (void)level;
  (void)xoffset;
  (void)yoffset;
  (void)x;
  (void)y;
  (void)width;
  (void)height;
}

void SoGLContext_glNewList(const SoGLContext * glue, GLuint list, GLenum mode)
{
  (void)glue;
  (void)list;
  (void)mode;
}

void SoGLContext_glEndList(const SoGLContext * glue)
{
  (void)glue;
}

void SoGLContext_glCallList(const SoGLContext * glue, GLuint list)
{
  (void)glue;
  (void)list;
}

void SoGLContext_glDeleteLists(const SoGLContext * glue, GLuint list, GLsizei range)
{
  (void)glue;
  (void)list;
  (void)range;
}

GLuint SoGLContext_glGenLists(const SoGLContext * glue, GLsizei range)
{
  (void)glue;
  (void)range;
  return 0;
}

void SoGLContext_glDrawArrays(const SoGLContext * glue, GLenum mode, GLint first, GLsizei count)
{
  (void)glue;
  (void)mode;
  (void)first;
  (void)count;
}

void SoGLContext_glDrawElements(const SoGLContext * glue, GLenum mode, GLsizei count, GLenum type, const GLvoid * indices)
{
  (void)glue;
  (void)mode;
  (void)count;
  (void)type;
  (void)indices;
}

void SoGLContext_glActiveTexture(const SoGLContext * glue, GLenum texture)
{
  (void)glue;
  (void)texture;
}

void SoGLContext_glArrayElement(const SoGLContext * glue, GLint i)
{
  (void)glue;
  (void)i;
}

void SoGLContext_glBindTexture(const SoGLContext * glue, GLenum target, GLuint texture)
{
  (void)glue;
  (void)target;
  (void)texture;
}

void SoGLContext_glBlendEquation(const SoGLContext * glue, GLenum mode)
{
  (void)glue;
  (void)mode;
}

void SoGLContext_glClientActiveTexture(const SoGLContext * glue, GLenum texture)
{
  (void)glue;
  (void)texture;
}

void SoGLContext_glColorPointer(const SoGLContext * glue, GLint size, GLenum type, GLsizei stride, const GLvoid * pointer)
{
  (void)glue;
  (void)size;
  (void)type;
  (void)stride;
  (void)pointer;
}

void SoGLContext_glColorSubTable(const SoGLContext * glue, GLenum target, GLsizei start, GLsizei count, GLenum format, GLenum type, const GLvoid * data)
{
  (void)glue;
  (void)target;
  (void)start;
  (void)count;
  (void)format;
  (void)type;
  (void)data;
}

void SoGLContext_glColorTable(const SoGLContext * glue, GLenum target, GLenum internalFormat, GLsizei width, GLenum format, GLenum type, const GLvoid *table)
{
  (void)glue;
  (void)target;
  (void)internalFormat;
  (void)width;
  (void)format;
  (void)type;
  (void)table;
}

void SoGLContext_glCompressedTexImage1D(const SoGLContext * glue, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLint border, GLsizei imageSize, const GLvoid *data)
{
  (void)glue;
  (void)target;
  (void)level;
  (void)internalformat;
  (void)width;
  (void)border;
  (void)imageSize;
  (void)data;
}

void SoGLContext_glCompressedTexImage2D(const SoGLContext * glue, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLint border, GLsizei imageSize, const GLvoid *data)
{
  (void)glue;
  (void)target;
  (void)level;
  (void)internalformat;
  (void)width;
  (void)height;
  (void)border;
  (void)imageSize;
  (void)data;
}

void SoGLContext_glCompressedTexImage3D(const SoGLContext * glue, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLsizei imageSize, const GLvoid * data)
{
  (void)glue;
  (void)target;
  (void)level;
  (void)internalformat;
  (void)width;
  (void)height;
  (void)depth;
  (void)border;
  (void)imageSize;
  (void)data;
}

void SoGLContext_glCompressedTexSubImage1D(const SoGLContext * glue, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLsizei imageSize, const GLvoid *data)
{
  (void)glue;
  (void)target;
  (void)level;
  (void)xoffset;
  (void)width;
  (void)format;
  (void)imageSize;
  (void)data;
}

void SoGLContext_glCompressedTexSubImage2D(const SoGLContext * glue, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLsizei imageSize, const GLvoid *data)
{
  (void)glue;
  (void)target;
  (void)level;
  (void)xoffset;
  (void)yoffset;
  (void)width;
  (void)height;
  (void)format;
  (void)imageSize;
  (void)data;
}

void SoGLContext_glCompressedTexSubImage3D(const SoGLContext * glue, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLsizei imageSize, const GLvoid *data)
{
  (void)glue;
  (void)target;
  (void)level;
  (void)xoffset;
  (void)yoffset;
  (void)zoffset;
  (void)width;
  (void)height;
  (void)depth;
  (void)format;
  (void)imageSize;
  (void)data;
}

void SoGLContext_glCopyTexSubImage3D(const SoGLContext * glue, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLint x, GLint y, GLsizei width, GLsizei height)
{
  (void)glue;
  (void)target;
  (void)level;
  (void)xoffset;
  (void)yoffset;
  (void)zoffset;
  (void)x;
  (void)y;
  (void)width;
  (void)height;
}

void SoGLContext_glDeleteTextures(const SoGLContext * glue, GLsizei n, const GLuint * textures)
{
  (void)glue;
  (void)n;
  (void)textures;
}

void SoGLContext_glDisableClientState(const SoGLContext * glue, GLenum array)
{
  (void)glue;
  (void)array;
}

void SoGLContext_glDrawRangeElements(const SoGLContext * glue, GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type, const GLvoid * indices)
{
  (void)glue;
  (void)mode;
  (void)start;
  (void)end;
  (void)count;
  (void)type;
  (void)indices;
}

void SoGLContext_glEnableClientState(const SoGLContext * glue, GLenum array)
{
  (void)glue;
  (void)array;
}

void SoGLContext_glGenTextures(const SoGLContext * glue, GLsizei n, GLuint *textures)
{
  (void)glue;
  (void)n;
  (void)textures;
}

void SoGLContext_glGetColorTable(const SoGLContext * glue, GLenum target, GLenum format, GLenum type, GLvoid *data)
{
  (void)glue;
  (void)target;
  (void)format;
  (void)type;
  (void)data;
}

void SoGLContext_glGetColorTableParameterfv(const SoGLContext * glue, GLenum target, GLenum pname, GLfloat *params)
{
  (void)glue;
  (void)target;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetColorTableParameteriv(const SoGLContext * glue, GLenum target, GLenum pname, GLint *params)
{
  (void)glue;
  (void)target;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetCompressedTexImage(const SoGLContext * glue, GLenum target, GLint level, void *img)
{
  (void)glue;
  (void)target;
  (void)level;
  (void)img;
}

void SoGLContext_glIndexPointer(const SoGLContext * glue, GLenum type, GLsizei stride, const GLvoid * pointer)
{
  (void)glue;
  (void)type;
  (void)stride;
  (void)pointer;
}

void SoGLContext_glInterleavedArrays(const SoGLContext * glue, GLenum format, GLsizei stride, const GLvoid * pointer)
{
  (void)glue;
  (void)format;
  (void)stride;
  (void)pointer;
}

void SoGLContext_glMultiTexCoord2f(const SoGLContext * glue, GLenum target, GLfloat s, GLfloat t)
{
  (void)glue;
  (void)target;
  (void)s;
  (void)t;
}

void SoGLContext_glMultiTexCoord2fv(const SoGLContext * glue, GLenum target, const GLfloat * v)
{
  (void)glue;
  (void)target;
  (void)v;
}

void SoGLContext_glMultiTexCoord3fv(const SoGLContext * glue, GLenum target, const GLfloat * v)
{
  (void)glue;
  (void)target;
  (void)v;
}

void SoGLContext_glMultiTexCoord4fv(const SoGLContext * glue, GLenum target, const GLfloat * v)
{
  (void)glue;
  (void)target;
  (void)v;
}

void SoGLContext_glNormalPointer(const SoGLContext * glue, GLenum type, GLsizei stride, const GLvoid *pointer)
{
  (void)glue;
  (void)type;
  (void)stride;
  (void)pointer;
}

void SoGLContext_glPolygonOffset(const SoGLContext * glue, GLfloat factor, GLfloat units)
{
  (void)glue;
  (void)factor;
  (void)units;
}

void SoGLContext_glPopClientAttrib(const SoGLContext * glue)
{
  (void)glue;
}

void SoGLContext_glPushClientAttrib(const SoGLContext * glue, GLbitfield mask)
{
  (void)glue;
  (void)mask;
}

void SoGLContext_glTexCoordPointer(const SoGLContext * glue, GLint size, GLenum type, GLsizei stride, const GLvoid * pointer)
{
  (void)glue;
  (void)size;
  (void)type;
  (void)stride;
  (void)pointer;
}

void SoGLContext_glTexImage3D(const SoGLContext * glue, GLenum target, GLint level, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type, const GLvoid *pixels)
{
  (void)glue;
  (void)target;
  (void)level;
  (void)internalformat;
  (void)width;
  (void)height;
  (void)depth;
  (void)border;
  (void)format;
  (void)type;
  (void)pixels;
}

void SoGLContext_glTexSubImage2D(const SoGLContext * glue, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid * pixels)
{
  (void)glue;
  (void)target;
  (void)level;
  (void)xoffset;
  (void)yoffset;
  (void)width;
  (void)height;
  (void)format;
  (void)type;
  (void)pixels;
}

void SoGLContext_glTexSubImage3D(const SoGLContext * glue, GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const GLvoid * pixels)
{
  (void)glue;
  (void)target;
  (void)level;
  (void)xoffset;
  (void)yoffset;
  (void)zoffset;
  (void)width;
  (void)height;
  (void)depth;
  (void)format;
  (void)type;
  (void)pixels;
}

void SoGLContext_glVertexPointer(const SoGLContext * glue, GLint size, GLenum type, GLsizei stride, const GLvoid * pointer)
{
  (void)glue;
  (void)size;
  (void)type;
  (void)stride;
  (void)pointer;
}

GLboolean SoGLContext_glAreTexturesResident(const SoGLContext * glue, GLsizei n, const GLuint * textures, GLboolean * residences)
{
  (void)glue;
  (void)n;
  (void)textures;
  (void)residences;
  return 0 /* FALSE */;
}

void SoGLContext_glBlendColor(const SoGLContext * glue, GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
  (void)glue;
  (void)red;
  (void)green;
  (void)blue;
  (void)alpha;
}

void SoGLContext_glCallLists(const SoGLContext * glue, GLsizei n, GLenum type, const GLvoid * lists)
{
  (void)glue;
  (void)n;
  (void)type;
  (void)lists;
}

void SoGLContext_glClearAccum(const SoGLContext * glue, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
  (void)glue;
  (void)red;
  (void)green;
  (void)blue;
  (void)alpha;
}

void SoGLContext_glClearDepth(const SoGLContext * glue, GLclampd depth)
{
  (void)glue;
  (void)depth;
}

void SoGLContext_glColor3b(const SoGLContext * glue, GLbyte red, GLbyte green, GLbyte blue)
{
  (void)glue;
  (void)red;
  (void)green;
  (void)blue;
}

void SoGLContext_glColor3bv(const SoGLContext * glue, const GLbyte * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glColor3d(const SoGLContext * glue, GLdouble red, GLdouble green, GLdouble blue)
{
  (void)glue;
  (void)red;
  (void)green;
  (void)blue;
}

void SoGLContext_glColor3dv(const SoGLContext * glue, const GLdouble * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glColor3i(const SoGLContext * glue, GLint red, GLint green, GLint blue)
{
  (void)glue;
  (void)red;
  (void)green;
  (void)blue;
}

void SoGLContext_glColor3iv(const SoGLContext * glue, const GLint * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glColor3s(const SoGLContext * glue, GLshort red, GLshort green, GLshort blue)
{
  (void)glue;
  (void)red;
  (void)green;
  (void)blue;
}

void SoGLContext_glColor3sv(const SoGLContext * glue, const GLshort * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glColor3ui(const SoGLContext * glue, GLuint red, GLuint green, GLuint blue)
{
  (void)glue;
  (void)red;
  (void)green;
  (void)blue;
}

void SoGLContext_glColor3uiv(const SoGLContext * glue, const GLuint * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glColor3us(const SoGLContext * glue, GLushort red, GLushort green, GLushort blue)
{
  (void)glue;
  (void)red;
  (void)green;
  (void)blue;
}

void SoGLContext_glColor3usv(const SoGLContext * glue, const GLushort * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glColor4b(const SoGLContext * glue, GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha)
{
  (void)glue;
  (void)red;
  (void)green;
  (void)blue;
  (void)alpha;
}

void SoGLContext_glColor4bv(const SoGLContext * glue, const GLbyte * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glColor4d(const SoGLContext * glue, GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha)
{
  (void)glue;
  (void)red;
  (void)green;
  (void)blue;
  (void)alpha;
}

void SoGLContext_glColor4dv(const SoGLContext * glue, const GLdouble * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glColor4f(const SoGLContext * glue, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
  (void)glue;
  (void)red;
  (void)green;
  (void)blue;
  (void)alpha;
}

void SoGLContext_glColor4fv(const SoGLContext * glue, const GLfloat * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glColor4i(const SoGLContext * glue, GLint red, GLint green, GLint blue, GLint alpha)
{
  (void)glue;
  (void)red;
  (void)green;
  (void)blue;
  (void)alpha;
}

void SoGLContext_glColor4iv(const SoGLContext * glue, const GLint * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glColor4s(const SoGLContext * glue, GLshort red, GLshort green, GLshort blue, GLshort alpha)
{
  (void)glue;
  (void)red;
  (void)green;
  (void)blue;
  (void)alpha;
}

void SoGLContext_glColor4sv(const SoGLContext * glue, const GLshort * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glColor4ubv(const SoGLContext * glue, const GLubyte * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glColor4ui(const SoGLContext * glue, GLuint red, GLuint green, GLuint blue, GLuint alpha)
{
  (void)glue;
  (void)red;
  (void)green;
  (void)blue;
  (void)alpha;
}

void SoGLContext_glColor4uiv(const SoGLContext * glue, const GLuint * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glColor4us(const SoGLContext * glue, GLushort red, GLushort green, GLushort blue, GLushort alpha)
{
  (void)glue;
  (void)red;
  (void)green;
  (void)blue;
  (void)alpha;
}

void SoGLContext_glColor4usv(const SoGLContext * glue, const GLushort * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glColorTableParameterfv(const SoGLContext * glue, GLenum target, GLenum pname, const GLfloat * params)
{
  (void)glue;
  (void)target;
  (void)pname;
  (void)params;
}

void SoGLContext_glColorTableParameteriv(const SoGLContext * glue, GLenum target, GLenum pname, const GLint * params)
{
  (void)glue;
  (void)target;
  (void)pname;
  (void)params;
}

void SoGLContext_glConvolutionFilter1D(const SoGLContext * glue, GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, const GLvoid * image)
{
  (void)glue;
  (void)target;
  (void)internalformat;
  (void)width;
  (void)format;
  (void)type;
  (void)image;
}

void SoGLContext_glConvolutionFilter2D(const SoGLContext * glue, GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid * image)
{
  (void)glue;
  (void)target;
  (void)internalformat;
  (void)width;
  (void)height;
  (void)format;
  (void)type;
  (void)image;
}

void SoGLContext_glConvolutionParameterf(const SoGLContext * glue, GLenum target, GLenum pname, GLfloat params)
{
  (void)glue;
  (void)target;
  (void)pname;
  (void)params;
}

void SoGLContext_glConvolutionParameterfv(const SoGLContext * glue, GLenum target, GLenum pname, const GLfloat * params)
{
  (void)glue;
  (void)target;
  (void)pname;
  (void)params;
}

void SoGLContext_glConvolutionParameteri(const SoGLContext * glue, GLenum target, GLenum pname, GLint params)
{
  (void)glue;
  (void)target;
  (void)pname;
  (void)params;
}

void SoGLContext_glConvolutionParameteriv(const SoGLContext * glue, GLenum target, GLenum pname, const GLint * params)
{
  (void)glue;
  (void)target;
  (void)pname;
  (void)params;
}

void SoGLContext_glCopyColorSubTable(const SoGLContext * glue, GLenum target, GLsizei start, GLint x, GLint y, GLsizei width)
{
  (void)glue;
  (void)target;
  (void)start;
  (void)x;
  (void)y;
  (void)width;
}

void SoGLContext_glCopyColorTable(const SoGLContext * glue, GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width)
{
  (void)glue;
  (void)target;
  (void)internalformat;
  (void)x;
  (void)y;
  (void)width;
}

void SoGLContext_glCopyConvolutionFilter1D(const SoGLContext * glue, GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width)
{
  (void)glue;
  (void)target;
  (void)internalformat;
  (void)x;
  (void)y;
  (void)width;
}

void SoGLContext_glCopyConvolutionFilter2D(const SoGLContext * glue, GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height)
{
  (void)glue;
  (void)target;
  (void)internalformat;
  (void)x;
  (void)y;
  (void)width;
  (void)height;
}

void SoGLContext_glCopyPixels(const SoGLContext * glue, GLint x, GLint y, GLsizei width, GLsizei height, GLenum type)
{
  (void)glue;
  (void)x;
  (void)y;
  (void)width;
  (void)height;
  (void)type;
}

void SoGLContext_glCopyTexImage1D(const SoGLContext * glue, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border)
{
  (void)glue;
  (void)target;
  (void)level;
  (void)internalformat;
  (void)x;
  (void)y;
  (void)width;
  (void)border;
}

void SoGLContext_glCopyTexSubImage1D(const SoGLContext * glue, GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width)
{
  (void)glue;
  (void)target;
  (void)level;
  (void)xoffset;
  (void)x;
  (void)y;
  (void)width;
}

void SoGLContext_glEdgeFlag(const SoGLContext * glue, GLboolean flag)
{
  (void)glue;
  (void)flag;
}

void SoGLContext_glEdgeFlagPointer(const SoGLContext * glue, GLsizei stride, const GLvoid * ptr)
{
  (void)glue;
  (void)stride;
  (void)ptr;
}

void SoGLContext_glEdgeFlagv(const SoGLContext * glue, const GLboolean * flag)
{
  (void)glue;
  (void)flag;
}

void SoGLContext_glEvalCoord1d(const SoGLContext * glue, GLdouble u)
{
  (void)glue;
  (void)u;
}

void SoGLContext_glEvalCoord1dv(const SoGLContext * glue, const GLdouble * u)
{
  (void)glue;
  (void)u;
}

void SoGLContext_glEvalCoord1f(const SoGLContext * glue, GLfloat u)
{
  (void)glue;
  (void)u;
}

void SoGLContext_glEvalCoord1fv(const SoGLContext * glue, const GLfloat * u)
{
  (void)glue;
  (void)u;
}

void SoGLContext_glEvalCoord2d(const SoGLContext * glue, GLdouble u, GLdouble v)
{
  (void)glue;
  (void)u;
  (void)v;
}

void SoGLContext_glEvalCoord2dv(const SoGLContext * glue, const GLdouble * u)
{
  (void)glue;
  (void)u;
}

void SoGLContext_glEvalCoord2f(const SoGLContext * glue, GLfloat u, GLfloat v)
{
  (void)glue;
  (void)u;
  (void)v;
}

void SoGLContext_glEvalCoord2fv(const SoGLContext * glue, const GLfloat * u)
{
  (void)glue;
  (void)u;
}

void SoGLContext_glEvalMesh1(const SoGLContext * glue, GLenum mode, GLint i1, GLint i2)
{
  (void)glue;
  (void)mode;
  (void)i1;
  (void)i2;
}

void SoGLContext_glEvalMesh2(const SoGLContext * glue, GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2)
{
  (void)glue;
  (void)mode;
  (void)i1;
  (void)i2;
  (void)j1;
  (void)j2;
}

void SoGLContext_glEvalPoint1(const SoGLContext * glue, GLint i)
{
  (void)glue;
  (void)i;
}

void SoGLContext_glEvalPoint2(const SoGLContext * glue, GLint i, GLint j)
{
  (void)glue;
  (void)i;
  (void)j;
}

void SoGLContext_glFeedbackBuffer(const SoGLContext * glue, GLsizei size, GLenum type, GLfloat * buffer)
{
  (void)glue;
  (void)size;
  (void)type;
  (void)buffer;
}

void SoGLContext_glFogiv(const SoGLContext * glue, GLenum pname, const GLint * params)
{
  (void)glue;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetClipPlane(const SoGLContext * glue, GLenum plane, GLdouble * equation)
{
  (void)glue;
  (void)plane;
  (void)equation;
}

void SoGLContext_glGetConvolutionFilter(const SoGLContext * glue, GLenum target, GLenum format, GLenum type, GLvoid * image)
{
  (void)glue;
  (void)target;
  (void)format;
  (void)type;
  (void)image;
}

void SoGLContext_glGetConvolutionParameterfv(const SoGLContext * glue, GLenum target, GLenum pname, GLfloat * params)
{
  (void)glue;
  (void)target;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetConvolutionParameteriv(const SoGLContext * glue, GLenum target, GLenum pname, GLint * params)
{
  (void)glue;
  (void)target;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetDoublev(const SoGLContext * glue, GLenum pname, GLdouble * params)
{
  (void)glue;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetHistogram(const SoGLContext * glue, GLenum target, GLboolean reset, GLenum format, GLenum type, GLvoid * values)
{
  (void)glue;
  (void)target;
  (void)reset;
  (void)format;
  (void)type;
  (void)values;
}

void SoGLContext_glGetHistogramParameterfv(const SoGLContext * glue, GLenum target, GLenum pname, GLfloat * params)
{
  (void)glue;
  (void)target;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetHistogramParameteriv(const SoGLContext * glue, GLenum target, GLenum pname, GLint * params)
{
  (void)glue;
  (void)target;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetLightfv(const SoGLContext * glue, GLenum light, GLenum pname, GLfloat * params)
{
  (void)glue;
  (void)light;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetLightiv(const SoGLContext * glue, GLenum light, GLenum pname, GLint * params)
{
  (void)glue;
  (void)light;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetMapdv(const SoGLContext * glue, GLenum target, GLenum query, GLdouble * v)
{
  (void)glue;
  (void)target;
  (void)query;
  (void)v;
}

void SoGLContext_glGetMapfv(const SoGLContext * glue, GLenum target, GLenum query, GLfloat * v)
{
  (void)glue;
  (void)target;
  (void)query;
  (void)v;
}

void SoGLContext_glGetMapiv(const SoGLContext * glue, GLenum target, GLenum query, GLint * v)
{
  (void)glue;
  (void)target;
  (void)query;
  (void)v;
}

void SoGLContext_glGetMaterialfv(const SoGLContext * glue, GLenum face, GLenum pname, GLfloat * params)
{
  (void)glue;
  (void)face;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetMaterialiv(const SoGLContext * glue, GLenum face, GLenum pname, GLint * params)
{
  (void)glue;
  (void)face;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetMinmax(const SoGLContext * glue, GLenum target, GLboolean reset, GLenum format, GLenum types, GLvoid * values)
{
  (void)glue;
  (void)target;
  (void)reset;
  (void)format;
  (void)types;
  (void)values;
}

void SoGLContext_glGetMinmaxParameterfv(const SoGLContext * glue, GLenum target, GLenum pname, GLfloat * params)
{
  (void)glue;
  (void)target;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetMinmaxParameteriv(const SoGLContext * glue, GLenum target, GLenum pname, GLint * params)
{
  (void)glue;
  (void)target;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetPixelMapfv(const SoGLContext * glue, GLenum map, GLfloat * values)
{
  (void)glue;
  (void)map;
  (void)values;
}

void SoGLContext_glGetPixelMapuiv(const SoGLContext * glue, GLenum map, GLuint * values)
{
  (void)glue;
  (void)map;
  (void)values;
}

void SoGLContext_glGetPixelMapusv(const SoGLContext * glue, GLenum map, GLushort * values)
{
  (void)glue;
  (void)map;
  (void)values;
}

void SoGLContext_glGetPointerv(const SoGLContext * glue, GLenum pname, GLvoid ** params)
{
  (void)glue;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetPolygonStipple(const SoGLContext * glue, GLubyte * mask)
{
  (void)glue;
  (void)mask;
}

void SoGLContext_glGetSeparableFilter(const SoGLContext * glue, GLenum target, GLenum format, GLenum type, GLvoid * row, GLvoid * column, GLvoid * span)
{
  (void)glue;
  (void)target;
  (void)format;
  (void)type;
  (void)row;
  (void)column;
  (void)span;
}

void SoGLContext_glGetTexEnvfv(const SoGLContext * glue, GLenum target, GLenum pname, GLfloat * params)
{
  (void)glue;
  (void)target;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetTexEnviv(const SoGLContext * glue, GLenum target, GLenum pname, GLint * params)
{
  (void)glue;
  (void)target;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetTexGendv(const SoGLContext * glue, GLenum coord, GLenum pname, GLdouble * params)
{
  (void)glue;
  (void)coord;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetTexGenfv(const SoGLContext * glue, GLenum coord, GLenum pname, GLfloat * params)
{
  (void)glue;
  (void)coord;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetTexGeniv(const SoGLContext * glue, GLenum coord, GLenum pname, GLint * params)
{
  (void)glue;
  (void)coord;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetTexImage(const SoGLContext * glue, GLenum target, GLint level, GLenum format, GLenum type, GLvoid * pixels)
{
  (void)glue;
  (void)target;
  (void)level;
  (void)format;
  (void)type;
  (void)pixels;
}

void SoGLContext_glGetTexLevelParameterfv(const SoGLContext * glue, GLenum target, GLint level, GLenum pname, GLfloat * params)
{
  (void)glue;
  (void)target;
  (void)level;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetTexLevelParameteriv(const SoGLContext * glue, GLenum target, GLint level, GLenum pname, GLint * params)
{
  (void)glue;
  (void)target;
  (void)level;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetTexParameterfv(const SoGLContext * glue, GLenum target, GLenum pname, GLfloat * params)
{
  (void)glue;
  (void)target;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetTexParameteriv(const SoGLContext * glue, GLenum target, GLenum pname, GLint * params)
{
  (void)glue;
  (void)target;
  (void)pname;
  (void)params;
}

void SoGLContext_glHint(const SoGLContext * glue, GLenum target, GLenum mode)
{
  (void)glue;
  (void)target;
  (void)mode;
}

void SoGLContext_glHistogram(const SoGLContext * glue, GLenum target, GLsizei width, GLenum internalformat, GLboolean sink)
{
  (void)glue;
  (void)target;
  (void)width;
  (void)internalformat;
  (void)sink;
}

void SoGLContext_glIndexMask(const SoGLContext * glue, GLuint mask)
{
  (void)glue;
  (void)mask;
}

void SoGLContext_glIndexd(const SoGLContext * glue, GLdouble c)
{
  (void)glue;
  (void)c;
}

void SoGLContext_glIndexdv(const SoGLContext * glue, const GLdouble * c)
{
  (void)glue;
  (void)c;
}

void SoGLContext_glIndexf(const SoGLContext * glue, GLfloat c)
{
  (void)glue;
  (void)c;
}

void SoGLContext_glIndexfv(const SoGLContext * glue, const GLfloat * c)
{
  (void)glue;
  (void)c;
}

void SoGLContext_glIndexiv(const SoGLContext * glue, const GLint * c)
{
  (void)glue;
  (void)c;
}

void SoGLContext_glIndexs(const SoGLContext * glue, GLshort c)
{
  (void)glue;
  (void)c;
}

void SoGLContext_glIndexsv(const SoGLContext * glue, const GLshort * c)
{
  (void)glue;
  (void)c;
}

void SoGLContext_glIndexub(const SoGLContext * glue, GLubyte c)
{
  (void)glue;
  (void)c;
}

void SoGLContext_glIndexubv(const SoGLContext * glue, const GLubyte * c)
{
  (void)glue;
  (void)c;
}

void SoGLContext_glInitNames(const SoGLContext * glue)
{
  (void)glue;
}

GLboolean SoGLContext_glIsList(const SoGLContext * glue, GLuint list)
{
  (void)glue;
  (void)list;
  return 0 /* FALSE */;
}

GLboolean SoGLContext_glIsTexture(const SoGLContext * glue, GLuint texture)
{
  (void)glue;
  (void)texture;
  return 0 /* FALSE */;
}

void SoGLContext_glLightModelf(const SoGLContext * glue, GLenum pname, GLfloat param)
{
  (void)glue;
  (void)pname;
  (void)param;
}

void SoGLContext_glLightModeliv(const SoGLContext * glue, GLenum pname, const GLint * params)
{
  (void)glue;
  (void)pname;
  (void)params;
}

void SoGLContext_glLighti(const SoGLContext * glue, GLenum light, GLenum pname, GLint param)
{
  (void)glue;
  (void)light;
  (void)pname;
  (void)param;
}

void SoGLContext_glLightiv(const SoGLContext * glue, GLenum light, GLenum pname, const GLint * params)
{
  (void)glue;
  (void)light;
  (void)pname;
  (void)params;
}

void SoGLContext_glListBase(const SoGLContext * glue, GLuint base)
{
  (void)glue;
  (void)base;
}

void SoGLContext_glLoadName(const SoGLContext * glue, GLuint name)
{
  (void)glue;
  (void)name;
}

void SoGLContext_glLoadTransposeMatrixd(const SoGLContext * glue, const GLdouble * m)
{
  (void)glue;
  (void)m;
}

void SoGLContext_glLoadTransposeMatrixf(const SoGLContext * glue, const GLfloat * m)
{
  (void)glue;
  (void)m;
}

void SoGLContext_glLogicOp(const SoGLContext * glue, GLenum opcode)
{
  (void)glue;
  (void)opcode;
}

void SoGLContext_glMap1d(const SoGLContext * glue, GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble * points)
{
  (void)glue;
  (void)target;
  (void)u1;
  (void)u2;
  (void)stride;
  (void)order;
  (void)points;
}

void SoGLContext_glMap1f(const SoGLContext * glue, GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat * points)
{
  (void)glue;
  (void)target;
  (void)u1;
  (void)u2;
  (void)stride;
  (void)order;
  (void)points;
}

void SoGLContext_glMap2d(const SoGLContext * glue, GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble * points)
{
  (void)glue;
  (void)target;
  (void)u1;
  (void)u2;
  (void)ustride;
  (void)uorder;
  (void)v1;
  (void)v2;
  (void)vstride;
  (void)vorder;
  (void)points;
}

void SoGLContext_glMap2f(const SoGLContext * glue, GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat * points)
{
  (void)glue;
  (void)target;
  (void)u1;
  (void)u2;
  (void)ustride;
  (void)uorder;
  (void)v1;
  (void)v2;
  (void)vstride;
  (void)vorder;
  (void)points;
}

void SoGLContext_glMapGrid1d(const SoGLContext * glue, GLint un, GLdouble u1, GLdouble u2)
{
  (void)glue;
  (void)un;
  (void)u1;
  (void)u2;
}

void SoGLContext_glMapGrid1f(const SoGLContext * glue, GLint un, GLfloat u1, GLfloat u2)
{
  (void)glue;
  (void)un;
  (void)u1;
  (void)u2;
}

void SoGLContext_glMapGrid2d(const SoGLContext * glue, GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2)
{
  (void)glue;
  (void)un;
  (void)u1;
  (void)u2;
  (void)vn;
  (void)v1;
  (void)v2;
}

void SoGLContext_glMapGrid2f(const SoGLContext * glue, GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2)
{
  (void)glue;
  (void)un;
  (void)u1;
  (void)u2;
  (void)vn;
  (void)v1;
  (void)v2;
}

void SoGLContext_glMateriali(const SoGLContext * glue, GLenum face, GLenum pname, GLint param)
{
  (void)glue;
  (void)face;
  (void)pname;
  (void)param;
}

void SoGLContext_glMaterialiv(const SoGLContext * glue, GLenum face, GLenum pname, const GLint * params)
{
  (void)glue;
  (void)face;
  (void)pname;
  (void)params;
}

void SoGLContext_glMinmax(const SoGLContext * glue, GLenum target, GLenum internalformat, GLboolean sink)
{
  (void)glue;
  (void)target;
  (void)internalformat;
  (void)sink;
}

void SoGLContext_glMultMatrixd(const SoGLContext * glue, const GLdouble * m)
{
  (void)glue;
  (void)m;
}

void SoGLContext_glMultTransposeMatrixd(const SoGLContext * glue, const GLdouble * m)
{
  (void)glue;
  (void)m;
}

void SoGLContext_glMultTransposeMatrixf(const SoGLContext * glue, const GLfloat * m)
{
  (void)glue;
  (void)m;
}

void SoGLContext_glMultiTexCoord1d(const SoGLContext * glue, GLenum target, GLdouble s)
{
  (void)glue;
  (void)target;
  (void)s;
}

void SoGLContext_glMultiTexCoord1dv(const SoGLContext * glue, GLenum target, const GLdouble * v)
{
  (void)glue;
  (void)target;
  (void)v;
}

void SoGLContext_glMultiTexCoord1f(const SoGLContext * glue, GLenum target, GLfloat s)
{
  (void)glue;
  (void)target;
  (void)s;
}

void SoGLContext_glMultiTexCoord1fv(const SoGLContext * glue, GLenum target, const GLfloat * v)
{
  (void)glue;
  (void)target;
  (void)v;
}

void SoGLContext_glMultiTexCoord1i(const SoGLContext * glue, GLenum target, GLint s)
{
  (void)glue;
  (void)target;
  (void)s;
}

void SoGLContext_glMultiTexCoord1iv(const SoGLContext * glue, GLenum target, const GLint * v)
{
  (void)glue;
  (void)target;
  (void)v;
}

void SoGLContext_glMultiTexCoord1s(const SoGLContext * glue, GLenum target, GLshort s)
{
  (void)glue;
  (void)target;
  (void)s;
}

void SoGLContext_glMultiTexCoord1sv(const SoGLContext * glue, GLenum target, const GLshort * v)
{
  (void)glue;
  (void)target;
  (void)v;
}

void SoGLContext_glMultiTexCoord2d(const SoGLContext * glue, GLenum target, GLdouble s, GLdouble t)
{
  (void)glue;
  (void)target;
  (void)s;
  (void)t;
}

void SoGLContext_glMultiTexCoord2dv(const SoGLContext * glue, GLenum target, const GLdouble * v)
{
  (void)glue;
  (void)target;
  (void)v;
}

void SoGLContext_glMultiTexCoord2i(const SoGLContext * glue, GLenum target, GLint s, GLint t)
{
  (void)glue;
  (void)target;
  (void)s;
  (void)t;
}

void SoGLContext_glMultiTexCoord2iv(const SoGLContext * glue, GLenum target, const GLint * v)
{
  (void)glue;
  (void)target;
  (void)v;
}

void SoGLContext_glMultiTexCoord2s(const SoGLContext * glue, GLenum target, GLshort s, GLshort t)
{
  (void)glue;
  (void)target;
  (void)s;
  (void)t;
}

void SoGLContext_glMultiTexCoord2sv(const SoGLContext * glue, GLenum target, const GLshort * v)
{
  (void)glue;
  (void)target;
  (void)v;
}

void SoGLContext_glMultiTexCoord3d(const SoGLContext * glue, GLenum target, GLdouble s, GLdouble t, GLdouble r)
{
  (void)glue;
  (void)target;
  (void)s;
  (void)t;
  (void)r;
}

void SoGLContext_glMultiTexCoord3dv(const SoGLContext * glue, GLenum target, const GLdouble * v)
{
  (void)glue;
  (void)target;
  (void)v;
}

void SoGLContext_glMultiTexCoord3f(const SoGLContext * glue, GLenum target, GLfloat s, GLfloat t, GLfloat r)
{
  (void)glue;
  (void)target;
  (void)s;
  (void)t;
  (void)r;
}

void SoGLContext_glMultiTexCoord3i(const SoGLContext * glue, GLenum target, GLint s, GLint t, GLint r)
{
  (void)glue;
  (void)target;
  (void)s;
  (void)t;
  (void)r;
}

void SoGLContext_glMultiTexCoord3iv(const SoGLContext * glue, GLenum target, const GLint * v)
{
  (void)glue;
  (void)target;
  (void)v;
}

void SoGLContext_glMultiTexCoord3s(const SoGLContext * glue, GLenum target, GLshort s, GLshort t, GLshort r)
{
  (void)glue;
  (void)target;
  (void)s;
  (void)t;
  (void)r;
}

void SoGLContext_glMultiTexCoord3sv(const SoGLContext * glue, GLenum target, const GLshort * v)
{
  (void)glue;
  (void)target;
  (void)v;
}

void SoGLContext_glMultiTexCoord4d(const SoGLContext * glue, GLenum target, GLdouble s, GLdouble t, GLdouble r, GLdouble q)
{
  (void)glue;
  (void)target;
  (void)s;
  (void)t;
  (void)r;
  (void)q;
}

void SoGLContext_glMultiTexCoord4dv(const SoGLContext * glue, GLenum target, const GLdouble * v)
{
  (void)glue;
  (void)target;
  (void)v;
}

void SoGLContext_glMultiTexCoord4f(const SoGLContext * glue, GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q)
{
  (void)glue;
  (void)target;
  (void)s;
  (void)t;
  (void)r;
  (void)q;
}

void SoGLContext_glMultiTexCoord4i(const SoGLContext * glue, GLenum target, GLint s, GLint t, GLint r, GLint q)
{
  (void)glue;
  (void)target;
  (void)s;
  (void)t;
  (void)r;
  (void)q;
}

void SoGLContext_glMultiTexCoord4iv(const SoGLContext * glue, GLenum target, const GLint * v)
{
  (void)glue;
  (void)target;
  (void)v;
}

void SoGLContext_glMultiTexCoord4s(const SoGLContext * glue, GLenum target, GLshort s, GLshort t, GLshort r, GLshort q)
{
  (void)glue;
  (void)target;
  (void)s;
  (void)t;
  (void)r;
  (void)q;
}

void SoGLContext_glMultiTexCoord4sv(const SoGLContext * glue, GLenum target, const GLshort * v)
{
  (void)glue;
  (void)target;
  (void)v;
}

void SoGLContext_glNormal3b(const SoGLContext * glue, GLbyte nx, GLbyte ny, GLbyte nz)
{
  (void)glue;
  (void)nx;
  (void)ny;
  (void)nz;
}

void SoGLContext_glNormal3bv(const SoGLContext * glue, const GLbyte * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glNormal3d(const SoGLContext * glue, GLdouble nx, GLdouble ny, GLdouble nz)
{
  (void)glue;
  (void)nx;
  (void)ny;
  (void)nz;
}

void SoGLContext_glNormal3dv(const SoGLContext * glue, const GLdouble * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glNormal3i(const SoGLContext * glue, GLint nx, GLint ny, GLint nz)
{
  (void)glue;
  (void)nx;
  (void)ny;
  (void)nz;
}

void SoGLContext_glNormal3iv(const SoGLContext * glue, const GLint * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glNormal3s(const SoGLContext * glue, GLshort nx, GLshort ny, GLshort nz)
{
  (void)glue;
  (void)nx;
  (void)ny;
  (void)nz;
}

void SoGLContext_glNormal3sv(const SoGLContext * glue, const GLshort * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glPassThrough(const SoGLContext * glue, GLfloat token)
{
  (void)glue;
  (void)token;
}

void SoGLContext_glPixelMapusv(const SoGLContext * glue, GLenum map, GLsizei mapsize, const GLushort * values)
{
  (void)glue;
  (void)map;
  (void)mapsize;
  (void)values;
}

void SoGLContext_glPixelStoref(const SoGLContext * glue, GLenum pname, GLfloat param)
{
  (void)glue;
  (void)pname;
  (void)param;
}

void SoGLContext_glPopName(const SoGLContext * glue)
{
  (void)glue;
}

void SoGLContext_glPrioritizeTextures(const SoGLContext * glue, GLsizei n, const GLuint * textures, const GLclampf * priorities)
{
  (void)glue;
  (void)n;
  (void)textures;
  (void)priorities;
}

void SoGLContext_glPushName(const SoGLContext * glue, GLuint name)
{
  (void)glue;
  (void)name;
}

void SoGLContext_glRasterPos2d(const SoGLContext * glue, GLdouble x, GLdouble y)
{
  (void)glue;
  (void)x;
  (void)y;
}

void SoGLContext_glRasterPos2dv(const SoGLContext * glue, const GLdouble * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glRasterPos2fv(const SoGLContext * glue, const GLfloat * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glRasterPos2i(const SoGLContext * glue, GLint x, GLint y)
{
  (void)glue;
  (void)x;
  (void)y;
}

void SoGLContext_glRasterPos2iv(const SoGLContext * glue, const GLint * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glRasterPos2s(const SoGLContext * glue, GLshort x, GLshort y)
{
  (void)glue;
  (void)x;
  (void)y;
}

void SoGLContext_glRasterPos2sv(const SoGLContext * glue, const GLshort * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glRasterPos3d(const SoGLContext * glue, GLdouble x, GLdouble y, GLdouble z)
{
  (void)glue;
  (void)x;
  (void)y;
  (void)z;
}

void SoGLContext_glRasterPos3dv(const SoGLContext * glue, const GLdouble * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glRasterPos3fv(const SoGLContext * glue, const GLfloat * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glRasterPos3i(const SoGLContext * glue, GLint x, GLint y, GLint z)
{
  (void)glue;
  (void)x;
  (void)y;
  (void)z;
}

void SoGLContext_glRasterPos3iv(const SoGLContext * glue, const GLint * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glRasterPos3s(const SoGLContext * glue, GLshort x, GLshort y, GLshort z)
{
  (void)glue;
  (void)x;
  (void)y;
  (void)z;
}

void SoGLContext_glRasterPos3sv(const SoGLContext * glue, const GLshort * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glRasterPos4d(const SoGLContext * glue, GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
  (void)glue;
  (void)x;
  (void)y;
  (void)z;
  (void)w;
}

void SoGLContext_glRasterPos4dv(const SoGLContext * glue, const GLdouble * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glRasterPos4f(const SoGLContext * glue, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
  (void)glue;
  (void)x;
  (void)y;
  (void)z;
  (void)w;
}

void SoGLContext_glRasterPos4fv(const SoGLContext * glue, const GLfloat * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glRasterPos4i(const SoGLContext * glue, GLint x, GLint y, GLint z, GLint w)
{
  (void)glue;
  (void)x;
  (void)y;
  (void)z;
  (void)w;
}

void SoGLContext_glRasterPos4iv(const SoGLContext * glue, const GLint * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glRasterPos4s(const SoGLContext * glue, GLshort x, GLshort y, GLshort z, GLshort w)
{
  (void)glue;
  (void)x;
  (void)y;
  (void)z;
  (void)w;
}

void SoGLContext_glRasterPos4sv(const SoGLContext * glue, const GLshort * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glReadBuffer(const SoGLContext * glue, GLenum mode)
{
  (void)glue;
  (void)mode;
}

void SoGLContext_glRectd(const SoGLContext * glue, GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2)
{
  (void)glue;
  (void)x1;
  (void)y1;
  (void)x2;
  (void)y2;
}

void SoGLContext_glRectdv(const SoGLContext * glue, const GLdouble * v1, const GLdouble * v2)
{
  (void)glue;
  (void)v1;
  (void)v2;
}

void SoGLContext_glRectf(const SoGLContext * glue, GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
  (void)glue;
  (void)x1;
  (void)y1;
  (void)x2;
  (void)y2;
}

void SoGLContext_glRectfv(const SoGLContext * glue, const GLfloat * v1, const GLfloat * v2)
{
  (void)glue;
  (void)v1;
  (void)v2;
}

void SoGLContext_glRecti(const SoGLContext * glue, GLint x1, GLint y1, GLint x2, GLint y2)
{
  (void)glue;
  (void)x1;
  (void)y1;
  (void)x2;
  (void)y2;
}

void SoGLContext_glRectiv(const SoGLContext * glue, const GLint * v1, const GLint * v2)
{
  (void)glue;
  (void)v1;
  (void)v2;
}

void SoGLContext_glRects(const SoGLContext * glue, GLshort x1, GLshort y1, GLshort x2, GLshort y2)
{
  (void)glue;
  (void)x1;
  (void)y1;
  (void)x2;
  (void)y2;
}

void SoGLContext_glRectsv(const SoGLContext * glue, const GLshort * v1, const GLshort * v2)
{
  (void)glue;
  (void)v1;
  (void)v2;
}

GLint SoGLContext_glRenderMode(const SoGLContext * glue, GLenum mode)
{
  (void)glue;
  (void)mode;
  return 0;
}

void SoGLContext_glResetHistogram(const SoGLContext * glue, GLenum target)
{
  (void)glue;
  (void)target;
}

void SoGLContext_glResetMinmax(const SoGLContext * glue, GLenum target)
{
  (void)glue;
  (void)target;
}

void SoGLContext_glRotated(const SoGLContext * glue, GLdouble angle, GLdouble x, GLdouble y, GLdouble z)
{
  (void)glue;
  (void)angle;
  (void)x;
  (void)y;
  (void)z;
}

void SoGLContext_glSampleCoverage(const SoGLContext * glue, GLclampf value, GLboolean invert)
{
  (void)glue;
  (void)value;
  (void)invert;
}

void SoGLContext_glScaled(const SoGLContext * glue, GLdouble x, GLdouble y, GLdouble z)
{
  (void)glue;
  (void)x;
  (void)y;
  (void)z;
}

void SoGLContext_glSelectBuffer(const SoGLContext * glue, GLsizei size, GLuint * buffer)
{
  (void)glue;
  (void)size;
  (void)buffer;
}

void SoGLContext_glSeparableFilter2D(const SoGLContext * glue, GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid * row, const GLvoid * column)
{
  (void)glue;
  (void)target;
  (void)internalformat;
  (void)width;
  (void)height;
  (void)format;
  (void)type;
  (void)row;
  (void)column;
}

void SoGLContext_glShadeModel(const SoGLContext * glue, GLenum mode)
{
  (void)glue;
  (void)mode;
}

void SoGLContext_glStencilMask(const SoGLContext * glue, GLuint mask)
{
  (void)glue;
  (void)mask;
}

void SoGLContext_glTexCoord1d(const SoGLContext * glue, GLdouble s)
{
  (void)glue;
  (void)s;
}

void SoGLContext_glTexCoord1dv(const SoGLContext * glue, const GLdouble * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glTexCoord1f(const SoGLContext * glue, GLfloat s)
{
  (void)glue;
  (void)s;
}

void SoGLContext_glTexCoord1fv(const SoGLContext * glue, const GLfloat * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glTexCoord1i(const SoGLContext * glue, GLint s)
{
  (void)glue;
  (void)s;
}

void SoGLContext_glTexCoord1iv(const SoGLContext * glue, const GLint * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glTexCoord1s(const SoGLContext * glue, GLshort s)
{
  (void)glue;
  (void)s;
}

void SoGLContext_glTexCoord1sv(const SoGLContext * glue, const GLshort * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glTexCoord2d(const SoGLContext * glue, GLdouble s, GLdouble t)
{
  (void)glue;
  (void)s;
  (void)t;
}

void SoGLContext_glTexCoord2dv(const SoGLContext * glue, const GLdouble * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glTexCoord2i(const SoGLContext * glue, GLint s, GLint t)
{
  (void)glue;
  (void)s;
  (void)t;
}

void SoGLContext_glTexCoord2iv(const SoGLContext * glue, const GLint * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glTexCoord2s(const SoGLContext * glue, GLshort s, GLshort t)
{
  (void)glue;
  (void)s;
  (void)t;
}

void SoGLContext_glTexCoord2sv(const SoGLContext * glue, const GLshort * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glTexCoord3d(const SoGLContext * glue, GLdouble s, GLdouble t, GLdouble r)
{
  (void)glue;
  (void)s;
  (void)t;
  (void)r;
}

void SoGLContext_glTexCoord3dv(const SoGLContext * glue, const GLdouble * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glTexCoord3i(const SoGLContext * glue, GLint s, GLint t, GLint r)
{
  (void)glue;
  (void)s;
  (void)t;
  (void)r;
}

void SoGLContext_glTexCoord3iv(const SoGLContext * glue, const GLint * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glTexCoord3s(const SoGLContext * glue, GLshort s, GLshort t, GLshort r)
{
  (void)glue;
  (void)s;
  (void)t;
  (void)r;
}

void SoGLContext_glTexCoord3sv(const SoGLContext * glue, const GLshort * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glTexCoord4d(const SoGLContext * glue, GLdouble s, GLdouble t, GLdouble r, GLdouble q)
{
  (void)glue;
  (void)s;
  (void)t;
  (void)r;
  (void)q;
}

void SoGLContext_glTexCoord4dv(const SoGLContext * glue, const GLdouble * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glTexCoord4f(const SoGLContext * glue, GLfloat s, GLfloat t, GLfloat r, GLfloat q)
{
  (void)glue;
  (void)s;
  (void)t;
  (void)r;
  (void)q;
}

void SoGLContext_glTexCoord4i(const SoGLContext * glue, GLint s, GLint t, GLint r, GLint q)
{
  (void)glue;
  (void)s;
  (void)t;
  (void)r;
  (void)q;
}

void SoGLContext_glTexCoord4iv(const SoGLContext * glue, const GLint * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glTexCoord4s(const SoGLContext * glue, GLshort s, GLshort t, GLshort r, GLshort q)
{
  (void)glue;
  (void)s;
  (void)t;
  (void)r;
  (void)q;
}

void SoGLContext_glTexCoord4sv(const SoGLContext * glue, const GLshort * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glTexEnviv(const SoGLContext * glue, GLenum target, GLenum pname, const GLint * params)
{
  (void)glue;
  (void)target;
  (void)pname;
  (void)params;
}

void SoGLContext_glTexGend(const SoGLContext * glue, GLenum coord, GLenum pname, GLdouble param)
{
  (void)glue;
  (void)coord;
  (void)pname;
  (void)param;
}

void SoGLContext_glTexGendv(const SoGLContext * glue, GLenum coord, GLenum pname, const GLdouble * params)
{
  (void)glue;
  (void)coord;
  (void)pname;
  (void)params;
}

void SoGLContext_glTexGeniv(const SoGLContext * glue, GLenum coord, GLenum pname, const GLint * params)
{
  (void)glue;
  (void)coord;
  (void)pname;
  (void)params;
}

void SoGLContext_glTexImage1D(const SoGLContext * glue, GLenum target, GLint level, GLint internalFormat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid * pixels)
{
  (void)glue;
  (void)target;
  (void)level;
  (void)internalFormat;
  (void)width;
  (void)border;
  (void)format;
  (void)type;
  (void)pixels;
}

void SoGLContext_glTexParameterfv(const SoGLContext * glue, GLenum target, GLenum pname, const GLfloat * params)
{
  (void)glue;
  (void)target;
  (void)pname;
  (void)params;
}

void SoGLContext_glTexParameteriv(const SoGLContext * glue, GLenum target, GLenum pname, const GLint * params)
{
  (void)glue;
  (void)target;
  (void)pname;
  (void)params;
}

void SoGLContext_glTexSubImage1D(const SoGLContext * glue, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid * pixels)
{
  (void)glue;
  (void)target;
  (void)level;
  (void)xoffset;
  (void)width;
  (void)format;
  (void)type;
  (void)pixels;
}

void SoGLContext_glTranslated(const SoGLContext * glue, GLdouble x, GLdouble y, GLdouble z)
{
  (void)glue;
  (void)x;
  (void)y;
  (void)z;
}

void SoGLContext_glVertex2d(const SoGLContext * glue, GLdouble x, GLdouble y)
{
  (void)glue;
  (void)x;
  (void)y;
}

void SoGLContext_glVertex2dv(const SoGLContext * glue, const GLdouble * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glVertex2fv(const SoGLContext * glue, const GLfloat * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glVertex2i(const SoGLContext * glue, GLint x, GLint y)
{
  (void)glue;
  (void)x;
  (void)y;
}

void SoGLContext_glVertex2iv(const SoGLContext * glue, const GLint * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glVertex2sv(const SoGLContext * glue, const GLshort * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glVertex3d(const SoGLContext * glue, GLdouble x, GLdouble y, GLdouble z)
{
  (void)glue;
  (void)x;
  (void)y;
  (void)z;
}

void SoGLContext_glVertex3dv(const SoGLContext * glue, const GLdouble * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glVertex3i(const SoGLContext * glue, GLint x, GLint y, GLint z)
{
  (void)glue;
  (void)x;
  (void)y;
  (void)z;
}

void SoGLContext_glVertex3iv(const SoGLContext * glue, const GLint * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glVertex3s(const SoGLContext * glue, GLshort x, GLshort y, GLshort z)
{
  (void)glue;
  (void)x;
  (void)y;
  (void)z;
}

void SoGLContext_glVertex3sv(const SoGLContext * glue, const GLshort * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glVertex4d(const SoGLContext * glue, GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
  (void)glue;
  (void)x;
  (void)y;
  (void)z;
  (void)w;
}

void SoGLContext_glVertex4dv(const SoGLContext * glue, const GLdouble * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glVertex4f(const SoGLContext * glue, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
  (void)glue;
  (void)x;
  (void)y;
  (void)z;
  (void)w;
}

void SoGLContext_glVertex4i(const SoGLContext * glue, GLint x, GLint y, GLint z, GLint w)
{
  (void)glue;
  (void)x;
  (void)y;
  (void)z;
  (void)w;
}

void SoGLContext_glVertex4iv(const SoGLContext * glue, const GLint * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_glVertex4s(const SoGLContext * glue, GLshort x, GLshort y, GLshort z, GLshort w)
{
  (void)glue;
  (void)x;
  (void)y;
  (void)z;
  (void)w;
}

void SoGLContext_glVertex4sv(const SoGLContext * glue, const GLshort * v)
{
  (void)glue;
  (void)v;
}

void SoGLContext_destruct(uint32_t contextid)
{
  (void)contextid;
}

const SoGLContext * SoGLContext_instance_from_context_ptr(void * ptr)
{
  (void)ptr;
  return nullptr;
}

int SoGLContext_debug(void)
{
  return 0;
}

int SoGLContext_extension_available(const char * extensions, const char * ext)
{
  (void)extensions;
  (void)ext;
  return 0;
}

int SoGLContext_stencil_bits_hack(void)
{
  return 0;
}

SbBool SoGLContext_has_arb_shader_objects(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

SbBool SoGLContext_is_texture_size_legal(const SoGLContext * glw,
                                         int xsize, int ysize, int zsize,
                                         GLenum internalformat,
                                         GLenum format,
                                         GLenum type,
                                         SbBool mipmap)
{
  (void)glw;
  (void)xsize;
  (void)ysize;
  (void)zsize;
  (void)internalformat;
  (void)format;
  (void)type;
  (void)mipmap;
  return 0 /* FALSE */;
}

GLint SoGLContext_get_internal_texture_format(const SoGLContext * glw,
                                              int numcomponents,
                                              SbBool compress)
{
  (void)glw;
  (void)numcomponents;
  (void)compress;
  return 0;
}

GLenum SoGLContext_get_texture_format(const SoGLContext * glw, int numcomponents)
{
  (void)glw;
  (void)numcomponents;
  return 0 /* GL_NO_ERROR */;
}

SbBool SoGLContext_vbo_in_displaylist_supported(const SoGLContext * glw)
{
  (void)glw;
  return 0 /* FALSE */;
}

SbBool SoGLContext_non_power_of_two_textures(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

SbBool SoGLContext_has_generate_mipmap(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

void SoGLContext_add_instance_created_callback(SoGLContext_instance_created_cb * cb,
                                               void * closure)
{
  (void)cb;
  (void)closure;
}

SbBool SoGLContext_has_nv_register_combiners(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

void SoGLContext_glCombinerParameterfvNV(const SoGLContext * glue, GLenum pname, const GLfloat * params)
{
  (void)glue;
  (void)pname;
  (void)params;
}

void SoGLContext_glCombinerParameterivNV(const SoGLContext * glue, GLenum pname, const GLint * params)
{
  (void)glue;
  (void)pname;
  (void)params;
}

void SoGLContext_glCombinerParameterfNV(const SoGLContext * glue, GLenum pname, GLfloat param)
{
  (void)glue;
  (void)pname;
  (void)param;
}

void SoGLContext_glCombinerParameteriNV(const SoGLContext * glue, GLenum pname, GLint param)
{
  (void)glue;
  (void)pname;
  (void)param;
}

void SoGLContext_glCombinerInputNV(const SoGLContext * glue, GLenum stage, GLenum portion, GLenum variable, GLenum input, GLenum mapping, GLenum componentUsage)
{
  (void)glue;
  (void)stage;
  (void)portion;
  (void)variable;
  (void)input;
  (void)mapping;
  (void)componentUsage;
}

void SoGLContext_glCombinerOutputNV(const SoGLContext * glue, GLenum stage, GLenum portion, GLenum abOutput, GLenum cdOutput, GLenum sumOutput, GLenum scale, GLenum bias, GLboolean abDotProduct, GLboolean cdDotProduct, GLboolean muxSum)
{
  (void)glue;
  (void)stage;
  (void)portion;
  (void)abOutput;
  (void)cdOutput;
  (void)sumOutput;
  (void)scale;
  (void)bias;
  (void)abDotProduct;
  (void)cdDotProduct;
  (void)muxSum;
}

void SoGLContext_glFinalCombinerInputNV(const SoGLContext * glue, GLenum variable, GLenum input, GLenum mapping, GLenum componentUsage)
{
  (void)glue;
  (void)variable;
  (void)input;
  (void)mapping;
  (void)componentUsage;
}

void SoGLContext_glGetCombinerInputParameterfvNV(const SoGLContext * glue, GLenum stage, GLenum portion, GLenum variable, GLenum pname, GLfloat * params)
{
  (void)glue;
  (void)stage;
  (void)portion;
  (void)variable;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetCombinerInputParameterivNV(const SoGLContext * glue, GLenum stage, GLenum portion, GLenum variable, GLenum pname, GLint * params)
{
  (void)glue;
  (void)stage;
  (void)portion;
  (void)variable;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetCombinerOutputParameterfvNV(const SoGLContext * glue, GLenum stage, GLenum portion, GLenum pname, GLfloat * params)
{
  (void)glue;
  (void)stage;
  (void)portion;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetCombinerOutputParameterivNV(const SoGLContext * glue, GLenum stage, GLenum portion, GLenum pname, GLint * params)
{
  (void)glue;
  (void)stage;
  (void)portion;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetFinalCombinerInputParameterfvNV(const SoGLContext * glue, GLenum variable, GLenum pname, GLfloat * params)
{
  (void)glue;
  (void)variable;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetFinalCombinerInputParameterivNV(const SoGLContext * glue, GLenum variable, GLenum pname, GLint * params)
{
  (void)glue;
  (void)variable;
  (void)pname;
  (void)params;
}

SbBool SoGLContext_has_nv_texture_rectangle(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

SbBool SoGLContext_has_ext_texture_rectangle(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

SbBool SoGLContext_has_nv_texture_shader(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

SbBool SoGLContext_has_arb_shadow(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

SbBool SoGLContext_has_arb_depth_texture(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

SbBool SoGLContext_has_texture_env_combine(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

uint32_t SoGLContext_get_contextid(const SoGLContext * glue)
{
  (void)glue;
  return 0;
}

const SoGLContext * SoGLContext_instance(int contextid)
{
  (void)contextid;
  return nullptr;
}

void SoGLContext_glversion(const SoGLContext * glue,
                         unsigned int * major,
                         unsigned int * minor,
                         unsigned int * release)
{
  (void)glue;
  (void)major;
  (void)minor;
  (void)release;
}

SbBool SoGLContext_glversion_matches_at_least(const SoGLContext * glue,
                                            unsigned int major,
                                            unsigned int minor,
                                            unsigned int release)
{
  (void)glue;
  (void)major;
  (void)minor;
  (void)release;
  return 0 /* FALSE */;
}

SbBool SoGLContext_glxversion_matches_at_least(const SoGLContext * glue,
                                             int major,
                                             int minor)
{
  (void)glue;
  (void)major;
  (void)minor;
  return 0 /* FALSE */;
}

SbBool SoGLContext_glext_supported(const SoGLContext * glue, const char * extname)
{
  (void)glue;
  (void)extname;
  return 0 /* FALSE */;
}

void * SoGLContext_getprocaddress(const SoGLContext * glue, const char * symname)
{
  (void)glue;
  (void)symname;
  return nullptr;
}

SbBool SoGLContext_isdirect(const SoGLContext * w)
{
  (void)w;
  return 0 /* FALSE */;
}

SbBool SoGLContext_has_polygon_offset(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

void SoGLContext_glPolygonOffsetEnable(const SoGLContext * glue,
                                     SbBool enable, int m)
{
  (void)glue;
  (void)enable;
  (void)m;
}

SbBool SoGLContext_has_texture_objects(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

SbBool SoGLContext_has_3d_textures(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

SbBool SoGLContext_has_multitexture(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

SbBool SoGLContext_has_texsubimage(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

SbBool SoGLContext_has_2d_proxy_textures(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

SbBool SoGLContext_has_texture_edge_clamp(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

SbBool SoGLContext_has_color_tables(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

SbBool SoGLContext_has_color_subtables(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

SbBool SoGLContext_has_paletted_textures(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

SbBool SoGLContext_has_blendequation(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

SbBool SoGLContext_has_blendfuncseparate(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

void SoGLContext_glBlendFuncSeparate(const SoGLContext * glue, 
                                   GLenum srgb, GLenum drgb,
                                   GLenum salpha, GLenum dalpha)
{
  (void)glue;
  (void)srgb;
  (void)drgb;
  (void)salpha;
  (void)dalpha;
}

SbBool SoGLContext_has_vertex_array(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

int SoGLContext_max_texture_units(const SoGLContext * glue)
{
  (void)glue;
  return 0;
}

SbBool SoGLContext_has_multidraw_vertex_arrays(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

void SoGLContext_glMultiDrawArrays(const SoGLContext * glue, GLenum mode, const GLint * first, 
                                 const GLsizei * count, GLsizei primcount)
{
  (void)glue;
  (void)mode;
  (void)first;
  (void)count;
  (void)primcount;
}

void SoGLContext_glMultiDrawElements(const SoGLContext * glue, GLenum mode, const GLsizei * count, 
                                   GLenum type, const GLvoid ** indices, GLsizei primcount)
{
  (void)glue;
  (void)mode;
  (void)count;
  (void)type;
  (void)indices;
  (void)primcount;
}

SbBool SoGLContext_has_nv_vertex_array_range(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

void SoGLContext_glFlushVertexArrayRangeNV(const SoGLContext * glue)
{
  (void)glue;
}

void SoGLContext_glVertexArrayRangeNV(const SoGLContext * glue, GLsizei size, const GLvoid * pointer)
{
  (void)glue;
  (void)size;
  (void)pointer;
}

void * SoGLContext_glAllocateMemoryNV(const SoGLContext * glue,
                                    GLsizei size, GLfloat readfreq,
                                    GLfloat writefreq, GLfloat priority)
{
  (void)glue;
  (void)size;
  (void)readfreq;
  (void)writefreq;
  (void)priority;
  return nullptr;
}

void SoGLContext_glFreeMemoryNV(const SoGLContext * glue, GLvoid * buffer)
{
  (void)glue;
  (void)buffer;
}

SbBool SoGLContext_has_vertex_buffer_object(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

void SoGLContext_glBindBuffer(const SoGLContext * glue, GLenum target, GLuint buffer)
{
  (void)glue;
  (void)target;
  (void)buffer;
}

void SoGLContext_glDeleteBuffers(const SoGLContext * glue, GLsizei n, const GLuint *buffers)
{
  (void)glue;
  (void)n;
  (void)buffers;
}

void SoGLContext_glGenBuffers(const SoGLContext * glue, GLsizei n, GLuint *buffers)
{
  (void)glue;
  (void)n;
  (void)buffers;
}

GLboolean SoGLContext_glIsBuffer(const SoGLContext * glue, GLuint buffer)
{
  (void)glue;
  (void)buffer;
  return 0 /* FALSE */;
}

void SoGLContext_glBufferData(const SoGLContext * glue,
                            GLenum target, 
                            intptr_t size, /* 64 bit on 64 bit systems */ 
                            const GLvoid *data, 
                            GLenum usage)
{
  (void)glue;
  (void)target;
  (void)size;
  (void)data;
  (void)usage;
}

void SoGLContext_glBufferSubData(const SoGLContext * glue,
                               GLenum target, 
                               intptr_t offset, /* 64 bit */ 
                               intptr_t size, /* 64 bit */ 
                               const GLvoid * data)
{
  (void)glue;
  (void)target;
  (void)offset;
  (void)size;
  (void)data;
}

void SoGLContext_glGetBufferSubData(const SoGLContext * glue,
                                  GLenum target, 
                                  intptr_t offset, /* 64 bit */ 
                                  intptr_t size, /* 64 bit */ 
                                  GLvoid *data)
{
  (void)glue;
  (void)target;
  (void)offset;
  (void)size;
  (void)data;
}

GLboolean SoGLContext_glUnmapBuffer(const SoGLContext * glue,
                                  GLenum target)
{
  (void)glue;
  (void)target;
  return 0 /* FALSE */;
}

void SoGLContext_glGetBufferParameteriv(const SoGLContext * glue,
                                      GLenum target, 
                                      GLenum pname, 
                                      GLint * params)
{
  (void)glue;
  (void)target;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetBufferPointerv(const SoGLContext * glue,
                                   GLenum target, 
                                   GLenum pname, 
                                   GLvoid ** params)
{
  (void)glue;
  (void)target;
  (void)pname;
  (void)params;
}

SbBool SoGLContext_has_arb_fragment_program(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

void SoGLContext_glProgramString(const SoGLContext * glue, GLenum target, GLenum format, 
                               GLsizei len, const GLvoid *string)
{
  (void)glue;
  (void)target;
  (void)format;
  (void)len;
  (void)string;
}

void SoGLContext_glBindProgram(const SoGLContext * glue, GLenum target, 
                             GLuint program)
{
  (void)glue;
  (void)target;
  (void)program;
}

void SoGLContext_glDeletePrograms(const SoGLContext * glue, GLsizei n, 
                                const GLuint *programs)
{
  (void)glue;
  (void)n;
  (void)programs;
}

void SoGLContext_glGenPrograms(const SoGLContext * glue, GLsizei n, GLuint *programs)
{
  (void)glue;
  (void)n;
  (void)programs;
}

void SoGLContext_glProgramEnvParameter4d(const SoGLContext * glue, GLenum target,
                                       GLuint index, GLdouble x, GLdouble y, 
                                       GLdouble z, GLdouble w)
{
  (void)glue;
  (void)target;
  (void)index;
  (void)x;
  (void)y;
  (void)z;
  (void)w;
}

void SoGLContext_glProgramEnvParameter4dv(const SoGLContext * glue, GLenum target,
                                        GLuint index, const GLdouble *params)
{
  (void)glue;
  (void)target;
  (void)index;
  (void)params;
}

void SoGLContext_glProgramEnvParameter4f(const SoGLContext * glue, GLenum target, 
                                       GLuint index, GLfloat x, 
                                       GLfloat y, GLfloat z, 
                                       GLfloat w)
{
  (void)glue;
  (void)target;
  (void)index;
  (void)x;
  (void)y;
  (void)z;
  (void)w;
}

void SoGLContext_glProgramEnvParameter4fv(const SoGLContext * glue, GLenum target, 
                                        GLuint index, const GLfloat *params)
{
  (void)glue;
  (void)target;
  (void)index;
  (void)params;
}

void SoGLContext_glProgramLocalParameter4d(const SoGLContext * glue, GLenum target, 
                                         GLuint index, GLdouble x, 
                                         GLdouble y, GLdouble z, 
                                         GLdouble w)
{
  (void)glue;
  (void)target;
  (void)index;
  (void)x;
  (void)y;
  (void)z;
  (void)w;
}

void SoGLContext_glProgramLocalParameter4dv(const SoGLContext * glue, GLenum target, 
                                          GLuint index, const GLdouble *params)
{
  (void)glue;
  (void)target;
  (void)index;
  (void)params;
}

void SoGLContext_glProgramLocalParameter4f(const SoGLContext * glue, GLenum target, 
                                         GLuint index, GLfloat x, GLfloat y, 
                                         GLfloat z, GLfloat w)
{
  (void)glue;
  (void)target;
  (void)index;
  (void)x;
  (void)y;
  (void)z;
  (void)w;
}

void SoGLContext_glProgramLocalParameter4fv(const SoGLContext * glue, GLenum target, 
                                          GLuint index, const GLfloat *params)
{
  (void)glue;
  (void)target;
  (void)index;
  (void)params;
}

void SoGLContext_glGetProgramEnvParameterdv(const SoGLContext * glue, GLenum target, 
                                          GLuint index, GLdouble *params)
{
  (void)glue;
  (void)target;
  (void)index;
  (void)params;
}

void SoGLContext_glGetProgramEnvParameterfv(const SoGLContext * glue, GLenum target, 
                                          GLuint index, GLfloat *params)
{
  (void)glue;
  (void)target;
  (void)index;
  (void)params;
}

void SoGLContext_glGetProgramLocalParameterdv(const SoGLContext * glue, GLenum target, 
                                            GLuint index, GLdouble *params)
{
  (void)glue;
  (void)target;
  (void)index;
  (void)params;
}

void SoGLContext_glGetProgramLocalParameterfv(const SoGLContext * glue, GLenum target, 
                                            GLuint index, GLfloat *params)
{
  (void)glue;
  (void)target;
  (void)index;
  (void)params;
}

void SoGLContext_glGetProgramiv(const SoGLContext * glue, GLenum target, 
                              GLenum pname, GLint *params)
{
  (void)glue;
  (void)target;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetProgramString(const SoGLContext * glue, GLenum target, 
                                  GLenum pname, GLvoid *string)
{
  (void)glue;
  (void)target;
  (void)pname;
  (void)string;
}

SbBool SoGLContext_glIsProgram(const SoGLContext * glue, GLuint program)
{
  (void)glue;
  (void)program;
  return 0 /* FALSE */;
}

SbBool SoGLContext_has_arb_vertex_program(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

void SoGLContext_glVertexAttrib1s(const SoGLContext * glue, GLuint index, GLshort x)
{
  (void)glue;
  (void)index;
  (void)x;
}

void SoGLContext_glVertexAttrib1f(const SoGLContext * glue, GLuint index, GLfloat x)
{
  (void)glue;
  (void)index;
  (void)x;
}

void SoGLContext_glVertexAttrib1d(const SoGLContext * glue, GLuint index, GLdouble x)
{
  (void)glue;
  (void)index;
  (void)x;
}

void SoGLContext_glVertexAttrib2s(const SoGLContext * glue, GLuint index, GLshort x, GLshort y)
{
  (void)glue;
  (void)index;
  (void)x;
  (void)y;
}

void SoGLContext_glVertexAttrib2f(const SoGLContext * glue, GLuint index, GLfloat x, GLfloat y)
{
  (void)glue;
  (void)index;
  (void)x;
  (void)y;
}

void SoGLContext_glVertexAttrib2d(const SoGLContext * glue, GLuint index, GLdouble x, GLdouble y)
{
  (void)glue;
  (void)index;
  (void)x;
  (void)y;
}

void SoGLContext_glVertexAttrib3s(const SoGLContext * glue, GLuint index, 
                                GLshort x, GLshort y, GLshort z)
{
  (void)glue;
  (void)index;
  (void)x;
  (void)y;
  (void)z;
}

void SoGLContext_glVertexAttrib3f(const SoGLContext * glue, GLuint index, 
                                GLfloat x, GLfloat y, GLfloat z)
{
  (void)glue;
  (void)index;
  (void)x;
  (void)y;
  (void)z;
}

void SoGLContext_glVertexAttrib3d(const SoGLContext * glue, GLuint index, 
                                GLdouble x, GLdouble y, GLdouble z)
{
  (void)glue;
  (void)index;
  (void)x;
  (void)y;
  (void)z;
}

void SoGLContext_glVertexAttrib4s(const SoGLContext * glue, GLuint index, GLshort x, 
                                GLshort y, GLshort z, GLshort w)
{
  (void)glue;
  (void)index;
  (void)x;
  (void)y;
  (void)z;
  (void)w;
}

void SoGLContext_glVertexAttrib4f(const SoGLContext * glue, GLuint index, GLfloat x, 
                                GLfloat y, GLfloat z, GLfloat w)
{
  (void)glue;
  (void)index;
  (void)x;
  (void)y;
  (void)z;
  (void)w;
}

void SoGLContext_glVertexAttrib4d(const SoGLContext * glue, GLuint index, GLdouble x, 
                                GLdouble y, GLdouble z, GLdouble w)
{
  (void)glue;
  (void)index;
  (void)x;
  (void)y;
  (void)z;
  (void)w;
}

void SoGLContext_glVertexAttrib4Nub(const SoGLContext * glue, GLuint index, GLubyte x, 
                                  GLubyte y, GLubyte z, GLubyte w)
{
  (void)glue;
  (void)index;
  (void)x;
  (void)y;
  (void)z;
  (void)w;
}

void SoGLContext_glVertexAttrib1sv(const SoGLContext * glue, GLuint index, const GLshort *v)
{
  (void)glue;
  (void)index;
  (void)v;
}

void SoGLContext_glVertexAttrib1fv(const SoGLContext * glue, GLuint index, const GLfloat *v)
{
  (void)glue;
  (void)index;
  (void)v;
}

void SoGLContext_glVertexAttrib1dv(const SoGLContext * glue, GLuint index, const GLdouble *v)
{
  (void)glue;
  (void)index;
  (void)v;
}

void SoGLContext_glVertexAttrib2sv(const SoGLContext * glue, GLuint index, const GLshort *v)
{
  (void)glue;
  (void)index;
  (void)v;
}

void SoGLContext_glVertexAttrib2fv(const SoGLContext * glue, GLuint index, const GLfloat *v)
{
  (void)glue;
  (void)index;
  (void)v;
}

void SoGLContext_glVertexAttrib2dv(const SoGLContext * glue, GLuint index, const GLdouble *v)
{
  (void)glue;
  (void)index;
  (void)v;
}

void SoGLContext_glVertexAttrib3sv(const SoGLContext * glue, GLuint index, const GLshort *v)
{
  (void)glue;
  (void)index;
  (void)v;
}

void SoGLContext_glVertexAttrib3fv(const SoGLContext * glue, GLuint index, const GLfloat *v)
{
  (void)glue;
  (void)index;
  (void)v;
}

void SoGLContext_glVertexAttrib3dv(const SoGLContext * glue, GLuint index, const GLdouble *v)
{
  (void)glue;
  (void)index;
  (void)v;
}

void SoGLContext_glVertexAttrib4bv(const SoGLContext * glue, GLuint index, const GLbyte *v)
{
  (void)glue;
  (void)index;
  (void)v;
}

void SoGLContext_glVertexAttrib4sv(const SoGLContext * glue, GLuint index, const GLshort *v)
{
  (void)glue;
  (void)index;
  (void)v;
}

void SoGLContext_glVertexAttrib4iv(const SoGLContext * glue, GLuint index, const GLint *v)
{
  (void)glue;
  (void)index;
  (void)v;
}

void SoGLContext_glVertexAttrib4ubv(const SoGLContext * glue, GLuint index, const GLubyte *v)
{
  (void)glue;
  (void)index;
  (void)v;
}

void SoGLContext_glVertexAttrib4usv(const SoGLContext * glue, GLuint index, const GLushort *v)
{
  (void)glue;
  (void)index;
  (void)v;
}

void SoGLContext_glVertexAttrib4uiv(const SoGLContext * glue, GLuint index, const GLuint *v)
{
  (void)glue;
  (void)index;
  (void)v;
}

void SoGLContext_glVertexAttrib4fv(const SoGLContext * glue, GLuint index, const GLfloat *v)
{
  (void)glue;
  (void)index;
  (void)v;
}

void SoGLContext_glVertexAttrib4dv(const SoGLContext * glue, GLuint index, const GLdouble *v)
{
  (void)glue;
  (void)index;
  (void)v;
}

void SoGLContext_glVertexAttrib4Nbv(const SoGLContext * glue, GLuint index, const GLbyte *v)
{
  (void)glue;
  (void)index;
  (void)v;
}

void SoGLContext_glVertexAttrib4Nsv(const SoGLContext * glue, GLuint index, const GLshort *v)
{
  (void)glue;
  (void)index;
  (void)v;
}

void SoGLContext_glVertexAttrib4Niv(const SoGLContext * glue, GLuint index, const GLint *v)
{
  (void)glue;
  (void)index;
  (void)v;
}

void SoGLContext_glVertexAttrib4Nubv(const SoGLContext * glue, GLuint index, const GLubyte *v)
{
  (void)glue;
  (void)index;
  (void)v;
}

void SoGLContext_glVertexAttrib4Nusv(const SoGLContext * glue, GLuint index, const GLushort *v)
{
  (void)glue;
  (void)index;
  (void)v;
}

void SoGLContext_glVertexAttrib4Nuiv(const SoGLContext * glue, GLuint index, const GLuint *v)
{
  (void)glue;
  (void)index;
  (void)v;
}

void SoGLContext_glVertexAttribPointer(const SoGLContext * glue, GLuint index, GLint size, 
                                     GLenum type, GLboolean normalized, GLsizei stride, 
                                     const GLvoid *pointer)
{
  (void)glue;
  (void)index;
  (void)size;
  (void)type;
  (void)normalized;
  (void)stride;
  (void)pointer;
}

void SoGLContext_glEnableVertexAttribArray(const SoGLContext * glue, GLuint index)
{
  (void)glue;
  (void)index;
}

void SoGLContext_glDisableVertexAttribArray(const SoGLContext * glue, GLuint index)
{
  (void)glue;
  (void)index;
}

void SoGLContext_glGetVertexAttribdv(const SoGLContext * glue, GLuint index, GLenum pname, 
                                   GLdouble *params)
{
  (void)glue;
  (void)index;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetVertexAttribfv(const SoGLContext * glue, GLuint index, GLenum pname, 
                                   GLfloat *params)
{
  (void)glue;
  (void)index;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetVertexAttribiv(const SoGLContext * glue, GLuint index, GLenum pname, 
                                   GLint *params)
{
  (void)glue;
  (void)index;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetVertexAttribPointerv(const SoGLContext * glue, GLuint index, GLenum pname, 
                                         GLvoid **pointer)
{
  (void)glue;
  (void)index;
  (void)pname;
  (void)pointer;
}

SbBool SoGLContext_has_arb_vertex_shader(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

SbBool SoGLContext_has_occlusion_query(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

void SoGLContext_glGenQueries(const SoGLContext * glue, 
                            GLsizei n, GLuint * ids)
{
  (void)glue;
  (void)n;
  (void)ids;
}

void SoGLContext_glDeleteQueries(const SoGLContext * glue, 
                               GLsizei n, const GLuint *ids)
{
  (void)glue;
  (void)n;
  (void)ids;
}

GLboolean SoGLContext_glIsQuery(const SoGLContext * glue, 
                            GLuint id)
{
  (void)glue;
  (void)id;
  return 0 /* FALSE */;
}

void SoGLContext_glBeginQuery(const SoGLContext * glue, 
                            GLenum target, GLuint id)
{
  (void)glue;
  (void)target;
  (void)id;
}

void SoGLContext_glEndQuery(const SoGLContext * glue, 
                          GLenum target)
{
  (void)glue;
  (void)target;
}

void SoGLContext_glGetQueryiv(const SoGLContext * glue, 
                            GLenum target, GLenum pname, 
                            GLint * params)
{
  (void)glue;
  (void)target;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetQueryObjectiv(const SoGLContext * glue, 
                                  GLuint id, GLenum pname, 
                                  GLint * params)
{
  (void)glue;
  (void)id;
  (void)pname;
  (void)params;
}

void SoGLContext_glGetQueryObjectuiv(const SoGLContext * glue, 
                                   GLuint id, GLenum pname, 
                                   GLuint * params)
{
  (void)glue;
  (void)id;
  (void)pname;
  (void)params;
}

void SoGLContext_glIsRenderbuffer(const SoGLContext * glue, GLuint renderbuffer)
{
  (void)glue;
  (void)renderbuffer;
}

void SoGLContext_glBindRenderbuffer(const SoGLContext * glue, GLenum target, GLuint renderbuffer)
{
  (void)glue;
  (void)target;
  (void)renderbuffer;
}

void SoGLContext_glDeleteRenderbuffers(const SoGLContext * glue, GLsizei n, const GLuint *renderbuffers)
{
  (void)glue;
  (void)n;
  (void)renderbuffers;
}

void SoGLContext_glGenRenderbuffers(const SoGLContext * glue, GLsizei n, GLuint *renderbuffers)
{
  (void)glue;
  (void)n;
  (void)renderbuffers;
}

void SoGLContext_glRenderbufferStorage(const SoGLContext * glue, GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
  (void)glue;
  (void)target;
  (void)internalformat;
  (void)width;
  (void)height;
}

void SoGLContext_glGetRenderbufferParameteriv(const SoGLContext * glue, GLenum target, GLenum pname, GLint *params)
{
  (void)glue;
  (void)target;
  (void)pname;
  (void)params;
}

GLboolean SoGLContext_glIsFramebuffer(const SoGLContext * glue, GLuint framebuffer)
{
  (void)glue;
  (void)framebuffer;
  return 0 /* FALSE */;
}

void SoGLContext_glBindFramebuffer(const SoGLContext * glue, GLenum target, GLuint framebuffer)
{
  (void)glue;
  (void)target;
  (void)framebuffer;
}

void SoGLContext_glDeleteFramebuffers(const SoGLContext * glue, GLsizei n, const GLuint *framebuffers)
{
  (void)glue;
  (void)n;
  (void)framebuffers;
}

void SoGLContext_glGenFramebuffers(const SoGLContext * glue, GLsizei n, GLuint *framebuffers)
{
  (void)glue;
  (void)n;
  (void)framebuffers;
}

GLenum SoGLContext_glCheckFramebufferStatus(const SoGLContext * glue, GLenum target)
{
  (void)glue;
  (void)target;
  return 0 /* GL_NO_ERROR */;
}

void SoGLContext_glFramebufferTexture1D(const SoGLContext * glue, GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
  (void)glue;
  (void)target;
  (void)attachment;
  (void)textarget;
  (void)texture;
  (void)level;
}

void SoGLContext_glFramebufferTexture2D(const SoGLContext * glue, GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
  (void)glue;
  (void)target;
  (void)attachment;
  (void)textarget;
  (void)texture;
  (void)level;
}

void SoGLContext_glFramebufferTexture3D(const SoGLContext * glue, GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset)
{
  (void)glue;
  (void)target;
  (void)attachment;
  (void)textarget;
  (void)texture;
  (void)level;
  (void)zoffset;
}

void SoGLContext_glFramebufferRenderbuffer(const SoGLContext * glue, GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
  (void)glue;
  (void)target;
  (void)attachment;
  (void)renderbuffertarget;
  (void)renderbuffer;
}

void SoGLContext_glGetFramebufferAttachmentParameteriv(const SoGLContext * glue, GLenum target, GLenum attachment, GLenum pname, GLint *params)
{
  (void)glue;
  (void)target;
  (void)attachment;
  (void)pname;
  (void)params;
}

void SoGLContext_glGenerateMipmap(const SoGLContext * glue, GLenum target)
{
  (void)glue;
  (void)target;
}

SbBool SoGLContext_has_framebuffer_objects(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

SbBool SoGLContext_can_do_bumpmapping(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

SbBool SoGLContext_can_do_sortedlayersblend(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

SbBool SoGLContext_can_do_anisotropic_filtering(const SoGLContext * glue)
{
  (void)glue;
  return 0 /* FALSE */;
}

int SoGLContext_get_max_lights(const SoGLContext * glue)
{
  (void)glue;
  return 0;
}

void * SoGLContext_glXGetCurrentDisplay(const SoGLContext * w)
{
  (void)w;
  return nullptr;
}

void SoGLContext_context_max_dimensions(void * mgr, unsigned int * width, unsigned int * height)
{
  (void)mgr;
  (void)width;
  (void)height;
}

void SoGLContext_context_bind_pbuffer(void * ctx)
{
  (void)ctx;
}

void SoGLContext_context_release_pbuffer(void * ctx)
{
  (void)ctx;
}

SbBool SoGLContext_context_pbuffer_is_bound(void * ctx)
{
  (void)ctx;
  return 0 /* FALSE */;
}

SbBool SoGLContext_context_can_render_to_texture(void * ctx)
{
  (void)ctx;
  return 0 /* FALSE */;
}

const void * SoGLContext_win32_HDC(void * ctx)
{
  (void)ctx;
  return nullptr;
}

void SoGLContext_win32_updateHDCBitmap(void * ctx)
{
  (void)ctx;
}


/* -----------------------------------------------------------------------
 * Additional functions from glp.h and dlp.h not covered by the generated
 * stubs above (different naming prefixes: coingl_, cc_glue_, coin_gl_).
 * --------------------------------------------------------------------- */

/* coin_gl_current_context: return NULL – no GL context in no-GL builds */
void * coin_gl_current_context(void)
{
  return nullptr;
}

/* coin_gl_getstring_ptr: return NULL – no GL function lookup */
void * coin_gl_getstring_ptr(void)
{
  return nullptr;
}

/* SoGLContext_get_max_anisotropy: return 1.0 (no anisotropic filtering) */
float SoGLContext_get_max_anisotropy(const SoGLContext * glue)
{
  (void)glue;
  return 1.0f;
}

/* Texture compression capability checks: return FALSE */
SbBool cc_glue_has_texture_compression(const SoGLContext * glue)
{ (void)glue; return 0; }
SbBool cc_glue_has_texture_compression_2d(const SoGLContext * glue)
{ (void)glue; return 0; }
SbBool cc_glue_has_texture_compression_3d(const SoGLContext * glue)
{ (void)glue; return 0; }

/* OSMesa context registration: no-ops in no-GL builds */
void coingl_register_osmesa_context(int contextid) { (void)contextid; }
void coingl_unregister_osmesa_context(int contextid) { (void)contextid; }

/* Per-context-ID manager registry: no-ops in no-GL builds */
void coingl_register_context_manager(int contextid, void * mgr)
{ (void)contextid; (void)mgr; }
void coingl_unregister_context_manager(int contextid) { (void)contextid; }

#endif /* OBOL_NO_OPENGL */
