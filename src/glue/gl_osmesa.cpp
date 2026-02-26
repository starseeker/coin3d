/*
 * gl_osmesa.cpp  --  OSMesa backend variant of the Obol GL glue layer.
 *
 * Architecture
 * ------------
 * This file is part of the COIN3D_BUILD_DUAL_GL support, which allows both
 * the system-OpenGL and the OSMesa GL backends to be compiled into a single
 * shared library so that applications can switch between them at runtime.
 *
 * The approach mirrors BRL-CAD's technique for embedding a private zlib
 * alongside a system zlib:
 *
 *   1. gl.cpp is the canonical implementation of the GL glue layer.
 *      When compiled normally it produces SoGLContext_instance(),
 *      SoGLContext_glGenTextures(), etc., all calling system GL symbols
 *      (glGetString, glGenTextures, …).
 *
 *   2. This file (gl_osmesa.cpp) recompiles the same source with:
 *        a. COIN_GLHEADERS_OSMESA_OVERRIDE  – makes gl-headers.h pull in
 *           OSMesa headers (<OSMesa/osmesa.h>, <OSMesa/gl.h>) instead of
 *           the system <GL/gl.h>.
 *        b. USE_MGL_NAMESPACE              – activates <OSMesa/gl_mangle.h>
 *           so every gl* call becomes mgl* (the OSMesa namespace).
 *        c. SOGL_PREFIX_SET / SOGL_PREFIX_STR=osmesa_  – every SoGLContext_*
 *           function defined in gl.cpp gets the osmesa_ prefix, e.g.
 *           osmesa_SoGLContext_instance(), osmesa_SoGLContext_glGenTextures().
 *
 * The result: the object file from this TU exports osmesa_SoGLContext_*
 * symbols that call mgl* (OSMesa) functions, while the object file from
 * gl.cpp exports the plain SoGLContext_* symbols that call system GL.
 * Both can coexist in the same .so without linker symbol collisions.
 *
 * Dispatch
 * --------
 * A thin dispatch layer in gl.cpp (compiled with COIN3D_BUILD_DUAL_GL
 * defined) keeps the stable SoGLContext_* API working at runtime: it
 * checks a per-context backend flag set at context-creation time and
 * forwards to either osmesa_SoGLContext_* or the system-GL implementation.
 *
 * OSMesa include path
 * -------------------
 * The OSMesa headers live in external/osmesa/include/OSMesa/ (submodule).
 * src/glue/CMakeLists.txt adds that path to the include directories for
 * this translation unit when COIN3D_BUILD_DUAL_GL is ON.
 */

/* -----------------------------------------------------------------------
 * Step 1 – override the GL header selector so that Inventor/system/gl.h
 *           (and therefore every source that pulls it in via glp.h) uses
 *           OSMesa headers rather than the system <GL/gl.h>.
 *           This define is tested in include/Inventor/system/gl-headers.h.
 * --------------------------------------------------------------------- */
#define COIN_GLHEADERS_OSMESA_OVERRIDE 1

/* -----------------------------------------------------------------------
 * Step 2 – enable MGL name mangling: after <OSMesa/gl_mangle.h> is
 *           processed, every gl* call in the included source becomes mgl*.
 *           This define is tested inside <OSMesa/gl.h> which includes
 *           <OSMesa/gl_mangle.h> when USE_MGL_NAMESPACE is set.
 * --------------------------------------------------------------------- */
#define USE_MGL_NAMESPACE 1

/* -----------------------------------------------------------------------
 * Step 3 – activate the SOGL function-name prefix so that every
 *           SoGLContext_* function defined in gl.cpp gets the osmesa_
 *           prefix in this compilation unit.
 * --------------------------------------------------------------------- */
#define SOGL_PREFIX_SET 1
#define SOGL_PREFIX_STR osmesa_

/* -----------------------------------------------------------------------
 * Step 4 – compile the primary GL glue implementation.
 *           The #defines above take effect inside the included file.
 * --------------------------------------------------------------------- */
#include "gl.cpp"
