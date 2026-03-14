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
  How to use OpenGL / GLX|WGL|AGL|CGL inside Coin
  ===============================================

  Creating portable OpenGL applications can be a complicated matter
  since you have to have both compile-time and runtime tests for
  OpenGL version, and what extensions are available. In addition, you
  might not have an entry point to the (extension) function in
  question on your build system.  The SoGLContext abstraction is here
  to relieve the application programmer for that burden.

  To use the SoGLContext interface, include glue/glp.h for internal code.

  The SoGLContext interface is part of the public API of Coin, but is
  not documented on the public documentation pages at
  https://coin3d.github.io/coin/ yet. The status for client application
  usage is "unofficial, use at own risk, interface may change without
  warning for major version number upgrade releases".

  Coin programmer's responsibilities
  ----------------------------------

  o OpenGL calls that are part of OpenGL 1.0 can safely be used
    without any kind of checking.

  o Do _not_ use SoGLContext unless you are sure that you have a valid
    OpenGL context. SoGLContext implicitly assumes that this is the case
    for most of its functions. In short, only use OpenGL functions
    inside an SoGLRenderAction.

  o To get hold of a SoGLContext instance:
      const SoGLContext * SoGLContext_instance(int contextid);
    or
      const SoGLContext * SoGLContext_instance_from_context_ptr(void * ctx);

    See header file for more information about these.

  o Always check that the capability you want to use is supported.
    Queries for this is supported through the SoGLContext_has_*()
    functions.

  o SoGLContext has some functions for querying OpenGL/GLX version and
    extension availability. Usually you shouldn't need to use these
    unless you want to bypass SoGLContext or your function isn't
    supported by SoGLContext (in which case you should add it).

  o SoGLCacheContextElement also has some functions for querying
    OpenGL version and extension availability. These are public, so
    you can use them even in external code. However, use SoGLContext
    internally for consistency.

  What SoGLContext supplies
  -----------------------

  o SoGLContext supplies function pointer to OpenGL and GLX functions
    used in Coin that are _not_ part of OpenGL 1.0 and GLX 1.1.  Note
    that SoGLContext supplies OpenGL extension functions as if they were
    standard functions (i.e. without the EXT suffix).

  o In addition, the Inventor/system/gl.h file supplies OpenGL enums
    that might not otherwise be present in your system's GL headers.

  The following example accesses OpenGL 3D texturing. It works both on
  OpenGL >= 1.2 and on OpenGL drivers with the GL_EXT_texture3D
  extension.

  ------ 8< --------- [snip] --------------------- 8< --------- [snip] -----

  const SoGLContext * glw = SoGLContext_instance(SoGLCacheContextElement::get(state));
  if (SoGLContext_has_3d_textures(glw)) {
    SoGLContext_glTexImage3D(glw, GL_PROXY_TEXTURE_3D, 0, GL_RGBA,
                           64, 64, 64, 0,
                           GL_RGBA, GL_UNSIGNED_BYTE,
                           NULL);
  }
  else {
    // Implement a proper fallback or error handling.
  }

  ------ 8< --------- [snip] --------------------- 8< --------- [snip] -----
*/

/*!
  For the library/API doc, here's the environment variables
  influencing the OpenGL binding:

  - OBOL_DEBUG_GLGLUE: set equal to "1" to make the wrapper
    initialization spit out lots of info about the underlying OpenGL
    implementation.

  - OBOL_PREFER_GLPOLYGONOFFSET_EXT: when set to "1" and both
    glPolygonOffset() and glPolygonOffsetEXT() are available, the
    latter will be used. This can be useful to work around a
    problematic glPolygonOffset() implementation for certain SGI
    platforms.

  - OBOL_FULL_INDIRECT_RENDERING: set to "1" to let Coin take
    advantage of OpenGL1.1+ and extensions even when doing
    remote/indirect rendering.

    We don't allow this by default now, for mainly two reasons: 1)
    we've seen NVidia GLX bugs when attempting this. 2) We generally
    prefer a "better safe than sorry" strategy.

    We might consider changing this strategy to allow it by default,
    and provide an environment variable to turn it off instead -- if
    we can get confirmation that the assumed NVidia driver bug is
    indeed NVidia's problem.

  - OBOL_FORCE_GL1_0_ONLY: set to "1" to disallow use of OpenGL1.1+
    and extensions under all circumstances.

  - OBOL_FORCE_AGL: set to "1" to prefer using the old AGL bindings over CGL.
    Note that AGL is not available on 64-bit systems. The AGL code is not
    compiled into Coin by default, but must be enabled at configure-time using
    --enable-agl in addition to using the environment variable.
*/


/*
  Useful resources:

   - About OpenGL 1.2, 1.3, 1.4:
     <URL:http://www.opengl.org/developers/documentation/OpenGL12.html>
     <URL:http://www.opengl.org/developers/documentation/OpenGL13.html>
     <URL:http://www.opengl.org/developers/documentation/OpenGL14.html>
     (explains all new features in depth)

   - The OpenGL Extension Registry:
     <URL:http://oss.sgi.com/projects/ogl-sample/registry/>

   - A great overview of what OpenGL driver capabilities are available
     for different cards, check out "3D Hardware Info" on
     <URL:http://www.delphi3d.net/>.

   - Brian Paul presentation "Using OpenGL Extensions" from SIGGRAPH '97:
     <URL:http://www.mesa3d.org/brianp/sig97/exten.htm>

   - Sun's man pages:
     <URL:http://wwws.sun.com/software/graphics/OpenGL/manpages>

   - IBM AIX GL man pages (try to find a "more official looking" link):
     <URL:http://molt.zdv.uni-mainz.de/doc_link/en_US/a_doc_lib/libs/openglrf/OpenGLXEnv.htm>

   - HP GL man pages:
     <URL:http://www.hp.com/workstations/support/documentation/manuals/user_guides/graphics/opengl/RefTOC.html>

   - An Apple Technical Q&A on how to do dynamic binding to OpenGL symbols:
     <URL:http://developer.apple.com/qa/qa2001/qa1188.html>

     Full documentation on all "Object File Image" functions, see:
     <URL:http://developer.apple.com/techpubs/macosx/DeveloperTools/MachORuntime/5rt_api_reference/_Object_Fil_e_Functions.html>
*/

#include <string>

#include "config.h"

// *************************************************************************

/* The configure script should protect against more than one of
   (HAVE_WGL), (HAVE_EGL) and (HAVE_AGL or HAVE_CGL) being defined at the same time, but
   we set up this little trip-wire in addition, just in case someone
   is either fiddling manually with config.h, or in case a change is
   made which breaks this protection in the configure script. */

#if defined(HAVE_WGL)
#define OBOL_HAVE_WGL_BIT 1
#else
#define OBOL_HAVE_WGL_BIT 0
#endif
#if defined(HAVE_EGL)
#define OBOL_HAVE_EGL_BIT 1
#else
#define OBOL_HAVE_EGL_BIT 0
#endif
#if defined(HAVE_AGL) || defined(HAVE_CGL)
#define OBOL_HAVE_AGL_CGL_BIT 1
#else
#define OBOL_HAVE_AGL_CGL_BIT 0
#endif
#define GRAPHICS_API_COUNT (OBOL_HAVE_WGL_BIT + OBOL_HAVE_EGL_BIT + OBOL_HAVE_AGL_CGL_BIT)

#if GRAPHICS_API_COUNT == 0
// Define HAVE_NOGL if no platform GL binding exists
#define HAVE_NOGL 1
#elif GRAPHICS_API_COUNT > 1
#error More than one of HAVE_WGL, HAVE_EGL, and HAVE_AGL|HAVE_CGL set simultaneously!
#endif

#undef GRAPHICS_API_COUNT
#undef OBOL_HAVE_WGL_BIT
#undef OBOL_HAVE_EGL_BIT
#undef OBOL_HAVE_AGL_CGL_BIT

// *************************************************************************

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <climits> /* SHRT_MAX */

#ifdef HAVE_AGL
#include <AGL/agl.h>
#endif /* HAVE_AGL */

#ifdef HAVE_OPENGL_CGLCURRENT_H
#include <OpenGL/CGLCurrent.h>
#endif

#ifdef HAVE_CGL
#include <OpenGL/OpenGL.h>
#endif

/* -----------------------------------------------------------------------
 * Note: EGL headers were previously conditionally included here but are
 * no longer used since context management moved to SoDB::ContextManager.
 * --------------------------------------------------------------------- */

#include "errors/CoinInternalError.h"
#include "CoinTidbits.h"
#include "base/list.h"


#include "base/dict.h"
#include "base/namemap.h"
#include "glue/glp.h"
#include "glue/dlp.h"
/* Platform-specific glue headers are no longer needed with callback-based contexts */
#include "misc/SoEnvironment.h"
#include <Inventor/SoDB.h>
#include <Inventor/elements/SoGLCacheContextElement.h>



/* ********************************************************************** */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if 0 /* emacs indentation fix */
}
#endif

static cc_list * gl_instance_created_cblist = NULL;
static int OBOL_MAXIMUM_TEXTURE2_SIZE = -1;
static int OBOL_MAXIMUM_TEXTURE3_SIZE = -1;
/* Removed old C-style callback system - now uses SoDB::ContextManager directly */

/* -----------------------------------------------------------------------
 * Dual-GL backend registry
 *
 * When OBOL_DUAL_GL_BUILD is defined and this is the primary (system-GL)
 * compilation unit (SOGL_PREFIX_SET is NOT set), we maintain a small set of
 * context IDs that were created against the OSMesa backend.
 *
 * The entire block below (registry + coingl_register_osmesa_context) is
 * excluded from the osmesa compilation unit (gl_osmesa.cpp) via
 * SOGL_PREFIX_SET so there is exactly ONE definition of each symbol.
 * --------------------------------------------------------------------- */
#ifndef SOGL_PREFIX_SET

/* These C++ STL includes must live OUTSIDE any extern "C" block. */
#ifdef __cplusplus
}  /* temporarily close the extern "C" block opened in glp.h */
#endif

#include <unordered_set>
#include <mutex>

#ifdef __cplusplus
extern "C" {  /* reopen extern "C" */
#endif

#if defined(OBOL_DUAL_GL_BUILD)
static std::unordered_set<int> * coingl_osmesa_context_ids = NULL;
static std::mutex coingl_osmesa_context_mutex;

static void coingl_osmesa_registry_cleanup(void)
{
  std::lock_guard<std::mutex> lock(coingl_osmesa_context_mutex);
  delete coingl_osmesa_context_ids;
  coingl_osmesa_context_ids = NULL;
}

/* Forward declarations of the osmesa-prefixed implementations compiled in
   gl_osmesa.cpp.  The linker resolves these from the glue_osmesa object. */
const SoGLContext * osmesa_SoGLContext_instance(int contextid);
void osmesa_SoGLContext_destruct(uint32_t contextid);
#endif /* OBOL_DUAL_GL_BUILD */

/* Public C API: register an OSMesa-backed render-context ID.
   Must be called (once, at context-ID assignment time) by the application
   or CoinOffscreenGLCanvas before the first SoGLContext_instance() call
   for that ID when OBOL_DUAL_GL_BUILD is enabled.
   Safe to call even when OBOL_DUAL_GL_BUILD is not defined (no-op). */
void
coingl_register_osmesa_context(int contextid)
{
#if defined(OBOL_DUAL_GL_BUILD)
  std::lock_guard<std::mutex> lock(coingl_osmesa_context_mutex);
  if (!coingl_osmesa_context_ids) {
    coingl_osmesa_context_ids = new std::unordered_set<int>();
    coin_atexit((coin_atexit_f *)coingl_osmesa_registry_cleanup, CC_ATEXIT_NORMAL);
  }
  coingl_osmesa_context_ids->insert(contextid);
#else
  (void)contextid;
#endif
}

/* Remove an OSMesa context ID from the backend registry.
   Called from SoGLContext_destruct() to keep the registry consistent.
   Safe to call even when OBOL_DUAL_GL_BUILD is not defined (no-op). */
void
coingl_unregister_osmesa_context(int contextid)
{
#if defined(OBOL_DUAL_GL_BUILD)
  std::lock_guard<std::mutex> lock(coingl_osmesa_context_mutex);
  if (coingl_osmesa_context_ids) {
    coingl_osmesa_context_ids->erase(contextid);
  }
#else
  (void)contextid;
#endif
}

/* Query whether a context ID was registered as an OSMesa context. */
[[maybe_unused]] static int
coingl_context_backend_is_osmesa(int contextid)
{
#if defined(OBOL_DUAL_GL_BUILD)
  std::lock_guard<std::mutex> lock(coingl_osmesa_context_mutex);
  return (coingl_osmesa_context_ids &&
          coingl_osmesa_context_ids->count(contextid) > 0) ? 1 : 0;
#else
  (void)contextid;
  return 0;
#endif
}

/* -----------------------------------------------------------------------
 * Per-context-ID manager registry
 *
 * Maps each render context ID to the SoDB::ContextManager that created it.
 * This allows SoGLContext_getprocaddress() and SoGLContext_instance() to
 * use the correct backend-specific resolver without consulting the global
 * singleton, enabling independent system-GL and OSMesa contexts to coexist
 * in the same process.
 *
 * Kept in the same SOGL_PREFIX_SET-guarded block as the OSMesa registry so
 * there is exactly ONE instance of the map regardless of how many GL
 * compilation units are linked in.
 * --------------------------------------------------------------------- */
#ifdef __cplusplus
} /* close extern "C" briefly for STL includes */
#endif
#include <unordered_map>
#include <mutex>
#ifdef __cplusplus
extern "C" {
#endif

static std::unordered_map<int, void*> * coingl_context_manager_map = NULL;
static std::mutex coingl_context_manager_mutex;

static void coingl_context_manager_map_cleanup(void)
{
  std::lock_guard<std::mutex> lock(coingl_context_manager_mutex);
  delete coingl_context_manager_map;
  coingl_context_manager_map = NULL;
}

void
coingl_register_context_manager(int contextid, void * mgr)
{
  std::lock_guard<std::mutex> lock(coingl_context_manager_mutex);
  if (!coingl_context_manager_map) {
    coingl_context_manager_map = new std::unordered_map<int, void*>();
    coin_atexit((coin_atexit_f *)coingl_context_manager_map_cleanup, CC_ATEXIT_NORMAL);
  }
  (*coingl_context_manager_map)[contextid] = mgr;
}

void
coingl_unregister_context_manager(int contextid)
{
  std::lock_guard<std::mutex> lock(coingl_context_manager_mutex);
  if (coingl_context_manager_map) {
    coingl_context_manager_map->erase(contextid);
  }
}

static SoDB::ContextManager *
coingl_get_context_manager(int contextid)
{
  std::lock_guard<std::mutex> lock(coingl_context_manager_mutex);
  if (!coingl_context_manager_map) return NULL;
  auto it = coingl_context_manager_map->find(contextid);
  return (it != coingl_context_manager_map->end())
         ? static_cast<SoDB::ContextManager*>(it->second) : NULL;
}

#endif /* !SOGL_PREFIX_SET */

/* ********************************************************************** */

/* Sanity checks for enum extension value assumed to be equal to the
 * final / "proper" / standard OpenGL enum values. (If not, we could
 * end up with hard-to-find bugs because of mismatches with the
 * compiled values versus the runtime values.)
 *
 * This doesn't really _fix_ anything, it is just meant as an aid to
 * smoke out platforms where we're getting unexpected enum values.
 */

#ifdef GL_CLAMP_TO_EDGE_EXT
#if GL_CLAMP_TO_EDGE != GL_CLAMP_TO_EDGE_EXT
#error dangerous enum mismatch
#endif /* cmp */
#endif /* GL_CLAMP_TO_EDGE_EXT */

#ifdef GL_CLAMP_TO_EDGE_SGIS
#if GL_CLAMP_TO_EDGE != GL_CLAMP_TO_EDGE_SGIS
#error dangerous enum mismatch
#endif /* cmp */
#endif /* GL_CLAMP_TO_EDGE_SGIS */

#ifdef GL_MAX_3D_TEXTURE_SIZE_EXT
#if GL_MAX_3D_TEXTURE_SIZE != GL_MAX_3D_TEXTURE_SIZE_EXT
#error dangerous enum mismatch
#endif /* cmp */
#endif /* GL_MAX_3D_TEXTURE_SIZE_EXT */

#ifdef GL_PACK_IMAGE_HEIGHT_EXT
#if GL_PACK_IMAGE_HEIGHT != GL_PACK_IMAGE_HEIGHT_EXT
#error dangerous enum mismatch
#endif /* cmp */
#endif /* GL_PACK_IMAGE_HEIGHT_EXT */

#ifdef GL_PACK_SKIP_IMAGES_EXT
#if GL_PACK_SKIP_IMAGES != GL_PACK_SKIP_IMAGES_EXT
#error dangerous enum mismatch
#endif /* cmp */
#endif /* GL_PACK_SKIP_IMAGES_EXT */

#ifdef GL_PROXY_TEXTURE_2D_EXT
#if GL_PROXY_TEXTURE_2D != GL_PROXY_TEXTURE_2D_EXT
#error dangerous enum mismatch
#endif /* cmp */
#endif /* GL_PROXY_TEXTURE_2D_EXT */

#ifdef GL_PROXY_TEXTURE_3D_EXT
#if GL_PROXY_TEXTURE_3D != GL_PROXY_TEXTURE_3D_EXT
#error dangerous enum mismatch
#endif /* cmp */
#endif /* GL_PROXY_TEXTURE_3D_EXT */

#ifdef GL_TEXTURE_3D_EXT
#if GL_TEXTURE_3D != GL_TEXTURE_3D_EXT
#error dangerous enum mismatch
#endif /* cmp */
#endif /* GL_TEXTURE_3D_EXT */

#ifdef GL_TEXTURE_DEPTH_EXT
#if GL_TEXTURE_DEPTH != GL_TEXTURE_DEPTH_EXT
#error dangerous enum mismatch
#endif /* cmp */
#endif /* GL_TEXTURE_DEPTH_EXT */

#ifdef GL_TEXTURE_WRAP_R_EXT
#if GL_TEXTURE_WRAP_R != GL_TEXTURE_WRAP_R_EXT
#error dangerous enum mismatch
#endif /* cmp */
#endif /* GL_TEXTURE_WRAP_R_EXT */

#ifdef GL_UNPACK_IMAGE_HEIGHT_EXT
#if GL_UNPACK_IMAGE_HEIGHT != GL_UNPACK_IMAGE_HEIGHT_EXT
#error dangerous enum mismatch
#endif /* cmp */
#endif /* GL_UNPACK_IMAGE_HEIGHT_EXT */

#ifdef GL_UNPACK_SKIP_IMAGES_EXT
#if GL_UNPACK_SKIP_IMAGES != GL_UNPACK_SKIP_IMAGES_EXT
#error dangerous enum mismatch
#endif /* cmp */
#endif /* GL_UNPACK_SKIP_IMAGES_EXT */

#ifdef GL_FUNC_ADD_EXT
#if GL_FUNC_ADD != GL_FUNC_ADD_EXT
#error dangerous enum mismatch
#endif /* cmp */
#endif /* GL_FUNC_ADD_EXT */

#ifdef GL_MIN_EXT
#if GL_MIN != GL_MIN_EXT
#error dangerous enum mismatch
#endif /* cmp */
#endif /* GL_MIN_EXT */

#ifdef GL_MAX_EXT
#if GL_MAX != GL_MAX_EXT
#error dangerous enum mismatch
#endif /* cmp */
#endif /* GL_MAX_EXT */

#ifdef GL_COLOR_TABLE_WIDTH_EXT
#if GL_COLOR_TABLE_WIDTH != GL_COLOR_TABLE_WIDTH_EXT
#error dangerous enum mismatch
#endif /* cmp */
#endif /* GL_COLOR_TABLE_WIDTH_EXT */

/* ********************************************************************** */

/* Resolve and return the integer value of an environment variable. */
static int
glglue_resolve_envvar(const char * txt)
{
  auto val = CoinInternal::getEnvironmentVariable(txt);
  return val.has_value() ? std::atoi(val->c_str()) : 0;
}

/* Returns a flag which indicates whether or not to allow the use of
   OpenGL 1.1+ features and extensions.

   We default to *not* allowing this if rendering is indirect, as
   we've seen major problems with at least NVidia GLX when using
   OpenGL 1.1+ features. It can be forced on by an environment
   variable, though.

   (A better strategy *might* be to default to allow it, but to smoke
   out and warn if we detect NVidia GLX, and in addition to provide an
   environment variable that disables it.)
*/
static SbBool
glglue_allow_newer_opengl(const SoGLContext * OBOL_UNUSED_ARG(w))
{
  static int force1_0_initialized = 0;
  static SbBool force1_0 = FALSE;

  if (!force1_0_initialized) {
    force1_0 = (glglue_resolve_envvar("OBOL_FORCE_GL1_0_ONLY") > 0);
    force1_0_initialized = 1;
  }

  if (force1_0) return FALSE;

#ifdef OBOL_SWRAST_BUILD
  /* For OSMesa builds, always allow newer OpenGL features.
     OSMesa provides OpenGL 2.0 capabilities which is our minimum target. */
  return TRUE;
#else
  static int fullindirect_initialized = 0;
  static SbBool fullindirect = FALSE;
  static const char * OBOL_FULL_INDIRECT_RENDERING = "OBOL_FULL_INDIRECT_RENDERING";

  if (!fullindirect_initialized) {
    fullindirect = (glglue_resolve_envvar(OBOL_FULL_INDIRECT_RENDERING) > 0);
    fullindirect_initialized = 1;
  }

  /* GLX direct-rendering detection was removed when context management
     was moved to SoDB::ContextManager callbacks.  Assume direct rendering
     unless the user explicitly requests indirect via the env var. */
  if (!fullindirect) {
    return TRUE;
  }

  return FALSE;
#endif
}


/* Returns whether or not OBOL_GLGLUE_SILENCE_DRIVER_WARNINGS is set
   to a value > 0. If so, all known driver bugs will just be silently
   accepted and attempted worked around. */
static int
SoGLContext_silence_all_driver_warnings(void)
{
  static int d = -1;
  if (d == -1) { d = glglue_resolve_envvar("OBOL_GLGLUE_SILENCE_DRIVER_WARNINGS"); }
  /* Note the inversion of the envvar value versus the return value. */
  return (d > 0) ? 0 : 1;
}

/* Return value of OBOL_GLGLUE_NO_RADEON_WARNING environment variable. */
static int
SoGLContext_radeon_warning(void)
{
  static int d = -1;

  if (SoGLContext_silence_all_driver_warnings()) { return 0; }

  if (d == -1) { d = glglue_resolve_envvar("OBOL_GLGLUE_NO_RADEON_WARNING"); }
  /* Note the inversion of the envvar value versus the return value. */
  return (d > 0) ? 0 : 1;
}

/* Return value of OBOL_GLGLUE_NO_G400_WARNING environment variable. */
static int
SoGLContext_old_matrox_warning(void)
{
  static int d = -1;

  if (SoGLContext_silence_all_driver_warnings()) { return 0; }

  if (d == -1) { d = glglue_resolve_envvar("OBOL_GLGLUE_NO_G400_WARNING"); }
  /* Note the inversion of the envvar value versus the return value. */
  return (d > 0) ? 0 : 1;
}

/* Return value of OBOL_GLGLUE_NO_ELSA_WARNING environment variable. */
static int
SoGLContext_old_elsa_warning(void)
{
  static int d = -1;

  if (SoGLContext_silence_all_driver_warnings()) { return 0; }

  if (d == -1) { d = glglue_resolve_envvar("OBOL_GLGLUE_NO_ELSA_WARNING"); }
  /* Note the inversion of the envvar value versus the return value. */
  return (d > 0) ? 0 : 1;
}

/* Return value of OBOL_GLGLUE_NO_SUN_EXPERT3D_WARNING environment variable. */
static int
SoGLContext_sun_expert3d_warning(void)
{
  static int d = -1;

  if (SoGLContext_silence_all_driver_warnings()) { return 0; }

  if (d == -1) { d = glglue_resolve_envvar("OBOL_GLGLUE_NO_SUN_EXPERT3D_WARNING"); }
  /* Note the inversion of the envvar value versus the return value. */
  return (d > 0) ? 0 : 1;
}

/* Return value of OBOL_GLGLUE_NO_TRIDENT_WARNING environment variable. */
static int
SoGLContext_trident_warning(void)
{
  static int d = -1;

  if (SoGLContext_silence_all_driver_warnings()) { return 0; }

  if (d == -1) { d = glglue_resolve_envvar("OBOL_GLGLUE_NO_TRIDENT_WARNING"); }
  /* Note the inversion of the envvar value versus the return value. */
  return (d > 0) ? 0 : 1;
}

/* Return value of OBOL_DEBUG_GLGLUE environment variable. */
int
SoGLContext_debug(void)
{
  static int d = -1;
  if (d == -1) { d = glglue_resolve_envvar("OBOL_DEBUG_GLGLUE"); }
  return (d > 0) ? 1 : 0;
}

/* Return value of OBOL_PREFER_GLPOLYGONOFFSET_EXT environment variable. */
static int
glglue_prefer_glPolygonOffsetEXT(void)
{
  static int d = -1;
  if (d == -1) { d = glglue_resolve_envvar("OBOL_PREFER_GLPOLYGONOFFSET_EXT"); }
  return (d > 0) ? 1 : 0;
}

/* FIXME: the following is a hack to get around a problem which really
   demands more effort to be solved properly.

   The problem is that there is no way in the API of the
   SoOffscreenRenderer class to specify what particular attributes to
   request. This most often manifests itself as a problem for app
   programmers in that they have made some kind of extension node
   which uses the OpenGL stencil buffer. If no stencil buffer happens
   to be part of the GL context format for the offscreen renderer,
   these will not work properly. At the same time, we don't want to
   default to requesting a stencil buffer, as that takes a non-trivial
   amount of extra memory resources on the gfx card.

   So until we have implemented the proper solution for making it
   possible to pass in a detailed specification of which attributes to
   request from offscreen GL contexts, we provide this temporary
   work-around: the app programmer can set an envvar with a value
   specifying the number of stencil buffer bits he/she wants.

   20060223 mortene.
*/
int
SoGLContext_stencil_bits_hack(void)
{
  auto env = CoinInternal::getEnvironmentVariable("OBOL_OFFSCREEN_STENCIL_BITS");
  if (!env.has_value()) { return -1; }
  return std::atoi(env->c_str());
}

cc_libhandle
SoGLContext_dl_handle(const SoGLContext * glue)
{
  if (!glue->dl_handle) {
    const_cast <SoGLContext *> (glue)->dl_handle = cc_dl_handle_with_gl_symbols();
  }
  return glue->dl_handle;
}

/* doc in header file */
void *
SoGLContext_getprocaddress(const SoGLContext * glue, const char * symname)
{
  void * ptr = NULL;

#if (defined(OBOL_SWRAST_BUILD) && !defined(OBOL_DUAL_GL_BUILD)) || defined(SOGL_PREFIX_SET)
  /* OSMesa path: resolve via OSMesaGetProcAddress first to guarantee we
     get an OSMesa function pointer and never accidentally pick up a system
     GL symbol from the process handle (the two implementations have
     different dispatch tables and mixing them causes subtle corruption).
     This path is active in:
       - pure swrast-only builds (OBOL_SWRAST_BUILD without OBOL_DUAL_GL_BUILD)
       - the osmesa_ compilation unit in dual-GL builds (SOGL_PREFIX_SET)
     It is NOT active in the system-GL compilation unit of a dual-GL build,
     because gl.cpp is compiled without OSMesa headers there. */
  ptr = (void*)OSMesaGetProcAddress(symname);
  if (SoGLContext_debug()) {
    cc_debugerror_postinfo("SoGLContext_getprocaddress",
                           "OSMesaGetProcAddress('%s') == %p", symname, ptr);
  }

  /* OSMesa uses MGL name mangling; if the standard name lookup failed,
     try the MGL-mangled version ("gl" -> "mgl") via dlsym as a secondary
     fallback (covers static builds where the mgl* symbol is directly in
     the binary). */
  if (ptr == NULL && strncmp(symname, "gl", 2) == 0) {
    size_t namelen = strlen(symname);
    char * mgl_name = (char*)malloc(namelen + 2); /* +1 for 'm', +1 for '\0' */
    if (mgl_name) {
      strcpy(mgl_name, "mgl");
      strcat(mgl_name, symname + 2); /* Skip "gl" prefix */
      ptr = cc_dl_sym(SoGLContext_dl_handle(glue), mgl_name);
      if (SoGLContext_debug()) {
        cc_debugerror_postinfo("SoGLContext_getprocaddress",
                               "MGL fallback: %s -> %s == %p",
                               symname, mgl_name, ptr);
      }
      free(mgl_name);
    }
  }
#else
  /* System GL path: try dlsym first (works for core functions that are
     directly exported by the GL library). */
  ptr = cc_dl_sym(SoGLContext_dl_handle(glue), symname);
#endif

  /* Final fallback: ask the context manager.  For system-GL contexts (GLX,
     WGL, EGL) the application provides a context manager that calls the
     platform's proc-address resolver (e.g. glXGetProcAddress).  This keeps
     all platform-specific code outside of Obol proper.
     NOTE: This fallback is intentionally skipped in the OSMesa compilation
     unit (SOGL_PREFIX_SET).  In a dual-GL build an OSMesa context's manager
     uses OSMesaGetProcAddress() while the system-GL context's manager uses
     the platform resolver; using the per-context manager here prevents
     cross-backend contamination of function pointers. */
#ifndef SOGL_PREFIX_SET
  if (ptr == NULL) {
    SoDB::ContextManager * mgr = static_cast<SoDB::ContextManager*>(glue->context_manager);
    if (mgr) {
      ptr = mgr->getProcAddress(symname);
      if (SoGLContext_debug()) {
        cc_debugerror_postinfo("SoGLContext_getprocaddress",
                               "context manager getProcAddress('%s') == %p",
                               symname, ptr);
      }
    }
  }
#endif /* !SOGL_PREFIX_SET */

  if (SoGLContext_debug()) {
    cc_debugerror_postinfo("SoGLContext_getprocaddress", "%s==%p", symname, ptr);
  }
  return ptr;
}

/* Global dictionary which stores the mappings from the context IDs to
   actual SoGLContext instances. */
static cc_dict * gldict = NULL;

static void
free_glglue_instance(uintptr_t OBOL_UNUSED_ARG(key), void * value, void * OBOL_UNUSED_ARG(closure))
{
  SoGLContext * glue = (SoGLContext*) value;
  cc_dict_destruct(glue->glextdict);
  free(value);
}

/* Cleans up at exit. */
static void
glglue_cleanup(void)
{
  if (gldict) {
    cc_dict_apply(gldict, free_glglue_instance, NULL);
    cc_dict_destruct(gldict);
    gldict = NULL;
  }
  /* No more callback cleanup needed - context manager is managed by SoDB */

  /* Platform-specific cleanup is no longer needed with callback-based contexts */
}


/*
  Set the OpenGL version variables in the given SoGLContext struct
  instance.

  Note: this code has been copied from GLUWrapper.c, so if any changes
  are made, make sure they are propagated over if necessary.
*/
static void
glglue_set_glVersion(SoGLContext * w)
{
  char buffer[256];
  char * dotptr;

  /* NB: if you are getting a crash here, it's because an attempt at
   * setting up a SoGLContext instance was made when there is no current
   * OpenGL context. */
  if (SoGLContext_debug()) {
    if (w->versionstr && strlen(w->versionstr) > 0) {
      cc_debugerror_postinfo("glglue_set_glVersion",
                             "glGetString(GL_VERSION)=='%s'", w->versionstr);
    } else {
      cc_debugerror_postinfo("glglue_set_glVersion",
                             "glGetString(GL_VERSION)==%p (NULL or empty)", w->versionstr);
    }
  }

  w->version.major = 0;
  w->version.minor = 0;
  w->version.release = 0;

  if (!w->versionstr) { return; }

  (void)strncpy(buffer, (const char *)w->versionstr, 255);
  buffer[255] = '\0'; /* strncpy() will not null-terminate if strlen > 255 */
  dotptr = strchr(buffer, '.');
  if (dotptr) {
    char * spaceptr;
    char * start = buffer;
    *dotptr = '\0';
    w->version.major = atoi(start);
    start = ++dotptr;

    dotptr = strchr(start, '.');
    spaceptr = strchr(start, ' ');
    if (!dotptr && spaceptr) dotptr = spaceptr;
    if (dotptr && spaceptr && spaceptr < dotptr) dotptr = spaceptr;
    if (dotptr) {
      int terminate = *dotptr == ' ';
      *dotptr = '\0';
      w->version.minor = atoi(start);
      if (!terminate) {
        start = ++dotptr;
        dotptr = strchr(start, ' ');
        if (dotptr) *dotptr = '\0';
        w->version.release = atoi(start);
      }
    }
    else {
      w->version.minor = atoi(start);
    }
  }

  if (SoGLContext_debug()) {
    cc_debugerror_postinfo("glglue_set_glVersion",
                           "parsed to major=='%d', minor=='%d', micro=='%d'",
                           w->version.major,
                           w->version.minor,
                           w->version.release);
  }
}

void
SoGLContext_glversion(const SoGLContext * w,
                    unsigned int * major,
                    unsigned int * minor,
                    unsigned int * release)
{
  if (!glglue_allow_newer_opengl(w)) {
    /* When newer OpenGL is disabled, report OpenGL 2.0 as minimum target
       instead of 1.0, since that's our actual minimum requirement */
    *major = 2;
    *minor = 0;
    *release = 0;
  }
  else {
    *major = w->version.major;
    *minor = w->version.minor;
    *release = w->version.release;
  }
}


SbBool
SoGLContext_glversion_matches_at_least(const SoGLContext * w,
                                     unsigned int major,
                                     unsigned int minor,
                                     unsigned int revision)
{
  unsigned int glmajor, glminor, glrev;
  SoGLContext_glversion(w, &glmajor, &glminor, &glrev);

  if (glmajor < major) return FALSE;
  else if (glmajor > major) return TRUE;
  if (glminor < minor) return FALSE;
  else if (glminor > minor) return TRUE;
  if (glminor < revision) return FALSE;
  return TRUE;
}

SbBool
SoGLContext_glxversion_matches_at_least(const SoGLContext * w,
                                      int major,
                                      int minor)
{
  /* GLX version detection was removed when context management was moved to
     SoDB::ContextManager callbacks.  Always returns FALSE. */
  (void)w; (void)major; (void)minor;
  return FALSE;
}

int
SoGLContext_extension_available(const char * extensions, const char * ext)
{
  const char * start;
  size_t extlen;
  SbBool found = FALSE;

  assert(ext && "NULL string");
  assert((ext[0] != '\0') && "empty string");
  assert((strchr(ext, ' ') == NULL) && "extension name can't have spaces");

  start = extensions;
  extlen = strlen(ext);

  while (start) {
    const char * where = strstr(start, ext);
    if (!where) goto done;

    if (where == start || *(where - 1) == ' ') {
      const char * terminator = where + extlen;
      if (*terminator == ' ' || *terminator == '\0') {
        found = TRUE;
        goto done;
      }
    }

    start = where + extlen;
  }

done:
  if (SoGLContext_debug()) {
    cc_debugerror_postinfo("SoGLContext_extension_available",
                           "extension '%s' is%s present",
                           ext, found ? "" : " NOT");
  }

  return found ? 1 : 0;
}

SbBool
SoGLContext_glext_supported(const SoGLContext * wrapper, const char * extension)
{
  const uintptr_t key = (uintptr_t)cc_namemap_get_address(extension);

  void * result = NULL;
  if (cc_dict_get(wrapper->glextdict, key, &result)) {
    return result != NULL;
  }
  result = SoGLContext_extension_available(wrapper->extensionsstr, extension) ?
    (void*) 1 : NULL;
  cc_dict_put(wrapper->glextdict, key, result);

  return result != NULL;
}

#ifdef HAVE_DYNAMIC_LINKING

#define PROC(_glue_, _func_) SoGLContext_getprocaddress(_glue_, SO__QUOTE(_func_))

/* The OpenGL library which we dynamically pick up symbols from
   /could/ have all these defined. For the code below which tries to
   dynamically resolve the methods, we will assume that they are all
   defined. By doing this little "trick", can we use the same code
   below for resolving stuff dynamically as we need anyway to resolve
   in a static manner. */
#define GL_VERSION_1_1 1
#define GL_VERSION_1_2 1
#define GL_VERSION_1_3 1
#define GL_VERSION_1_4 1
#define GL_VERSION_1_5 1
#define GL_EXT_polygon_offset 1
#define GL_EXT_texture_object 1
#define GL_EXT_subtexture 1
#define GL_EXT_texture3D 1
#define GL_ARB_multitexture 1
#define GL_ARB_texture_compression 1
#define GL_EXT_paletted_texture 1
#define GL_ARB_imaging 1
#define GL_EXT_blend_minmax 1
#define GL_EXT_color_table 1
#define GL_EXT_color_subtable 1
#define GL_SGI_color_table 1
#define GL_SGI_texture_color_table 1
#define GL_ARB_vertex_buffer_object 1
#define GL_EXT_multi_draw_arrays 1
#define GL_NV_vertex_array_range 1
#define GL_NV_register_combiners 1
#define GL_NV_texture_rectangle 1
#define GL_NV_texture_shader 1
#define GL_ARB_depth_texture 1
#define GL_ARB_shadow 1
#define GL_EXT_texture_rectangle 1
#define GL_ARB_fragment_program 1
#define GL_ARB_vertex_program 1
#define GL_ARB_shader_objects 1
#define GL_ARB_vertex_shader 1
#define GL_ARB_occlusion_query 1

#else /* static binding */

#define PROC(_glue_, _func_) (&_func_)

#endif /* static binding */


static void
glglue_resolve_symbols(SoGLContext * w)
{
  /* Note that there's a good reason why we use version checking
     *along* with dynamic resolving (if the platform allows it): the
     OpenGL library could (prematurely) include function symbols
     without having an actual valid implementation behind them. */

  /* Appeared in OpenGL v1.1. We store both the "real" function
     pointer and the extension pointer, in case we need to work around
     an SGI bug (see comments in SoGLContext_glPolygonOffset(). */
  w->glPolygonOffset = NULL;
  w->glPolygonOffsetEXT = NULL;
#ifdef GL_VERSION_1_1
  if (SoGLContext_glversion_matches_at_least(w, 1, 1, 0)) {
    w->glPolygonOffset = (OBOL_PFNGLPOLYGONOFFSETPROC)PROC(w, glPolygonOffset);
  }
#endif /* GL_VERSION_1_1 */
#ifdef GL_EXT_polygon_offset
  if (SoGLContext_glext_supported(w, "GL_EXT_polygon_offset")) {
    w->glPolygonOffsetEXT = (OBOL_PFNGLPOLYGONOFFSETPROC)PROC(w, glPolygonOffsetEXT);
  }
#endif /* GL_EXT_polygon_offset */



  /* Core GL 1.0/1.1 functions — always available, stored as function
     pointers so dual-GL builds (OBOL_DUAL_GL_BUILD) dispatch through
     the correct backend rather than always calling system GL. */
  w->glTexImage2D      = (OBOL_PFNGLTEXIMAGE2DPROC)PROC(w, glTexImage2D);
  w->glTexParameteri   = (OBOL_PFNGLTEXPARAMETERIPROC)PROC(w, glTexParameteri);
  w->glTexParameterf   = (OBOL_PFNGLTEXPARAMETERFPROC)PROC(w, glTexParameterf);
  w->glGetIntegerv     = (OBOL_PFNGLGETINTEGERVPROC)PROC(w, glGetIntegerv);
  w->glGetFloatv       = (OBOL_PFNGLGETFLOATVPROC)PROC(w, glGetFloatv);
  w->glClearColor      = (OBOL_PFNGLCLEARCOLORPROC)PROC(w, glClearColor);
  w->glClear           = (OBOL_PFNGLCLEARPROC)PROC(w, glClear);
  w->glFlush           = (OBOL_PFNGLFLUSHPROC)PROC(w, glFlush);
  w->glFinish          = (OBOL_PFNGLFINISHPROC)PROC(w, glFinish);
  w->glGetError        = (OBOL_PFNGLGETERRORPROC)PROC(w, glGetError);
  w->glGetString       = (OBOL_PFNGLGETSTRINGPROC)PROC(w, glGetString);
  w->glEnable          = (OBOL_PFNGLENABLEPROC)PROC(w, glEnable);
  w->glDisable         = (OBOL_PFNGLDISABLEPROC)PROC(w, glDisable);
  w->glIsEnabled       = (OBOL_PFNGLISENABLEDPROC)PROC(w, glIsEnabled);
  w->glPixelStorei     = (OBOL_PFNGLPIXELSTOREIPROC)PROC(w, glPixelStorei);
  w->glReadPixels      = (OBOL_PFNGLREADPIXELSPROC)PROC(w, glReadPixels);
  w->glCopyTexSubImage2D = (OBOL_PFNGLCOPYTEXSUBIMAGE2DPROC)PROC(w, glCopyTexSubImage2D);

  /* Additional core GL 1.0/1.1 dispatch pointers. */
  w->glBegin          = (OBOL_PFNGLBEGINPROC)PROC(w, glBegin);
  w->glEnd            = (OBOL_PFNGLENDPROC)PROC(w, glEnd);
  w->glVertex2f       = (OBOL_PFNGLVERTEX2FPROC)PROC(w, glVertex2f);
  w->glVertex2s       = (OBOL_PFNGLVERTEX2SPROC)PROC(w, glVertex2s);
  w->glVertex3f       = (OBOL_PFNGLVERTEX3FPROC)PROC(w, glVertex3f);
  w->glVertex3fv      = (OBOL_PFNGLVERTEX3FVPROC)PROC(w, glVertex3fv);
  w->glVertex4fv      = (OBOL_PFNGLVERTEX4FVPROC)PROC(w, glVertex4fv);
  w->glNormal3f       = (OBOL_PFNGLNORMAL3FPROC)PROC(w, glNormal3f);
  w->glNormal3fv      = (OBOL_PFNGLNORMAL3FVPROC)PROC(w, glNormal3fv);
  w->glColor3f        = (OBOL_PFNGLCOLOR3FPROC)PROC(w, glColor3f);
  w->glColor3fv       = (OBOL_PFNGLCOLOR3FVPROC)PROC(w, glColor3fv);
  w->glColor3ub       = (OBOL_PFNGLCOLOR3UBPROC)PROC(w, glColor3ub);
  w->glColor3ubv      = (OBOL_PFNGLCOLOR3UBVPROC)PROC(w, glColor3ubv);
  w->glColor4ub       = (OBOL_PFNGLCOLOR4UBPROC)PROC(w, glColor4ub);
  w->glTexCoord2f     = (OBOL_PFNGLTEXCOORD2FPROC)PROC(w, glTexCoord2f);
  w->glTexCoord2fv    = (OBOL_PFNGLTEXCOORD2FVPROC)PROC(w, glTexCoord2fv);
  w->glTexCoord3f     = (OBOL_PFNGLTEXCOORD3FPROC)PROC(w, glTexCoord3f);
  w->glTexCoord3fv    = (OBOL_PFNGLTEXCOORD3FVPROC)PROC(w, glTexCoord3fv);
  w->glTexCoord4fv    = (OBOL_PFNGLTEXCOORD4FVPROC)PROC(w, glTexCoord4fv);
  w->glIndexi         = (OBOL_PFNGLINDEXIPROC)PROC(w, glIndexi);
  w->glMatrixMode     = (OBOL_PFNGLMATRIXMODEPROC)PROC(w, glMatrixMode);
  w->glLoadIdentity   = (OBOL_PFNGLLOADIDENTITYPROC)PROC(w, glLoadIdentity);
  w->glLoadMatrixf    = (OBOL_PFNGLLOADMATRIXFPROC)PROC(w, glLoadMatrixf);
  w->glLoadMatrixd    = (OBOL_PFNGLLOADMATRIXDPROC)PROC(w, glLoadMatrixd);
  w->glMultMatrixf    = (OBOL_PFNGLMULTMATRIXFPROC)PROC(w, glMultMatrixf);
  w->glPushMatrix     = (OBOL_PFNGLPUSHMATRIXPROC)PROC(w, glPushMatrix);
  w->glPopMatrix      = (OBOL_PFNGLPOPMATRIXPROC)PROC(w, glPopMatrix);
  w->glOrtho          = (OBOL_PFNGLORTHOPROC)PROC(w, glOrtho);
  w->glFrustum        = (OBOL_PFNGLFRUSTUMPROC)PROC(w, glFrustum);
  w->glTranslatef     = (OBOL_PFNGLTRANSLATEFPROC)PROC(w, glTranslatef);
  w->glRotatef        = (OBOL_PFNGLROTATEFPROC)PROC(w, glRotatef);
  w->glScalef         = (OBOL_PFNGLSCALEFPROC)PROC(w, glScalef);
  w->glLightf         = (OBOL_PFNGLLIGHTFPROC)PROC(w, glLightf);
  w->glLightfv        = (OBOL_PFNGLLIGHTFVPROC)PROC(w, glLightfv);
  w->glLightModeli    = (OBOL_PFNGLLIGHTMODELIPROC)PROC(w, glLightModeli);
  w->glLightModelfv   = (OBOL_PFNGLLIGHTMODELFVPROC)PROC(w, glLightModelfv);
  w->glMaterialf      = (OBOL_PFNGLMATERIALFPROC)PROC(w, glMaterialf);
  w->glMaterialfv     = (OBOL_PFNGLMATERIALFVPROC)PROC(w, glMaterialfv);
  w->glColorMaterial  = (OBOL_PFNGLCOLORMATERIALPROC)PROC(w, glColorMaterial);
  w->glFogi           = (OBOL_PFNGLFOGLPROC)PROC(w, glFogi);
  w->glFogf           = (OBOL_PFNGLFOGFPROC)PROC(w, glFogf);
  w->glFogfv          = (OBOL_PFNGLFOGFVPROC)PROC(w, glFogfv);
  w->glTexEnvi        = (OBOL_PFNGLTEXENVIPROC)PROC(w, glTexEnvi);
  w->glTexEnvf        = (OBOL_PFNGLTEXENVFPROC)PROC(w, glTexEnvf);
  w->glTexEnvfv       = (OBOL_PFNGLTEXENVFVPROC)PROC(w, glTexEnvfv);
  w->glTexGeni        = (OBOL_PFNGLTEXGENIPROC)PROC(w, glTexGeni);
  w->glTexGenf        = (OBOL_PFNGLTEXGENFPROC)PROC(w, glTexGenf);
  w->glTexGenfv       = (OBOL_PFNGLTEXGENFVPROC)PROC(w, glTexGenfv);
  w->glCopyTexImage2D = (OBOL_PFNGLCOPYTEXIMAGE2DPROC)PROC(w, glCopyTexImage2D);
  w->glRasterPos2f    = (OBOL_PFNGLRASTERPOS2FPROC)PROC(w, glRasterPos2f);
  w->glRasterPos3f    = (OBOL_PFNGLRASTERPOS3FPROC)PROC(w, glRasterPos3f);
  w->glBitmap         = (OBOL_PFNGLBITMAPPROC)PROC(w, glBitmap);
  w->glDrawPixels     = (OBOL_PFNGLDRAWPIXELSPROC)PROC(w, glDrawPixels);
  w->glPixelTransferf = (OBOL_PFNGLPIXELTRANSFERFPROC)PROC(w, glPixelTransferf);
  w->glPixelTransferi = (OBOL_PFNGLPIXELTRANSFERIPROC)PROC(w, glPixelTransferi);
  w->glPixelMapfv     = (OBOL_PFNGLPIXELMAPFVPROC)PROC(w, glPixelMapfv);
  w->glPixelMapuiv    = (OBOL_PFNGLPIXELMAPUIVPROC)PROC(w, glPixelMapuiv);
  w->glPixelZoom      = (OBOL_PFNGLPIXELZOOMPROC)PROC(w, glPixelZoom);
  w->glViewport       = (OBOL_PFNGLVIEWPORTPROC)PROC(w, glViewport);
  w->glScissor        = (OBOL_PFNGLSCISSORPROC)PROC(w, glScissor);
  w->glDepthMask      = (OBOL_PFNGLDEPTHMASKPROC)PROC(w, glDepthMask);
  w->glDepthFunc      = (OBOL_PFNGLDEPTHFUNCPROC)PROC(w, glDepthFunc);
  w->glDepthRange     = (OBOL_PFNGLDEPTHRANGEPROC)PROC(w, glDepthRange);
  w->glStencilFunc    = (OBOL_PFNGLSTENCILFUNCPROC)PROC(w, glStencilFunc);
  w->glStencilOp      = (OBOL_PFNGLSTENCILOPPROC)PROC(w, glStencilOp);
  w->glBlendFunc      = (OBOL_PFNGLBLENDFUNCPROC)PROC(w, glBlendFunc);
  w->glAlphaFunc      = (OBOL_PFNGLALPHAFUNCPROC)PROC(w, glAlphaFunc);
  w->glFrontFace      = (OBOL_PFNGLFRONTFACEPROC)PROC(w, glFrontFace);
  w->glCullFace       = (OBOL_PFNGLCULLFACEPROC)PROC(w, glCullFace);
  w->glPolygonMode    = (OBOL_PFNGLPOLYGONMODEPROC)PROC(w, glPolygonMode);
  w->glPolygonStipple = (OBOL_PFNGLPOLYGONSTIPPLEPROC)PROC(w, glPolygonStipple);
  w->glLineWidth      = (OBOL_PFNGLLINEWIDTHPROC)PROC(w, glLineWidth);
  w->glLineStipple    = (OBOL_PFNGLLINESTIPPLEPROC)PROC(w, glLineStipple);
  w->glPointSize      = (OBOL_PFNGLPOINTSIZEPROC)PROC(w, glPointSize);
  w->glColorMask      = (OBOL_PFNGLCOLORMASKPROC)PROC(w, glColorMask);
  w->glClipPlane      = (OBOL_PFNGLCLIPPLANEPROC)PROC(w, glClipPlane);
  w->glDrawBuffer     = (OBOL_PFNGLDRAWBUFFERPROC)PROC(w, glDrawBuffer);
  w->glClearIndex     = (OBOL_PFNGLCLEARINDEXPROC)PROC(w, glClearIndex);
  w->glClearStencil   = (OBOL_PFNGLCLEARSTENCILPROC)PROC(w, glClearStencil);
  w->glAccum          = (OBOL_PFNGLACCUMPROC)PROC(w, glAccum);
  w->glGetBooleanv    = (OBOL_PFNGLGETBOOLEANVPROC)PROC(w, glGetBooleanv);
  w->glNewList        = (OBOL_PFNGLNEWLISTPROC)PROC(w, glNewList);
  w->glEndList        = (OBOL_PFNGLENDLISTPROC)PROC(w, glEndList);
  w->glCallList       = (OBOL_PFNGLCALLLISTPROC)PROC(w, glCallList);
  w->glDeleteLists    = (OBOL_PFNGLDELETELISTSPROC)PROC(w, glDeleteLists);
  w->glGenLists       = (OBOL_PFNGLGENLISTSPROC)PROC(w, glGenLists);
  w->glPushAttrib     = (OBOL_PFNGLPUSHATTRIBPROC)PROC(w, glPushAttrib);
  w->glPopAttrib      = (OBOL_PFNGLPOPATTRIBPROC)PROC(w, glPopAttrib);

  /* Additional PROC resolutions for full OSMesa GL 1.0-1.3 feature coverage. */
w->glAreTexturesResident = (OBOL_PFNGLARETEXTURESRESIDENTPROC)PROC(w, glAreTexturesResident);
  w->glBlendColor = (OBOL_PFNGLBLENDCOLORPROC)PROC(w, glBlendColor);
  w->glCallLists = (OBOL_PFNGLCALLLISTSPROC)PROC(w, glCallLists);
  w->glClearAccum = (OBOL_PFNGLCLEARACCUMPROC)PROC(w, glClearAccum);
  w->glClearDepth = (OBOL_PFNGLCLEARDEPTHPROC)PROC(w, glClearDepth);
  w->glColor3b = (OBOL_PFNGLCOLOR3BPROC)PROC(w, glColor3b);
  w->glColor3bv = (OBOL_PFNGLCOLOR3BVPROC)PROC(w, glColor3bv);
  w->glColor3d = (OBOL_PFNGLCOLOR3DPROC)PROC(w, glColor3d);
  w->glColor3dv = (OBOL_PFNGLCOLOR3DVPROC)PROC(w, glColor3dv);
  w->glColor3i = (OBOL_PFNGLCOLOR3IPROC)PROC(w, glColor3i);
  w->glColor3iv = (OBOL_PFNGLCOLOR3IVPROC)PROC(w, glColor3iv);
  w->glColor3s = (OBOL_PFNGLCOLOR3SPROC)PROC(w, glColor3s);
  w->glColor3sv = (OBOL_PFNGLCOLOR3SVPROC)PROC(w, glColor3sv);
  w->glColor3ui = (OBOL_PFNGLCOLOR3UIPROC)PROC(w, glColor3ui);
  w->glColor3uiv = (OBOL_PFNGLCOLOR3UIVPROC)PROC(w, glColor3uiv);
  w->glColor3us = (OBOL_PFNGLCOLOR3USPROC)PROC(w, glColor3us);
  w->glColor3usv = (OBOL_PFNGLCOLOR3USVPROC)PROC(w, glColor3usv);
  w->glColor4b = (OBOL_PFNGLCOLOR4BPROC)PROC(w, glColor4b);
  w->glColor4bv = (OBOL_PFNGLCOLOR4BVPROC)PROC(w, glColor4bv);
  w->glColor4d = (OBOL_PFNGLCOLOR4DPROC)PROC(w, glColor4d);
  w->glColor4dv = (OBOL_PFNGLCOLOR4DVPROC)PROC(w, glColor4dv);
  w->glColor4f = (OBOL_PFNGLCOLOR4FPROC)PROC(w, glColor4f);
  w->glColor4fv = (OBOL_PFNGLCOLOR4FVPROC)PROC(w, glColor4fv);
  w->glColor4i = (OBOL_PFNGLCOLOR4IPROC)PROC(w, glColor4i);
  w->glColor4iv = (OBOL_PFNGLCOLOR4IVPROC)PROC(w, glColor4iv);
  w->glColor4s = (OBOL_PFNGLCOLOR4SPROC)PROC(w, glColor4s);
  w->glColor4sv = (OBOL_PFNGLCOLOR4SVPROC)PROC(w, glColor4sv);
  w->glColor4ubv = (OBOL_PFNGLCOLOR4UBVPROC)PROC(w, glColor4ubv);
  w->glColor4ui = (OBOL_PFNGLCOLOR4UIPROC)PROC(w, glColor4ui);
  w->glColor4uiv = (OBOL_PFNGLCOLOR4UIVPROC)PROC(w, glColor4uiv);
  w->glColor4us = (OBOL_PFNGLCOLOR4USPROC)PROC(w, glColor4us);
  w->glColor4usv = (OBOL_PFNGLCOLOR4USVPROC)PROC(w, glColor4usv);
  w->glColorTableParameterfv = (OBOL_PFNGLCOLORTABLEPARAMETERFVPROC)PROC(w, glColorTableParameterfv);
  w->glColorTableParameteriv = (OBOL_PFNGLCOLORTABLEPARAMETERIVPROC)PROC(w, glColorTableParameteriv);
  w->glConvolutionFilter1D = (OBOL_PFNGLCONVOLUTIONFILTER1DPROC)PROC(w, glConvolutionFilter1D);
  w->glConvolutionFilter2D = (OBOL_PFNGLCONVOLUTIONFILTER2DPROC)PROC(w, glConvolutionFilter2D);
  w->glConvolutionParameterf = (OBOL_PFNGLCONVOLUTIONPARAMETERFPROC)PROC(w, glConvolutionParameterf);
  w->glConvolutionParameterfv = (OBOL_PFNGLCONVOLUTIONPARAMETERFVPROC)PROC(w, glConvolutionParameterfv);
  w->glConvolutionParameteri = (OBOL_PFNGLCONVOLUTIONPARAMETERIPROC)PROC(w, glConvolutionParameteri);
  w->glConvolutionParameteriv = (OBOL_PFNGLCONVOLUTIONPARAMETERIVPROC)PROC(w, glConvolutionParameteriv);
  w->glCopyColorSubTable = (OBOL_PFNGLCOPYCOLORSUBTABLEPROC)PROC(w, glCopyColorSubTable);
  w->glCopyColorTable = (OBOL_PFNGLCOPYCOLORTABLEPROC)PROC(w, glCopyColorTable);
  w->glCopyConvolutionFilter1D = (OBOL_PFNGLCOPYCONVOLUTIONFILTER1DPROC)PROC(w, glCopyConvolutionFilter1D);
  w->glCopyConvolutionFilter2D = (OBOL_PFNGLCOPYCONVOLUTIONFILTER2DPROC)PROC(w, glCopyConvolutionFilter2D);
  w->glCopyPixels = (OBOL_PFNGLCOPYPIXELSPROC)PROC(w, glCopyPixels);
  w->glCopyTexImage1D = (OBOL_PFNGLCOPYTEXIMAGE1DPROC)PROC(w, glCopyTexImage1D);
  w->glCopyTexSubImage1D = (OBOL_PFNGLCOPYTEXSUBIMAGE1DPROC)PROC(w, glCopyTexSubImage1D);
  w->glEdgeFlag = (OBOL_PFNGLEDGEFLAGPROC)PROC(w, glEdgeFlag);
  w->glEdgeFlagPointer = (OBOL_PFNGLEDGEFLAGPOINTERPROC)PROC(w, glEdgeFlagPointer);
  w->glEdgeFlagv = (OBOL_PFNGLEDGEFLAGVPROC)PROC(w, glEdgeFlagv);
  w->glEvalCoord1d = (OBOL_PFNGLEVALCOORD1DPROC)PROC(w, glEvalCoord1d);
  w->glEvalCoord1dv = (OBOL_PFNGLEVALCOORD1DVPROC)PROC(w, glEvalCoord1dv);
  w->glEvalCoord1f = (OBOL_PFNGLEVALCOORD1FPROC)PROC(w, glEvalCoord1f);
  w->glEvalCoord1fv = (OBOL_PFNGLEVALCOORD1FVPROC)PROC(w, glEvalCoord1fv);
  w->glEvalCoord2d = (OBOL_PFNGLEVALCOORD2DPROC)PROC(w, glEvalCoord2d);
  w->glEvalCoord2dv = (OBOL_PFNGLEVALCOORD2DVPROC)PROC(w, glEvalCoord2dv);
  w->glEvalCoord2f = (OBOL_PFNGLEVALCOORD2FPROC)PROC(w, glEvalCoord2f);
  w->glEvalCoord2fv = (OBOL_PFNGLEVALCOORD2FVPROC)PROC(w, glEvalCoord2fv);
  w->glEvalMesh1 = (OBOL_PFNGLEVALMESH1PROC)PROC(w, glEvalMesh1);
  w->glEvalMesh2 = (OBOL_PFNGLEVALMESH2PROC)PROC(w, glEvalMesh2);
  w->glEvalPoint1 = (OBOL_PFNGLEVALPOINT1PROC)PROC(w, glEvalPoint1);
  w->glEvalPoint2 = (OBOL_PFNGLEVALPOINT2PROC)PROC(w, glEvalPoint2);
  w->glFeedbackBuffer = (OBOL_PFNGLFEEDBACKBUFFERPROC)PROC(w, glFeedbackBuffer);
  w->glFogiv = (OBOL_PFNGLFOGIVPROC)PROC(w, glFogiv);
  w->glGetClipPlane = (OBOL_PFNGLGETCLIPPLANEPROC)PROC(w, glGetClipPlane);
  w->glGetConvolutionFilter = (OBOL_PFNGLGETCONVOLUTIONFILTERPROC)PROC(w, glGetConvolutionFilter);
  w->glGetConvolutionParameterfv = (OBOL_PFNGLGETCONVOLUTIONPARAMETERFVPROC)PROC(w, glGetConvolutionParameterfv);
  w->glGetConvolutionParameteriv = (OBOL_PFNGLGETCONVOLUTIONPARAMETERIVPROC)PROC(w, glGetConvolutionParameteriv);
  w->glGetDoublev = (OBOL_PFNGLGETDOUBLEVPROC)PROC(w, glGetDoublev);
  w->glGetHistogram = (OBOL_PFNGLGETHISTOGRAMPROC)PROC(w, glGetHistogram);
  w->glGetHistogramParameterfv = (OBOL_PFNGLGETHISTOGRAMPARAMETERFVPROC)PROC(w, glGetHistogramParameterfv);
  w->glGetHistogramParameteriv = (OBOL_PFNGLGETHISTOGRAMPARAMETERIVPROC)PROC(w, glGetHistogramParameteriv);
  w->glGetLightfv = (OBOL_PFNGLGETLIGHTFVPROC)PROC(w, glGetLightfv);
  w->glGetLightiv = (OBOL_PFNGLGETLIGHTIVPROC)PROC(w, glGetLightiv);
  w->glGetMapdv = (OBOL_PFNGLGETMAPDVPROC)PROC(w, glGetMapdv);
  w->glGetMapfv = (OBOL_PFNGLGETMAPFVPROC)PROC(w, glGetMapfv);
  w->glGetMapiv = (OBOL_PFNGLGETMAPIVPROC)PROC(w, glGetMapiv);
  w->glGetMaterialfv = (OBOL_PFNGLGETMATERIALFVPROC)PROC(w, glGetMaterialfv);
  w->glGetMaterialiv = (OBOL_PFNGLGETMATERIALIVPROC)PROC(w, glGetMaterialiv);
  w->glGetMinmax = (OBOL_PFNGLGETMINMAXPROC)PROC(w, glGetMinmax);
  w->glGetMinmaxParameterfv = (OBOL_PFNGLGETMINMAXPARAMETERFVPROC)PROC(w, glGetMinmaxParameterfv);
  w->glGetMinmaxParameteriv = (OBOL_PFNGLGETMINMAXPARAMETERIVPROC)PROC(w, glGetMinmaxParameteriv);
  w->glGetPixelMapfv = (OBOL_PFNGLGETPIXELMAPFVPROC)PROC(w, glGetPixelMapfv);
  w->glGetPixelMapuiv = (OBOL_PFNGLGETPIXELMAPUIVPROC)PROC(w, glGetPixelMapuiv);
  w->glGetPixelMapusv = (OBOL_PFNGLGETPIXELMAPUSVPROC)PROC(w, glGetPixelMapusv);
  w->glGetPointerv = (OBOL_PFNGLGETPOINTERVPROC)PROC(w, glGetPointerv);
  w->glGetPolygonStipple = (OBOL_PFNGLGETPOLYGONSTIPPLEPROC)PROC(w, glGetPolygonStipple);
  w->glGetSeparableFilter = (OBOL_PFNGLGETSEPARABLEFILTERPROC)PROC(w, glGetSeparableFilter);
  w->glGetTexEnvfv = (OBOL_PFNGLGETTEXENVFVPROC)PROC(w, glGetTexEnvfv);
  w->glGetTexEnviv = (OBOL_PFNGLGETTEXENVIVPROC)PROC(w, glGetTexEnviv);
  w->glGetTexGendv = (OBOL_PFNGLGETTEXGENDVPROC)PROC(w, glGetTexGendv);
  w->glGetTexGenfv = (OBOL_PFNGLGETTEXGENFVPROC)PROC(w, glGetTexGenfv);
  w->glGetTexGeniv = (OBOL_PFNGLGETTEXGENIVPROC)PROC(w, glGetTexGeniv);
  w->glGetTexImage = (OBOL_PFNGLGETTEXIMAGEPROC)PROC(w, glGetTexImage);
  w->glGetTexLevelParameterfv = (OBOL_PFNGLGETTEXLEVELPARAMETERFVPROC)PROC(w, glGetTexLevelParameterfv);
  w->glGetTexLevelParameteriv = (OBOL_PFNGLGETTEXLEVELPARAMETERIVPROC)PROC(w, glGetTexLevelParameteriv);
  w->glGetTexParameterfv = (OBOL_PFNGLGETTEXPARAMETERFVPROC)PROC(w, glGetTexParameterfv);
  w->glGetTexParameteriv = (OBOL_PFNGLGETTEXPARAMETERIVPROC)PROC(w, glGetTexParameteriv);
  w->glHint = (OBOL_PFNGLHINTPROC)PROC(w, glHint);
  w->glHistogram = (OBOL_PFNGLHISTOGRAMPROC)PROC(w, glHistogram);
  w->glIndexMask = (OBOL_PFNGLINDEXMASKPROC)PROC(w, glIndexMask);
  w->glIndexd = (OBOL_PFNGLINDEXDPROC)PROC(w, glIndexd);
  w->glIndexdv = (OBOL_PFNGLINDEXDVPROC)PROC(w, glIndexdv);
  w->glIndexf = (OBOL_PFNGLINDEXFPROC)PROC(w, glIndexf);
  w->glIndexfv = (OBOL_PFNGLINDEXFVPROC)PROC(w, glIndexfv);
  w->glIndexiv = (OBOL_PFNGLINDEXIVPROC)PROC(w, glIndexiv);
  w->glIndexs = (OBOL_PFNGLINDEXSPROC)PROC(w, glIndexs);
  w->glIndexsv = (OBOL_PFNGLINDEXSVPROC)PROC(w, glIndexsv);
  w->glIndexub = (OBOL_PFNGLINDEXUBPROC)PROC(w, glIndexub);
  w->glIndexubv = (OBOL_PFNGLINDEXUBVPROC)PROC(w, glIndexubv);
  w->glInitNames = (OBOL_PFNGLINITNAMESPROC)PROC(w, glInitNames);
  w->glIsList = (OBOL_PFNGLISLISTPROC)PROC(w, glIsList);
  w->glIsTexture = (OBOL_PFNGLISTEXTUREPROC)PROC(w, glIsTexture);
  w->glLightModelf = (OBOL_PFNGLLIGHTMODELFPROC)PROC(w, glLightModelf);
  w->glLightModeliv = (OBOL_PFNGLLIGHTMODELIVPROC)PROC(w, glLightModeliv);
  w->glLighti = (OBOL_PFNGLLIGHTIPROC)PROC(w, glLighti);
  w->glLightiv = (OBOL_PFNGLLIGHTIVPROC)PROC(w, glLightiv);
  w->glListBase = (OBOL_PFNGLLISTBASEPROC)PROC(w, glListBase);
  w->glLoadName = (OBOL_PFNGLLOADNAMEPROC)PROC(w, glLoadName);
  w->glLoadTransposeMatrixd = (OBOL_PFNGLLOADTRANSPOSEMATRIXDPROC)PROC(w, glLoadTransposeMatrixd);
  w->glLoadTransposeMatrixf = (OBOL_PFNGLLOADTRANSPOSEMATRIXFPROC)PROC(w, glLoadTransposeMatrixf);
  w->glLogicOp = (OBOL_PFNGLLOGICOPPROC)PROC(w, glLogicOp);
  w->glMap1d = (OBOL_PFNGLMAP1DPROC)PROC(w, glMap1d);
  w->glMap1f = (OBOL_PFNGLMAP1FPROC)PROC(w, glMap1f);
  w->glMap2d = (OBOL_PFNGLMAP2DPROC)PROC(w, glMap2d);
  w->glMap2f = (OBOL_PFNGLMAP2FPROC)PROC(w, glMap2f);
  w->glMapGrid1d = (OBOL_PFNGLMAPGRID1DPROC)PROC(w, glMapGrid1d);
  w->glMapGrid1f = (OBOL_PFNGLMAPGRID1FPROC)PROC(w, glMapGrid1f);
  w->glMapGrid2d = (OBOL_PFNGLMAPGRID2DPROC)PROC(w, glMapGrid2d);
  w->glMapGrid2f = (OBOL_PFNGLMAPGRID2FPROC)PROC(w, glMapGrid2f);
  w->glMateriali = (OBOL_PFNGLMATERIALIPROC)PROC(w, glMateriali);
  w->glMaterialiv = (OBOL_PFNGLMATERIALIVPROC)PROC(w, glMaterialiv);
  w->glMinmax = (OBOL_PFNGLMINMAXPROC)PROC(w, glMinmax);
  w->glMultMatrixd = (OBOL_PFNGLMULTMATRIXDPROC)PROC(w, glMultMatrixd);
  w->glMultTransposeMatrixd = (OBOL_PFNGLMULTTRANSPOSEMATRIXDPROC)PROC(w, glMultTransposeMatrixd);
  w->glMultTransposeMatrixf = (OBOL_PFNGLMULTTRANSPOSEMATRIXFPROC)PROC(w, glMultTransposeMatrixf);
  w->glMultiTexCoord1d = (OBOL_PFNGLMULTITEXCOORD1DPROC)PROC(w, glMultiTexCoord1d);
  w->glMultiTexCoord1dv = (OBOL_PFNGLMULTITEXCOORD1DVPROC)PROC(w, glMultiTexCoord1dv);
  w->glMultiTexCoord1f = (OBOL_PFNGLMULTITEXCOORD1FPROC)PROC(w, glMultiTexCoord1f);
  w->glMultiTexCoord1fv = (OBOL_PFNGLMULTITEXCOORD1FVPROC)PROC(w, glMultiTexCoord1fv);
  w->glMultiTexCoord1i = (OBOL_PFNGLMULTITEXCOORD1IPROC)PROC(w, glMultiTexCoord1i);
  w->glMultiTexCoord1iv = (OBOL_PFNGLMULTITEXCOORD1IVPROC)PROC(w, glMultiTexCoord1iv);
  w->glMultiTexCoord1s = (OBOL_PFNGLMULTITEXCOORD1SPROC)PROC(w, glMultiTexCoord1s);
  w->glMultiTexCoord1sv = (OBOL_PFNGLMULTITEXCOORD1SVPROC)PROC(w, glMultiTexCoord1sv);
  w->glMultiTexCoord2d = (OBOL_PFNGLMULTITEXCOORD2DPROC)PROC(w, glMultiTexCoord2d);
  w->glMultiTexCoord2dv = (OBOL_PFNGLMULTITEXCOORD2DVPROC)PROC(w, glMultiTexCoord2dv);
  w->glMultiTexCoord2i = (OBOL_PFNGLMULTITEXCOORD2IPROC)PROC(w, glMultiTexCoord2i);
  w->glMultiTexCoord2iv = (OBOL_PFNGLMULTITEXCOORD2IVPROC)PROC(w, glMultiTexCoord2iv);
  w->glMultiTexCoord2s = (OBOL_PFNGLMULTITEXCOORD2SPROC)PROC(w, glMultiTexCoord2s);
  w->glMultiTexCoord2sv = (OBOL_PFNGLMULTITEXCOORD2SVPROC)PROC(w, glMultiTexCoord2sv);
  w->glMultiTexCoord3d = (OBOL_PFNGLMULTITEXCOORD3DPROC)PROC(w, glMultiTexCoord3d);
  w->glMultiTexCoord3dv = (OBOL_PFNGLMULTITEXCOORD3DVPROC)PROC(w, glMultiTexCoord3dv);
  w->glMultiTexCoord3f = (OBOL_PFNGLMULTITEXCOORD3FPROC)PROC(w, glMultiTexCoord3f);
  w->glMultiTexCoord3i = (OBOL_PFNGLMULTITEXCOORD3IPROC)PROC(w, glMultiTexCoord3i);
  w->glMultiTexCoord3iv = (OBOL_PFNGLMULTITEXCOORD3IVPROC)PROC(w, glMultiTexCoord3iv);
  w->glMultiTexCoord3s = (OBOL_PFNGLMULTITEXCOORD3SPROC)PROC(w, glMultiTexCoord3s);
  w->glMultiTexCoord3sv = (OBOL_PFNGLMULTITEXCOORD3SVPROC)PROC(w, glMultiTexCoord3sv);
  w->glMultiTexCoord4d = (OBOL_PFNGLMULTITEXCOORD4DPROC)PROC(w, glMultiTexCoord4d);
  w->glMultiTexCoord4dv = (OBOL_PFNGLMULTITEXCOORD4DVPROC)PROC(w, glMultiTexCoord4dv);
  w->glMultiTexCoord4f = (OBOL_PFNGLMULTITEXCOORD4FPROC)PROC(w, glMultiTexCoord4f);
  w->glMultiTexCoord4i = (OBOL_PFNGLMULTITEXCOORD4IPROC)PROC(w, glMultiTexCoord4i);
  w->glMultiTexCoord4iv = (OBOL_PFNGLMULTITEXCOORD4IVPROC)PROC(w, glMultiTexCoord4iv);
  w->glMultiTexCoord4s = (OBOL_PFNGLMULTITEXCOORD4SPROC)PROC(w, glMultiTexCoord4s);
  w->glMultiTexCoord4sv = (OBOL_PFNGLMULTITEXCOORD4SVPROC)PROC(w, glMultiTexCoord4sv);
  w->glNormal3b = (OBOL_PFNGLNORMAL3BPROC)PROC(w, glNormal3b);
  w->glNormal3bv = (OBOL_PFNGLNORMAL3BVPROC)PROC(w, glNormal3bv);
  w->glNormal3d = (OBOL_PFNGLNORMAL3DPROC)PROC(w, glNormal3d);
  w->glNormal3dv = (OBOL_PFNGLNORMAL3DVPROC)PROC(w, glNormal3dv);
  w->glNormal3i = (OBOL_PFNGLNORMAL3IPROC)PROC(w, glNormal3i);
  w->glNormal3iv = (OBOL_PFNGLNORMAL3IVPROC)PROC(w, glNormal3iv);
  w->glNormal3s = (OBOL_PFNGLNORMAL3SPROC)PROC(w, glNormal3s);
  w->glNormal3sv = (OBOL_PFNGLNORMAL3SVPROC)PROC(w, glNormal3sv);
  w->glPassThrough = (OBOL_PFNGLPASSTHROUGHPROC)PROC(w, glPassThrough);
  w->glPixelMapusv = (OBOL_PFNGLPIXELMAPUSVPROC)PROC(w, glPixelMapusv);
  w->glPixelStoref = (OBOL_PFNGLPIXELSTOREFPROC)PROC(w, glPixelStoref);
  w->glPopName = (OBOL_PFNGLPOPNAMEPROC)PROC(w, glPopName);
  w->glPrioritizeTextures = (OBOL_PFNGLPRIORITIZETEXTURESPROC)PROC(w, glPrioritizeTextures);
  w->glPushName = (OBOL_PFNGLPUSHNAMEPROC)PROC(w, glPushName);
  w->glRasterPos2d = (OBOL_PFNGLRASTERPOS2DPROC)PROC(w, glRasterPos2d);
  w->glRasterPos2dv = (OBOL_PFNGLRASTERPOS2DVPROC)PROC(w, glRasterPos2dv);
  w->glRasterPos2fv = (OBOL_PFNGLRASTERPOS2FVPROC)PROC(w, glRasterPos2fv);
  w->glRasterPos2i = (OBOL_PFNGLRASTERPOS2IPROC)PROC(w, glRasterPos2i);
  w->glRasterPos2iv = (OBOL_PFNGLRASTERPOS2IVPROC)PROC(w, glRasterPos2iv);
  w->glRasterPos2s = (OBOL_PFNGLRASTERPOS2SPROC)PROC(w, glRasterPos2s);
  w->glRasterPos2sv = (OBOL_PFNGLRASTERPOS2SVPROC)PROC(w, glRasterPos2sv);
  w->glRasterPos3d = (OBOL_PFNGLRASTERPOS3DPROC)PROC(w, glRasterPos3d);
  w->glRasterPos3dv = (OBOL_PFNGLRASTERPOS3DVPROC)PROC(w, glRasterPos3dv);
  w->glRasterPos3fv = (OBOL_PFNGLRASTERPOS3FVPROC)PROC(w, glRasterPos3fv);
  w->glRasterPos3i = (OBOL_PFNGLRASTERPOS3IPROC)PROC(w, glRasterPos3i);
  w->glRasterPos3iv = (OBOL_PFNGLRASTERPOS3IVPROC)PROC(w, glRasterPos3iv);
  w->glRasterPos3s = (OBOL_PFNGLRASTERPOS3SPROC)PROC(w, glRasterPos3s);
  w->glRasterPos3sv = (OBOL_PFNGLRASTERPOS3SVPROC)PROC(w, glRasterPos3sv);
  w->glRasterPos4d = (OBOL_PFNGLRASTERPOS4DPROC)PROC(w, glRasterPos4d);
  w->glRasterPos4dv = (OBOL_PFNGLRASTERPOS4DVPROC)PROC(w, glRasterPos4dv);
  w->glRasterPos4f = (OBOL_PFNGLRASTERPOS4FPROC)PROC(w, glRasterPos4f);
  w->glRasterPos4fv = (OBOL_PFNGLRASTERPOS4FVPROC)PROC(w, glRasterPos4fv);
  w->glRasterPos4i = (OBOL_PFNGLRASTERPOS4IPROC)PROC(w, glRasterPos4i);
  w->glRasterPos4iv = (OBOL_PFNGLRASTERPOS4IVPROC)PROC(w, glRasterPos4iv);
  w->glRasterPos4s = (OBOL_PFNGLRASTERPOS4SPROC)PROC(w, glRasterPos4s);
  w->glRasterPos4sv = (OBOL_PFNGLRASTERPOS4SVPROC)PROC(w, glRasterPos4sv);
  w->glReadBuffer = (OBOL_PFNGLREADBUFFERPROC)PROC(w, glReadBuffer);
  w->glRectd = (OBOL_PFNGLRECTDPROC)PROC(w, glRectd);
  w->glRectdv = (OBOL_PFNGLRECTDVPROC)PROC(w, glRectdv);
  w->glRectf = (OBOL_PFNGLRECTFPROC)PROC(w, glRectf);
  w->glRectfv = (OBOL_PFNGLRECTFVPROC)PROC(w, glRectfv);
  w->glRecti = (OBOL_PFNGLRECTIPROC)PROC(w, glRecti);
  w->glRectiv = (OBOL_PFNGLRECTIVPROC)PROC(w, glRectiv);
  w->glRects = (OBOL_PFNGLRECTSPROC)PROC(w, glRects);
  w->glRectsv = (OBOL_PFNGLRECTSVPROC)PROC(w, glRectsv);
  w->glRenderMode = (OBOL_PFNGLRENDERMODEPROC)PROC(w, glRenderMode);
  w->glResetHistogram = (OBOL_PFNGLRESETHISTOGRAMPROC)PROC(w, glResetHistogram);
  w->glResetMinmax = (OBOL_PFNGLRESETMINMAXPROC)PROC(w, glResetMinmax);
  w->glRotated = (OBOL_PFNGLROTATEDPROC)PROC(w, glRotated);
  w->glSampleCoverage = (OBOL_PFNGLSAMPLECOVERAGEPROC)PROC(w, glSampleCoverage);
  w->glScaled = (OBOL_PFNGLSCALEDPROC)PROC(w, glScaled);
  w->glSelectBuffer = (OBOL_PFNGLSELECTBUFFERPROC)PROC(w, glSelectBuffer);
  w->glSeparableFilter2D = (OBOL_PFNGLSEPARABLEFILTER2DPROC)PROC(w, glSeparableFilter2D);
  w->glShadeModel = (OBOL_PFNGLSHADEMODELPROC)PROC(w, glShadeModel);
  w->glStencilMask = (OBOL_PFNGLSTENCILMASKPROC)PROC(w, glStencilMask);
  w->glTexCoord1d = (OBOL_PFNGLTEXCOORD1DPROC)PROC(w, glTexCoord1d);
  w->glTexCoord1dv = (OBOL_PFNGLTEXCOORD1DVPROC)PROC(w, glTexCoord1dv);
  w->glTexCoord1f = (OBOL_PFNGLTEXCOORD1FPROC)PROC(w, glTexCoord1f);
  w->glTexCoord1fv = (OBOL_PFNGLTEXCOORD1FVPROC)PROC(w, glTexCoord1fv);
  w->glTexCoord1i = (OBOL_PFNGLTEXCOORD1IPROC)PROC(w, glTexCoord1i);
  w->glTexCoord1iv = (OBOL_PFNGLTEXCOORD1IVPROC)PROC(w, glTexCoord1iv);
  w->glTexCoord1s = (OBOL_PFNGLTEXCOORD1SPROC)PROC(w, glTexCoord1s);
  w->glTexCoord1sv = (OBOL_PFNGLTEXCOORD1SVPROC)PROC(w, glTexCoord1sv);
  w->glTexCoord2d = (OBOL_PFNGLTEXCOORD2DPROC)PROC(w, glTexCoord2d);
  w->glTexCoord2dv = (OBOL_PFNGLTEXCOORD2DVPROC)PROC(w, glTexCoord2dv);
  w->glTexCoord2i = (OBOL_PFNGLTEXCOORD2IPROC)PROC(w, glTexCoord2i);
  w->glTexCoord2iv = (OBOL_PFNGLTEXCOORD2IVPROC)PROC(w, glTexCoord2iv);
  w->glTexCoord2s = (OBOL_PFNGLTEXCOORD2SPROC)PROC(w, glTexCoord2s);
  w->glTexCoord2sv = (OBOL_PFNGLTEXCOORD2SVPROC)PROC(w, glTexCoord2sv);
  w->glTexCoord3d = (OBOL_PFNGLTEXCOORD3DPROC)PROC(w, glTexCoord3d);
  w->glTexCoord3dv = (OBOL_PFNGLTEXCOORD3DVPROC)PROC(w, glTexCoord3dv);
  w->glTexCoord3i = (OBOL_PFNGLTEXCOORD3IPROC)PROC(w, glTexCoord3i);
  w->glTexCoord3iv = (OBOL_PFNGLTEXCOORD3IVPROC)PROC(w, glTexCoord3iv);
  w->glTexCoord3s = (OBOL_PFNGLTEXCOORD3SPROC)PROC(w, glTexCoord3s);
  w->glTexCoord3sv = (OBOL_PFNGLTEXCOORD3SVPROC)PROC(w, glTexCoord3sv);
  w->glTexCoord4d = (OBOL_PFNGLTEXCOORD4DPROC)PROC(w, glTexCoord4d);
  w->glTexCoord4dv = (OBOL_PFNGLTEXCOORD4DVPROC)PROC(w, glTexCoord4dv);
  w->glTexCoord4f = (OBOL_PFNGLTEXCOORD4FPROC)PROC(w, glTexCoord4f);
  w->glTexCoord4i = (OBOL_PFNGLTEXCOORD4IPROC)PROC(w, glTexCoord4i);
  w->glTexCoord4iv = (OBOL_PFNGLTEXCOORD4IVPROC)PROC(w, glTexCoord4iv);
  w->glTexCoord4s = (OBOL_PFNGLTEXCOORD4SPROC)PROC(w, glTexCoord4s);
  w->glTexCoord4sv = (OBOL_PFNGLTEXCOORD4SVPROC)PROC(w, glTexCoord4sv);
  w->glTexEnviv = (OBOL_PFNGLTEXENVIVPROC)PROC(w, glTexEnviv);
  w->glTexGend = (OBOL_PFNGLTEXGENDPROC)PROC(w, glTexGend);
  w->glTexGendv = (OBOL_PFNGLTEXGENDVPROC)PROC(w, glTexGendv);
  w->glTexGeniv = (OBOL_PFNGLTEXGENIVPROC)PROC(w, glTexGeniv);
  w->glTexImage1D = (OBOL_PFNGLTEXIMAGE1DPROC)PROC(w, glTexImage1D);
  w->glTexParameterfv = (OBOL_PFNGLTEXPARAMETERFVPROC)PROC(w, glTexParameterfv);
  w->glTexParameteriv = (OBOL_PFNGLTEXPARAMETERIVPROC)PROC(w, glTexParameteriv);
  w->glTexSubImage1D = (OBOL_PFNGLTEXSUBIMAGE1DPROC)PROC(w, glTexSubImage1D);
  w->glTranslated = (OBOL_PFNGLTRANSLATEDPROC)PROC(w, glTranslated);
  w->glVertex2d = (OBOL_PFNGLVERTEX2DPROC)PROC(w, glVertex2d);
  w->glVertex2dv = (OBOL_PFNGLVERTEX2DVPROC)PROC(w, glVertex2dv);
  w->glVertex2fv = (OBOL_PFNGLVERTEX2FVPROC)PROC(w, glVertex2fv);
  w->glVertex2i = (OBOL_PFNGLVERTEX2IPROC)PROC(w, glVertex2i);
  w->glVertex2iv = (OBOL_PFNGLVERTEX2IVPROC)PROC(w, glVertex2iv);
  w->glVertex2sv = (OBOL_PFNGLVERTEX2SVPROC)PROC(w, glVertex2sv);
  w->glVertex3d = (OBOL_PFNGLVERTEX3DPROC)PROC(w, glVertex3d);
  w->glVertex3dv = (OBOL_PFNGLVERTEX3DVPROC)PROC(w, glVertex3dv);
  w->glVertex3i = (OBOL_PFNGLVERTEX3IPROC)PROC(w, glVertex3i);
  w->glVertex3iv = (OBOL_PFNGLVERTEX3IVPROC)PROC(w, glVertex3iv);
  w->glVertex3s = (OBOL_PFNGLVERTEX3SPROC)PROC(w, glVertex3s);
  w->glVertex3sv = (OBOL_PFNGLVERTEX3SVPROC)PROC(w, glVertex3sv);
  w->glVertex4d = (OBOL_PFNGLVERTEX4DPROC)PROC(w, glVertex4d);
  w->glVertex4dv = (OBOL_PFNGLVERTEX4DVPROC)PROC(w, glVertex4dv);
  w->glVertex4f = (OBOL_PFNGLVERTEX4FPROC)PROC(w, glVertex4f);
  w->glVertex4i = (OBOL_PFNGLVERTEX4IPROC)PROC(w, glVertex4i);
  w->glVertex4iv = (OBOL_PFNGLVERTEX4IVPROC)PROC(w, glVertex4iv);
  w->glVertex4s = (OBOL_PFNGLVERTEX4SPROC)PROC(w, glVertex4s);
  w->glVertex4sv = (OBOL_PFNGLVERTEX4SVPROC)PROC(w, glVertex4sv);

  /* Appeared in OpenGL v1.1. */
  w->glGenTextures = NULL;
  w->glBindTexture = NULL;
  w->glDeleteTextures = NULL;
#ifdef GL_VERSION_1_1
  if (SoGLContext_glversion_matches_at_least(w, 1, 1, 0)) {
    w->glGenTextures = (OBOL_PFNGLGENTEXTURESPROC)PROC(w, glGenTextures);
    w->glBindTexture = (OBOL_PFNGLBINDTEXTUREPROC)PROC(w, glBindTexture);
    w->glDeleteTextures = (OBOL_PFNGLDELETETEXTURESPROC)PROC(w, glDeleteTextures);
  }
#endif /* GL_VERSION_1_1 */
#ifdef GL_EXT_texture_object
  if (!w->glGenTextures && SoGLContext_glext_supported(w, "GL_EXT_texture_object")) {
    w->glGenTextures = (OBOL_PFNGLGENTEXTURESPROC)PROC(w, glGenTexturesEXT);
    w->glBindTexture = (OBOL_PFNGLBINDTEXTUREPROC)PROC(w, glBindTextureEXT);
    w->glDeleteTextures = (OBOL_PFNGLDELETETEXTURESPROC)PROC(w, glDeleteTexturesEXT);
  }
#endif /* GL_EXT_texture_object */

  /* Appeared in OpenGL v1.1. */
  w->glTexSubImage2D = NULL;
#ifdef GL_VERSION_1_1
  if (SoGLContext_glversion_matches_at_least(w, 1, 1, 0)) {
    w->glTexSubImage2D = (OBOL_PFNGLTEXSUBIMAGE2DPROC)PROC(w, glTexSubImage2D);
  }
#endif /* GL_VERSION_1_1 */
#ifdef GL_EXT_subtexture
  if (!w->glTexSubImage2D && SoGLContext_glext_supported(w, "GL_EXT_subtexture")) {
    w->glTexSubImage2D = (OBOL_PFNGLTEXSUBIMAGE2DPROC)PROC(w, glTexSubImage2DEXT);
  }
#endif /* GL_EXT_subtexture */

  /* Appeared in OpenGL 1.1 */
  w->glPushClientAttrib = NULL;
  w->glPopClientAttrib = NULL;
#ifdef GL_VERSION_1_1
  if (SoGLContext_glversion_matches_at_least(w, 1, 1, 0)) {
    w->glPushClientAttrib = (OBOL_PFNGLPUSHCLIENTATTRIBPROC) PROC(w, glPushClientAttrib);
    w->glPopClientAttrib = (OBOL_PFNGLPOPCLIENTATTRIBPROC) PROC(w, glPopClientAttrib);
  }
#endif /* GL_VERSION_1_1 */

  /* These were introduced with OpenGL v1.2. */
  w->glTexImage3D = NULL;
  w->glCopyTexSubImage3D = NULL;
  w->glTexSubImage3D = NULL;
#ifdef GL_VERSION_1_2
  if (SoGLContext_glversion_matches_at_least(w, 1, 2, 0)) {
    w->glTexImage3D = (OBOL_PFNGLTEXIMAGE3DPROC)PROC(w, glTexImage3D);
    w->glCopyTexSubImage3D = (OBOL_PFNGLCOPYTEXSUBIMAGE3DPROC)PROC(w, glCopyTexSubImage3D);
    w->glTexSubImage3D = (OBOL_PFNGLTEXSUBIMAGE3DPROC)PROC(w, glTexSubImage3D);
  }
#endif /* GL_VERSION_1_2 */
#ifdef GL_EXT_texture3D
  if (!w->glTexImage3D && SoGLContext_glext_supported(w, "GL_EXT_texture3D")) {
    w->glTexImage3D = (OBOL_PFNGLTEXIMAGE3DPROC)PROC(w, glTexImage3DEXT);
    /* These are implicitly given if GL_EXT_texture3D is defined. */
    w->glCopyTexSubImage3D = (OBOL_PFNGLCOPYTEXSUBIMAGE3DPROC)PROC(w, glCopyTexSubImage3DEXT);
    w->glTexSubImage3D = (OBOL_PFNGLTEXSUBIMAGE3DPROC)PROC(w, glTexSubImage3DEXT);
  }
#endif /* GL_EXT_texture3D */

  /* Multi-texturing appeared in OpenGL v1.3, or with the
     GL_ARB_multitexture extension before that.
  */
  /*
     FIXME: we've found a bug prevalent in drivers for the "Intel
     Solano" graphics chipset / driver. It manifests itself in the way
     that visual artifacts are seen when multi-textured polygons are
     partially outside the canvas view.

     The SoGuiExamples/nodes/textureunit example can be used to
     reproduce the error. The driver info from one confirmed affected
     system is as follows:

     GL_VERSION == 1.1.2 - Build 4.13.01.3196
     GL_VENDOR == Intel
     GL_RENDERER == Intel Solano
     GL_EXTENSIONS = GL_ARB_multitexture [...]

     This problem is not yet handled in any way by Coin. What we
     should do about this is to detect the above chipset / driver and
     issue an on-screen warning to the end user (in very
     "end-user-friendly" terms) when multi-texturing is first
     attempted used, *plus* make a wgl- or glut-based example which
     demonstrates the bug, for reporting to Intel.

     The bug was tested and confirmed with the latest Intel Solano
     driver as of today.

     20041108 mortene, based on information provided by handegar.
  */
  w->glActiveTexture = NULL;
  w->glClientActiveTexture = NULL;
  w->glMultiTexCoord2f = NULL;
  w->glMultiTexCoord2fv = NULL;
  w->glMultiTexCoord3fv = NULL;
  w->glMultiTexCoord4fv = NULL;
#ifdef GL_VERSION_1_3
  if (SoGLContext_glversion_matches_at_least(w, 1, 3, 0)) {
    w->glActiveTexture = (OBOL_PFNGLACTIVETEXTUREPROC)PROC(w, glActiveTexture);
    w->glClientActiveTexture = (OBOL_PFNGLCLIENTACTIVETEXTUREPROC)PROC(w, glClientActiveTexture);
    w->glMultiTexCoord2f = (OBOL_PFNGLMULTITEXCOORD2FPROC)PROC(w, glMultiTexCoord2f);
    w->glMultiTexCoord2fv = (OBOL_PFNGLMULTITEXCOORD2FVPROC)PROC(w, glMultiTexCoord2fv);
    w->glMultiTexCoord3fv = (OBOL_PFNGLMULTITEXCOORD3FVPROC)PROC(w, glMultiTexCoord3fv);
    w->glMultiTexCoord4fv = (OBOL_PFNGLMULTITEXCOORD4FVPROC)PROC(w, glMultiTexCoord4fv);
  }
#endif /* GL_VERSION_1_3 */
#ifdef GL_ARB_multitexture
  if (!w->glActiveTexture && SoGLContext_glext_supported(w, "GL_ARB_multitexture")) {
    w->glActiveTexture = (OBOL_PFNGLACTIVETEXTUREPROC)PROC(w, glActiveTextureARB);
    w->glClientActiveTexture = (OBOL_PFNGLACTIVETEXTUREPROC)PROC(w, glClientActiveTextureARB);
    w->glMultiTexCoord2f = (OBOL_PFNGLMULTITEXCOORD2FPROC)PROC(w, glMultiTexCoord2fARB);
    w->glMultiTexCoord2fv = (OBOL_PFNGLMULTITEXCOORD2FVPROC)PROC(w, glMultiTexCoord2fvARB);
    w->glMultiTexCoord3fv = (OBOL_PFNGLMULTITEXCOORD3FVPROC)PROC(w, glMultiTexCoord3fvARB);
    w->glMultiTexCoord4fv = (OBOL_PFNGLMULTITEXCOORD4FVPROC)PROC(w, glMultiTexCoord4fvARB);
  }
#endif /* GL_ARB_multitexture */

  if (w->glActiveTexture) {
    if (!w->glClientActiveTexture ||
        !w->glMultiTexCoord2f ||
        !w->glMultiTexCoord2fv ||
        !w->glMultiTexCoord3fv ||
        !w->glMultiTexCoord4fv) {
      w->glActiveTexture = NULL; /* SoGLContext_has_multitexture() will return FALSE */
      if (OBOL_DEBUG || SoGLContext_debug()) {
        cc_debugerror_postwarning("glglue_init",
                                  "glActiveTexture found, but one or more of the other "
                                  "multitexture functions were not found");
      }
    }
  }
  w->maxtextureunits = 1; /* when multitexturing is not available */
  if (w->glActiveTexture) {
    /* GL_MAX_TEXTURE_COORDS_ARB (== GL_MAX_TEXTURE_COORDS, 0x8871) reports
       the number of texture coordinate units available for glMultiTexCoord*.
       Some implementations (including older Mesa/OSMesa builds) only accept
       this query when GL_ARB_fragment_program or GL_NV_fragment_program is
       present, even though it is a core GL 2.0 token.  Initialise tmp to 0
       so that we can detect a silent failure (GL_INVALID_ENUM without the
       extension) and fall back to GL_MAX_TEXTURE_UNITS_ARB which is always
       available whenever ARB_multitexture is supported. */
    GLint tmp = 0;
    glGetIntegerv(GL_MAX_TEXTURE_COORDS_ARB, &tmp);
    if (tmp < 1) {
      /* Clear any GL_INVALID_ENUM error raised by the failed query, then
         fall back to GL_MAX_TEXTURE_UNITS_ARB (requires ARB_multitexture). */
      while (glGetError() != GL_NO_ERROR) { }
      glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &tmp);
    }
    w->maxtextureunits = (int) tmp;
  }

  w->glCompressedTexImage1D = NULL;
  w->glCompressedTexImage2D = NULL;
  w->glCompressedTexImage3D = NULL;
  w->glCompressedTexSubImage1D = NULL;
  w->glCompressedTexSubImage2D = NULL;
  w->glCompressedTexSubImage3D = NULL;
  w->glGetCompressedTexImage = NULL;

#ifdef GL_VERSION_1_3
  if (SoGLContext_glversion_matches_at_least(w, 1, 3, 0)) {
    w->glCompressedTexImage1D = (OBOL_PFNGLCOMPRESSEDTEXIMAGE1DPROC)PROC(w, glCompressedTexImage1D);
    w->glCompressedTexImage2D = (OBOL_PFNGLCOMPRESSEDTEXIMAGE2DPROC)PROC(w, glCompressedTexImage2D);
    w->glCompressedTexImage3D = (OBOL_PFNGLCOMPRESSEDTEXIMAGE3DPROC)PROC(w, glCompressedTexImage3D);
    w->glCompressedTexSubImage1D = (OBOL_PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC)PROC(w, glCompressedTexSubImage1D);
    w->glCompressedTexSubImage2D = (OBOL_PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC)PROC(w, glCompressedTexSubImage2D);
    w->glCompressedTexSubImage3D = (OBOL_PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC)PROC(w, glCompressedTexSubImage3D);
    w->glGetCompressedTexImage = (OBOL_PFNGLGETCOMPRESSEDTEXIMAGEPROC)PROC(w, glGetCompressedTexImage);
  }
#endif /* GL_VERSION_1_3 */

#ifdef GL_ARB_texture_compression
  if ((w->glCompressedTexImage1D == NULL) &&
      SoGLContext_glext_supported(w, "GL_ARB_texture_compression")) {
    w->glCompressedTexImage1D = (OBOL_PFNGLCOMPRESSEDTEXIMAGE1DPROC)PROC(w, glCompressedTexImage1DARB);
    w->glCompressedTexImage2D = (OBOL_PFNGLCOMPRESSEDTEXIMAGE2DPROC)PROC(w, glCompressedTexImage2DARB);
    w->glCompressedTexImage3D = (OBOL_PFNGLCOMPRESSEDTEXIMAGE3DPROC)PROC(w, glCompressedTexImage3DARB);
    w->glCompressedTexSubImage1D = (OBOL_PFNGLCOMPRESSEDTEXSUBIMAGE1DPROC)PROC(w, glCompressedTexSubImage1DARB);
    w->glCompressedTexSubImage2D = (OBOL_PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC)PROC(w, glCompressedTexSubImage2DARB);
    w->glCompressedTexSubImage3D = (OBOL_PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC)PROC(w, glCompressedTexSubImage3DARB);
    w->glGetCompressedTexImage = (OBOL_PFNGLGETCOMPRESSEDTEXIMAGEPROC)PROC(w, glGetCompressedTexImageARB);
  }
#endif /* GL_ARB_texture_compression */

  w->glColorTable = NULL;
  w->glColorSubTable = NULL;
  w->glGetColorTable = NULL;
  w->glGetColorTableParameteriv = NULL;
  w->glGetColorTableParameterfv = NULL;

#if defined(GL_VERSION_1_2) && defined(GL_ARB_imaging)
  if (SoGLContext_glversion_matches_at_least(w, 1, 2, 0) &&
      SoGLContext_glext_supported(w, "GL_ARB_imaging")) {
    w->glColorTable = (OBOL_PFNGLCOLORTABLEPROC)PROC(w, glColorTable);
    w->glColorSubTable = (OBOL_PFNGLCOLORSUBTABLEPROC)PROC(w, glColorSubTable);
    w->glGetColorTable = (OBOL_PFNGLGETCOLORTABLEPROC)PROC(w, glGetColorTable);
    w->glGetColorTableParameteriv = (OBOL_PFNGLGETCOLORTABLEPARAMETERIVPROC)PROC(w, glGetColorTableParameteriv);
    w->glGetColorTableParameterfv = (OBOL_PFNGLGETCOLORTABLEPARAMETERFVPROC)PROC(w, glGetColorTableParameterfv);
  }
#endif /* GL_VERSION_1_2 && GL_ARB_imaging */

#if defined(GL_EXT_color_table)
  if ((w->glColorTable == NULL) &&
      SoGLContext_glext_supported(w, "GL_EXT_color_table")) {
    w->glColorTable = (OBOL_PFNGLCOLORTABLEPROC)PROC(w, glColorTableEXT);
    w->glGetColorTable = (OBOL_PFNGLGETCOLORTABLEPROC)PROC(w, glGetColorTableEXT);
    w->glGetColorTableParameteriv = (OBOL_PFNGLGETCOLORTABLEPARAMETERIVPROC)PROC(w, glGetColorTableParameterivEXT);
    w->glGetColorTableParameterfv = (OBOL_PFNGLGETCOLORTABLEPARAMETERFVPROC)PROC(w, glGetColorTableParameterfvEXT);
  }
#endif /* GL_EXT_color_table */

#if defined(GL_SGI_color_table)
  if ((w->glColorTable == NULL) &&
      SoGLContext_glext_supported(w, "GL_SGI_color_table")) {
    w->glColorTable = (OBOL_PFNGLCOLORTABLEPROC)PROC(w, glColorTableSGI);
    w->glGetColorTable = (OBOL_PFNGLGETCOLORTABLEPROC)PROC(w, glGetColorTableSGI);
    w->glGetColorTableParameteriv = (OBOL_PFNGLGETCOLORTABLEPARAMETERIVPROC)PROC(w, glGetColorTableParameterivSGI);
    w->glGetColorTableParameterfv = (OBOL_PFNGLGETCOLORTABLEPARAMETERFVPROC)PROC(w, glGetColorTableParameterfvSGI);
  }
#endif /* GL_SGI_color_table */

#if defined(GL_EXT_color_subtable)
  if ((w->glColorSubTable == NULL) &&
      SoGLContext_glext_supported(w, "GL_EXT_color_subtable")) {
    w->glColorSubTable = (OBOL_PFNGLCOLORSUBTABLEPROC)PROC(w, glColorSubTableEXT);
  }
#endif /* GL_EXT_color_subtable */

  w->supportsPalettedTextures =
    SoGLContext_glext_supported(w, "GL_EXT_paletted_texture");
  /* FIXME: is paletted textures _really_ not supported through any
     non-extension mechanism for the later OpenGL spec versions?
     Investigate. 20031027 mortene. */

#ifdef GL_EXT_paletted_texture
  /* Note that EXT_paletted_texture defines glColorTableEXT et al
     "on it's own", i.e. it doesn't need the presence of
     EXT_color_table / SGI_color_table / OGL1.2+ + ARB_imaging. It
     only defines a *subset* of what EXT_color_table etc defines,
     though. */
  if ((w->glColorTable == NULL) &&
      SoGLContext_glext_supported(w, "GL_EXT_paletted_texture")) {
    w->glColorTable = (OBOL_PFNGLCOLORTABLEPROC)PROC(w, glColorTableEXT);
    w->glColorSubTable = (OBOL_PFNGLCOLORSUBTABLEPROC)PROC(w, glColorSubTableEXT);
    w->glGetColorTable = (OBOL_PFNGLGETCOLORTABLEPROC)PROC(w, glGetColorTableEXT);
    w->glGetColorTableParameteriv = (OBOL_PFNGLGETCOLORTABLEPARAMETERIVPROC)PROC(w, glGetColorTableParameterivEXT);
    w->glGetColorTableParameterfv = (OBOL_PFNGLGETCOLORTABLEPARAMETERFVPROC)PROC(w, glGetColorTableParameterfvEXT);
  }
#endif /* GL_EXT_paletted_texture */

  w->glBlendEquation = NULL;
  w->glBlendEquationEXT = NULL;

#if defined(GL_VERSION_1_4)
  if (SoGLContext_glversion_matches_at_least(w, 1, 4, 0)) {
    w->glBlendEquation = (OBOL_PFNGLBLENDEQUATIONPROC)PROC(w, glBlendEquation);
  }
#endif /* GL_VERSION_1_4 */

  if (w->glBlendEquation == NULL) {
#if defined(GL_VERSION_1_2) && defined(GL_ARB_imaging)
    if (SoGLContext_glversion_matches_at_least(w, 1, 2, 0) &&
        SoGLContext_glext_supported(w, "GL_ARB_imaging")) {
      w->glBlendEquation = (OBOL_PFNGLBLENDEQUATIONPROC)PROC(w, glBlendEquation);
    }
#endif /* GL_VERSION_1_2 && GL_ARB_imaging */
  }

#ifdef GL_EXT_blend_minmax
  if (SoGLContext_glext_supported(w, "GL_EXT_blend_minmax")) {
    w->glBlendEquationEXT = (OBOL_PFNGLBLENDEQUATIONPROC)PROC(w, glBlendEquationEXT);
  }
#endif /* GL_EXT_blend_minmax */

  w->glBlendFuncSeparate = NULL;
#if defined(GL_VERSION_1_4)
  if (SoGLContext_glversion_matches_at_least(w, 1, 4, 0)) {
    w->glBlendFuncSeparate = (OBOL_PFNGLBLENDFUNCSEPARATEPROC)PROC(w, glBlendFuncSeparate);
  }
#endif /* GL_VERSION_1_4 */

  w->glVertexPointer = NULL; /* for SoGLContext_has_vertex_array() */
#if defined(GL_VERSION_1_1)
  if (SoGLContext_glversion_matches_at_least(w, 1, 1, 0)) {
    w->glVertexPointer = (OBOL_PFNGLVERTEXPOINTERPROC) PROC(w, glVertexPointer);
    w->glTexCoordPointer = (OBOL_PFNGLTEXCOORDPOINTERPROC) PROC(w, glTexCoordPointer);
    w->glNormalPointer = (OBOL_PFNGLNORMALPOINTERPROC) PROC(w, glNormalPointer);
    w->glColorPointer = (OBOL_PNFGLCOLORPOINTERPROC) PROC(w, glColorPointer);
    w->glIndexPointer = (OBOL_PFNGLINDEXPOINTERPROC) PROC(w, glIndexPointer);
    w->glEnableClientState = (OBOL_PFNGLENABLECLIENTSTATEPROC) PROC(w, glEnableClientState);
    w->glDisableClientState = (OBOL_PFNGLDISABLECLIENTSTATEPROC) PROC(w, glDisableClientState);
    w->glInterleavedArrays = (OBOL_PFNGLINTERLEAVEDARRAYSPROC) PROC(w, glInterleavedArrays);
    w->glDrawArrays = (OBOL_PFNGLDRAWARRAYSPROC) PROC(w, glDrawArrays);
    w->glDrawElements = (OBOL_PFNGLDRAWELEMENTSPROC) PROC(w, glDrawElements);
    w->glArrayElement = (OBOL_PFNGLARRAYELEMENTPROC) PROC(w, glArrayElement);
  }
  if (w->glVertexPointer) {
    if (!w->glTexCoordPointer ||
        !w->glNormalPointer ||
        !w->glColorPointer ||
        !w->glIndexPointer ||
        !w->glEnableClientState ||
        !w->glDisableClientState ||
        !w->glInterleavedArrays ||
        !w->glDrawArrays ||
        !w->glDrawElements ||
        !w->glArrayElement) {
      w->glVertexPointer = NULL; /* SoGLContext_has_vertex_array() will return FALSE */
      if (OBOL_DEBUG || SoGLContext_debug()) {
        cc_debugerror_postwarning("glglue_init",
                                  "glVertexPointer found, but one or more of the other "
                                  "vertex array functions were not found");
      }
    }
  }
#endif /* GL_VERSION_1_1 */


#if defined(GL_VERSION_1_2)
  w->glDrawRangeElements = NULL;
  if (SoGLContext_glversion_matches_at_least(w, 1, 2, 0))
    w->glDrawRangeElements = (OBOL_PFNGLDRAWRANGEELEMENTSPROC) PROC(w, glDrawRangeElements);
#endif /* GL_VERSION_1_2 */


  /* Appeared in OpenGL v1.4 (but also in GL_EXT_multi_draw_array extension */
  w->glMultiDrawArrays = NULL;
  w->glMultiDrawElements = NULL;
#if defined(GL_VERSION_1_4)
  if (SoGLContext_glversion_matches_at_least(w, 1, 4, 0)) {
    w->glMultiDrawArrays = (OBOL_PFNGLMULTIDRAWARRAYSPROC) PROC(w, glMultiDrawArrays);
    w->glMultiDrawElements = (OBOL_PFNGLMULTIDRAWELEMENTSPROC) PROC(w, glMultiDrawElements);
  }
#endif /* GL_VERSION_1_4 */
#if defined(GL_EXT_multi_draw_arrays)
  if ((w->glMultiDrawArrays == NULL) && SoGLContext_glext_supported(w, "GL_EXT_multi_draw_arrays")) {
    w->glMultiDrawArrays = (OBOL_PFNGLMULTIDRAWARRAYSPROC) PROC(w, glMultiDrawArraysEXT);
    w->glMultiDrawElements = (OBOL_PFNGLMULTIDRAWELEMENTSPROC) PROC(w, glMultiDrawElementsEXT);
  }
#endif /* GL_EXT_multi_draw_arrays */

  w->glBindBuffer = NULL; /* so that SoGLContext_has_vertex_buffer_objects() works  */
#if defined(GL_VERSION_1_5)
  if (SoGLContext_glversion_matches_at_least(w, 1, 5, 0)) {
    w->glBindBuffer = (OBOL_PFNGLBINDBUFFERPROC) PROC(w, glBindBuffer);
    w->glDeleteBuffers = (OBOL_PFNGLDELETEBUFFERSPROC) PROC(w, glDeleteBuffers);
    w->glGenBuffers = (OBOL_PFNGLGENBUFFERSPROC) PROC(w, glGenBuffers);
    w->glIsBuffer = (OBOL_PFNGLISBUFFERPROC) PROC(w, glIsBuffer);
    w->glBufferData = (OBOL_PFNGLBUFFERDATAPROC) PROC(w, glBufferData);
    w->glBufferSubData = (OBOL_PFNGLBUFFERSUBDATAPROC) PROC(w, glBufferSubData);
    w->glGetBufferSubData = (OBOL_PFNGLGETBUFFERSUBDATAPROC) PROC(w, glGetBufferSubData);
    w->glMapBuffer = (OBOL_PNFGLMAPBUFFERPROC) PROC(w, glMapBuffer);
    w->glUnmapBuffer = (OBOL_PFNGLUNMAPBUFFERPROC) PROC(w, glUnmapBuffer);
    w->glGetBufferParameteriv = (OBOL_PFNGLGETBUFFERPARAMETERIVPROC) PROC(w, glGetBufferParameteriv);
    w->glGetBufferPointerv = (OBOL_PFNGLGETBUFFERPOINTERVPROC) PROC(w, glGetBufferPointerv);
  }
#endif /* GL_VERSION_1_5 */

#if defined(GL_ARB_vertex_buffer_object)
  if ((w->glBindBuffer == NULL) && SoGLContext_glext_supported(w, "GL_ARB_vertex_buffer_object")) {
    w->glBindBuffer = (OBOL_PFNGLBINDBUFFERPROC) PROC(w, glBindBufferARB);
    w->glDeleteBuffers = (OBOL_PFNGLDELETEBUFFERSPROC) PROC(w, glDeleteBuffersARB);
    w->glGenBuffers = (OBOL_PFNGLGENBUFFERSPROC) PROC(w, glGenBuffersARB);
    w->glIsBuffer = (OBOL_PFNGLISBUFFERPROC) PROC(w, glIsBufferARB);
    w->glBufferData = (OBOL_PFNGLBUFFERDATAPROC) PROC(w, glBufferDataARB);
    w->glBufferSubData = (OBOL_PFNGLBUFFERSUBDATAPROC) PROC(w, glBufferSubDataARB);
    w->glGetBufferSubData = (OBOL_PFNGLGETBUFFERSUBDATAPROC) PROC(w, glGetBufferSubDataARB);
    w->glMapBuffer = (OBOL_PNFGLMAPBUFFERPROC) PROC(w, glMapBufferARB);
    w->glUnmapBuffer = (OBOL_PFNGLUNMAPBUFFERPROC) PROC(w, glUnmapBufferARB);
    w->glGetBufferParameteriv = (OBOL_PFNGLGETBUFFERPARAMETERIVPROC) PROC(w, glGetBufferParameterivARB);
    w->glGetBufferPointerv = (OBOL_PFNGLGETBUFFERPOINTERVPROC) PROC(w, glGetBufferPointervARB);
  }

  /* VBO support has been found to often trigger bugs in OpenGL
     drivers, so we make it possible to selectively disable that
     feature through an envvar.

     (Specifically, I've seen the following driver crash when using
     VBOs in an offscreen context: GL_RENDERER="GeForce 7950
     GX2/PCI/SSE2", GL_VERSION="2.0.2 NVIDIA 87.62", on an AMD64 with
     Linux. On-screen contexts with VBOs was ok on the exact same
     machine. -mortene.)
  */
  if (w->glBindBuffer) {
    auto env = CoinInternal::getEnvironmentVariable("OBOL_GL_DISABLE_VBO");
    if (env.has_value() && (std::atoi(env->c_str()) > 0)) { w->glBindBuffer = NULL; }
  }

  /*
    Sylvain Carette reported problems with some old 3DLabs drivers and VBO rendering.
    The drivers were from 2006, so we disable VBO rendering if 3DLabs and that driver
    version is detected (the driver version was 2.0)
   */
  if (w->glBindBuffer && w->vendor_is_3dlabs
      && !SoGLContext_glversion_matches_at_least(w, 2,0,1)) {
    /* Enable users to override this workaround by setting OBOL_VBO=1 */
    auto env = CoinInternal::getEnvironmentVariable("OBOL_VBO");
    if (!env.has_value() || (std::atoi(env->c_str()) > 0)) {
      w->glBindBuffer = NULL;
    }
  }

#endif /* GL_ARB_vertex_buffer_object */

  if (w->glBindBuffer) {
    if (!w->glDeleteBuffers ||
        !w->glGenBuffers ||
        !w->glIsBuffer ||
        !w->glBufferData ||
        !w->glBufferSubData ||
        !w->glGetBufferSubData ||
        !w->glMapBuffer ||
        !w->glUnmapBuffer ||
        !w->glGetBufferParameteriv ||
        !w->glGetBufferPointerv) {
      w->glBindBuffer = NULL; /* so that SoGLContext_has_vertex_buffer_object() will return FALSE */
      if (OBOL_DEBUG || SoGLContext_debug()) {
        cc_debugerror_postwarning("glglue_init",
                                  "glBindBuffer found, but one or more of the other "
                                  "vertex buffer object functions were not found");
      }
    }
  }

  /* GL_NV_register_combiners */
  w->glCombinerParameterfvNV = NULL;
  w->glCombinerParameterivNV = NULL;
  w->glCombinerParameterfNV = NULL;
  w->glCombinerParameteriNV = NULL;
  w->glCombinerInputNV = NULL;
  w->glCombinerOutputNV = NULL;
  w->glFinalCombinerInputNV = NULL;
  w->glGetCombinerInputParameterfvNV = NULL;
  w->glGetCombinerInputParameterivNV = NULL;
  w->glGetCombinerOutputParameterfvNV = NULL;
  w->glGetCombinerOutputParameterivNV = NULL;
  w->glGetFinalCombinerInputParameterfvNV = NULL;
  w->glGetFinalCombinerInputParameterivNV = NULL;
  w->has_nv_register_combiners = FALSE;

#ifdef GL_NV_register_combiners

  if (SoGLContext_glext_supported(w, "GL_NV_register_combiners")) {

#define BIND_FUNCTION_WITH_WARN(_func_, _type_) \
   w->_func_ = (_type_)PROC(w, _func_); \
   do { \
     if (!w->_func_) { \
       w->has_nv_register_combiners = FALSE; \
       if (OBOL_DEBUG || SoGLContext_debug()) { \
         static SbBool error_reported = FALSE; \
         if (!error_reported) { \
           cc_debugerror_postwarning("glglue_init", \
                                     "GL_NV_register_combiners found, but %s " \
                                     "function missing.", SO__QUOTE(_func_)); \
           error_reported = TRUE; \
         } \
       } \
     } \
   } while (0)

    w->has_nv_register_combiners = TRUE;
    BIND_FUNCTION_WITH_WARN(glCombinerParameterfvNV, OBOL_PFNGLCOMBINERPARAMETERFVNVPROC);
    BIND_FUNCTION_WITH_WARN(glCombinerParameterivNV, OBOL_PFNGLCOMBINERPARAMETERIVNVPROC);
    BIND_FUNCTION_WITH_WARN(glCombinerParameterfNV, OBOL_PFNGLCOMBINERPARAMETERFNVPROC);
    BIND_FUNCTION_WITH_WARN(glCombinerParameteriNV, OBOL_PFNGLCOMBINERPARAMETERINVPROC);
    BIND_FUNCTION_WITH_WARN(glCombinerInputNV, OBOL_PFNGLCOMBINERINPUTNVPROC);
    BIND_FUNCTION_WITH_WARN(glCombinerOutputNV, OBOL_PFNGLCOMBINEROUTPUTNVPROC);
    BIND_FUNCTION_WITH_WARN(glFinalCombinerInputNV, OBOL_PFNGLFINALCOMBINERINPUTNVPROC);
    BIND_FUNCTION_WITH_WARN(glGetCombinerInputParameterfvNV, OBOL_PFNGLGETCOMBINERINPUTPARAMETERFVNVPROC);
    BIND_FUNCTION_WITH_WARN(glGetCombinerInputParameterivNV, OBOL_PFNGLGETCOMBINERINPUTPARAMETERIVNVPROC);
    BIND_FUNCTION_WITH_WARN(glGetCombinerOutputParameterfvNV, OBOL_PFNGLGETCOMBINEROUTPUTPARAMETERFVNVPROC);
    BIND_FUNCTION_WITH_WARN(glGetCombinerOutputParameterivNV, OBOL_PFNGLGETCOMBINEROUTPUTPARAMETERIVNVPROC);
    BIND_FUNCTION_WITH_WARN(glGetFinalCombinerInputParameterfvNV, OBOL_PFNGLGETFINALCOMBINERINPUTPARAMETERFVNVPROC);
    BIND_FUNCTION_WITH_WARN(glGetFinalCombinerInputParameterivNV, OBOL_PFNGLGETFINALCOMBINERINPUTPARAMETERIVNVPROC);

#undef BIND_FUNCTION_WITH_WARN
  }
#endif /* GL_NV_register_combiners */


  /* GL_[NV/EXT]_texture_rectangle */
  w->has_ext_texture_rectangle = (SoGLContext_glext_supported(w, "GL_EXT_texture_rectangle") ||
                                  SoGLContext_glext_supported(w, "GL_NV_texture_rectangle"));

  /* GL_NV_texture_shader */
  w->has_nv_texture_shader = SoGLContext_glext_supported(w, "GL_NV_texture_shader");

  /* GL_ARB_shadow */
  w->has_shadow = (SoGLContext_glext_supported(w, "GL_ARB_shadow") ||
                   SoGLContext_glversion_matches_at_least(w, 1, 4, 0));

  /* GL_ARB_depth_texture */
  w->has_depth_texture = (SoGLContext_glext_supported(w, "GL_ARB_depth_texture") ||
                          SoGLContext_glversion_matches_at_least(w, 1, 4, 0));

  /* GL_[ARB/EXT]_texture_env_combine */
  w->has_texture_env_combine = (SoGLContext_glext_supported(w, "GL_ARB_texture_env_combine") ||
                                SoGLContext_glext_supported(w, "GL_EXT_texture_env_combine") ||
                                SoGLContext_glversion_matches_at_least(w, 1, 3, 0));

  /* GL_ARB_fragment_program */
  w->glProgramStringARB = NULL;
  w->glBindProgramARB = NULL;
  w->glDeleteProgramsARB = NULL;
  w->glGenProgramsARB = NULL;
  w->glProgramEnvParameter4dARB = NULL;
  w->glProgramEnvParameter4dvARB = NULL;
  w->glProgramEnvParameter4fARB = NULL;
  w->glProgramEnvParameter4fvARB = NULL;
  w->glProgramLocalParameter4dARB = NULL;
  w->glProgramLocalParameter4dvARB = NULL;
  w->glProgramLocalParameter4fARB = NULL;
  w->glProgramLocalParameter4fvARB = NULL;
  w->glGetProgramEnvParameterdvARB = NULL;
  w->glGetProgramEnvParameterfvARB = NULL;
  w->glGetProgramLocalParameterdvARB = NULL;
  w->glGetProgramLocalParameterfvARB = NULL;
  w->glGetProgramivARB = NULL;
  w->glGetProgramStringARB = NULL;
  w->glIsProgramARB = NULL;
  w->has_arb_fragment_program = FALSE;

#ifdef GL_ARB_fragment_program
  if (SoGLContext_glext_supported(w, "GL_ARB_fragment_program")) {

#define BIND_FUNCTION_WITH_WARN(_func_, _type_) \
   w->_func_ = (_type_)PROC(w, _func_); \
   do { \
     if (!w->_func_) { \
       w->has_arb_fragment_program = FALSE; \
       if (OBOL_DEBUG || SoGLContext_debug()) { \
         static SbBool error_reported = FALSE; \
         if (!error_reported) { \
           cc_debugerror_postwarning("glglue_init", \
                                     "GL_ARB_fragment_program found, but %s " \
                                     "function missing.", SO__QUOTE(_func_)); \
           error_reported = TRUE; \
         } \
       } \
     } \
   } while (0)

    w->has_arb_fragment_program = TRUE;
    BIND_FUNCTION_WITH_WARN(glProgramStringARB, OBOL_PFNGLPROGRAMSTRINGARBPROC);
    BIND_FUNCTION_WITH_WARN(glBindProgramARB, OBOL_PFNGLBINDPROGRAMARBPROC);
    BIND_FUNCTION_WITH_WARN(glDeleteProgramsARB, OBOL_PFNGLDELETEPROGRAMSARBPROC);
    BIND_FUNCTION_WITH_WARN(glGenProgramsARB, OBOL_PFNGLGENPROGRAMSARBPROC);
    BIND_FUNCTION_WITH_WARN(glProgramEnvParameter4dARB, OBOL_PFNGLPROGRAMENVPARAMETER4DARBPROC);
    BIND_FUNCTION_WITH_WARN(glProgramEnvParameter4dvARB, OBOL_PFNGLPROGRAMENVPARAMETER4DVARBPROC);
    BIND_FUNCTION_WITH_WARN(glProgramEnvParameter4fARB, OBOL_PFNGLPROGRAMENVPARAMETER4FARBPROC);
    BIND_FUNCTION_WITH_WARN(glProgramEnvParameter4fvARB, OBOL_PFNGLPROGRAMENVPARAMETER4FVARBPROC);
    BIND_FUNCTION_WITH_WARN(glProgramLocalParameter4dARB, OBOL_PFNGLPROGRAMLOCALPARAMETER4DARBPROC);
    BIND_FUNCTION_WITH_WARN(glProgramLocalParameter4dvARB, OBOL_PFNGLPROGRAMLOCALPARAMETER4DVARBPROC);
    BIND_FUNCTION_WITH_WARN(glProgramLocalParameter4fARB, OBOL_PFNGLPROGRAMLOCALPARAMETER4FARBPROC);
    BIND_FUNCTION_WITH_WARN(glProgramLocalParameter4fvARB, OBOL_PFNGLPROGRAMLOCALPARAMETER4FVARBPROC);
    BIND_FUNCTION_WITH_WARN(glGetProgramEnvParameterdvARB, OBOL_PFNGLGETPROGRAMENVPARAMETERDVARBPROC);
    BIND_FUNCTION_WITH_WARN(glGetProgramEnvParameterfvARB, OBOL_PFNGLGETPROGRAMENVPARAMETERFVARBPROC);
    BIND_FUNCTION_WITH_WARN(glGetProgramLocalParameterdvARB, OBOL_PFNGLGETPROGRAMLOCALPARAMETERDVARBPROC);
    BIND_FUNCTION_WITH_WARN(glGetProgramLocalParameterfvARB, OBOL_PFNGLGETPROGRAMLOCALPARAMETERFVARBPROC);
    BIND_FUNCTION_WITH_WARN(glGetProgramivARB, OBOL_PFNGLGETPROGRAMIVARBPROC);
    BIND_FUNCTION_WITH_WARN(glGetProgramStringARB, OBOL_PFNGLGETPROGRAMSTRINGARBPROC);
    BIND_FUNCTION_WITH_WARN(glIsProgramARB, OBOL_PFNGLISPROGRAMARBPROC);

#undef BIND_FUNCTION_WITH_WARN
 }
#endif /* GL_ARB_fragment_program */

  w->has_arb_vertex_program = FALSE;
  w->glVertexAttrib1sARB = NULL;
  w->glVertexAttrib1fARB = NULL;
  w->glVertexAttrib1dARB = NULL;
  w->glVertexAttrib2sARB = NULL;
  w->glVertexAttrib2fARB = NULL;
  w->glVertexAttrib2dARB = NULL;
  w->glVertexAttrib3sARB = NULL;
  w->glVertexAttrib3fARB = NULL;
  w->glVertexAttrib3dARB = NULL;
  w->glVertexAttrib4sARB = NULL;
  w->glVertexAttrib4fARB = NULL;
  w->glVertexAttrib4dARB = NULL;
  w->glVertexAttrib4NubARB = NULL;
  w->glVertexAttrib1svARB = NULL;
  w->glVertexAttrib1fvARB = NULL;
  w->glVertexAttrib1dvARB = NULL;
  w->glVertexAttrib2svARB = NULL;
  w->glVertexAttrib2fvARB = NULL;
  w->glVertexAttrib2dvARB = NULL;
  w->glVertexAttrib3svARB = NULL;
  w->glVertexAttrib3fvARB = NULL;
  w->glVertexAttrib3dvARB = NULL;
  w->glVertexAttrib4bvARB = NULL;
  w->glVertexAttrib4svARB = NULL;
  w->glVertexAttrib4ivARB = NULL;
  w->glVertexAttrib4ubvARB = NULL;
  w->glVertexAttrib4usvARB = NULL;
  w->glVertexAttrib4uivARB = NULL;
  w->glVertexAttrib4fvARB = NULL;
  w->glVertexAttrib4dvARB = NULL;
  w->glVertexAttrib4NbvARB = NULL;
  w->glVertexAttrib4NsvARB = NULL;
  w->glVertexAttrib4NivARB = NULL;
  w->glVertexAttrib4NubvARB = NULL;
  w->glVertexAttrib4NusvARB = NULL;
  w->glVertexAttrib4NuivARB = NULL;
  w->glVertexAttribPointerARB = NULL;
  w->glEnableVertexAttribArrayARB = NULL;
  w->glDisableVertexAttribArrayARB = NULL;
  w->glProgramStringARB = NULL;
  w->glBindProgramARB = NULL;
  w->glDeleteProgramsARB = NULL;
  w->glGenProgramsARB = NULL;
  w->glProgramEnvParameter4dARB = NULL;
  w->glProgramEnvParameter4dvARB = NULL;
  w->glProgramEnvParameter4fARB = NULL;
  w->glProgramEnvParameter4fvARB = NULL;
  w->glProgramLocalParameter4dARB = NULL;
  w->glProgramLocalParameter4dvARB = NULL;
  w->glProgramLocalParameter4fARB = NULL;
  w->glProgramLocalParameter4fvARB = NULL;
  w->glGetProgramEnvParameterdvARB = NULL;
  w->glGetProgramEnvParameterfvARB = NULL;
  w->glGetProgramLocalParameterdvARB = NULL;
  w->glGetProgramLocalParameterfvARB = NULL;
  w->glGetProgramivARB = NULL;
  w->glGetProgramStringARB = NULL;
  w->glGetVertexAttribdvARB = NULL;
  w->glGetVertexAttribfvARB = NULL;
  w->glGetVertexAttribivARB = NULL;
  w->glGetVertexAttribPointervARB = NULL;
  w->glIsProgramARB = NULL;


#ifdef GL_ARB_vertex_program

  if (SoGLContext_glext_supported(w, "GL_ARB_vertex_program")) {

#define BIND_FUNCTION_WITH_WARN(_func_, _type_) \
   w->_func_ = (_type_)PROC(w, _func_); \
   do { \
     if (!w->_func_) { \
       w->has_arb_vertex_program = FALSE; \
       if (OBOL_DEBUG || SoGLContext_debug()) { \
         static SbBool error_reported = FALSE; \
         if (!error_reported) { \
           cc_debugerror_postwarning("glglue_init", \
                                     "GL_ARB_vertex_program found, but %s " \
                                     "function missing.", SO__QUOTE(_func_)); \
           error_reported = TRUE; \
         } \
       } \
     } \
   } while (0)

    w->has_arb_vertex_program = TRUE;
    BIND_FUNCTION_WITH_WARN(glVertexAttrib1sARB, OBOL_PFNGLVERTEXATTRIB1SARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib1fARB, OBOL_PFNGLVERTEXATTRIB1FARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib1dARB, OBOL_PFNGLVERTEXATTRIB1DARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib2sARB, OBOL_PFNGLVERTEXATTRIB2SARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib2fARB, OBOL_PFNGLVERTEXATTRIB2FARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib2dARB, OBOL_PFNGLVERTEXATTRIB2DARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib3sARB, OBOL_PFNGLVERTEXATTRIB3SARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib3fARB, OBOL_PFNGLVERTEXATTRIB3FARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib3dARB, OBOL_PFNGLVERTEXATTRIB3DARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib4sARB, OBOL_PFNGLVERTEXATTRIB4SARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib4fARB, OBOL_PFNGLVERTEXATTRIB4FARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib4dARB, OBOL_PFNGLVERTEXATTRIB4DARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib4NubARB, OBOL_PFNGLVERTEXATTRIB4NUBARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib1svARB, OBOL_PFNGLVERTEXATTRIB1SVARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib1fvARB, OBOL_PFNGLVERTEXATTRIB1FVARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib1dvARB, OBOL_PFNGLVERTEXATTRIB1DVARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib2svARB, OBOL_PFNGLVERTEXATTRIB2SVARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib2fvARB, OBOL_PFNGLVERTEXATTRIB2FVARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib2dvARB, OBOL_PFNGLVERTEXATTRIB2DVARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib3svARB, OBOL_PFNGLVERTEXATTRIB3SVARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib3fvARB, OBOL_PFNGLVERTEXATTRIB3FVARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib3dvARB, OBOL_PFNGLVERTEXATTRIB3DVARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib4bvARB, OBOL_PFNGLVERTEXATTRIB4BVARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib4svARB, OBOL_PFNGLVERTEXATTRIB4SVARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib4ivARB, OBOL_PFNGLVERTEXATTRIB4IVARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib4ubvARB, OBOL_PFNGLVERTEXATTRIB4UBVARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib4usvARB, OBOL_PFNGLVERTEXATTRIB4USVARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib4uivARB, OBOL_PFNGLVERTEXATTRIB4UIVARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib4fvARB, OBOL_PFNGLVERTEXATTRIB4FVARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib4dvARB, OBOL_PFNGLVERTEXATTRIB4DVARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib4NbvARB, OBOL_PFNGLVERTEXATTRIB4NBVARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib4NsvARB, OBOL_PFNGLVERTEXATTRIB4NSVARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib4NivARB, OBOL_PFNGLVERTEXATTRIB4NIVARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib4NubvARB, OBOL_PFNGLVERTEXATTRIB4NUBVARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib4NusvARB, OBOL_PFNGLVERTEXATTRIB4NUSVARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttrib4NuivARB, OBOL_PFNGLVERTEXATTRIB4NUIVARBPROC);
    BIND_FUNCTION_WITH_WARN(glVertexAttribPointerARB, OBOL_PFNGLVERTEXATTRIBPOINTERARBPROC);
    BIND_FUNCTION_WITH_WARN(glEnableVertexAttribArrayARB, OBOL_PFNGLENABLEVERTEXATTRIBARRAYARBPROC);
    BIND_FUNCTION_WITH_WARN(glDisableVertexAttribArrayARB, OBOL_PFNGLDISABLEVERTEXATTRIBARRAYARBPROC);
    BIND_FUNCTION_WITH_WARN(glProgramStringARB, OBOL_PFNGLPROGRAMSTRINGARBPROC);
    BIND_FUNCTION_WITH_WARN(glBindProgramARB, OBOL_PFNGLBINDPROGRAMARBPROC);
    BIND_FUNCTION_WITH_WARN(glDeleteProgramsARB, OBOL_PFNGLDELETEPROGRAMSARBPROC);
    BIND_FUNCTION_WITH_WARN(glGenProgramsARB, OBOL_PFNGLGENPROGRAMSARBPROC);
    BIND_FUNCTION_WITH_WARN(glProgramEnvParameter4dARB, OBOL_PFNGLPROGRAMENVPARAMETER4DARBPROC);
    BIND_FUNCTION_WITH_WARN(glProgramEnvParameter4dvARB, OBOL_PFNGLPROGRAMENVPARAMETER4DVARBPROC);
    BIND_FUNCTION_WITH_WARN(glProgramEnvParameter4fARB, OBOL_PFNGLPROGRAMENVPARAMETER4FARBPROC);
    BIND_FUNCTION_WITH_WARN(glProgramEnvParameter4fvARB, OBOL_PFNGLPROGRAMENVPARAMETER4FVARBPROC);
    BIND_FUNCTION_WITH_WARN(glProgramLocalParameter4dARB, OBOL_PFNGLPROGRAMLOCALPARAMETER4DARBPROC);
    BIND_FUNCTION_WITH_WARN(glProgramLocalParameter4dvARB, OBOL_PFNGLPROGRAMLOCALPARAMETER4DVARBPROC);
    BIND_FUNCTION_WITH_WARN(glProgramLocalParameter4fARB, OBOL_PFNGLPROGRAMLOCALPARAMETER4FARBPROC);
    BIND_FUNCTION_WITH_WARN(glProgramLocalParameter4fvARB, OBOL_PFNGLPROGRAMLOCALPARAMETER4FVARBPROC);
    BIND_FUNCTION_WITH_WARN(glGetProgramEnvParameterdvARB, OBOL_PFNGLGETPROGRAMENVPARAMETERDVARBPROC);
    BIND_FUNCTION_WITH_WARN(glGetProgramEnvParameterfvARB, OBOL_PFNGLGETPROGRAMENVPARAMETERFVARBPROC);
    BIND_FUNCTION_WITH_WARN(glGetProgramLocalParameterdvARB, OBOL_PFNGLGETPROGRAMLOCALPARAMETERDVARBPROC);
    BIND_FUNCTION_WITH_WARN(glGetProgramLocalParameterfvARB, OBOL_PFNGLGETPROGRAMLOCALPARAMETERFVARBPROC);
    BIND_FUNCTION_WITH_WARN(glGetProgramivARB, OBOL_PFNGLGETPROGRAMIVARBPROC);
    BIND_FUNCTION_WITH_WARN(glGetProgramStringARB, OBOL_PFNGLGETPROGRAMSTRINGARBPROC);
    BIND_FUNCTION_WITH_WARN(glGetVertexAttribdvARB, OBOL_PFNGLGETVERTEXATTRIBDVARBPROC);
    BIND_FUNCTION_WITH_WARN(glGetVertexAttribfvARB, OBOL_PFNGLGETVERTEXATTRIBFVARBPROC);
    BIND_FUNCTION_WITH_WARN(glGetVertexAttribivARB, OBOL_PFNGLGETVERTEXATTRIBIVARBPROC);
    BIND_FUNCTION_WITH_WARN(glGetVertexAttribPointervARB, OBOL_PFNGLGETVERTEXATTRIBPOINTERVARBPROC);
    BIND_FUNCTION_WITH_WARN(glIsProgramARB, OBOL_PFNGLISPROGRAMARBPROC);

#undef BIND_FUNCTION_WITH_WARN
  }
#endif /* GL_ARB_vertex_program */


#ifdef GL_ARB_vertex_shader

  w->glBindAttribLocationARB = NULL;
  w->glGetActiveAttribARB = NULL;
  w->glGetAttribLocationARB = NULL;

  if (SoGLContext_glext_supported(w, "GL_ARB_vertex_shader")) {

#define BIND_FUNCTION_WITH_WARN(_func_, _type_) \
   w->_func_ = (_type_)PROC(w, _func_); \
   do { \
     if (!w->_func_) { \
       w->has_arb_vertex_shader = FALSE; \
       if (OBOL_DEBUG || SoGLContext_debug()) { \
         static SbBool error_reported = FALSE; \
         if (!error_reported) { \
           cc_debugerror_postwarning("glglue_init", \
                                     "GL_ARB_vertex_shader found, but %s " \
                                     "function missing.", SO__QUOTE(_func_)); \
           error_reported = TRUE; \
         } \
       } \
     } \
   } while (0)

    w->has_arb_vertex_shader = TRUE;
    BIND_FUNCTION_WITH_WARN(glBindAttribLocationARB, OBOL_PFNGLBINDATTRIBLOCATIONARBPROC);
    BIND_FUNCTION_WITH_WARN(glGetActiveAttribARB, OBOL_PFNGLGETACTIVEATTRIBARBPROC);
    BIND_FUNCTION_WITH_WARN(glGetAttribLocationARB, OBOL_PFNGLGETATTRIBLOCATIONARBPROC);

#undef BIND_FUNCTION_WITH_WARN
  }
#endif /* GL_ARB_vertex_shader */


  w->glGetUniformLocationARB = NULL;
  w->glGetActiveUniformARB = NULL;
  w->glUniform1fARB = NULL;
  w->glUniform2fARB = NULL;
  w->glUniform3fARB = NULL;
  w->glUniform4fARB = NULL;
  w->glCreateShaderObjectARB = NULL;
  w->glShaderSourceARB = NULL;
  w->glCompileShaderARB = NULL;
  w->glGetObjectParameterivARB = NULL;
  w->glDeleteObjectARB = NULL;
  w->glAttachObjectARB = NULL;
  w->glDetachObjectARB = NULL;
  w->glGetInfoLogARB = NULL;
  w->glLinkProgramARB = NULL;
  w->glUseProgramObjectARB = NULL;
  w->glCreateProgramObjectARB = NULL;
  w->has_arb_shader_objects = FALSE;
  w->glUniform1fvARB = NULL;
  w->glUniform2fvARB = NULL;
  w->glUniform3fvARB = NULL;
  w->glUniform4fvARB = NULL;
  w->glUniform1iARB = NULL;
  w->glUniform2iARB = NULL;
  w->glUniform3iARB = NULL;
  w->glUniform4iARB = NULL;
  w->glUniform1ivARB = NULL;
  w->glUniform2ivARB = NULL;
  w->glUniform3ivARB = NULL;
  w->glUniform4ivARB = NULL;
  w->glUniformMatrix2fvARB = NULL;
  w->glUniformMatrix3fvARB = NULL;
  w->glUniformMatrix4fvARB = NULL;


#ifdef GL_ARB_shader_objects

  if (SoGLContext_glext_supported(w, "GL_ARB_shader_objects")) {

#define BIND_FUNCTION_WITH_WARN(_func_, _type_) \
   w->_func_ = (_type_)PROC(w, _func_); \
   do { \
     if (!w->_func_) { \
       w->has_arb_shader_objects = FALSE; \
       if (OBOL_DEBUG || SoGLContext_debug()) { \
         static SbBool error_reported = FALSE; \
         if (!error_reported) { \
           cc_debugerror_postwarning("glglue_init", \
                                     "GL_ARB_shader_objects found, but %s " \
                                     "function missing.", SO__QUOTE(_func_)); \
           error_reported = TRUE; \
         } \
       } \
     } \
   } while (0)

    w->has_arb_shader_objects = TRUE;
    BIND_FUNCTION_WITH_WARN(glGetUniformLocationARB, OBOL_PFNGLGETUNIFORMLOCATIONARBPROC);
    BIND_FUNCTION_WITH_WARN(glGetActiveUniformARB, OBOL_PFNGLGETACTIVEUNIFORMARBPROC);
    BIND_FUNCTION_WITH_WARN(glUniform1fARB, OBOL_PFNGLUNIFORM1FARBPROC);
    BIND_FUNCTION_WITH_WARN(glUniform2fARB, OBOL_PFNGLUNIFORM2FARBPROC);
    BIND_FUNCTION_WITH_WARN(glUniform3fARB, OBOL_PFNGLUNIFORM3FARBPROC);
    BIND_FUNCTION_WITH_WARN(glUniform4fARB, OBOL_PFNGLUNIFORM4FARBPROC);
    BIND_FUNCTION_WITH_WARN(glCreateShaderObjectARB, OBOL_PFNGLCREATESHADEROBJECTARBPROC);
    BIND_FUNCTION_WITH_WARN(glShaderSourceARB, OBOL_PFNGLSHADERSOURCEARBPROC);
    BIND_FUNCTION_WITH_WARN(glCompileShaderARB, OBOL_PFNGLCOMPILESHADERARBPROC);
    BIND_FUNCTION_WITH_WARN(glGetObjectParameterivARB, OBOL_PFNGLGETOBJECTPARAMETERIVARBPROC);
    BIND_FUNCTION_WITH_WARN(glDeleteObjectARB, OBOL_PFNGLDELETEOBJECTARBPROC);
    BIND_FUNCTION_WITH_WARN(glAttachObjectARB, OBOL_PFNGLATTACHOBJECTARBPROC);
    BIND_FUNCTION_WITH_WARN(glDetachObjectARB, OBOL_PFNGLDETACHOBJECTARBPROC);
    BIND_FUNCTION_WITH_WARN(glGetInfoLogARB, OBOL_PFNGLGETINFOLOGARBPROC);
    BIND_FUNCTION_WITH_WARN(glLinkProgramARB, OBOL_PFNGLLINKPROGRAMARBPROC);
    BIND_FUNCTION_WITH_WARN(glUseProgramObjectARB, OBOL_PFNGLUSEPROGRAMOBJECTARBPROC);
    BIND_FUNCTION_WITH_WARN(glCreateProgramObjectARB, OBOL_PFNGLCREATEPROGRAMOBJECTARBPROC);
    BIND_FUNCTION_WITH_WARN(glUniform1fvARB, OBOL_PFNGLUNIFORM1FVARBPROC);
    BIND_FUNCTION_WITH_WARN(glUniform2fvARB, OBOL_PFNGLUNIFORM2FVARBPROC);
    BIND_FUNCTION_WITH_WARN(glUniform3fvARB, OBOL_PFNGLUNIFORM3FVARBPROC);
    BIND_FUNCTION_WITH_WARN(glUniform4fvARB, OBOL_PFNGLUNIFORM4FVARBPROC);
    BIND_FUNCTION_WITH_WARN(glUniform1iARB, OBOL_PFNGLUNIFORM1IARBPROC);
    BIND_FUNCTION_WITH_WARN(glUniform2iARB, OBOL_PFNGLUNIFORM2IARBPROC);
    BIND_FUNCTION_WITH_WARN(glUniform3iARB, OBOL_PFNGLUNIFORM3IARBPROC);
    BIND_FUNCTION_WITH_WARN(glUniform4iARB, OBOL_PFNGLUNIFORM4IARBPROC);
    BIND_FUNCTION_WITH_WARN(glUniform1ivARB, OBOL_PFNGLUNIFORM1IVARBPROC);
    BIND_FUNCTION_WITH_WARN(glUniform2ivARB, OBOL_PFNGLUNIFORM2IVARBPROC);
    BIND_FUNCTION_WITH_WARN(glUniform3ivARB, OBOL_PFNGLUNIFORM3IVARBPROC);
    BIND_FUNCTION_WITH_WARN(glUniform4ivARB, OBOL_PFNGLUNIFORM4IVARBPROC);
    BIND_FUNCTION_WITH_WARN(glUniformMatrix2fvARB, OBOL_PFNGLUNIFORMMATRIX2FVARBPROC);
    BIND_FUNCTION_WITH_WARN(glUniformMatrix3fvARB, OBOL_PFNGLUNIFORMMATRIX3FVARBPROC);
    BIND_FUNCTION_WITH_WARN(glUniformMatrix4fvARB, OBOL_PFNGLUNIFORMMATRIX4FVARBPROC);


    w->glProgramParameteriEXT = NULL;
    if (SoGLContext_glext_supported(w, "GL_EXT_geometry_shader4")) {
      BIND_FUNCTION_WITH_WARN(glProgramParameteriEXT, OBOL_PFNGLPROGRAMPARAMETERIEXT);
    }
#undef BIND_FUNCTION_WITH_WARN
  }
#endif /* GL_ARB_shader_objects */

  w->glGenQueries = NULL; /* so that SoGLContext_has_occlusion_query() works  */
#if defined(GL_VERSION_1_5)
  if (SoGLContext_glversion_matches_at_least(w, 1, 5, 0)) {
    w->glGenQueries = (OBOL_PFNGLGENQUERIESPROC)PROC(w, glGenQueries);
    w->glDeleteQueries = (OBOL_PFNGLDELETEQUERIESPROC)PROC(w, glDeleteQueries);
    w->glIsQuery = (OBOL_PFNGLISQUERYPROC)PROC(w, glIsQuery);
    w->glBeginQuery = (OBOL_PFNGLBEGINQUERYPROC)PROC(w, glBeginQuery);
    w->glEndQuery = (OBOL_PFNGLENDQUERYPROC)PROC(w, glEndQuery);
    w->glGetQueryiv = (OBOL_PFNGLGETQUERYIVPROC)PROC(w, glGetQueryiv);
    w->glGetQueryObjectiv = (OBOL_PFNGLGETQUERYOBJECTIVPROC)PROC(w, glGetQueryObjectiv);
    w->glGetQueryObjectuiv = (OBOL_PFNGLGETQUERYOBJECTUIVPROC)PROC(w, glGetQueryObjectuiv);
  }
#endif /* GL_VERSION_1_5 */

#if defined(GL_ARB_occlusion_query)
  if ((w->glGenQueries == NULL) && SoGLContext_glext_supported(w, "GL_ARB_occlusion_query")) {
    w->glGenQueries = (OBOL_PFNGLGENQUERIESPROC)PROC(w, glGenQueriesARB);
    w->glDeleteQueries = (OBOL_PFNGLDELETEQUERIESPROC)PROC(w, glDeleteQueriesARB);
    w->glIsQuery = (OBOL_PFNGLISQUERYPROC)PROC(w, glIsQueryARB);
    w->glBeginQuery = (OBOL_PFNGLBEGINQUERYPROC)PROC(w, glBeginQueryARB);
    w->glEndQuery = (OBOL_PFNGLENDQUERYPROC)PROC(w, glEndQueryARB);
    w->glGetQueryiv = (OBOL_PFNGLGETQUERYIVPROC)PROC(w, glGetQueryivARB);
    w->glGetQueryObjectiv = (OBOL_PFNGLGETQUERYOBJECTIVPROC)PROC(w, glGetQueryObjectivARB);
    w->glGetQueryObjectuiv = (OBOL_PFNGLGETQUERYOBJECTUIVPROC)PROC(w, glGetQueryObjectuivARB);
  }
#endif /* GL_ARB_occlusion_query */

  if (w->glGenQueries) {
    if (!w->glDeleteQueries ||
        !w->glIsQuery ||
        !w->glBeginQuery ||
        !w->glEndQuery ||
        !w->glGetQueryiv ||
        !w->glGetQueryObjectiv ||
        !w->glGetQueryObjectuiv) {
      w->glGenQueries = NULL; /* so that SoGLContext_has_occlusion_query() will return FALSE */
      if (OBOL_DEBUG || SoGLContext_debug()) {
        cc_debugerror_postwarning("glglue_init",
                                  "glGenQueries found, but one or more of the other "
                                  "occlusion query functions were not found");
      }
    }
  }

  w->glVertexArrayRangeNV = NULL;
#if defined(GL_NV_vertex_array_range) && defined(HAVE_WGL)
  if (SoGLContext_glext_supported(w, "GL_NV_vertex_array_range")) {
    w->glVertexArrayRangeNV = (OBOL_PFNGLVERTEXARRAYRANGENVPROC) PROC(w, glVertexArrayRangeNV);
    w->glFlushVertexArrayRangeNV = (OBOL_PFNGLFLUSHVERTEXARRAYRANGENVPROC) PROC(w, glFlushVertexArrayRangeNV);
#ifdef HAVE_WGL
    w->glAllocateMemoryNV = (OBOL_PFNGLALLOCATEMEMORYNVPROC) PROC(w, wglAllocateMemoryNV);
    w->glFreeMemoryNV = (OBOL_PFNGLFREEMEMORYNVPROC) PROC(w, wglFreeMemoryNV);
#endif /* HAVE_WGL */
    if (w->glVertexArrayRangeNV) {
      if (!w->glFlushVertexArrayRangeNV ||
          !w->glAllocateMemoryNV ||
          !w->glFreeMemoryNV) {
        w->glVertexArrayRangeNV = NULL;
        if (OBOL_DEBUG || SoGLContext_debug()) {
          cc_debugerror_postwarning("glglue_init",
                                    "glVertexArrayRangeNV found, but one or more of the other "
                                    "vertex array functions were not found");
        }
      }
    }
  }
#endif /* HAVE_WGL */

  w->can_do_bumpmapping = FALSE;
  if (w->glActiveTexture &&
      (SoGLContext_glversion_matches_at_least(w, 1, 3, 0) ||
       (SoGLContext_glext_supported(w, "GL_ARB_texture_cube_map") &&
        w->has_texture_env_combine &&
        SoGLContext_glext_supported(w, "GL_ARB_texture_env_dot3")))) {
    w->can_do_bumpmapping = TRUE;
  }

  /* FIXME: We should be able to support more than one way to do order
     independent transparency (eg. by using fragment
     programming). This would demand a different combinations of
     extensions (and thus; a different codepath in
     SoGLRenderAction). (20031124 handegar) */
  w->can_do_sortedlayersblend =
    (w->has_nv_register_combiners &&
     w->has_ext_texture_rectangle &&
     w->has_nv_texture_shader &&
     w->has_depth_texture &&
     w->has_shadow) ||
    w->has_arb_fragment_program;
  
  /* Try ARB framebuffer objects first (OpenGL 3.0+ core or ARB extension) */
  if (SoGLContext_glext_supported(w, "GL_ARB_framebuffer_object") ||
      SoGLContext_glversion_matches_at_least(w, 3, 0, 0)) {
    
    /* Load ARB/Core framebuffer functions */
    w->glIsRenderbuffer = (OBOL_PFNGLISRENDERBUFFERPROC) SoGLContext_getprocaddress(w, "glIsRenderbuffer");
    w->glBindRenderbuffer = (OBOL_PFNGLBINDRENDERBUFFERPROC) SoGLContext_getprocaddress(w, "glBindRenderbuffer");
    w->glDeleteRenderbuffers = (OBOL_PFNGLDELETERENDERBUFFERSPROC)SoGLContext_getprocaddress(w, "glDeleteRenderbuffers");
    w->glGenRenderbuffers = (OBOL_PFNGLGENRENDERBUFFERSPROC)SoGLContext_getprocaddress(w, "glGenRenderbuffers");
    w->glRenderbufferStorage = (OBOL_PFNGLRENDERBUFFERSTORAGEPROC)SoGLContext_getprocaddress(w, "glRenderbufferStorage");
    w->glGetRenderbufferParameteriv = (OBOL_PFNGLGETRENDERBUFFERPARAMETERIVPROC)SoGLContext_getprocaddress(w, "glGetRenderbufferParameteriv");
    w->glIsFramebuffer = (OBOL_PFNGLISFRAMEBUFFERPROC)SoGLContext_getprocaddress(w, "glIsFramebuffer");
    w->glBindFramebuffer = (OBOL_PFNGLBINDFRAMEBUFFERPROC)SoGLContext_getprocaddress(w, "glBindFramebuffer");
    w->glDeleteFramebuffers = (OBOL_PFNGLDELETEFRAMEBUFFERSPROC)SoGLContext_getprocaddress(w, "glDeleteFramebuffers");
    w->glGenFramebuffers = (OBOL_PFNGLGENFRAMEBUFFERSPROC)SoGLContext_getprocaddress(w, "glGenFramebuffers");
    w->glCheckFramebufferStatus = (OBOL_PFNGLCHECKFRAMEBUFFERSTATUSPROC)SoGLContext_getprocaddress(w, "glCheckFramebufferStatus");
    w->glFramebufferTexture1D = (OBOL_PFNGLFRAMEBUFFERTEXTURE1DPROC)SoGLContext_getprocaddress(w, "glFramebufferTexture1D");
    w->glFramebufferTexture2D = (OBOL_PFNGLFRAMEBUFFERTEXTURE2DPROC)SoGLContext_getprocaddress(w, "glFramebufferTexture2D");
    w->glFramebufferTexture3D = (OBOL_PFNGLFRAMEBUFFERTEXTURE3DPROC)SoGLContext_getprocaddress(w, "glFramebufferTexture3D");
    w->glFramebufferRenderbuffer = (OBOL_PFNGLFRAMEBUFFERRENDERBUFFERPROC)SoGLContext_getprocaddress(w, "glFramebufferRenderbuffer");
    w->glGetFramebufferAttachmentParameteriv = (OBOL_PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC)
      SoGLContext_getprocaddress(w, "glGetFramebufferAttachmentParameteriv");
    w->glGenerateMipmap = (OBOL_PFNGLGENERATEMIPMAPPROC)
      SoGLContext_getprocaddress(w, "glGenerateMipmap");
    if (!w->glGenerateMipmap) {
      w->glGenerateMipmap = (OBOL_PFNGLGENERATEMIPMAPPROC)
        SoGLContext_getprocaddress(w, "glGenerateMipmapARB");
    }
    
    /* Check if all ARB functions were loaded successfully */
    if (w->glIsRenderbuffer && w->glBindRenderbuffer && w->glDeleteRenderbuffers &&
        w->glGenRenderbuffers && w->glRenderbufferStorage && w->glGetRenderbufferParameteriv &&
        w->glIsFramebuffer && w->glBindFramebuffer && w->glDeleteFramebuffers &&
        w->glGenFramebuffers && w->glCheckFramebufferStatus && w->glFramebufferTexture1D &&
        w->glFramebufferTexture2D && w->glFramebufferTexture3D && w->glFramebufferRenderbuffer &&
        w->glGetFramebufferAttachmentParameteriv && w->glGenerateMipmap) {
      w->has_fbo = TRUE;
    }
  }
  
  /* Fall back to EXT framebuffer objects if ARB version not available */
  if (!w->has_fbo && SoGLContext_glext_supported(w, "GL_EXT_framebuffer_object")) {
    
    /* Load EXT framebuffer functions */
    w->glIsRenderbuffer = (OBOL_PFNGLISRENDERBUFFERPROC) SoGLContext_getprocaddress(w, "glIsRenderbufferEXT");
    w->glBindRenderbuffer = (OBOL_PFNGLBINDRENDERBUFFERPROC) SoGLContext_getprocaddress(w, "glBindRenderbufferEXT");
    w->glDeleteRenderbuffers = (OBOL_PFNGLDELETERENDERBUFFERSPROC)SoGLContext_getprocaddress(w, "glDeleteRenderbuffersEXT");
    w->glGenRenderbuffers = (OBOL_PFNGLGENRENDERBUFFERSPROC)SoGLContext_getprocaddress(w, "glGenRenderbuffersEXT");
    w->glRenderbufferStorage = (OBOL_PFNGLRENDERBUFFERSTORAGEPROC)SoGLContext_getprocaddress(w, "glRenderbufferStorageEXT");
    w->glGetRenderbufferParameteriv = (OBOL_PFNGLGETRENDERBUFFERPARAMETERIVPROC)SoGLContext_getprocaddress(w, "glGetRenderbufferParameterivEXT");
    w->glIsFramebuffer = (OBOL_PFNGLISFRAMEBUFFERPROC)SoGLContext_getprocaddress(w, "glIsFramebufferEXT");
    w->glBindFramebuffer = (OBOL_PFNGLBINDFRAMEBUFFERPROC)SoGLContext_getprocaddress(w, "glBindFramebufferEXT");
    w->glDeleteFramebuffers = (OBOL_PFNGLDELETEFRAMEBUFFERSPROC)SoGLContext_getprocaddress(w, "glDeleteFramebuffersEXT");
    w->glGenFramebuffers = (OBOL_PFNGLGENFRAMEBUFFERSPROC)SoGLContext_getprocaddress(w, "glGenFramebuffersEXT");
    w->glCheckFramebufferStatus = (OBOL_PFNGLCHECKFRAMEBUFFERSTATUSPROC)SoGLContext_getprocaddress(w, "glCheckFramebufferStatusEXT");
    w->glFramebufferTexture1D = (OBOL_PFNGLFRAMEBUFFERTEXTURE1DPROC)SoGLContext_getprocaddress(w, "glFramebufferTexture1DEXT");
    w->glFramebufferTexture2D = (OBOL_PFNGLFRAMEBUFFERTEXTURE2DPROC)SoGLContext_getprocaddress(w, "glFramebufferTexture2DEXT");
    w->glFramebufferTexture3D = (OBOL_PFNGLFRAMEBUFFERTEXTURE3DPROC)SoGLContext_getprocaddress(w, "glFramebufferTexture3DEXT");
    w->glFramebufferRenderbuffer = (OBOL_PFNGLFRAMEBUFFERRENDERBUFFERPROC)SoGLContext_getprocaddress(w, "glFramebufferRenderbufferEXT");
    w->glGetFramebufferAttachmentParameteriv = (OBOL_PFNGLGETFRAMEBUFFERATTACHMENTPARAMETERIVPROC)
      SoGLContext_getprocaddress(w, "glGetFramebufferAttachmentParameterivEXT");
    
    /* Load glGenerateMipmap for EXT version if not already loaded */
    if (!w->glGenerateMipmap) {
      w->glGenerateMipmap = (OBOL_PFNGLGENERATEMIPMAPPROC)
        SoGLContext_getprocaddress(w, "glGenerateMipmapEXT");
    }

    /* Check if all EXT functions were loaded successfully */
    if (w->glIsRenderbuffer && w->glBindRenderbuffer && w->glDeleteRenderbuffers &&
        w->glGenRenderbuffers && w->glRenderbufferStorage && w->glGetRenderbufferParameteriv &&
        w->glIsFramebuffer && w->glBindFramebuffer && w->glDeleteFramebuffers &&
        w->glGenFramebuffers && w->glCheckFramebufferStatus && w->glFramebufferTexture1D &&
        w->glFramebufferTexture2D && w->glFramebufferTexture3D && w->glFramebufferRenderbuffer &&
        w->glGetFramebufferAttachmentParameteriv && w->glGenerateMipmap) {
      w->has_fbo = TRUE;
    }
  }

  /*
     Disable features based on known driver bugs  here.
     FIXME: move the driver workarounds to some other module. pederb, 2007-07-04
  */


  /*
     Option to disable FBO feature even if it is available.
     FIXME: FBO rendering fails in at least one application. To fix it properly
     we need to reproduce this bug in a minimal testcase. jkg, 2007-09-28
  */
  if ((glglue_resolve_envvar("OBOL_DONT_USE_FBO") == 1) && w->has_fbo) {
    w->has_fbo = FALSE;
  }

}

#undef PROC

static SbBool
glglue_check_trident_clampedge_bug(const char * vendor,
                                   const char * renderer,
                                   const char * version)
{
  return
    (strcmp(vendor, "Trident") == 0) &&
    (strcmp(renderer, "Blade XP/AGP") == 0) &&
    (strcmp(version, "1.2.1") == 0);
}

/* Give warnings on known faulty drivers. */
static void
glglue_check_driver(const char * vendor, const char * renderer,
                    const char * version)
{
#ifdef OBOL_DEBUG
  /* Only spit out this in debug builds, as the bug was never properly
     confirmed. */
  if (SoGLContext_radeon_warning()) {
    if (strcmp(renderer, "Radeon 7500 DDR x86/SSE2") == 0) {
      cc_debugerror_postwarning("glglue_check_driver",
                                "We've had an unconfirmed bugreport that "
                                "this OpenGL driver ('%s') may crash upon "
                                "attempts to use 3D texturing. "
                                "We would like to get assistance to help "
                                "us debug the cause of this problem, so "
                                "please get in touch with us at "
                                "<coin-support@coin3d.org>. "
                                "This debug message can be turned off "
                                "permanently by setting the environment "
                                "variable OBOL_GLGLUE_NO_RADEON_WARNING=1.",
                                renderer);

      /*
        Some additional information:

        The full driver information for the driver where this was
        reported is as follows:

        GL_VENDOR == 'ATI Technologies Inc.'
        GL_RENDERER == 'Radeon 7500 DDR x86/SSE2'
        GL_VERSION == '1.3.3302 Win2000 Release'

        The driver was reported to crash on MSWin with the
        SoGuiExamples/nodes/texture3 example. The reporter couldn't
        help us debug it, as he could a) not get a call-stack
        backtrace, and b) swapped his card for an NVidia card.

        Perhaps we should get hold of a Radeon card ourselves, to test
        and debug the problem.

        <mortene@sim.no>
      */
    }
  }
#endif /* OBOL_DEBUG */

  if (SoGLContext_old_matrox_warning() &&
      (strcmp(renderer, "Matrox G400") == 0) &&
      (strcmp(version, "1.1.3 Aug 30 2001") == 0)) {
    cc_debugerror_postwarning("glglue_check_driver",
                              "This old OpenGL driver (\"%s\" \"%s\") has "
                              "known bugs, please upgrade.  "
                              "(This debug message can be turned off "
                              "permanently by setting the environment "
                              "variable OBOL_GLGLUE_NO_G400_WARNING=1).",
                              renderer, version);
  }

  if (SoGLContext_old_elsa_warning() &&
      (strcmp(renderer, "ELSA TNT2 Vanta/PCI/SSE") == 0) &&
      (strcmp(version, "1.1.4 (4.06.00.266)") == 0)) {
    cc_debugerror_postwarning("glglue_check_driver",
                              "This old OpenGL driver (\"%s\" \"%s\") has "
                              "known bugs, please upgrade.  "
                              "(This debug message can be turned off "
                              "permanently by setting the environment "
                              "variable OBOL_GLGLUE_NO_ELSA_WARNING=1).",
                              renderer, version);
  }

  /*
    The full driver information for the driver where this was reported
    is as follows:

    GL_VENDOR == 'Matrox Graphics Inc.'
    GL_RENDERER == 'Matrox G400'
    GL_VERSION == '1.1.3 Aug 30 2001'

    GL_VENDOR == 'ELSA AG (Aachen, Germany).'
    GL_RENDERER == 'ELSA TNT2 Vanta/PCI/SSE'
    GL_VERSION == '1.1.4 (4.06.00.266)'

    The driver was reported to crash on MSWin under following
    conditions, quoted verbatim from the problem report:

    ------8<---- [snip] -----------8<---- [snip] -----

    I observe a bit of strange behaviour on my NT4 systems. I have an
    application which uses the following bit of code:

    // Define line width
    SoDrawStyle *drawStyle = new SoDrawStyle;
    drawStyle->lineWidth.setValue(3);
    drawStyle->linePattern.setValue(0x0F0F);
    root->addChild(drawStyle);

    // Define line connection
    SoCoordinate3 *coords = new SoCoordinate3;
    coords->point.setValues(0, 2, vert);
    root->addChild(coords);

    SoLineSet *lineSet = new SoLineSet ;
    lineSet->numVertices.set1Value(0, 2) ;
    root->addChild(lineSet);

    It defines a line with a dashed pattern. When the line is in a
    direction and the viewing direction is not parallel to this line
    all works fine. In case the viewing direction is the same as the
    line direction one of my systems crashes [...]

    ------8<---- [snip] -----------8<---- [snip] -----

    <mortene@sim.no>

    UPDATE 20030116 mortene: as of this date, the most recent Matrox
    driver (version 5.86.032, from 2002-11-21) still exhibits the same
    problem, while the ELSA driver can be upgraded to a version that
    does not have the bug any more.
  */

  if (SoGLContext_sun_expert3d_warning() &&
      (strcmp(renderer, "Sun Expert3D, VIS") == 0) &&
      (strcmp(version, "1.2 Sun OpenGL 1.2.1 patch 109544-19 for Solaris") == 0)) {
    cc_debugerror_postwarning("glglue_check_driver",
                              "This OpenGL driver (\"%s\" \"%s\") has known "
                              "problems with dual screen configurations, "
                              "please upgrade.  "
                              "(This debug message can be turned off "
                              "permanently by setting the environment variable"
                              " OBOL_GLGLUE_NO_SUN_EXPERT3D_WARNING=1).",
                              renderer, version);
  /*
    The full driver information for the driver where this was reported
    is as follows:

    GL_VENDOR == 'Sun Microsystems, Inc.'
    GL_RENDERER == 'Sun Expert3D, VIS'
    GL_VERSION == '1.2 Sun OpenGL 1.2.1 patch 109544-19 for Solaris'

    The driver was reported to fail when running on a Sun Solaris
    system with the XVR1000 graphics card. Quoted verbatim from the
    problem report:

    ------8<---- [snip] -----------8<---- [snip] -----

    [The client] works with two screens. One of the screen works as it
    should, while the other one has erroneous appearance (see uploaded
    image). The errors are the stripes on the texture (It should be
    one continuous texture). The texture is wrapped on a rectangle
    (i.e. two large triangles). It is not only the OpenGl part of the
    window that is weird.  Some buttons are missing and other buttons
    have wrong colors++.

    ------8<---- [snip] -----------8<---- [snip] -----

    The error disappeared after a driver upgrade.

    <mortene@sim.no>
  */
  }

  if (SoGLContext_trident_warning() &&
      glglue_check_trident_clampedge_bug(vendor, renderer, version)) {
    cc_debugerror_postwarning("glglue_check_driver",
                              "This OpenGL driver (\"%s\" \"%s\" \"%s\") has "
                              "a known problem: it doesn't support the "
                              "GL_CLAMP_TO_EDGE texture wrapping mode. "
                              "(This debug message can be turned off "
                              "permanently by setting the environment variable"
                              " OBOL_GLGLUE_NO_TRIDENT_WARNING=1).",
                              vendor, renderer, version);
    /*
      This problem manifests itself in the form of a glGetError()
      reporting GL_INVALID_ENUM if GL_TEXTURE_WRAP_[S|T] is attempted
      set to GL_CLAMP_TO_EDGE. GL_CLAMP_TO_EDGE was introduced with
      OpenGL v1.2.0, and the driver reports v1.2.1, so it is supposed
      to work.
    */
  }
}

/* Platform detection functions have been removed as they are no longer needed
   with callback-based contexts */

/* We're basically using the Singleton pattern to instantiate and
   return OpenGL-glue "object structs". We're constructing one
   instance for each OpenGL context, though.  */
const SoGLContext *
SoGLContext_instance(int contextid)
{
#if defined(OBOL_DUAL_GL_BUILD) && !defined(SOGL_PREFIX_SET)
  /* Dual-GL dispatch: if this context ID was registered as an OSMesa
     context, forward to the osmesa_ variant compiled in gl_osmesa.cpp. */
  if (coingl_context_backend_is_osmesa(contextid)) {
    return osmesa_SoGLContext_instance(contextid);
  }
#endif

  // Add debugging for OSMesa builds at function entry
#ifdef OBOL_SWRAST_BUILD
  if (SoGLContext_debug()) {
    cc_debugerror_postinfo("SoGLContext_instance", "ENTRY: contextid=%d", contextid);
  }
#endif

  SbBool found;
  void * ptr;
  GLint gltmp;

  SoGLContext * gi = NULL;

#ifdef OBOL_SWRAST_BUILD
  if (SoGLContext_debug()) {
    cc_debugerror_postinfo("SoGLContext_instance", "About to begin sync");
  }
#endif


  /* check environment variables */
#ifdef OBOL_SWRAST_BUILD
  if (SoGLContext_debug()) {
    cc_debugerror_postinfo("SoGLContext_instance", "Checking environment variables");
  }
#endif
  if (OBOL_MAXIMUM_TEXTURE2_SIZE == 0) {
    auto env = CoinInternal::getEnvironmentVariable("OBOL_MAXIMUM_TEXTURE2_SIZE");
    if (env.has_value()) OBOL_MAXIMUM_TEXTURE2_SIZE = std::atoi(env->c_str());
    else OBOL_MAXIMUM_TEXTURE2_SIZE = -1;
  }
  if (OBOL_MAXIMUM_TEXTURE3_SIZE == 0) {
    auto env = CoinInternal::getEnvironmentVariable("OBOL_MAXIMUM_TEXTURE3_SIZE");
    if (env.has_value()) OBOL_MAXIMUM_TEXTURE3_SIZE = std::atoi(env->c_str());
    else OBOL_MAXIMUM_TEXTURE3_SIZE = -1;
  }
  /* Platform detection calls removed */

#ifdef OBOL_SWRAST_BUILD
  if (SoGLContext_debug()) {
    cc_debugerror_postinfo("SoGLContext_instance", "Checking global dict");
  }
#endif
  if (!gldict) {  /* First invocation, do initializations. */
    gldict = cc_dict_construct(16, 0.75f);
    coin_atexit((coin_atexit_f *)glglue_cleanup, CC_ATEXIT_NORMAL);
  }

#ifdef OBOL_SWRAST_BUILD
  if (SoGLContext_debug()) {
    cc_debugerror_postinfo("SoGLContext_instance", "Looking up context %d in dict", contextid);
  }
#endif
  found = cc_dict_get(gldict, (uintptr_t)contextid, &ptr);

  if (!found) {
#ifdef OBOL_SWRAST_BUILD
    if (SoGLContext_debug()) {
      cc_debugerror_postinfo("SoGLContext_instance", "Context not found, creating new glglue instance");
    }
#endif
    GLenum glerr;

    /* Internal consistency checking.

       Make it possible to disabled this assert because GLX in Mesa
       version 3.4.2 (GL_VENDOR "VA Linux Systems, Inc", GL_RENDERER
       "Mesa GLX Indirect", GL_VERSION "1.2 Mesa 3.4.2") returns NULL
       even though there really is a current context set up. (Reported
       by kintel.)
    */
    static int chk = -1;
    if (chk == -1) {
      /* Note: don't change envvar name without updating the assert
         text below. */
      chk = CoinInternal::getEnvironmentVariable("OBOL_GL_NO_CURRENT_CONTEXT_CHECK").has_value() ? 0 : 1;
    }
#ifdef OBOL_SWRAST_BUILD
    if (SoGLContext_debug()) {
      cc_debugerror_postinfo("SoGLContext_instance", "Context check flag: %d", chk);
    }
#endif
    if (chk) {
#ifdef OBOL_SWRAST_BUILD
      if (SoGLContext_debug()) {
        cc_debugerror_postinfo("SoGLContext_instance", "About to call coin_gl_current_context()");
      }
#endif
      const void * current_ctx = coin_gl_current_context();
#ifdef OBOL_SWRAST_BUILD
      if (SoGLContext_debug()) {
        cc_debugerror_postinfo("SoGLContext_instance", "coin_gl_current_context() returned: %p", current_ctx);
#ifndef SOGL_PREFIX_SET
        SoDB::ContextManager* manager = coingl_get_context_manager(contextid);
        cc_debugerror_postinfo("SoGLContext_instance", "context_manager = %p", manager);
#endif
      }
#endif
      // For callback-based contexts, coin_gl_current_context() always returns NULL.
      // Skip the assertion when a per-context manager is registered (system-GL build)
      // or when we are in the OSMesa compilation unit (all contexts are managed there).
#ifndef SOGL_PREFIX_SET
      SoDB::ContextManager* manager = coingl_get_context_manager(contextid);
      if (!manager) {
        assert(current_ctx && "Must have a current GL context when instantiating SoGLContext!! (Note: if you are using an old Mesa GL version, set the environment variable OBOL_GL_NO_CURRENT_CONTEXT_CHECK to get around what may be a Mesa bug.)");
      }
#ifdef OBOL_SWRAST_BUILD
      else {
        if (SoGLContext_debug()) {
          cc_debugerror_postinfo("SoGLContext_instance", "Skipping context check for callback-based contexts");
        }
      }
#endif
#else /* SOGL_PREFIX_SET: osmesa build – all contexts are managed, skip assertion */
      if (SoGLContext_debug()) {
        cc_debugerror_postinfo("SoGLContext_instance", "Skipping context check (osmesa build)");
      }
#endif /* !SOGL_PREFIX_SET */
      (void)current_ctx; /* avoid unused variable warning */
    }

    /* FIXME: this is not free'd until app exit, which is bad because
       it opens a small window of possibility for errors; the value of
       id/key inputs could in principle be reused, so we'd get an old,
       invalid instance instead of creating a new one. Should rather
       hook into SoContextHandler and kill off an instance when a GL
       context is taken out. 20051104 mortene. */
    gi = (SoGLContext*)malloc(sizeof(SoGLContext));
    /* clear to set all pointers and variables to NULL or 0 */
    memset(gi, 0, sizeof(SoGLContext));
    /* FIXME: handle out-of-memory on malloc(). 20000928 mortene. */

    gi->contextid = (uint32_t) contextid;

    /* Record the per-context manager so that SoGLContext_getprocaddress()
       can use the correct backend resolver without consulting the global.
       In the OSMesa compilation unit (SOGL_PREFIX_SET), the registry is not
       available; proc-address lookup goes through OSMesaGetProcAddress instead. */
#ifndef SOGL_PREFIX_SET
    gi->context_manager = coingl_get_context_manager(contextid);
#endif

    /* create dict that makes a quick lookup for GL extensions */
    gi->glextdict = cc_dict_construct(256, 0.75f);

    ptr = gi;
    cc_dict_put(gldict, (uintptr_t)contextid, ptr);

    /*
       Make sure all GL errors are cleared before we do our assert
       test below. The OpenGL context might be set up by the user, and
       it's better to print a warning than asserting here if the user
       did something wrong while creating it.
    */
    glerr = glGetError();
    while (glerr != GL_NO_ERROR) {
      const char* errorstr = coin_glerror_string(glerr);
      cc_debugerror_postinfo("SoGLContext_instance",
                             "Clearing pre-existing OpenGL error 0x%x (%s) during context "
                             "initialization. This can occur during normal cleanup or if the "
                             "context was set up incorrectly.", 
                             glerr, errorstr ? errorstr : "unknown");
      glerr = glGetError();

      /* We might get this error if there is no current context.
         Break out and assert later in that case */
      if (glerr == GL_INVALID_OPERATION) break;
    }

    /* NB: if you are getting a crash here, it's because an attempt at
     * setting up a SoGLContext instance was made when there is no
     * current OpenGL context. */
    gi->versionstr = (const char *)glGetString(GL_VERSION);
    
    /* Additional debugging for OSMesa context */
    if (SoGLContext_debug()) {
      cc_debugerror_postinfo("SoGLContext_instance", "glGetString(GL_VERSION) returned: %p", gi->versionstr);
      if (gi->versionstr) {
        /* Try to safely check if the string is readable */
        volatile char testchar = gi->versionstr[0];
        cc_debugerror_postinfo("SoGLContext_instance", "Version string first char: 0x%02x ('%c')", 
                              (unsigned char)testchar, (testchar >= 32 && testchar < 127) ? testchar : '?');
      }
    }
    
    if (!gi->versionstr) {
      cc_debugerror_postwarning("SoGLContext_instance",
                                "glGetString(GL_VERSION) returned NULL -- no current GL context?");
      (void)cc_dict_remove(gldict, (uintptr_t)contextid);
      if (gi->glextdict) { cc_dict_destruct(gi->glextdict); }
      free(gi);
      return NULL;
    }

    glglue_set_glVersion(gi);

#ifdef OBOL_SWRAST_BUILD
    if (SoGLContext_debug()) {
      cc_debugerror_postinfo("SoGLContext_instance", "About to call glGetString(GL_VENDOR)");
    }
#endif

  /* Platform-specific initialization is no longer needed with callback-based contexts.
     Applications are responsible for providing complete OpenGL contexts through callbacks. */

    gi->vendorstr = (const char *)glGetString(GL_VENDOR);

#ifdef OBOL_SWRAST_BUILD
    if (SoGLContext_debug()) {
      cc_debugerror_postinfo("SoGLContext_instance", "glGetString(GL_VENDOR)=='%s' (=> vendor_is_SGI==%s)", 
                            gi->vendorstr ? gi->vendorstr : "(null)",
                            strcmp((const char *)gi->vendorstr, "SGI") == 0 ? "TRUE" : "FALSE");
    }
#endif
    gi->vendor_is_SGI = strcmp((const char *)gi->vendorstr, "SGI") == 0;
    gi->vendor_is_intel =
      strstr((const char *)gi->vendorstr, "Tungsten") ||
      strstr((const char *)gi->vendorstr, "Intel");
    gi->vendor_is_ati = (strcmp((const char *) gi->vendorstr, "ATI Technologies Inc.") == 0);
    gi->vendor_is_3dlabs = strcmp((const char *) gi->vendorstr, "3Dlabs") == 0;

#ifdef OBOL_SWRAST_BUILD
    if (SoGLContext_debug()) {
      cc_debugerror_postinfo("SoGLContext_instance", "About to call glGetString(GL_RENDERER)");
    }
#endif

    // Add error checking to isolate the crash
    GLenum error_before_renderer = glGetError();
    if (error_before_renderer != GL_NO_ERROR) {
      cc_debugerror_postinfo("SoGLContext_instance", "OpenGL error before GL_RENDERER: 0x%x", error_before_renderer);
    }
    
    gi->rendererstr = (const char *)glGetString(GL_RENDERER);
    
    GLenum error_after_renderer = glGetError();
    if (error_after_renderer != GL_NO_ERROR) {
      cc_debugerror_postinfo("SoGLContext_instance", "OpenGL error after GL_RENDERER: 0x%x", error_after_renderer);
    }
    
#ifdef OBOL_SWRAST_BUILD
    if (SoGLContext_debug()) {
      cc_debugerror_postinfo("SoGLContext_instance", "GL_RENDERER call completed, rendererstr = %p", gi->rendererstr);
      if (gi->rendererstr) {
        cc_debugerror_postinfo("SoGLContext_instance", "Renderer string: %s", gi->rendererstr);
      }
    }
#endif
    
    gi->extensionsstr = (const char *)glGetString(GL_EXTENSIONS);

#ifdef OBOL_SWRAST_BUILD
    if (SoGLContext_debug()) {
      cc_debugerror_postinfo("SoGLContext_instance", "Extensions string: %s", 
                            gi->extensionsstr ? gi->extensionsstr : "(null)");
    }
#endif

    /* Randall O'Reilly reports that the above call is deprecated from OpenGL 3.0
       onwards and may, particularly on some Linux systems, return NULL.

       The recommended method is to use glGetStringi to get each string in turn.
       The following code, supplied by Randall, implements this to end up with the
       same result as the old method.
    */
    if (gi->extensionsstr == NULL) {
#ifdef OBOL_SWRAST_BUILD
      if (SoGLContext_debug()) {
        cc_debugerror_postinfo("SoGLContext_instance", "Extensions string is NULL, trying glGetStringi fallback");
      }
#endif
      OBOL_PFNGLGETSTRINGIPROC glGetStringi = NULL;
      glGetStringi = (OBOL_PFNGLGETSTRINGIPROC)SoGLContext_getprocaddress(gi, "glGetStringi");
#ifdef OBOL_SWRAST_BUILD
      if (SoGLContext_debug()) {
        cc_debugerror_postinfo("SoGLContext_instance", "glGetStringi = %p", glGetStringi);
      }
#endif
      if (glGetStringi != NULL) {
        GLint num_strings = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &num_strings);
        if (num_strings > 0) {
          int buffer_size = 1024;
          char *ext_strings_buffer = (char *)malloc(buffer_size * sizeof (char));
          int buffer_pos = 0;
          for (int i_string = 0 ; i_string < num_strings ; i_string++) {
            const char * extension_string = (char *)glGetStringi (GL_EXTENSIONS, i_string);
            int extension_string_length = (int)strlen(extension_string);
            if (buffer_pos + extension_string_length + 1 > buffer_size) {
              buffer_size += 1024;
              ext_strings_buffer = (char *)realloc(ext_strings_buffer, buffer_size * sizeof (char));
            }
            strcpy(ext_strings_buffer + buffer_pos, extension_string);
            buffer_pos += extension_string_length;
            ext_strings_buffer[buffer_pos++] = ' '; // Space separated, overwrites NULL.
          }
          ext_strings_buffer[++buffer_pos] = '\0';  // NULL terminate.
          gi->extensionsstr = ext_strings_buffer;   // Handing over ownership, don't free here.
        } else {
          cc_debugerror_postwarning ("SoGLContext_instance",
                                     "glGetIntegerv(GL_NUM_EXTENSIONS) did not return a value, "
                                     "so unable to get extensions for this GL driver, ",
                                     "version: %s, vendor: %s", gi->versionstr, gi->vendorstr);
        }
      } else {
        cc_debugerror_postwarning ("SoGLContext_instance",
                                   "glGetString(GL_EXTENSIONS) returned null, but glGetStringi "
                                   "procedure not found, so unable to get extensions for this GL driver, "
                                   "version: %s, vendor: %s", gi->versionstr, gi->vendorstr);
      }
    }

    /* read some limits */

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &gltmp);
    gi->max_texture_size = gltmp;

    glGetIntegerv(GL_MAX_LIGHTS, &gltmp);
    gi->max_lights = (int) gltmp;

    {
      GLfloat vals[2];
      glGetFloatv(GL_POINT_SIZE_RANGE, vals);

      /* Matthias Koenig reported on coin-discuss that the OpenGL
         implementation on SGI Onyx 2 InfiniteReality returns 0 for the
         lowest pointsize, but it will still set the return value of
         glGetError() to GL_INVALID_VALUE if this size is attempted
         used. So the boundary range fix in the next line of code is a
         workaround for that OpenGL implementation bug.

         0.0f and lower values are explicitly disallowed, according to
         the OpenGL 1.3 specification, Chapter 3.3. */
      if (vals[0] <= 0.0f) {
        vals[0] = vals[1] < 1.0f ? vals[1] : 1.0f;
      }
      gi->point_size_range[0] = vals[0];
      gi->point_size_range[1] = vals[1];
    }
    {
      GLfloat vals[2];
      glGetFloatv(GL_LINE_WIDTH_RANGE, vals);

      /* Matthias Koenig reported on coin-discuss that the OpenGL
         implementation on SGI Onyx 2 InfiniteReality returns 0 for the
         lowest linewidth, but it will still set the return value of
         glGetError() to GL_INVALID_VALUE if this size is attempted
         used. This is a workaround for what looks like an OpenGL bug. */

      if (vals[0] <= 0.0f) {
        vals[0] = vals[1] < 1.0f ? vals[1] : 1.0f;
      }
      gi->line_width_range[0] = vals[0];
      gi->line_width_range[1] = vals[1];
    }

    if (SoGLContext_debug()) {
      cc_debugerror_postinfo("SoGLContext_instance",
                             "glGetString(GL_VENDOR)=='%s' (=> vendor_is_SGI==%s)",
                             gi->vendorstr,
                             gi->vendor_is_SGI ? "TRUE" : "FALSE");
      cc_debugerror_postinfo("SoGLContext_instance",
                             "glGetString(GL_RENDERER)=='%s'",
                             gi->rendererstr);
      cc_debugerror_postinfo("SoGLContext_instance",
                             "glGetString(GL_EXTENSIONS)=='%s'",
                             gi->extensionsstr);

      cc_debugerror_postinfo("SoGLContext_instance",
                             "Rendering is direct (GLX context info no longer tracked).");
    }

    /* anisotropic test */
    gi->can_do_anisotropic_filtering = FALSE;
    gi->max_anisotropy = 0.0f;
    if (SoGLContext_glext_supported(gi, "GL_EXT_texture_filter_anisotropic")) {
      gi->can_do_anisotropic_filtering = TRUE;
      glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &gi->max_anisotropy);
      if (SoGLContext_debug()) {
        cc_debugerror_postinfo("SoGLContext_instance",
                               "Anisotropic filtering: %s (%g)",
                               gi->can_do_anisotropic_filtering ? "TRUE" : "FALSE",
                               gi->max_anisotropy);
      }
    }

    glglue_check_driver(gi->vendorstr, gi->rendererstr, gi->versionstr);

    gi->non_power_of_two_textures =
      (SoGLContext_glversion_matches_at_least(gi, 2, 1, 0) ||
       SoGLContext_glext_supported(gi, "GL_ARB_texture_non_power_of_two"));

    /* Resolve our function pointers. */
      glglue_resolve_symbols(gi);
  }
  else {
    gi = (SoGLContext *)ptr;
  }


#ifdef OBOL_SWRAST_BUILD
  if (SoGLContext_debug()) {
    cc_debugerror_postinfo("SoGLContext_instance", "About to execute instance created callbacks");
  }
#endif

  if (!found && gl_instance_created_cblist) {
    int i, n = cc_list_get_length(gl_instance_created_cblist) / 2;
    for (i = 0; i < n; i++) {
      SoGLContext_instance_created_cb * cb =
        (SoGLContext_instance_created_cb *) cc_list_get(gl_instance_created_cblist, i*2);
      cb(contextid, cc_list_get(gl_instance_created_cblist, i*2+1));
    }
  }

#ifdef OBOL_SWRAST_BUILD
  if (SoGLContext_debug()) {
    cc_debugerror_postinfo("SoGLContext_instance", "RETURN: SoGLContext_instance returning successfully");
  }
#endif

  return gi;
}

const SoGLContext *
SoGLContext_instance_from_context_ptr(void * ctx)
{
  /* The id can really be anything unique for the current context, but
     we should avoid a crash with the possible ids defined by
     SoGLCacheContextElement. It's a bit of a hack, this. */

  /* MSVC7 on 64-bit Windows wants this extra cast. */
  const uintptr_t cast_aid = (uintptr_t)ctx;
  /* FIXME: holy shit! This doesn't look sensible at all! (Could this
     e.g. be where the remote rendering bugs are coming from?)
     20050525 mortene.*/
  const int id = (int)cast_aid;

  return SoGLContext_instance(id);
}

void
SoGLContext_destruct(uint32_t contextid)
{
#if defined(OBOL_DUAL_GL_BUILD) && !defined(SOGL_PREFIX_SET)
  /* In dual-GL builds, OSMesa contexts are stored in the osmesa variant's
     own dictionary.  Dispatch to the osmesa implementation and clean up the
     backend registry entries so the context ID can be treated as system-GL in
     the future if it were ever reused (monotonic IDs make reuse impossible
     in practice, but correctness demands we keep the registries consistent). */
  if (coingl_context_backend_is_osmesa(static_cast<int>(contextid))) {
    osmesa_SoGLContext_destruct(contextid);
    coingl_unregister_osmesa_context(static_cast<int>(contextid));
    coingl_unregister_context_manager(static_cast<int>(contextid));
    return;
  }
#endif
#ifndef SOGL_PREFIX_SET
  coingl_unregister_context_manager(static_cast<int>(contextid));
#endif
  SbBool found;
  void * ptr;
  if (gldict) { // might happen if a context is destructed without using the SoGLContext interface
    found = cc_dict_get(gldict, (uintptr_t)contextid, &ptr);
    if (found) {
      SoGLContext * glue = (SoGLContext*) ptr;
      if (glue->normalizationcubemap) {
        SoGLContext_glDeleteTextures(glue, 1, &glue->normalizationcubemap);
      }
      (void)cc_dict_remove(gldict, (uintptr_t)contextid);

      if (glue->dl_handle) {
        cc_dl_close(glue->dl_handle);
      }
    }
  }
}

SbBool
SoGLContext_isdirect(const SoGLContext * w)
{
  /* GLX direct-rendering detection was removed when context management was
     moved to SoDB::ContextManager callbacks.  Always returns TRUE (direct). */
  (void)w;
  return TRUE;
}


/* =========================================================================
 * Warn-once helpers for missing GL capabilities
 *
 * Each sogl_warn_* function posts a single SoDebugError warning the first
 * time an unsupported feature is used, then stays silent on subsequent
 * calls.  This lets the application discover capability gaps from the log
 * without being flooded by per-frame spam.
 *
 * The warnings deliberately name the missing extension / minimum GL version
 * so that users can quickly understand what their context must provide.
 * ========================================================================= */

static void sogl_warn_no_polygon_offset(void)
{
  static SbBool warned = FALSE;
  if (!warned) {
    warned = TRUE;
    cc_debugerror_postwarning("SoGLContext",
      "glPolygonOffset is not available in this GL context "
      "(requires OpenGL >= 1.1 or GL_EXT_polygon_offset). "
      "Polygon-offset rendering will be skipped.");
  }
}

static void sogl_warn_no_texture_objects(void)
{
  static SbBool warned = FALSE;
  if (!warned) {
    warned = TRUE;
    cc_debugerror_postwarning("SoGLContext",
      "Texture objects are not available in this GL context "
      "(requires OpenGL >= 1.1 or GL_EXT_texture_object). "
      "Texture rendering will be degraded or skipped.");
  }
}

static void sogl_warn_no_texsubimage(void)
{
  static SbBool warned = FALSE;
  if (!warned) {
    warned = TRUE;
    cc_debugerror_postwarning("SoGLContext",
      "glTexSubImage2D is not available in this GL context "
      "(requires OpenGL >= 1.1). "
      "Incremental texture updates will be skipped.");
  }
}

static void sogl_warn_no_3d_textures(void)
{
  static SbBool warned = FALSE;
  if (!warned) {
    warned = TRUE;
    cc_debugerror_postwarning("SoGLContext",
      "3D textures are not available in this GL context "
      "(requires OpenGL >= 1.2 or GL_EXT_texture3D). "
      "3D texture features will be skipped.");
  }
}

static void sogl_warn_no_multitexture(void)
{
  static SbBool warned = FALSE;
  if (!warned) {
    warned = TRUE;
    cc_debugerror_postwarning("SoGLContext",
      "Multitexture units are not available in this GL context "
      "(requires OpenGL >= 1.3 or GL_ARB_multitexture). "
      "Multitexture features will be skipped.");
  }
}

static void sogl_warn_no_client_attrib(void)
{
  static SbBool warned = FALSE;
  if (!warned) {
    warned = TRUE;
    cc_debugerror_postwarning("SoGLContext",
      "glPushClientAttrib/glPopClientAttrib are not available in this GL context. "
      "Client attribute stack operations will be skipped.");
  }
}

static void sogl_warn_no_compressed_textures(void)
{
  static SbBool warned = FALSE;
  if (!warned) {
    warned = TRUE;
    cc_debugerror_postwarning("SoGLContext",
      "Compressed textures are not available in this GL context "
      "(requires OpenGL >= 1.3 or GL_ARB_texture_compression). "
      "Compressed texture features will be skipped.");
  }
}

static void sogl_warn_no_color_tables(void)
{
  static SbBool warned = FALSE;
  if (!warned) {
    warned = TRUE;
    cc_debugerror_postwarning("SoGLContext",
      "Color table operations are not available in this GL context "
      "(requires GL_ARB_imaging or OpenGL >= 1.2 with imaging subset). "
      "Paletted texture features will be skipped.");
  }
}

static void sogl_warn_no_blend_equation(void)
{
  static SbBool warned = FALSE;
  if (!warned) {
    warned = TRUE;
    cc_debugerror_postwarning("SoGLContext",
      "glBlendEquation is not available in this GL context "
      "(requires OpenGL >= 1.4 or GL_EXT_blend_minmax). "
      "Advanced blending modes will be skipped.");
  }
}

static void sogl_warn_no_blend_func_separate(void)
{
  static SbBool warned = FALSE;
  if (!warned) {
    warned = TRUE;
    cc_debugerror_postwarning("SoGLContext",
      "glBlendFuncSeparate is not available in this GL context "
      "(requires OpenGL >= 1.4 or GL_EXT_blend_func_separate). "
      "Separate blend-function features will be skipped.");
  }
}

static void sogl_warn_no_vertex_arrays(void)
{
  static SbBool warned = FALSE;
  if (!warned) {
    warned = TRUE;
    cc_debugerror_postwarning("SoGLContext",
      "Vertex arrays are not available in this GL context "
      "(requires OpenGL >= 1.1). "
      "Vertex array rendering will be skipped.");
  }
}

static void sogl_warn_no_draw_range_elements(void)
{
  static SbBool warned = FALSE;
  if (!warned) {
    warned = TRUE;
    cc_debugerror_postwarning("SoGLContext",
      "glDrawRangeElements is not available in this GL context "
      "(requires OpenGL >= 1.2 or GL_EXT_draw_range_elements). "
      "Optimised indexed draw calls will fall back to glDrawElements.");
  }
}

static void sogl_warn_no_multidraw(void)
{
  static SbBool warned = FALSE;
  if (!warned) {
    warned = TRUE;
    cc_debugerror_postwarning("SoGLContext",
      "glMultiDrawArrays/glMultiDrawElements are not available in this GL context "
      "(requires OpenGL >= 1.4). "
      "Multi-draw operations will be skipped.");
  }
}

static void sogl_warn_no_nv_varray(void)
{
  static SbBool warned = FALSE;
  if (!warned) {
    warned = TRUE;
    cc_debugerror_postwarning("SoGLContext",
      "GL_NV_vertex_array_range is not available in this GL context. "
      "NV vertex array range operations will be skipped.");
  }
}

static void sogl_warn_no_vbo(void)
{
  static SbBool warned = FALSE;
  if (!warned) {
    warned = TRUE;
    cc_debugerror_postwarning("SoGLContext",
      "Vertex buffer objects are not available in this GL context "
      "(requires OpenGL >= 1.5 or GL_ARB_vertex_buffer_object). "
      "VBO-accelerated rendering will fall back to immediate mode.");
  }
}

static void sogl_warn_no_fbo(void)
{
  static SbBool warned = FALSE;
  if (!warned) {
    warned = TRUE;
    cc_debugerror_postwarning("SoGLContext",
      "Framebuffer objects are not available in this GL context "
      "(requires OpenGL >= 3.0 or GL_EXT_framebuffer_object). "
      "Features that require off-screen render targets — including "
      "SoSceneTexture2, shadow maps and some transparency modes — "
      "will be skipped. Provide a context with at least OpenGL 3.0 "
      "for full Obol functionality.");
  }
}

static void sogl_warn_no_core_func(const char * funcname)
{
  cc_debugerror_postwarning("SoGLContext",
    "Core GL function '%s' is not available in this context. "
    "This indicates a severely broken or uninitialized GL context. "
    "Rendering results will be incorrect.", funcname);
}

/* Alias used by the Python-generated guards below. */
static void sogl_warn_core_func_unavailable(const char * funcname)
{
  sogl_warn_no_core_func(funcname);
}

/*!
  Whether glPolygonOffset() is available or not: either we're on OpenGL
  1.1 or the GL_EXT_polygon_offset extension is available.

  Method then available for use:
  \li SoGLContext_glPolygonOffset
*/
SbBool
SoGLContext_has_polygon_offset(const SoGLContext * w)
{
  if (!glglue_allow_newer_opengl(w)) return FALSE;

  return (w->glPolygonOffset || w->glPolygonOffsetEXT) ? TRUE : FALSE;
}

/* Returns the glPolygonOffset() we're actually going to use. */
static OBOL_PFNGLPOLYGONOFFSETPROC
glglue_glPolygonOffset(const SoGLContext * w)
{
  OBOL_PFNGLPOLYGONOFFSETPROC poff = NULL;

  if (!w->glPolygonOffset && !w->glPolygonOffsetEXT) {
    sogl_warn_no_polygon_offset();
    return NULL;
  }

  poff = w->glPolygonOffset;

  /* Some SGI OpenGL 1.1 driver(s) seems to have a buggy
     implementation of glPolygonOffset(), according to pederb after
     some debugging he did for Fedem. These drivers'
     glPolygonOffsetEXT() actually seems to work better, so we prefer
     that if available. */
  if (w->vendor_is_SGI && w->glPolygonOffsetEXT &&
      SoGLContext_glversion_matches_at_least(w, 1, 1, 0) &&
      !SoGLContext_glversion_matches_at_least(w, 1, 2, 0)) {
    poff = w->glPolygonOffsetEXT;
  }

  /* Since we know glPolygonOffset() can be problematic, we also
     provide a way to prefer the EXT function instead through an
     environment variable "OBOL_PREFER_GLPOLYGONOFFSET_EXT" (which
     could be handy for help debugging remote systems, at least). */
  if (w->glPolygonOffsetEXT && glglue_prefer_glPolygonOffsetEXT()) {
    poff = w->glPolygonOffsetEXT;
  }

  /* If glPolygonOffset() is not available (and the function pointer
     was not set by any of the bug workaround if-checks above), fall
     back on extension. */
  if (poff == NULL) { poff = w->glPolygonOffsetEXT; }

  return poff;
}

/*!
  Enable or disable z-buffer offsetting for the given primitive types.
*/
void
SoGLContext_glPolygonOffsetEnable(const SoGLContext * w,
                                SbBool enable, int m)
{
  OBOL_PFNGLPOLYGONOFFSETPROC poff = glglue_glPolygonOffset(w);
  if (!poff) return;

  if (enable) {
    if (poff == w->glPolygonOffset) {
      if (m & SoGLContext_FILLED) glEnable(GL_POLYGON_OFFSET_FILL);
      else glDisable(GL_POLYGON_OFFSET_FILL);
      if (m & SoGLContext_LINES) glEnable(GL_POLYGON_OFFSET_LINE);
      else glDisable(GL_POLYGON_OFFSET_LINE);
      if (m & SoGLContext_POINTS) glEnable(GL_POLYGON_OFFSET_POINT);
      else glDisable(GL_POLYGON_OFFSET_POINT);
    }
    else { /* using glPolygonOffsetEXT() */
      /* The old pre-1.1 extension only supports filled polygon
         offsetting. */
      if (m & SoGLContext_FILLED) glEnable(GL_POLYGON_OFFSET_EXT);
      else glDisable(GL_POLYGON_OFFSET_EXT);

      if (SoGLContext_debug() && (m != SoGLContext_FILLED)) {
        static SbBool first = TRUE;
        if (first) {
          cc_debugerror_postwarning("SoGLContext_glPolygonOffsetEnable",
                                    "using EXT_polygon_offset, which only "
                                    "supports filled-polygon offsetting");
          first = FALSE;
        }
      }
    }
  }
  else { /* disable */
    if (poff == w->glPolygonOffset) {
      if (m & SoGLContext_FILLED) glDisable(GL_POLYGON_OFFSET_FILL);
      if (m & SoGLContext_LINES) glDisable(GL_POLYGON_OFFSET_LINE);
      if (m & SoGLContext_POINTS) glDisable(GL_POLYGON_OFFSET_POINT);
    }
    else { /* using glPolygonOffsetEXT() */
      if (m & SoGLContext_FILLED) glDisable(GL_POLYGON_OFFSET_EXT);
      /* Pre-1.1 glPolygonOffset extension only supported filled primitives.*/
    }
  }
}

void
SoGLContext_glPolygonOffset(const SoGLContext * w,
                          GLfloat factor,
                          GLfloat units)
{
  OBOL_PFNGLPOLYGONOFFSETPROC poff = glglue_glPolygonOffset(w);
  if (!poff) return;

  if (poff == w->glPolygonOffsetEXT) {
    /* Try to detect if user actually attempted to specify a valid
       bias value, like the old glPolygonOffsetEXT() extension
       needs. If not, assume that the "units" argument was set up for
       the "real" glPolygonOffset() function, and use a default value
       that should work fairly ok under most circumstances. */
    SbBool isbias = (units > 0.0f) && (units < 0.01f);
    if (!isbias) units = 0.000001f;

    /* FIXME: shouldn't there be an attempt to convert the other way
       around too? I.e., if it *is* a "bias" value and we're using the
       "real" 1.1 glPolygonOffset() function, try to convert it into a
       valid "units" value? 20020919 mortene. */
  }

  poff(factor, units);
}

/*!
  Whether 3D texture objects are available or not: either we're on OpenGL
  1.1, or the GL_EXT_texture_object extension is available.

  Methods then available for use:

  \li SoGLContext_glGenTextures
  \li SoGLContext_glBindTexture
  \li SoGLContext_glDeleteTextures
*/
SbBool
SoGLContext_has_texture_objects(const SoGLContext * w)
{
  if (!glglue_allow_newer_opengl(w)) return FALSE;

  return w->glGenTextures && w->glBindTexture && w->glDeleteTextures;
}

void
SoGLContext_glGenTextures(const SoGLContext * w, GLsizei n, GLuint * textures)
{
  if (!w || !w->glGenTextures) { sogl_warn_no_texture_objects(); return; }
  w->glGenTextures(n, textures);
}

void
SoGLContext_glBindTexture(const SoGLContext * w, GLenum target, GLuint texture)
{
  if (!w || !w->glBindTexture) { sogl_warn_no_texture_objects(); return; }
  w->glBindTexture(target, texture);
}

void
SoGLContext_glDeleteTextures(const SoGLContext * w, GLsizei n, const GLuint * textures)
{
  if (!w || !w->glDeleteTextures) { sogl_warn_no_texture_objects(); return; }
  w->glDeleteTextures(n, textures);
}

/*!
  Whether sub-textures are supported: either we're on OpenGL 1.2, or
  the GL_EXT_texture3D extension is available.

  Methods then available for use:

  \li SoGLContext_glTexImage3D
  \li SoGLContext_glTexSubImage3D
  \li SoGLContext_glCopyTexSubImage3D
*/
SbBool
SoGLContext_has_texsubimage(const SoGLContext * w)
{
  if (!glglue_allow_newer_opengl(w)) return FALSE;

  return w->glTexSubImage2D ? TRUE : FALSE;
}

void
SoGLContext_glTexSubImage2D(const SoGLContext * w,
                          GLenum target,
                          GLint level,
                          GLint xoffset,
                          GLint yoffset,
                          GLsizei width,
                          GLsizei height,
                          GLenum format,
                          GLenum type,
                          const GLvoid * pixels)
{
  if (!w || !w->glTexSubImage2D) { sogl_warn_no_texsubimage(); return; }
  w->glTexSubImage2D(target, level, xoffset, yoffset,
                     width, height, format, type, pixels);
}

/*!
  Whether 3D textures are available or not: either we're on OpenGL
  1.2, or the GL_EXT_texture3D extension is available.

  Methods then available for use:

  \li SoGLContext_glTexImage3D
  \li SoGLContext_glTexSubImage3D
  \li SoGLContext_glCopyTexSubImage3D
*/
SbBool
SoGLContext_has_3d_textures(const SoGLContext * w)
{
  if (!glglue_allow_newer_opengl(w)) return FALSE;

  return
    w->glTexImage3D &&
    w->glCopyTexSubImage3D &&
    w->glTexSubImage3D;
}

SbBool
SoGLContext_has_2d_proxy_textures(const SoGLContext * w)
{
  if (!glglue_allow_newer_opengl(w)) return FALSE;

  // Our Proxy code seems to not be compatible with Intel drivers
  // FIXME: should be handled by SoGLDriverDatabase
  if (w->vendor_is_intel) return FALSE;

  /* FIXME: there are differences between the 1.1 proxy mechanisms and
     the GL_EXT_texture proxy extension; the 1.1 support considers
     mipmaps. I think. Check documentation in the GL spec. If that is
     correct, we can't really use them interchangeable versus each
     other like we now do in Coin code. 20030121 mortene. */
  return
    SoGLContext_glversion_matches_at_least(w, 1, 1, 0) ||
    SoGLContext_glext_supported(w, "GL_EXT_texture");
}

SbBool
SoGLContext_has_texture_edge_clamp(const SoGLContext * w)
{
  static int buggytrident = -1;

  if (!glglue_allow_newer_opengl(w)) return FALSE;

  if (buggytrident == -1) {
    buggytrident = glglue_check_trident_clampedge_bug(w->vendorstr,
                                                      w->rendererstr,
                                                      w->versionstr);
  }
  if (buggytrident) { return FALSE; }

  return
    SoGLContext_glversion_matches_at_least(w, 1, 2, 0) ||
    SoGLContext_glext_supported(w, "GL_EXT_texture_edge_clamp") ||
    SoGLContext_glext_supported(w, "GL_SGIS_texture_edge_clamp");
}

void
SoGLContext_glPushClientAttrib(const SoGLContext * w, GLbitfield mask)
{
  if (!glglue_allow_newer_opengl(w)) return;
  if (!w->glPushClientAttrib) { sogl_warn_no_client_attrib(); return; }
  w->glPushClientAttrib(mask);
}

void
SoGLContext_glPopClientAttrib(const SoGLContext * w)
{
  if (!glglue_allow_newer_opengl(w)) return;
  if (!w->glPopClientAttrib) { sogl_warn_no_client_attrib(); return; }
  w->glPopClientAttrib();
}


SbBool
SoGLContext_has_multitexture(const SoGLContext * w)
{
  if (!glglue_allow_newer_opengl(w)) return FALSE;
  return w->glActiveTexture != NULL;
}

int
SoGLContext_max_texture_units(const SoGLContext * w)
{
  if (!glglue_allow_newer_opengl(w)) return 1;
  return w->maxtextureunits; /* will be 1 when multitexturing is not available */
}


void
SoGLContext_glTexImage3D(const SoGLContext * w,
                       GLenum target,
                       GLint level,
                       GLenum internalformat,
                       GLsizei width,
                       GLsizei height,
                       GLsizei depth,
                       GLint border,
                       GLenum format,
                       GLenum type,
                       const GLvoid *pixels)
{
  if (!w || !w->glTexImage3D) { sogl_warn_no_3d_textures(); return; }
  w->glTexImage3D(target, level, internalformat,
                  width, height, depth, border,
                  format, type, pixels);
}

void
SoGLContext_glTexSubImage3D(const SoGLContext * w,
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
                          const GLvoid * pixels)
{
  if (!w || !w->glTexSubImage3D) { sogl_warn_no_3d_textures(); return; }
  w->glTexSubImage3D(target, level, xoffset, yoffset,
                     zoffset, width, height, depth, format,
                     type, pixels);
}

void
SoGLContext_glCopyTexSubImage3D(const SoGLContext * w,
                              GLenum target,
                              GLint level,
                              GLint xoffset,
                              GLint yoffset,
                              GLint zoffset,
                              GLint x,
                              GLint y,
                              GLsizei width,
                              GLsizei height)
{
  if (!w || !w->glCopyTexSubImage3D) { sogl_warn_no_3d_textures(); return; }
  w->glCopyTexSubImage3D(target,
                         level,
                         xoffset,
                         yoffset,
                         zoffset,
                         x,
                         y,
                         width,
                         height);
}

void
SoGLContext_glActiveTexture(const SoGLContext * w,
                          GLenum texture)
{
  if (!w || !w->glActiveTexture) { sogl_warn_no_multitexture(); return; }
  w->glActiveTexture(texture);
}

void
SoGLContext_glClientActiveTexture(const SoGLContext * w,
                                GLenum texture)
{
  if (!w->glClientActiveTexture && texture == GL_TEXTURE0)
    return;
  if (!w->glClientActiveTexture) { sogl_warn_no_multitexture(); return; }
  w->glClientActiveTexture(texture);
}
void
SoGLContext_glMultiTexCoord2f(const SoGLContext * w,
                            GLenum target,
                            GLfloat s,
                            GLfloat t)
{
  if (!w || !w->glMultiTexCoord2f) { sogl_warn_no_multitexture(); return; }
  w->glMultiTexCoord2f(target, s, t);
}

void
SoGLContext_glMultiTexCoord2fv(const SoGLContext * w,
                             GLenum target,
                             const GLfloat * v)
{
  if (!w || !w->glMultiTexCoord2fv) { sogl_warn_no_multitexture(); return; }
  w->glMultiTexCoord2fv(target, v);
}

void
SoGLContext_glMultiTexCoord3fv(const SoGLContext * w,
                             GLenum target,
                             const GLfloat * v)
{
  if (!w || !w->glMultiTexCoord3fv) { sogl_warn_no_multitexture(); return; }
  w->glMultiTexCoord3fv(target, v);
}

void
SoGLContext_glMultiTexCoord4fv(const SoGLContext * w,
                             GLenum target,
                             const GLfloat * v)
{
  if (!w || !w->glMultiTexCoord4fv) { sogl_warn_no_multitexture(); return; }
  w->glMultiTexCoord4fv(target, v);
}

SbBool
cc_glue_has_texture_compression(const SoGLContext * glue)
{
  if (!glglue_allow_newer_opengl(glue)) return FALSE;

  return
    glue->glCompressedTexImage1D &&
    glue->glCompressedTexImage2D &&
    glue->glCompressedTexImage3D &&
    glue->glGetCompressedTexImage;
}

SbBool
cc_glue_has_texture_compression_2d(const SoGLContext * glue)
{
  if (!glglue_allow_newer_opengl(glue)) return FALSE;
  return glue->glCompressedTexImage2D && glue->glGetCompressedTexImage;
}

SbBool
cc_glue_has_texture_compression_3d(const SoGLContext * glue)
{
  if (!glglue_allow_newer_opengl(glue)) return FALSE;
  return glue->glCompressedTexImage3D && glue->glGetCompressedTexImage;
}

void
SoGLContext_glCompressedTexImage3D(const SoGLContext * glue,
                                 GLenum target,
                                 GLint level,
                                 GLenum internalformat,
                                 GLsizei width,
                                 GLsizei height,
                                 GLsizei depth,
                                 GLint border,
                                 GLsizei imageSize,
                                 const GLvoid * data)
{
  if (!glue || !glue->glCompressedTexImage3D) { sogl_warn_no_compressed_textures(); return; }
  glue->glCompressedTexImage3D(target,
                               level,
                               internalformat,
                               width,
                               height,
                               depth,
                               border,
                               imageSize,
                               data);
}

void
SoGLContext_glCompressedTexImage2D(const SoGLContext * glue,
                                 GLenum target,
                                 GLint level,
                                 GLenum internalformat,
                                 GLsizei width,
                                 GLsizei height,
                                 GLint border,
                                 GLsizei imageSize,
                                 const GLvoid *data)
{
  if (!glue || !glue->glCompressedTexImage2D) { sogl_warn_no_compressed_textures(); return; }
  glue->glCompressedTexImage2D(target,
                               level,
                               internalformat,
                               width,
                               height,
                               border,
                               imageSize,
                               data);
}

void
SoGLContext_glCompressedTexImage1D(const SoGLContext * glue,
                                 GLenum target,
                                 GLint level,
                                 GLenum internalformat,
                                 GLsizei width,
                                 GLint border,
                                 GLsizei imageSize,
                                 const GLvoid *data)
{
  if (!glue || !glue->glCompressedTexImage1D) { sogl_warn_no_compressed_textures(); return; }
  glue->glCompressedTexImage1D(target,
                               level,
                               internalformat,
                               width,
                               border,
                               imageSize,
                               data);
}

void
SoGLContext_glCompressedTexSubImage3D(const SoGLContext * glue,
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
                                    const GLvoid *data)
{
  if (!glue || !glue->glCompressedTexSubImage3D) { sogl_warn_no_compressed_textures(); return; }
  glue->glCompressedTexSubImage3D(target,
                                  level,
                                  xoffset,
                                  yoffset,
                                  zoffset,
                                  width,
                                  height,
                                  depth,
                                  format,
                                  imageSize,
                                  data);
}

void
SoGLContext_glCompressedTexSubImage2D(const SoGLContext * glue,
                                    GLenum target,
                                    GLint level,
                                    GLint xoffset,
                                    GLint yoffset,
                                    GLsizei width,
                                    GLsizei height,
                                    GLenum format,
                                    GLsizei imageSize,
                                    const GLvoid *data)
{
  if (!glue || !glue->glCompressedTexSubImage2D) { sogl_warn_no_compressed_textures(); return; }
  glue->glCompressedTexSubImage2D(target,
                                  level,
                                  xoffset,
                                  yoffset,
                                  width,
                                  height,
                                  format,
                                  imageSize,
                                  data);
}

void
SoGLContext_glCompressedTexSubImage1D(const SoGLContext * glue,
                                    GLenum target,
                                    GLint level,
                                    GLint xoffset,
                                    GLsizei width,
                                    GLenum format,
                                    GLsizei imageSize,
                                    const GLvoid *data)
{
  if (!glue || !glue->glCompressedTexSubImage1D) { sogl_warn_no_compressed_textures(); return; }
  glue->glCompressedTexSubImage1D(target,
                                  level,
                                  xoffset,
                                  width,
                                  format,
                                  imageSize,
                                  data);
}

void
SoGLContext_glGetCompressedTexImage(const SoGLContext * glue,
                                  GLenum target,
                                  GLint level,
                                  void * img)
{
  if (!glue || !glue->glGetCompressedTexImage) { sogl_warn_no_compressed_textures(); return; }
  glue->glGetCompressedTexImage(target,
                                level,
                                img);
}

SbBool
SoGLContext_has_paletted_textures(const SoGLContext * glue)
{
  static int disable = -1;
  if (disable == -1) {
    disable = glglue_resolve_envvar("OBOL_GLGLUE_DISABLE_PALETTED_TEXTURE");
  }
  if (disable) { return FALSE; }

  if (!glglue_allow_newer_opengl(glue)) { return FALSE; }
  return glue->supportsPalettedTextures;
}

SbBool
SoGLContext_has_color_tables(const SoGLContext * glue)
{
  if (!glglue_allow_newer_opengl(glue)) return FALSE;
  return glue->glColorTable != NULL;
}

SbBool
SoGLContext_has_color_subtables(const SoGLContext * glue)
{
  if (!glglue_allow_newer_opengl(glue)) return FALSE;
  return glue->glColorSubTable != NULL;
}

void
SoGLContext_glColorTable(const SoGLContext * glue,
                       GLenum target,
                       GLenum internalFormat,
                       GLsizei width,
                       GLenum format,
                       GLenum type,
                       const GLvoid *table)
{
  if (!glue || !glue->glColorTable) { sogl_warn_no_color_tables(); return; }
  glue->glColorTable(target,
                     internalFormat,
                     width,
                     format,
                     type,
                     table);
}

void
SoGLContext_glColorSubTable(const SoGLContext * glue,
                          GLenum target,
                          GLsizei start,
                          GLsizei count,
                          GLenum format,
                          GLenum type,
                          const GLvoid * data)
{
  if (!glue || !glue->glColorSubTable) { sogl_warn_no_color_tables(); return; }
  glue->glColorSubTable(target,
                        start,
                        count,
                        format,
                        type,
                        data);
}

void
SoGLContext_glGetColorTable(const SoGLContext * glue,
                          GLenum target,
                          GLenum format,
                          GLenum type,
                          GLvoid *data)
{
  if (!glue || !glue->glGetColorTable) { sogl_warn_no_color_tables(); return; }
  glue->glGetColorTable(target,
                        format,
                        type,
                        data);
}

void
SoGLContext_glGetColorTableParameteriv(const SoGLContext * glue,
                                     GLenum target,
                                     GLenum pname,
                                     GLint *params)
{
  if (!glue || !glue->glGetColorTableParameteriv) { sogl_warn_no_color_tables(); return; }
  glue->glGetColorTableParameteriv(target,
                                   pname,
                                   params);
}

void
SoGLContext_glGetColorTableParameterfv(const SoGLContext * glue,
                                     GLenum target,
                                     GLenum pname,
                                     GLfloat *params)
{
  if (!glue || !glue->glGetColorTableParameterfv) { sogl_warn_no_color_tables(); return; }
  glue->glGetColorTableParameterfv(target,
                                   pname,
                                   params);
}

SbBool
SoGLContext_has_blendequation(const SoGLContext * glue)
{
  if (!glglue_allow_newer_opengl(glue)) return FALSE;

  return glue->glBlendEquation || glue->glBlendEquationEXT;
}

void
SoGLContext_glBlendEquation(const SoGLContext * glue, GLenum mode)
{
  if (!glue->glBlendEquation && !glue->glBlendEquationEXT) { sogl_warn_no_blend_equation(); return; }

  if (glue->glBlendEquation) glue->glBlendEquation(mode);
  else glue->glBlendEquationEXT(mode);
}

SbBool
SoGLContext_has_blendfuncseparate(const SoGLContext * glue)
{
  if (!glglue_allow_newer_opengl(glue)) return FALSE;

  return glue->glBlendFuncSeparate != NULL;
}

void
SoGLContext_glBlendFuncSeparate(const SoGLContext * glue,
                              GLenum rgbsrc, GLenum rgbdst,
                              GLenum alphasrc, GLenum alphadst)
{
  if (!glue || !glue->glBlendFuncSeparate) { sogl_warn_no_blend_func_separate(); return; }
  glue->glBlendFuncSeparate(rgbsrc, rgbdst, alphasrc, alphadst);
}

SbBool
SoGLContext_has_vertex_array(const SoGLContext * glue)
{
  if (!glglue_allow_newer_opengl(glue)) return FALSE;
  return glue->glVertexPointer != NULL;
}

void
SoGLContext_glVertexPointer(const SoGLContext * glue,
                          GLint size, GLenum type, GLsizei stride, const GLvoid * pointer)
{
  if (!glue || !glue->glVertexPointer) { sogl_warn_no_vertex_arrays(); return; }
  glue->glVertexPointer(size, type, stride, pointer);
}

void
SoGLContext_glTexCoordPointer(const SoGLContext * glue,
                            GLint size, GLenum type,
                            GLsizei stride, const GLvoid * pointer)
{
  if (!glue || !glue->glTexCoordPointer) { sogl_warn_no_vertex_arrays(); return; }
  glue->glTexCoordPointer(size, type, stride, pointer);
}

void
SoGLContext_glNormalPointer(const SoGLContext * glue,
                          GLenum type, GLsizei stride, const GLvoid *pointer)
{
  if (!glue || !glue->glNormalPointer) { sogl_warn_no_vertex_arrays(); return; }
  glue->glNormalPointer(type, stride, pointer);
}

void
SoGLContext_glColorPointer(const SoGLContext * glue,
                         GLint size, GLenum type,
                         GLsizei stride, const GLvoid * pointer)
{
  if (!glue || !glue->glColorPointer) { sogl_warn_no_vertex_arrays(); return; }
  glue->glColorPointer(size, type, stride, pointer);
}

void
SoGLContext_glIndexPointer(const SoGLContext * glue,
                         GLenum type, GLsizei stride, const GLvoid * pointer)
{
  if (!glue || !glue->glIndexPointer) { sogl_warn_no_vertex_arrays(); return; }
  glue->glIndexPointer(type, stride, pointer);
}

void
SoGLContext_glEnableClientState(const SoGLContext * glue, GLenum array)
{
  if (!glue || !glue->glEnableClientState) { sogl_warn_no_vertex_arrays(); return; }
  glue->glEnableClientState(array);
}

void
SoGLContext_glDisableClientState(const SoGLContext * glue, GLenum array)
{
  if (!glue || !glue->glDisableClientState) { sogl_warn_no_vertex_arrays(); return; }
  glue->glDisableClientState(array);
}

void
SoGLContext_glInterleavedArrays(const SoGLContext * glue,
                              GLenum format, GLsizei stride, const GLvoid * pointer)
{
  if (!glue || !glue->glInterleavedArrays) { sogl_warn_no_vertex_arrays(); return; }
  glue->glInterleavedArrays(format, stride, pointer);
}

void
SoGLContext_glDrawArrays(const SoGLContext * glue,
                       GLenum mode, GLint first, GLsizei count)
{
  if (!glue || !glue->glDrawArrays) { sogl_warn_no_vertex_arrays(); return; }
  glue->glDrawArrays(mode, first, count);
}

void
SoGLContext_glDrawElements(const SoGLContext * glue,
                         GLenum mode, GLsizei count, GLenum type,
                         const GLvoid * indices)
{
  if (!glue || !glue->glDrawElements) { sogl_warn_no_vertex_arrays(); return; }
  glue->glDrawElements(mode, count, type, indices);
}

void
SoGLContext_glDrawRangeElements(const SoGLContext * glue,
                              GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type,
                              const GLvoid * indices)
{
  if (!glue || !glue->glDrawRangeElements) { sogl_warn_no_draw_range_elements(); return; }
  glue->glDrawRangeElements(mode, start, end, count, type, indices);
}

void
SoGLContext_glArrayElement(const SoGLContext * glue, GLint i)
{
  if (!glue || !glue->glArrayElement) { sogl_warn_no_vertex_arrays(); return; }
  glue->glArrayElement(i);
}

SbBool
SoGLContext_has_multidraw_vertex_arrays(const SoGLContext * glue)
{
  if (!glglue_allow_newer_opengl(glue)) return FALSE;
  return glue->glMultiDrawArrays && glue->glMultiDrawElements;
}

void
SoGLContext_glMultiDrawArrays(const SoGLContext * glue, GLenum mode, const GLint * first,
                            const GLsizei * count, GLsizei primcount)
{
  if (!glue || !glue->glMultiDrawArrays) { sogl_warn_no_multidraw(); return; }
  glue->glMultiDrawArrays(mode, first, count, primcount);
}

void
SoGLContext_glMultiDrawElements(const SoGLContext * glue, GLenum mode, const GLsizei * count,
                              GLenum type, const GLvoid ** indices, GLsizei primcount)
{
  if (!glue || !glue->glMultiDrawElements) { sogl_warn_no_multidraw(); return; }
  glue->glMultiDrawElements(mode, count, type, indices, primcount);
}

SbBool
SoGLContext_has_nv_vertex_array_range(const SoGLContext * glue)
{
  if (!glglue_allow_newer_opengl(glue)) return FALSE;
  return glue->glVertexArrayRangeNV != NULL;
}

void
SoGLContext_glFlushVertexArrayRangeNV(const SoGLContext * glue)
{
  if (!glue || !glue->glFlushVertexArrayRangeNV) { sogl_warn_no_nv_varray(); return; }
  glue->glFlushVertexArrayRangeNV();
}

void
SoGLContext_glVertexArrayRangeNV(const SoGLContext * glue, GLsizei size, const GLvoid * pointer)
{
  if (!glue || !glue->glVertexArrayRangeNV) { sogl_warn_no_nv_varray(); return; }
  glue->glVertexArrayRangeNV(size, pointer);
}

void *
SoGLContext_glAllocateMemoryNV(const SoGLContext * glue,
                             GLsizei size, GLfloat readfreq,
                             GLfloat writefreq, GLfloat priority)
{
  if (!glue || !glue->glAllocateMemoryNV) { sogl_warn_no_nv_varray(); return NULL; }
  return glue->glAllocateMemoryNV(size, readfreq, writefreq, priority);
}

void
SoGLContext_glFreeMemoryNV(const SoGLContext * glue, GLvoid * buffer)
{
  if (!glue || !glue->glFreeMemoryNV) { sogl_warn_no_nv_varray(); return; }
  glue->glFreeMemoryNV(buffer);
}

SbBool
SoGLContext_has_vertex_buffer_object(const SoGLContext * glue)
{
  if (!glglue_allow_newer_opengl(glue)) return FALSE;

  /* check only one function for speed. It's set to NULL when
     initializing if one of the other functions wasn't found */
  return glue->glBindBuffer != NULL;
}

void
SoGLContext_glBindBuffer(const SoGLContext * glue, GLenum target, GLuint buffer)
{
  if (!glue || !glue->glBindBuffer) { sogl_warn_no_vbo(); return; }
  glue->glBindBuffer(target, buffer);
}

void
SoGLContext_glDeleteBuffers(const SoGLContext * glue, GLsizei n, const GLuint *buffers)
{
  if (!glue || !glue->glDeleteBuffers) { sogl_warn_no_vbo(); return; }
  glue->glDeleteBuffers(n, buffers);
}

void
SoGLContext_glGenBuffers(const SoGLContext * glue, GLsizei n, GLuint *buffers)
{
  if (!glue || !glue->glGenBuffers) { sogl_warn_no_vbo(); return; }
  glue->glGenBuffers(n, buffers);
}

GLboolean
SoGLContext_glIsBuffer(const SoGLContext * glue, GLuint buffer)
{
  if (!glue || !glue->glIsBuffer) { sogl_warn_no_vbo(); return GL_FALSE; }
  return glue->glIsBuffer(buffer);
}

void
SoGLContext_glBufferData(const SoGLContext * glue,
                       GLenum target,
                       intptr_t size, /* 64 bit on 64 bit systems */
                       const GLvoid *data,
                       GLenum usage)
{
  if (!glue || !glue->glBufferData) { sogl_warn_no_vbo(); return; }
  glue->glBufferData(target, size, data, usage);
}

void
SoGLContext_glBufferSubData(const SoGLContext * glue,
                          GLenum target,
                          intptr_t offset, /* 64 bit */
                          intptr_t size, /* 64 bit */
                          const GLvoid * data)
{
  if (!glue || !glue->glBufferSubData) { sogl_warn_no_vbo(); return; }
  glue->glBufferSubData(target, offset, size, data);
}

void
SoGLContext_glGetBufferSubData(const SoGLContext * glue,
                             GLenum target,
                             intptr_t offset, /* 64 bit */
                             intptr_t size, /* 64 bit */
                             GLvoid *data)
{
  if (!glue || !glue->glGetBufferSubData) { sogl_warn_no_vbo(); return; }
  glue->glGetBufferSubData(target, offset, size, data);
}

GLvoid *
SoGLContext_glMapBuffer(const SoGLContext * glue,
                      GLenum target, GLenum access)
{
  if (!glue || !glue->glMapBuffer) { sogl_warn_no_vbo(); return NULL; }
  return glue->glMapBuffer(target, access);
}

GLboolean
SoGLContext_glUnmapBuffer(const SoGLContext * glue,
                        GLenum target)
{
  if (!glue || !glue->glUnmapBuffer) { sogl_warn_no_vbo(); return GL_FALSE; }
  return glue->glUnmapBuffer(target);
}

void
SoGLContext_glGetBufferParameteriv(const SoGLContext * glue,
                                 GLenum target,
                                 GLenum pname,
                                 GLint * params)
{
  if (!glue || !glue->glGetBufferParameteriv) { sogl_warn_no_vbo(); return; }
  glue->glGetBufferParameteriv(target, pname, params);
}

void
SoGLContext_glGetBufferPointerv(const SoGLContext * glue,
                              GLenum target,
                              GLenum pname,
                              GLvoid ** params)
{
  if (!glue || !glue->glGetBufferPointerv) { sogl_warn_no_vbo(); return; }
  glue->glGetBufferPointerv(target, pname, params);
}


SbBool
SoGLContext_can_do_bumpmapping(const SoGLContext * glue)
{
  if (!glglue_allow_newer_opengl(glue)) return FALSE;
  return glue->can_do_bumpmapping;
}

SbBool
SoGLContext_can_do_sortedlayersblend(const SoGLContext * glue)
{
  if (!glglue_allow_newer_opengl(glue)) return FALSE;
  return glue->can_do_sortedlayersblend;
}

int
SoGLContext_get_max_lights(const SoGLContext * glue)
{
  return glue->max_lights;
}

const float *
SoGLContext_get_line_width_range(const SoGLContext * glue)
{
  return glue->line_width_range;
}

const float *
SoGLContext_get_point_size_range(const SoGLContext * glue)
{
  return glue->point_size_range;
}

/* GL_NV_register_combiners functions */
SbBool
SoGLContext_has_nv_register_combiners(const SoGLContext * glue)
{
  if (!glglue_allow_newer_opengl(glue)) return FALSE;
  return glue->has_nv_register_combiners;
}

void
SoGLContext_glCombinerParameterfvNV(const SoGLContext * glue,
                                  GLenum pname,
                                  const GLfloat *params)
{
  glue->glCombinerParameterfvNV(pname, params);
}

void
SoGLContext_glCombinerParameterivNV(const SoGLContext * glue,
                                  GLenum pname,
                                  const GLint *params)
{
  glue->glCombinerParameterivNV(pname, params);
}

void
SoGLContext_glCombinerParameterfNV(const SoGLContext * glue,
                                 GLenum pname,
                                 GLfloat param)
{
  glue->glCombinerParameterfNV(pname, param);
}

void
SoGLContext_glCombinerParameteriNV(const SoGLContext * glue,
                                 GLenum pname,
                                 GLint param)
{
  glue->glCombinerParameteriNV(pname, param);
}

void
SoGLContext_glCombinerInputNV(const SoGLContext * glue,
                            GLenum stage,
                            GLenum portion,
                            GLenum variable,
                            GLenum input,
                            GLenum mapping,
                            GLenum componentUsage)
{
  glue->glCombinerInputNV(stage, portion, variable, input, mapping, componentUsage);
}

void
SoGLContext_glCombinerOutputNV(const SoGLContext * glue,
                             GLenum stage,
                             GLenum portion,
                             GLenum abOutput,
                             GLenum cdOutput,
                             GLenum sumOutput,
                             GLenum scale,
                             GLenum bias,
                             GLboolean abDotProduct,
                             GLboolean cdDotProduct,
                             GLboolean muxSum)
{
  glue->glCombinerOutputNV(stage, portion, abOutput, cdOutput, sumOutput, scale, bias,
                           abDotProduct, cdDotProduct, muxSum);
}

void
SoGLContext_glFinalCombinerInputNV(const SoGLContext * glue,
                                 GLenum variable,
                                 GLenum input,
                                 GLenum mapping,
                                 GLenum componentUsage)
{
  glue->glFinalCombinerInputNV(variable, input, mapping, componentUsage);
}

void
SoGLContext_glGetCombinerInputParameterfvNV(const SoGLContext * glue,
                                          GLenum stage,
                                          GLenum portion,
                                          GLenum variable,
                                          GLenum pname,
                                          GLfloat *params)
{
  glue->glGetCombinerInputParameterfvNV(stage, portion, variable, pname, params);
}

void
SoGLContext_glGetCombinerInputParameterivNV(const SoGLContext * glue,
                                          GLenum stage,
                                          GLenum portion,
                                          GLenum variable,
                                          GLenum pname,
                                          GLint *params)
{
  glue->glGetCombinerInputParameterivNV(stage, portion, variable, pname, params);
}

void
SoGLContext_glGetCombinerOutputParameterfvNV(const SoGLContext * glue,
                                           GLenum stage,
                                           GLenum portion,
                                           GLenum pname,
                                           GLfloat *params)
{
  glue->glGetCombinerOutputParameterfvNV(stage, portion, pname, params);
}

void
SoGLContext_glGetCombinerOutputParameterivNV(const SoGLContext * glue,
                                           GLenum stage,
                                           GLenum portion,
                                           GLenum pname,
                                           GLint *params)
{
  glue->glGetCombinerOutputParameterivNV(stage, portion, pname, params);
}

void
SoGLContext_glGetFinalCombinerInputParameterfvNV(const SoGLContext * glue,
                                               GLenum variable,
                                               GLenum pname,
                                               GLfloat *params)
{
  glue->glGetFinalCombinerInputParameterfvNV(variable, pname, params);
}

void
SoGLContext_glGetFinalCombinerInputParameterivNV(const SoGLContext * glue,
                                               GLenum variable,
                                               GLenum pname,
                                               GLint *params)
{
  glue->glGetFinalCombinerInputParameterivNV(variable, pname, params);
}

/* ARB_shader_objects */
SbBool
SoGLContext_has_arb_shader_objects(const SoGLContext * glue)
{
  if (!glglue_allow_newer_opengl(glue)) return FALSE;
  return glue->has_arb_shader_objects;
}


/* ARB_fragment_program functions */
SbBool
SoGLContext_has_arb_fragment_program(const SoGLContext * glue)
{
  if (!glglue_allow_newer_opengl(glue)) return FALSE;
  return glue->has_arb_fragment_program;
}

void
SoGLContext_glProgramString(const SoGLContext * glue,
                          GLenum target,
                          GLenum format,
                          GLsizei len,
                          const GLvoid *string)
{
  glue->glProgramStringARB(target, format, len, string);
}

void
SoGLContext_glBindProgram(const SoGLContext * glue,
                        GLenum target,
                        GLuint program)
{
  glue->glBindProgramARB(target, program);
}

void
SoGLContext_glDeletePrograms(const SoGLContext * glue,
                           GLsizei n,
                           const GLuint *programs)
{
  glue->glDeleteProgramsARB(n, programs);
}

void
SoGLContext_glGenPrograms(const SoGLContext * glue,
                        GLsizei n,
                        GLuint *programs)
{
  glue->glGenProgramsARB(n, programs);
}

void
SoGLContext_glProgramEnvParameter4d(const SoGLContext * glue,
                                  GLenum target,
                                  GLuint index,
                                  GLdouble x,
                                  GLdouble y,
                                  GLdouble z,
                                  GLdouble w)
{
  glue->glProgramEnvParameter4dARB(target, index, x, y, z, w);
}

void
SoGLContext_glProgramEnvParameter4dv(const SoGLContext * glue,
                                   GLenum target,
                                   GLuint index,
                                   const GLdouble *params)
{
  glue->glProgramEnvParameter4dvARB(target, index, params);
}

void
SoGLContext_glProgramEnvParameter4f(const SoGLContext * glue,
                                  GLenum target,
                                  GLuint index,
                                  GLfloat x,
                                  GLfloat y,
                                  GLfloat z,
                                  GLfloat w)
{
  glue->glProgramEnvParameter4fARB(target, index, x, y, z, w);
}

void
SoGLContext_glProgramEnvParameter4fv(const SoGLContext * glue,
                                   GLenum target,
                                   GLuint index,
                                   const GLfloat *params)
{
  glue->glProgramEnvParameter4fvARB(target, index, params);
}

void
SoGLContext_glProgramLocalParameter4d(const SoGLContext * glue,
                                    GLenum target,
                                    GLuint index,
                                    GLdouble x,
                                    GLdouble y,
                                    GLdouble z,
                                    GLdouble w)
{
  glue->glProgramLocalParameter4dARB(target, index, x, y, z, w);
}

void
SoGLContext_glProgramLocalParameter4dv(const SoGLContext * glue,
                                     GLenum target,
                                     GLuint index,
                                     const GLdouble *params)
{
  glue->glProgramLocalParameter4dvARB(target, index, params);
}

void
SoGLContext_glProgramLocalParameter4f(const SoGLContext * glue,
                                    GLenum target,
                                    GLuint index,
                                    GLfloat x,
                                    GLfloat y,
                                    GLfloat z,
                                    GLfloat w)
{
  glue->glProgramLocalParameter4fARB(target, index, x, y, z, w);
}

void
SoGLContext_glProgramLocalParameter4fv(const SoGLContext * glue,
                                     GLenum target,
                                     GLuint index,
                                     const GLfloat *params)
{
  glue->glProgramLocalParameter4fvARB(target, index, params);
}

void
SoGLContext_glGetProgramEnvParameterdv(const SoGLContext * glue,
                                     GLenum target,
                                     GLuint index,
                                     GLdouble *params)
{
  glue->glGetProgramEnvParameterdvARB(target, index, params);
}

void
SoGLContext_glGetProgramEnvParameterfv(const SoGLContext * glue,
                                     GLenum target,
                                     GLuint index,
                                     GLfloat *params)
{
  glue->glGetProgramEnvParameterfvARB(target, index, params);
}

void
SoGLContext_glGetProgramLocalParameterdv(const SoGLContext * glue,
                                       GLenum target,
                                       GLuint index,
                                       GLdouble *params)
{
  glue->glGetProgramLocalParameterdvARB(target, index, params);
}

void
SoGLContext_glGetProgramLocalParameterfv(const SoGLContext * glue,
                                       GLenum target,
                                       GLuint index,
                                       GLfloat *params)
{
  glue->glGetProgramLocalParameterfvARB(target, index, params);
}

void
SoGLContext_glGetProgramiv(const SoGLContext * glue,
                         GLenum target,
                         GLenum pname,
                         GLint *params)
{
  glue->glGetProgramivARB(target, pname, params);
}

void
SoGLContext_glGetProgramString(const SoGLContext * glue,
                             GLenum target,
                             GLenum pname,
                             GLvoid *string)
{
  glue->glGetProgramStringARB(target, pname, string);
}

SbBool
SoGLContext_glIsProgram(const SoGLContext * glue,
                      GLuint program)
{
  return glue->glIsProgramARB(program);
}


/* ARB_vertex_program functions */
SbBool
SoGLContext_has_arb_vertex_program(const SoGLContext * glue)
{
  if (!glglue_allow_newer_opengl(glue)) return FALSE;
  return glue->has_arb_vertex_program;
}

/* ARB_vertex_shaders functions */
SbBool
SoGLContext_has_arb_vertex_shader(const SoGLContext * glue)
{
  if (!glglue_allow_newer_opengl(glue)) return FALSE;
  return glue->has_arb_vertex_shader;
}

void
SoGLContext_glVertexAttrib1s(const SoGLContext * glue,
                           GLuint index, GLshort x)
{
  glue->glVertexAttrib1sARB(index, x);
}

void
SoGLContext_glVertexAttrib1f(const SoGLContext * glue,
                           GLuint index, GLfloat x)
{
  glue->glVertexAttrib1fARB(index, x);
}

void
SoGLContext_glVertexAttrib1d(const SoGLContext * glue,
                           GLuint index, GLdouble x)
{
  glue->glVertexAttrib1dARB(index, x);
}

void
SoGLContext_glVertexAttrib2s(const SoGLContext * glue,
                           GLuint index, GLshort x, GLshort y)
{
  glue->glVertexAttrib2sARB(index, x, y);
}

void
SoGLContext_glVertexAttrib2f(const SoGLContext * glue,
                           GLuint index, GLfloat x, GLfloat y)
{
  glue->glVertexAttrib2fARB(index, x, y);
}

void
SoGLContext_glVertexAttrib2d(const SoGLContext * glue,
                           GLuint index, GLdouble x, GLdouble y)
{
  glue->glVertexAttrib2dARB(index, x, y);
}

void
SoGLContext_glVertexAttrib3s(const SoGLContext * glue,
                           GLuint index, GLshort x,
                           GLshort y, GLshort z)
{
  glue->glVertexAttrib3sARB(index, x, y, z);
}

void
SoGLContext_glVertexAttrib3f(const SoGLContext * glue,
                           GLuint index, GLfloat x,
                           GLfloat y, GLfloat z)
{
  glue->glVertexAttrib3fARB(index, x, y, z);
}

void
SoGLContext_glVertexAttrib3d(const SoGLContext * glue,
                           GLuint index, GLdouble x,
                           GLdouble y, GLdouble z)
{
  glue->glVertexAttrib3dARB(index, x, y, z);
}

void
SoGLContext_glVertexAttrib4s(const SoGLContext * glue,
                           GLuint index, GLshort x,
                           GLshort y, GLshort z, GLshort w)
{
  glue->glVertexAttrib4sARB(index, x, y, z, w);
}

void
SoGLContext_glVertexAttrib4f(const SoGLContext * glue,
                           GLuint index, GLfloat x,
                           GLfloat y, GLfloat z, GLfloat w)
{
  glue->glVertexAttrib4fARB(index, x, y, z, w);
}

void
SoGLContext_glVertexAttrib4d(const SoGLContext * glue,
                           GLuint index, GLdouble x,
                           GLdouble y, GLdouble z, GLdouble w)
{
  glue->glVertexAttrib4dARB(index, x, y, z, w);
}

void
SoGLContext_glVertexAttrib4Nub(const SoGLContext * glue,
                             GLuint index, GLubyte x,
                             GLubyte y, GLubyte z, GLubyte w)
{
  glue->glVertexAttrib4NubARB(index, x, y, z, w);
}

void
SoGLContext_glVertexAttrib1sv(const SoGLContext * glue,
                            GLuint index, const GLshort *v)
{
  glue->glVertexAttrib1svARB(index, v);
}

void
SoGLContext_glVertexAttrib1fv(const SoGLContext * glue,
                            GLuint index, const GLfloat *v)
{
  glue->glVertexAttrib1fvARB(index, v);
}

void
SoGLContext_glVertexAttrib1dv(const SoGLContext * glue,
                            GLuint index, const GLdouble *v)
{
  glue->glVertexAttrib1dvARB(index, v);
}

void
SoGLContext_glVertexAttrib2sv(const SoGLContext * glue,
                            GLuint index, const GLshort *v)
{
  glue->glVertexAttrib2svARB(index, v);
}

void
SoGLContext_glVertexAttrib2fv(const SoGLContext * glue,
                            GLuint index, const GLfloat *v)
{
  glue->glVertexAttrib2fvARB(index, v);
}

void
SoGLContext_glVertexAttrib2dv(const SoGLContext * glue,
                            GLuint index, const GLdouble *v)
{
  glue->glVertexAttrib2dvARB(index, v);
}

void
SoGLContext_glVertexAttrib3sv(const SoGLContext * glue,
                            GLuint index, const GLshort *v)
{
  glue->glVertexAttrib3svARB(index, v);
}

void
SoGLContext_glVertexAttrib3fv(const SoGLContext * glue,
                            GLuint index, const GLfloat *v)
{
  glue->glVertexAttrib3fvARB(index, v);
}

void
SoGLContext_glVertexAttrib3dv(const SoGLContext * glue,
                            GLuint index, const GLdouble *v)
{
  glue->glVertexAttrib3dvARB(index, v);
}

void
SoGLContext_glVertexAttrib4bv(const SoGLContext * glue,
                            GLuint index, const GLbyte *v)
{
  glue->glVertexAttrib4bvARB(index, v);
}

void
SoGLContext_glVertexAttrib4sv(const SoGLContext * glue,
                            GLuint index, const GLshort *v)
{
  glue->glVertexAttrib4svARB(index, v);
}

void
SoGLContext_glVertexAttrib4iv(const SoGLContext * glue,
                            GLuint index, const GLint *v)
{
  glue->glVertexAttrib4ivARB(index, v);
}

void
SoGLContext_glVertexAttrib4ubv(const SoGLContext * glue,
                             GLuint index, const GLubyte *v)
{
  glue->glVertexAttrib4ubvARB(index, v);
}

void
SoGLContext_glVertexAttrib4usv(const SoGLContext * glue,
                             GLuint index, const GLushort *v)
{
  glue->glVertexAttrib4usvARB(index, v);
}

void
SoGLContext_glVertexAttrib4uiv(const SoGLContext * glue,
                             GLuint index, const GLuint *v)
{
  glue->glVertexAttrib4uivARB(index, v);
}

void
SoGLContext_glVertexAttrib4fv(const SoGLContext * glue,
                            GLuint index, const GLfloat *v)
{
  glue->glVertexAttrib4fvARB(index, v);
}

void
SoGLContext_glVertexAttrib4dv(const SoGLContext * glue,
                            GLuint index, const GLdouble *v)
{
  glue->glVertexAttrib4dvARB(index, v);
}

void
SoGLContext_glVertexAttrib4Nbv(const SoGLContext * glue,
                             GLuint index, const GLbyte *v)
{
  glue->glVertexAttrib4NbvARB(index, v);
}

void
SoGLContext_glVertexAttrib4Nsv(const SoGLContext * glue,
                             GLuint index, const GLshort *v)
{
  glue->glVertexAttrib4NsvARB(index, v);
}

void
SoGLContext_glVertexAttrib4Niv(const SoGLContext * glue,
                             GLuint index, const GLint *v)
{
  glue->glVertexAttrib4NivARB(index, v);
}

void
SoGLContext_glVertexAttrib4Nubv(const SoGLContext * glue,
                              GLuint index, const GLubyte *v)
{
  glue->glVertexAttrib4NubvARB(index, v);
}

void
SoGLContext_glVertexAttrib4Nusv(const SoGLContext * glue,
                              GLuint index, const GLushort *v)
{
  glue->glVertexAttrib4NusvARB(index, v);
}

void
SoGLContext_glVertexAttrib4Nuiv(const SoGLContext * glue,
                              GLuint index, const GLuint *v)
{
  glue->glVertexAttrib4NuivARB(index, v);
}

void
SoGLContext_glVertexAttribPointer(const SoGLContext * glue,
                                GLuint index, GLint size,
                                GLenum type, GLboolean normalized,
                                GLsizei stride,
                                const GLvoid *pointer)
{
  glue->glVertexAttribPointerARB(index, size, type, normalized, stride, pointer);
}

void
SoGLContext_glEnableVertexAttribArray(const SoGLContext * glue,
                                    GLuint index)
{
  glue->glEnableVertexAttribArrayARB(index);
}

void
SoGLContext_glDisableVertexAttribArray(const SoGLContext * glue,
                                     GLuint index)
{
  glue->glDisableVertexAttribArrayARB(index);
}

void
SoGLContext_glGetVertexAttribdv(const SoGLContext * glue,
                              GLuint index, GLenum pname,
                              GLdouble *params)
{
  glue->glGetVertexAttribdvARB(index, pname, params);
}

void
SoGLContext_glGetVertexAttribfv(const SoGLContext * glue,
                              GLuint index, GLenum pname,
                              GLfloat *params)
{
  glue->glGetVertexAttribfvARB(index, pname, params);
}

void
SoGLContext_glGetVertexAttribiv(const SoGLContext * glue,
                              GLuint index, GLenum pname,
                              GLint *params)
{
  glue->glGetVertexAttribivARB(index, pname, params);
}

void
SoGLContext_glGetVertexAttribPointerv(const SoGLContext * glue,
                                    GLuint index, GLenum pname,
                                    GLvoid **pointer)
{
  glue->glGetVertexAttribPointervARB(index, pname, pointer);
}

/* GL_ARB_occlusion_query */

SbBool
SoGLContext_has_occlusion_query(const SoGLContext * glue)
{
  if (!glglue_allow_newer_opengl(glue)) return FALSE;

  /* check only one function for speed. It's set to NULL when
     initializing if one of the other functions wasn't found */
  return glue->glGenQueries != NULL;
}

void
SoGLContext_glGenQueries(const SoGLContext * glue,
                       GLsizei n, GLuint * ids)
{
  glue->glGenQueries(n, ids);
}

void
SoGLContext_glDeleteQueries(const SoGLContext * glue,
                          GLsizei n, const GLuint *ids)
{
  glue->glDeleteQueries(n, ids);
}

GLboolean
SoGLContext_glIsQuery(const SoGLContext * glue,
                    GLuint id)
{
  return glue->glIsQuery(id);
}

void
SoGLContext_glBeginQuery(const SoGLContext * glue,
                       GLenum target, GLuint id)
{
  glue->glBeginQuery(target, id);
}

void
SoGLContext_glEndQuery(const SoGLContext * glue,
                     GLenum target)
{
  glue->glEndQuery(target);
}

void
SoGLContext_glGetQueryiv(const SoGLContext * glue,
                       GLenum target, GLenum pname,
                       GLint * params)
{
  glue->glGetQueryiv(target, pname, params);
}

void
SoGLContext_glGetQueryObjectiv(const SoGLContext * glue,
                             GLuint id, GLenum pname,
                             GLint * params)
{
  glue->glGetQueryObjectiv(id, pname, params);
}

void
SoGLContext_glGetQueryObjectuiv(const SoGLContext * glue,
                              GLuint id, GLenum pname,
                              GLuint * params)
{
  glue->glGetQueryObjectuiv(id, pname, params);
}

/* GL_NV_texture_rectangle (identical to GL_EXT_texture_rectangle) */
SbBool
SoGLContext_has_nv_texture_rectangle(const SoGLContext * glue)
{
  if (!glglue_allow_newer_opengl(glue)) return FALSE;
  return glue->has_ext_texture_rectangle;
}

/* GL_EXT_texture_rectangle */
SbBool
SoGLContext_has_ext_texture_rectangle(const SoGLContext * glue)
{
  if (!glglue_allow_newer_opengl(glue)) return FALSE;
  return glue->has_ext_texture_rectangle;
}

/* GL_NV_texture_shader */
SbBool
SoGLContext_has_nv_texture_shader(const SoGLContext * glue)
{
  if (!glglue_allow_newer_opengl(glue)) return FALSE;
  return glue->has_nv_texture_shader;
}

/* GL_ARB_shadow */
SbBool
SoGLContext_has_arb_shadow(const SoGLContext * glue)
{
  if (!glglue_allow_newer_opengl(glue)) return FALSE;
  return glue->has_shadow;
}

/* GL_ARB_depth_texture */
SbBool
SoGLContext_has_arb_depth_texture(const SoGLContext * glue)
{
  if (!glglue_allow_newer_opengl(glue)) return FALSE;
  return glue->has_depth_texture;
}

/* GL_EXT_texture_env_combine || GL_ARB_texture_env_combine || OGL 1.4 */
SbBool
SoGLContext_has_texture_env_combine(const SoGLContext * glue)
{
  if (!glglue_allow_newer_opengl(glue)) return FALSE;
  return glue->has_texture_env_combine;
}

/*!
  Returns current X11 display the OpenGL context is in.  Always returns
  NULL since GLX context management was removed; context management is
  now handled via SoDB::ContextManager callbacks.
*/
void *
SoGLContext_glXGetCurrentDisplay(const SoGLContext * w)
{
  (void)w;
  return NULL;
}

/*** Offscreen buffer handling. *********************************************/

/*
  Below is a standalone example that can be compiled and linked with
  the Coin library for testing that the context handling interface
  works:
 */
/*
  #include "glue/glp.h"
  #include <Inventor/elements/SoGLCacheContextElement.h>
  #include <Inventor/SoDB.h>
  #include <cassert>
  #include <cstdio>

  int
  main(void)
  {
    SoDB::init();
    void * ctx = SoGLContext_context_create_offscreen(128, 128);
    assert(ctx);
    SbBool ok = SoGLContext_context_make_current(ctx);
    assert(ok);

    const GLubyte * str = glGetString(GL_VERSION);
    assert(str && "could not call glGetString() -- no current GL context?");
    assert(glGetError() == GL_NO_ERROR && "GL error when calling glGetString() -- no current GL context?");

    (void)fprintf(stdout, "glGetString(GL_VERSION)=='%s'\n", str);
    (void)fprintf(stdout, "glGetString(GL_VENDOR)=='%s'\n", glGetString(GL_VENDOR));
    (void)fprintf(stdout, "glGetString(GL_RENDERER)=='%s'\n", glGetString(GL_RENDERER));

    uint32_t contextid = SoGLCacheContextElement::getUniqueCacheContext();
    const SoGLContext * glue = SoGLContext_instance(contextid);
    (void)fprintf(stdout, "glGenTextures=='%p'\n",
                  SoGLContext_getprocaddress(glue, "glGenTextures"));

    (void)fprintf(stdout, "glGenTexturesEXT=='%p'\n",
                  SoGLContext_getprocaddress(glue, "glGenTexturesEXT"));

    SoGLContext_context_reinstate_previous(ctx);
    SoGLContext_context_destruct(ctx);
    return 0;
  }
*/


/* offscreen rendering - max dimensions probe */

/*!
  Returns the \e theoretical maximum dimensions for an offscreen
  buffer.

  Note that we're still not guaranteed that allocation of this size
  will succeed, as that is also subject to e.g. memory constraints,
  which is something that will dynamically change during the running
  time of an application.

  So the values returned from this function should be taken as hints,
  and callers should re-request offscreen contexts with lower dimensions
  if creation or activation fails.
*/
void
SoGLContext_context_max_dimensions(void * mgr_ptr, unsigned int * width, unsigned int * height)
{
  void * ctx;
  SbBool ok;
  const char * vendor;
  GLint size[2];
  static SbBool cached = FALSE;
  static unsigned int dim[2] = { 0, 0 };

    *width = dim[0];
    *height = dim[1];

  if (cached) { /* value cached */ return; }

  if (SoGLContext_debug()) {
    cc_debugerror_postinfo("SoGLContext_context_max_dimensions",
                           "query by making a dummy offscreen context");
  }

  cached = TRUE; /* Flip flag on first run. Note: it doesn't matter
                    that the detection below might fail -- as we
                    should report <0,0> on consecutive runs. */

  SoDB::ContextManager * manager = static_cast<SoDB::ContextManager *>(mgr_ptr);
  if (!manager) { return; }

  /* The below calls *can* fail, due to e.g. lack of resources, or no
     usable visual for the GL context. We try to handle gracefully.
     This is straightforward to do here, simply returning dimensions
     of <0,0>, but note that we also need to handle the exception in
     the callers. */

  ctx = manager->createOffscreenContext(32, 32);
  if (!ctx) { return; }
  ok = manager->makeContextCurrent(ctx);
  if (!ok) { manager->destroyContext(ctx); return; }

  glGetIntegerv(GL_MAX_VIEWPORT_DIMS, size);
  if (SoGLContext_debug()) {
    cc_debugerror_postinfo("SoGLContext_context_max_dimensions",
                           "GL_MAX_VIEWPORT_DIMS==<%d, %d>",
                           size[0], size[1]);
  }

  vendor = (const char *)glGetString(GL_VENDOR);
  if (!vendor) {
    /* glGetString(GL_VENDOR) returns NULL when no system GL context is current
     * (e.g. an OSMesa context was made current but system GL has a separate
     * context slot).  System glGetIntegerv(GL_MAX_VIEWPORT_DIMS) also has no
     * current context in this scenario so size[] is unreliable.
     * Provide a large default: OSMesa's size limit is RAM-only, so 16384 is
     * safe and covers any reasonable offscreen rendering request. */
    *width  = 16384;
    *height = 16384;
    dim[0]  = *width;
    dim[1]  = *height;
    manager->restorePreviousContext(ctx);
    manager->destroyContext(ctx);
    return;
  }

  *width = (unsigned int) size[0];
  *height = (unsigned int) size[1];

  /* With callback-based contexts and FBO rendering, pbuffer limitations
     do not apply. The maximum dimensions are determined by OpenGL texture
     size limits and the standard 4096x4096 clamp below. */

  manager->restorePreviousContext(ctx);
  manager->destroyContext(ctx);

  /* Force an additional limit to the maximum tilesize to 4096x4096
     pixels.

     This is done to work around a problem with some OpenGL drivers: a
     huge value is returned for the maximum offscreen OpenGL canvas,
     where the driver obviously does not take into account the amount
     of memory needed to actually allocate such a large buffer.

     This problem has at least been observed with the Microsoft Windows XP
     software OpenGL renderer, which reports a maximum viewport size
     of 16k x 16k pixels.

     FIXME: we really shouldn't f*ck around with this here, but rather
     make the client code of this more robust. That means
     SoOffscreenRenderer should try with successively smaller buffers
     of allocation if the maximum (or wanted) buffer size fails. For
     further discussion, see the FIXME at the top of the
     SoOffscreenRendererP::renderFromBase() method. 20040714 mortene.

     UPDATE 20050712 mortene: this has now been fixed in
     SoOffscreenRenderer -- it will try with successively smaller
     sizes. I'm still keeping the max clamping below, though, to avoid
     unexpected problems with external applications, as we're
     currently between patch-level releases with Coin-2, and I have
     only limited time right now for testing that removing this would
     not cause badness.
  */
  *width = std::min(*width, 4096u);
  *height = std::min(*height, 4096u);

  if (SoGLContext_debug()) {
    cc_debugerror_postinfo("SoGLContext_context_max_dimensions",
                           "clamped max dimensions==<%u, %u>",
                           *width, *height);
  }

  /* cache values for next invocation */

  dim[0] = *width;
  dim[1] = *height;
}

SbBool
SoGLContext_context_can_render_to_texture(void * OBOL_UNUSED_ARG(ctx))
{
  /* Render-to-texture is not supported with the current FBO+callback architecture.
     This functionality has been superseded by the portable FBO-based offscreen rendering. */
  return FALSE;
}


void
SoGLContext_context_bind_pbuffer(void * OBOL_UNUSED_ARG(ctx))
{
  /* PBuffer functionality is not needed with FBO-based offscreen rendering.
     This is a no-op in the current architecture. */
}

void
SoGLContext_context_release_pbuffer(void * OBOL_UNUSED_ARG(ctx))
{
  /* PBuffer functionality is not needed with FBO-based offscreen rendering.
     This is a no-op in the current architecture. */
}

SbBool
SoGLContext_context_pbuffer_is_bound(void * OBOL_UNUSED_ARG(ctx))
{
  /* PBuffer functionality is not needed with FBO-based offscreen rendering.
     Always return FALSE as PBuffers are not used. */
  return FALSE;
}

/* Win32 HDC support has been removed as it was platform-specific
   and is not needed with the callback-based context architecture. */
const void *
SoGLContext_win32_HDC(void * OBOL_UNUSED_ARG(ctx))
{
  return NULL;
}

void SoGLContext_win32_updateHDCBitmap(void * OBOL_UNUSED_ARG(ctx))
{
  /* No-op: HDC functionality has been removed */
}

/*** </Offscreen buffer handling.> ******************************************/

/*** <PROXY texture handling> ***********************************************/

static int
compute_log(int value)
{
  int i = 0;
  while (value > 1) { value>>=1; i++; }
  return i;
}

/*  proxy mipmap creation */
static SbBool
proxy_mipmap_2d(int width, int height,
                GLenum internalFormat,
                GLenum format,
                GLenum type,
                SbBool mipmap)
{
  GLint w;
  int level;
  int levels = compute_log(std::max(width, height));

  glTexImage2D(GL_PROXY_TEXTURE_2D, 0, internalFormat, width, height, 0,
               format, type, NULL);
  glGetTexLevelParameteriv(GL_PROXY_TEXTURE_2D, 0,
                           GL_TEXTURE_WIDTH, &w);

  if (w == 0) return FALSE;
  if (!mipmap) return TRUE;

  for (level = 1; level <= levels; level++) {
    if (width > 1) width >>= 1;
    if (height > 1) height >>= 1;
    glTexImage2D(GL_PROXY_TEXTURE_2D, level, internalFormat, width,
                 height, 0, format, type,
                 NULL);
    glGetTexLevelParameteriv(GL_PROXY_TEXTURE_2D, 0,
                             GL_TEXTURE_WIDTH, &w);
    if (w == 0) return FALSE;
  }
  return TRUE;
}

/* proxy mipmap creation. 3D version. */
static SbBool
proxy_mipmap_3d(const SoGLContext * glw, int width, int height, int depth,
                GLenum internalFormat,
                GLenum format,
                GLenum type,
                SbBool mipmap)
{
  GLint w;
  int level;
  int levels = compute_log(std::max({width, height, depth}));

  SoGLContext_glTexImage3D(glw, GL_PROXY_TEXTURE_3D, 0, internalFormat,
                         width, height, depth, 0, format, type,
                         NULL);
  glGetTexLevelParameteriv(GL_PROXY_TEXTURE_3D, 0,
                           GL_TEXTURE_WIDTH, &w);
  if (w == 0) return FALSE;
  if (!mipmap) return TRUE;

  for (level = 1; level <= levels; level++) {
    if (width > 1) width >>= 1;
    if (height > 1) height >>= 1;
    if (depth > 1) depth >>= 1;
    SoGLContext_glTexImage3D(glw, GL_PROXY_TEXTURE_3D, level, internalFormat,
                           width, height, depth, 0, format, type,
                           NULL);
    glGetTexLevelParameteriv(GL_PROXY_TEXTURE_3D, 0,
                             GL_TEXTURE_WIDTH, &w);
    if (w == 0) return FALSE;
  }
  return TRUE;
}

/*!
  Note that the \e internalformat parameter corresponds to the \e
  internalFormat parameter to glTexImage2D; either the number of
  components per texel or a constant specifying the internal texture format.
 */
SbBool
SoGLContext_is_texture_size_legal(const SoGLContext * glw,
                                  int xsize, int ysize, int zsize,
                                  GLenum internalformat,
                                  GLenum format,
                                  GLenum type,
                                  SbBool mipmap)
 {
  if (zsize == 0) { /* 2D textures */
    if (OBOL_MAXIMUM_TEXTURE2_SIZE > 0) {
      if (xsize > OBOL_MAXIMUM_TEXTURE2_SIZE) return FALSE;
      if (ysize > OBOL_MAXIMUM_TEXTURE2_SIZE) return FALSE;
      return TRUE;
    }
    if (SoGLContext_has_2d_proxy_textures(glw)) {
      return proxy_mipmap_2d(xsize, ysize, internalformat, format, type, mipmap);
    }
    else {
      if (xsize > glw->max_texture_size) return FALSE;
      if (ysize > glw->max_texture_size) return FALSE;
      return TRUE;
    }
  }
  else { /*  3D textures */
    if (SoGLContext_has_3d_textures(glw)) {
      if (OBOL_MAXIMUM_TEXTURE3_SIZE > 0) {
        if (xsize > OBOL_MAXIMUM_TEXTURE3_SIZE) return FALSE;
        if (ysize > OBOL_MAXIMUM_TEXTURE3_SIZE) return FALSE;
        if (zsize > OBOL_MAXIMUM_TEXTURE3_SIZE) return FALSE;
        return TRUE;
      }
      return proxy_mipmap_3d(glw, xsize, ysize, zsize, internalformat, format, type, mipmap);
    }
    else {
#if OBOL_DEBUG
      static SbBool first = TRUE;
      if (first) {
        cc_debugerror_post("glglue_is_texture_size_legal",
                           "3D not supported with this OpenGL driver");
        first = FALSE;
      }
#endif /*  OBOL_DEBUG */
      return FALSE;
    }
  }
}

/*
  Convert from num components to internal texture format for use
  in glTexImage*D's internalFormat parameter.
*/
GLint SoGLContext_get_internal_texture_format(const SoGLContext * glw,
                                              int numcomponents,
                                              SbBool compress)
{
  GLenum format;
  if (compress) {
    switch (numcomponents) {
    case 1:
      format = GL_COMPRESSED_LUMINANCE_ARB;
      break;
    case 2:
      format = GL_COMPRESSED_LUMINANCE_ALPHA_ARB;
      break;
    case 3:
      format = GL_COMPRESSED_RGB_ARB;
      break;
    case 4:
    default:
      format = GL_COMPRESSED_RGBA_ARB;
      break;
    }
  }
  else {
    SbBool usenewenums = glglue_allow_newer_opengl(glw) && SoGLContext_glversion_matches_at_least(glw,1,1,0);
    switch (numcomponents) {
    case 1:
      format = usenewenums ? GL_LUMINANCE8 : GL_LUMINANCE;
      break;
    case 2:
      format = usenewenums ? GL_LUMINANCE8_ALPHA8 : GL_LUMINANCE_ALPHA;
      break;
    case 3:
      format = usenewenums ? GL_RGB8 : GL_RGB;
      break;
    case 4:
    default:
      format = usenewenums ? GL_RGBA8 : GL_RGBA;
      break;
    }
  }
  return format;
}

/*
  Convert from num components to client texture format for use
  in glTexImage*D's format parameter.
*/
GLenum SoGLContext_get_texture_format(const SoGLContext * OBOL_UNUSED_ARG(glw), int numcomponents)
{
  GLenum format;
  switch (numcomponents) {
  case 1:
    format = GL_LUMINANCE;
    break;
  case 2:
    format = GL_LUMINANCE_ALPHA;
    break;
  case 3:
    format = GL_RGB;
    break;
  case 4:
  default:
    format = GL_RGBA;
    break;
  }
  return format;
}

/*** </PROXY texture handling> ***********************************************/

/*** <Anisotropic filtering> *************************************************/

float SoGLContext_get_max_anisotropy(const SoGLContext * glue)
{
  return glue->max_anisotropy;
}

SbBool
SoGLContext_can_do_anisotropic_filtering(const SoGLContext * glue)
{
  if (!glglue_allow_newer_opengl(glue)) return FALSE;
  return glue->can_do_anisotropic_filtering;
}

/*** </Anisotropic filtering> *************************************************/

/* Convert an OpenGL enum error code to a textual representation. */
const char *
coin_glerror_string(GLenum errorcode)
{
  static const char INVALID_VALUE[] = "GL_INVALID_VALUE";
  static const char INVALID_ENUM[] = "GL_INVALID_ENUM";
  static const char INVALID_OPERATION[] = "GL_INVALID_OPERATION";
  static const char STACK_OVERFLOW[] = "GL_STACK_OVERFLOW";
  static const char STACK_UNDERFLOW[] = "GL_STACK_UNDERFLOW";
  static const char OUT_OF_MEMORY[] = "GL_OUT_OF_MEMORY";
  static const char unknown[] = "Unknown OpenGL error";

  switch (errorcode) {
  case GL_INVALID_VALUE:
    return INVALID_VALUE;
  case GL_INVALID_ENUM:
    return INVALID_ENUM;
  case GL_INVALID_OPERATION:
    return INVALID_OPERATION;
  case GL_STACK_OVERFLOW:
    return STACK_OVERFLOW;
  case GL_STACK_UNDERFLOW:
    return STACK_UNDERFLOW;
  case GL_OUT_OF_MEMORY:
    return OUT_OF_MEMORY;
  default:
    return unknown;
  }
  return NULL; /* avoid compiler warning */
}

/* Simple utility function for dumping the current set of error codes
   returned from glGetError(). Returns number of errors reported by
   OpenGL. */

unsigned int
coin_catch_gl_errors(std::string * str)
{
  unsigned int errs = 0;
  GLenum glerr = glGetError();
  while (glerr != GL_NO_ERROR) {
    if (errs < 10) {
      if (errs > 0) {
        str->push_back(' ');
      }
      str->append(coin_glerror_string(glerr));
    }
    /* ignore > 10, so we don't run into a situation were we end up
       practically locking up the app due to vast amounts of errors */
    else if (errs == 10) {
      str->append("... and more");
    }

    errs++;
    glerr = glGetError();
  }
  return errs;
}

/* ********************************************************************** */

void *
coin_gl_current_context(void)
{
  /* With callback-based contexts, current context information is managed
     by the application and not available through this interface. */
  return NULL;
}

/* ********************************************************************** */

#ifndef SOGL_PREFIX_SET
void *
coin_gl_getstring_ptr(void)
{
  /* Return the address of glGetString as seen from this translation unit
     (which includes the real OpenGL headers).  dl.cpp uses this to verify
     that cc_dl_opengl_handle() opened the same GL library, without needing
     to include raw GL headers itself. */
  return (void *)glGetString;
}
#endif /* !SOGL_PREFIX_SET */

/* ********************************************************************** */

SbBool
SoGLContext_vbo_in_displaylist_supported(const SoGLContext * OBOL_UNUSED_ARG(glue))
{
  // Older ATI Windows/Linux drivers had a nasty bug which caused a crash
  // in OpenGL whenever a VBO render call was added to a display list.
  // Newer drivers are known to work.
  static int disable = -1;
  if (disable == -1) {
    disable = glglue_resolve_envvar("OBOL_GLGLUE_DISABLE_VBO_IN_DISPLAYLIST");
  }
  if (disable) { return FALSE; }

  return TRUE;
}

/* ********************************************************************** */

SbBool
SoGLContext_non_power_of_two_textures(const SoGLContext * glue)
{
  // ATi and Intel both have had problems with this feature, especially
  // on old drivers.  Newer drivers are known to work.
  static int disable = -1;
  if (disable == -1) {
    disable = glglue_resolve_envvar("OBOL_GLGLUE_DISABLE_NON_POWER_OF_TWO_TEXTURES");
  }
  if (disable) { return FALSE; }

  if (!glglue_allow_newer_opengl(glue)) return FALSE;
  return glue->non_power_of_two_textures;
}

/* ********************************************************************** */

uint32_t
SoGLContext_get_contextid(const SoGLContext * glue)
{
  return glue->contextid;
}

/* ********************************************************************** */

static void cleanup_instance_created_list(void)
{
  cc_list_destruct(gl_instance_created_cblist);
  gl_instance_created_cblist = NULL;
}

void
SoGLContext_add_instance_created_callback(SoGLContext_instance_created_cb * cb,
                                          void * closure)
{
  if (gl_instance_created_cblist == NULL) {
    gl_instance_created_cblist = cc_list_construct();
    coin_atexit((coin_atexit_f *)cleanup_instance_created_list, CC_ATEXIT_NORMAL);
  }
  cc_list_append(gl_instance_created_cblist, (void*)cb);
  cc_list_append(gl_instance_created_cblist, closure);
}

/* ********************************************************************** */

void
SoGLContext_glIsRenderbuffer(const SoGLContext * glue, GLuint renderbuffer)
{
  if (!glue || !glue->has_fbo) { sogl_warn_no_fbo(); return; }
  glue->glIsRenderbuffer(renderbuffer);
}

void
SoGLContext_glBindRenderbuffer(const SoGLContext * glue, GLenum target, GLuint renderbuffer)
{
  if (!glue || !glue->has_fbo) { sogl_warn_no_fbo(); return; }
  glue->glBindRenderbuffer(target, renderbuffer);
}

void
SoGLContext_glDeleteRenderbuffers(const SoGLContext * glue, GLsizei n, const GLuint *renderbuffers)
{
  if (!glue || !glue->has_fbo) { sogl_warn_no_fbo(); return; }
  glue->glDeleteRenderbuffers(n, renderbuffers);
}

void
SoGLContext_glGenRenderbuffers(const SoGLContext * glue, GLsizei n, GLuint *renderbuffers)
{
  if (!glue || !glue->has_fbo) { sogl_warn_no_fbo(); return; }
  glue->glGenRenderbuffers(n, renderbuffers);
}

void
SoGLContext_glRenderbufferStorage(const SoGLContext * glue, GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
  if (!glue || !glue->has_fbo) { sogl_warn_no_fbo(); return; }
  glue->glRenderbufferStorage(target, internalformat, width, height);
}

void
SoGLContext_glGetRenderbufferParameteriv(const SoGLContext * glue, GLenum target, GLenum pname, GLint * params)
{
  if (!glue || !glue->has_fbo) { sogl_warn_no_fbo(); return; }
  glue->glGetRenderbufferParameteriv(target, pname, params);
}

GLboolean
SoGLContext_glIsFramebuffer(const SoGLContext * glue, GLuint framebuffer)
{
  if (!glue || !glue->has_fbo) { sogl_warn_no_fbo(); return GL_FALSE; }
  return glue->glIsFramebuffer(framebuffer);
}

void
SoGLContext_glBindFramebuffer(const SoGLContext * glue, GLenum target, GLuint framebuffer)
{
  if (!glue || !glue->has_fbo) { sogl_warn_no_fbo(); return; }
  glue->glBindFramebuffer(target, framebuffer);
}

void
SoGLContext_glDeleteFramebuffers(const SoGLContext * glue, GLsizei n, const GLuint * framebuffers)
{
  if (!glue || !glue->has_fbo) { sogl_warn_no_fbo(); return; }
  glue->glDeleteFramebuffers(n, framebuffers);
}

void
SoGLContext_glGenFramebuffers(const SoGLContext * glue, GLsizei n, GLuint * framebuffers)
{
  if (!glue || !glue->has_fbo) { sogl_warn_no_fbo(); return; }
  glue->glGenFramebuffers(n, framebuffers);
}

GLenum
SoGLContext_glCheckFramebufferStatus(const SoGLContext * glue, GLenum target)
{
  if (!glue || !glue->has_fbo) { sogl_warn_no_fbo(); return GL_FRAMEBUFFER_UNSUPPORTED_EXT; }
  return glue->glCheckFramebufferStatus(target);
}

void
SoGLContext_glFramebufferTexture1D(const SoGLContext * glue, GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
  if (!glue || !glue->has_fbo) { sogl_warn_no_fbo(); return; }
  glue->glFramebufferTexture1D(target, attachment, textarget, texture, level);
}

void
SoGLContext_glFramebufferTexture2D(const SoGLContext * glue, GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
  if (!glue || !glue->has_fbo) { sogl_warn_no_fbo(); return; }
  glue->glFramebufferTexture2D(target, attachment, textarget, texture, level);
}

void
SoGLContext_glFramebufferTexture3D(const SoGLContext * glue, GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset)
{
  if (!glue || !glue->has_fbo) { sogl_warn_no_fbo(); return; }
  glue->glFramebufferTexture3D(target, attachment, textarget, texture, level,zoffset);
}

void
SoGLContext_glFramebufferRenderbuffer(const SoGLContext * glue, GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
  if (!glue || !glue->has_fbo) { sogl_warn_no_fbo(); return; }
  glue->glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
}

void
SoGLContext_glGetFramebufferAttachmentParameteriv(const SoGLContext * glue, GLenum target, GLenum attachment, GLenum pname, GLint * params)
{
  if (!glue || !glue->has_fbo) { sogl_warn_no_fbo(); return; }
  glue->glGetFramebufferAttachmentParameteriv(target, attachment, pname, params);
}

SbBool
SoGLContext_has_generate_mipmap(const SoGLContext * glue)
{
  if (!glglue_allow_newer_opengl(glue)) return FALSE; 
  // ATi's handling of this function is very buggy. It's possible to
  // work around these quirks, but we just disable it for now since we
  // have other ways to generate mipmaps. Only disable on Windows. The
  // OS X and Linux drivers are probably ok.
  if ((coin_runtime_os() == OBOL_MSWINDOWS) && glue->vendor_is_ati) {
    return FALSE;
  }
  return (glue->glGenerateMipmap != NULL);
}

void
SoGLContext_glGenerateMipmap(const SoGLContext * glue, GLenum target)
{
  glue->glGenerateMipmap(target);
}

SbBool
SoGLContext_has_framebuffer_objects(const SoGLContext * glue)
{
  if (!glglue_allow_newer_opengl(glue)) return FALSE;
  return glue->has_fbo;
}

/* Core GL 1.0/1.1 wrappers.
 *
 * These must be used instead of bare gl* calls anywhere the active
 * backend (system GL vs. OSMesa) is not known at compile time — in
 * particular inside SoSceneTexture2 and any other node that runs in
 * both a system-GL context and an OSMesa context within the same
 * process (OBOL_DUAL_GL_BUILD).  The function pointers were filled in
 * by glglue_resolve_symbols() from the correct backend. */

void
SoGLContext_glTexImage2D(const SoGLContext * glue,
                         GLenum target, GLint level, GLint internalformat,
                         GLsizei width, GLsizei height, GLint border,
                         GLenum format, GLenum type, const GLvoid * pixels)
{
  if (!glue || !glue->glTexImage2D) { sogl_warn_core_func_unavailable("glTexImage2D"); return; }
  glue->glTexImage2D(target, level, internalformat, width, height,
                     border, format, type, pixels);
}

void
SoGLContext_glTexParameteri(const SoGLContext * glue,
                            GLenum target, GLenum pname, GLint param)
{
  if (!glue || !glue->glTexParameteri) { sogl_warn_core_func_unavailable("glTexParameteri"); return; }
  glue->glTexParameteri(target, pname, param);
}

void
SoGLContext_glTexParameterf(const SoGLContext * glue,
                            GLenum target, GLenum pname, GLfloat param)
{
  if (!glue || !glue->glTexParameterf) { sogl_warn_core_func_unavailable("glTexParameterf"); return; }
  glue->glTexParameterf(target, pname, param);
}

void
SoGLContext_glGetIntegerv(const SoGLContext * glue,
                          GLenum pname, GLint * params)
{
  if (!glue || !glue->glGetIntegerv) { sogl_warn_core_func_unavailable("glGetIntegerv"); return; }
  glue->glGetIntegerv(pname, params);
}

void
SoGLContext_glGetFloatv(const SoGLContext * glue,
                        GLenum pname, GLfloat * params)
{
  if (!glue || !glue->glGetFloatv) { sogl_warn_core_func_unavailable("glGetFloatv"); return; }
  glue->glGetFloatv(pname, params);
}

void
SoGLContext_glClearColor(const SoGLContext * glue,
                         GLclampf red, GLclampf green,
                         GLclampf blue, GLclampf alpha)
{
  if (!glue || !glue->glClearColor) { sogl_warn_core_func_unavailable("glClearColor"); return; }
  glue->glClearColor(red, green, blue, alpha);
}

void
SoGLContext_glClear(const SoGLContext * glue, GLbitfield mask)
{
  if (!glue || !glue->glClear) { sogl_warn_core_func_unavailable("glClear"); return; }
  glue->glClear(mask);
}

void
SoGLContext_glFlush(const SoGLContext * glue)
{
  if (!glue || !glue->glFlush) { sogl_warn_core_func_unavailable("glFlush"); return; }
  glue->glFlush();
}

void
SoGLContext_glFinish(const SoGLContext * glue)
{
  if (!glue || !glue->glFinish) { sogl_warn_core_func_unavailable("glFinish"); return; }
  glue->glFinish();
}

GLenum
SoGLContext_glGetError(const SoGLContext * glue)
{
  if (!glue || !glue->glGetError) { sogl_warn_core_func_unavailable("glGetError"); return GL_NO_ERROR; }
  return glue->glGetError();
}

const GLubyte *
SoGLContext_glGetString(const SoGLContext * glue, GLenum name)
{
  if (!glue || !glue->glGetString) { sogl_warn_core_func_unavailable("glGetString"); return NULL; }
  return glue->glGetString(name);
}

void
SoGLContext_glEnable(const SoGLContext * glue, GLenum cap)
{
  if (!glue || !glue->glEnable) { sogl_warn_core_func_unavailable("glEnable"); return; }
  glue->glEnable(cap);
}

void
SoGLContext_glDisable(const SoGLContext * glue, GLenum cap)
{
  if (!glue || !glue->glDisable) { sogl_warn_core_func_unavailable("glDisable"); return; }
  glue->glDisable(cap);
}

GLboolean
SoGLContext_glIsEnabled(const SoGLContext * glue, GLenum cap)
{
  if (!glue || !glue->glIsEnabled) { sogl_warn_core_func_unavailable("glIsEnabled"); return GL_FALSE; }
  return glue->glIsEnabled(cap);
}

void
SoGLContext_glPixelStorei(const SoGLContext * glue,
                          GLenum pname, GLint param)
{
  if (!glue || !glue->glPixelStorei) { sogl_warn_core_func_unavailable("glPixelStorei"); return; }
  glue->glPixelStorei(pname, param);
}

void
SoGLContext_glReadPixels(const SoGLContext * glue,
                         GLint x, GLint y, GLsizei width, GLsizei height,
                         GLenum format, GLenum type, GLvoid * pixels)
{
  if (!glue || !glue->glReadPixels) { sogl_warn_core_func_unavailable("glReadPixels"); return; }
  glue->glReadPixels(x, y, width, height, format, type, pixels);
}

void
SoGLContext_glCopyTexSubImage2D(const SoGLContext * glue,
                                GLenum target, GLint level,
                                GLint xoffset, GLint yoffset,
                                GLint x, GLint y,
                                GLsizei width, GLsizei height)
{
  if (!glue || !glue->glCopyTexSubImage2D) { sogl_warn_core_func_unavailable("glCopyTexSubImage2D"); return; }
  glue->glCopyTexSubImage2D(target, level, xoffset, yoffset,
                            x, y, width, height);
}

/* -----------------------------------------------------------------------
 * Additional core GL 1.0/1.1 dispatch wrappers.
 *
 * These functions dispatch through the SoGLContext function-pointer table
 * so that dual-GL builds (OBOL_DUAL_GL_BUILD) correctly call either system
 * OpenGL or OSMesa based on which backend owns the current context, without
 * ever mixing backends within the same render pass.
 * --------------------------------------------------------------------- */

void
SoGLContext_glBegin(const SoGLContext * glue, GLenum mode)
{
  if (!glue || !glue->glBegin) { sogl_warn_core_func_unavailable("glBegin"); return; }
  glue->glBegin(mode);
}

void
SoGLContext_glEnd(const SoGLContext * glue)
{
  if (!glue || !glue->glEnd) { sogl_warn_core_func_unavailable("glEnd"); return; }
  glue->glEnd();
}

void
SoGLContext_glVertex2f(const SoGLContext * glue, GLfloat x, GLfloat y)
{
  if (!glue || !glue->glVertex2f) { sogl_warn_core_func_unavailable("glVertex2f"); return; }
  glue->glVertex2f(x, y);
}

void
SoGLContext_glVertex2s(const SoGLContext * glue, GLshort x, GLshort y)
{
  if (!glue || !glue->glVertex2s) { sogl_warn_core_func_unavailable("glVertex2s"); return; }
  glue->glVertex2s(x, y);
}

void
SoGLContext_glVertex3f(const SoGLContext * glue, GLfloat x, GLfloat y, GLfloat z)
{
  if (!glue || !glue->glVertex3f) { sogl_warn_core_func_unavailable("glVertex3f"); return; }
  glue->glVertex3f(x, y, z);
}

void
SoGLContext_glVertex3fv(const SoGLContext * glue, const GLfloat * v)
{
  if (!glue || !glue->glVertex3fv) { sogl_warn_core_func_unavailable("glVertex3fv"); return; }
  glue->glVertex3fv(v);
}

void
SoGLContext_glVertex4fv(const SoGLContext * glue, const GLfloat * v)
{
  if (!glue || !glue->glVertex4fv) { sogl_warn_core_func_unavailable("glVertex4fv"); return; }
  glue->glVertex4fv(v);
}

void
SoGLContext_glNormal3f(const SoGLContext * glue, GLfloat nx, GLfloat ny, GLfloat nz)
{
  if (!glue || !glue->glNormal3f) { sogl_warn_core_func_unavailable("glNormal3f"); return; }
  glue->glNormal3f(nx, ny, nz);
}

void
SoGLContext_glNormal3fv(const SoGLContext * glue, const GLfloat * v)
{
  if (!glue || !glue->glNormal3fv) { sogl_warn_core_func_unavailable("glNormal3fv"); return; }
  glue->glNormal3fv(v);
}

void
SoGLContext_glColor3f(const SoGLContext * glue, GLfloat r, GLfloat g, GLfloat b)
{
  if (!glue || !glue->glColor3f) { sogl_warn_core_func_unavailable("glColor3f"); return; }
  glue->glColor3f(r, g, b);
}

void
SoGLContext_glColor3fv(const SoGLContext * glue, const GLfloat * v)
{
  if (!glue || !glue->glColor3fv) { sogl_warn_core_func_unavailable("glColor3fv"); return; }
  glue->glColor3fv(v);
}

void
SoGLContext_glColor3ub(const SoGLContext * glue, GLubyte r, GLubyte g, GLubyte b)
{
  if (!glue || !glue->glColor3ub) { sogl_warn_core_func_unavailable("glColor3ub"); return; }
  glue->glColor3ub(r, g, b);
}

void
SoGLContext_glColor3ubv(const SoGLContext * glue, const GLubyte * v)
{
  if (!glue || !glue->glColor3ubv) { sogl_warn_core_func_unavailable("glColor3ubv"); return; }
  glue->glColor3ubv(v);
}

void
SoGLContext_glColor4ub(const SoGLContext * glue, GLubyte r, GLubyte g, GLubyte b, GLubyte a)
{
  if (!glue || !glue->glColor4ub) { sogl_warn_core_func_unavailable("glColor4ub"); return; }
  glue->glColor4ub(r, g, b, a);
}

void
SoGLContext_glTexCoord2f(const SoGLContext * glue, GLfloat s, GLfloat t)
{
  if (!glue || !glue->glTexCoord2f) { sogl_warn_core_func_unavailable("glTexCoord2f"); return; }
  glue->glTexCoord2f(s, t);
}

void
SoGLContext_glTexCoord2fv(const SoGLContext * glue, const GLfloat * v)
{
  if (!glue || !glue->glTexCoord2fv) { sogl_warn_core_func_unavailable("glTexCoord2fv"); return; }
  glue->glTexCoord2fv(v);
}

void
SoGLContext_glTexCoord3f(const SoGLContext * glue, GLfloat s, GLfloat t, GLfloat r)
{
  if (!glue || !glue->glTexCoord3f) { sogl_warn_core_func_unavailable("glTexCoord3f"); return; }
  glue->glTexCoord3f(s, t, r);
}

void
SoGLContext_glTexCoord3fv(const SoGLContext * glue, const GLfloat * v)
{
  if (!glue || !glue->glTexCoord3fv) { sogl_warn_core_func_unavailable("glTexCoord3fv"); return; }
  glue->glTexCoord3fv(v);
}

void
SoGLContext_glTexCoord4fv(const SoGLContext * glue, const GLfloat * v)
{
  if (!glue || !glue->glTexCoord4fv) { sogl_warn_core_func_unavailable("glTexCoord4fv"); return; }
  glue->glTexCoord4fv(v);
}

void
SoGLContext_glIndexi(const SoGLContext * glue, GLint c)
{
  if (!glue || !glue->glIndexi) { sogl_warn_core_func_unavailable("glIndexi"); return; }
  glue->glIndexi(c);
}

void
SoGLContext_glMatrixMode(const SoGLContext * glue, GLenum mode)
{
  if (!glue || !glue->glMatrixMode) { sogl_warn_core_func_unavailable("glMatrixMode"); return; }
  glue->glMatrixMode(mode);
}

void
SoGLContext_glLoadIdentity(const SoGLContext * glue)
{
  if (!glue || !glue->glLoadIdentity) { sogl_warn_core_func_unavailable("glLoadIdentity"); return; }
  glue->glLoadIdentity();
}

void
SoGLContext_glLoadMatrixf(const SoGLContext * glue, const GLfloat * m)
{
  if (!glue || !glue->glLoadMatrixf) { sogl_warn_core_func_unavailable("glLoadMatrixf"); return; }
  glue->glLoadMatrixf(m);
}

void
SoGLContext_glLoadMatrixd(const SoGLContext * glue, const GLdouble * m)
{
  if (!glue || !glue->glLoadMatrixd) { sogl_warn_core_func_unavailable("glLoadMatrixd"); return; }
  glue->glLoadMatrixd(m);
}

void
SoGLContext_glMultMatrixf(const SoGLContext * glue, const GLfloat * m)
{
  if (!glue || !glue->glMultMatrixf) { sogl_warn_core_func_unavailable("glMultMatrixf"); return; }
  glue->glMultMatrixf(m);
}

void
SoGLContext_glPushMatrix(const SoGLContext * glue)
{
  if (!glue || !glue->glPushMatrix) { sogl_warn_core_func_unavailable("glPushMatrix"); return; }
  glue->glPushMatrix();
}

void
SoGLContext_glPopMatrix(const SoGLContext * glue)
{
  if (!glue || !glue->glPopMatrix) { sogl_warn_core_func_unavailable("glPopMatrix"); return; }
  glue->glPopMatrix();
}

void
SoGLContext_glOrtho(const SoGLContext * glue,
                    GLdouble left, GLdouble right,
                    GLdouble bottom, GLdouble top,
                    GLdouble near_val, GLdouble far_val)
{
  if (!glue || !glue->glOrtho) { sogl_warn_core_func_unavailable("glOrtho"); return; }
  glue->glOrtho(left, right, bottom, top, near_val, far_val);
}

void
SoGLContext_glFrustum(const SoGLContext * glue,
                      GLdouble left, GLdouble right,
                      GLdouble bottom, GLdouble top,
                      GLdouble near_val, GLdouble far_val)
{
  if (!glue || !glue->glFrustum) { sogl_warn_core_func_unavailable("glFrustum"); return; }
  glue->glFrustum(left, right, bottom, top, near_val, far_val);
}

void
SoGLContext_glTranslatef(const SoGLContext * glue, GLfloat x, GLfloat y, GLfloat z)
{
  if (!glue || !glue->glTranslatef) { sogl_warn_core_func_unavailable("glTranslatef"); return; }
  glue->glTranslatef(x, y, z);
}

void
SoGLContext_glRotatef(const SoGLContext * glue, GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
  if (!glue || !glue->glRotatef) { sogl_warn_core_func_unavailable("glRotatef"); return; }
  glue->glRotatef(angle, x, y, z);
}

void
SoGLContext_glScalef(const SoGLContext * glue, GLfloat x, GLfloat y, GLfloat z)
{
  if (!glue || !glue->glScalef) { sogl_warn_core_func_unavailable("glScalef"); return; }
  glue->glScalef(x, y, z);
}

void
SoGLContext_glLightf(const SoGLContext * glue, GLenum light, GLenum pname, GLfloat param)
{
  if (!glue || !glue->glLightf) { sogl_warn_core_func_unavailable("glLightf"); return; }
  glue->glLightf(light, pname, param);
}

void
SoGLContext_glLightfv(const SoGLContext * glue, GLenum light, GLenum pname, const GLfloat * params)
{
  if (!glue || !glue->glLightfv) { sogl_warn_core_func_unavailable("glLightfv"); return; }
  glue->glLightfv(light, pname, params);
}

void
SoGLContext_glLightModeli(const SoGLContext * glue, GLenum pname, GLint param)
{
  if (!glue || !glue->glLightModeli) { sogl_warn_core_func_unavailable("glLightModeli"); return; }
  glue->glLightModeli(pname, param);
}

void
SoGLContext_glLightModelfv(const SoGLContext * glue, GLenum pname, const GLfloat * params)
{
  if (!glue || !glue->glLightModelfv) { sogl_warn_core_func_unavailable("glLightModelfv"); return; }
  glue->glLightModelfv(pname, params);
}

void
SoGLContext_glMaterialf(const SoGLContext * glue, GLenum face, GLenum pname, GLfloat param)
{
  if (!glue || !glue->glMaterialf) { sogl_warn_core_func_unavailable("glMaterialf"); return; }
  glue->glMaterialf(face, pname, param);
}

void
SoGLContext_glMaterialfv(const SoGLContext * glue, GLenum face, GLenum pname, const GLfloat * params)
{
  if (!glue || !glue->glMaterialfv) { sogl_warn_core_func_unavailable("glMaterialfv"); return; }
  glue->glMaterialfv(face, pname, params);
}

void
SoGLContext_glColorMaterial(const SoGLContext * glue, GLenum face, GLenum mode)
{
  if (!glue || !glue->glColorMaterial) { sogl_warn_core_func_unavailable("glColorMaterial"); return; }
  glue->glColorMaterial(face, mode);
}

void
SoGLContext_glFogi(const SoGLContext * glue, GLenum pname, GLint param)
{
  if (!glue || !glue->glFogi) { sogl_warn_core_func_unavailable("glFogi"); return; }
  glue->glFogi(pname, param);
}

void
SoGLContext_glFogf(const SoGLContext * glue, GLenum pname, GLfloat param)
{
  if (!glue || !glue->glFogf) { sogl_warn_core_func_unavailable("glFogf"); return; }
  glue->glFogf(pname, param);
}

void
SoGLContext_glFogfv(const SoGLContext * glue, GLenum pname, const GLfloat * params)
{
  if (!glue || !glue->glFogfv) { sogl_warn_core_func_unavailable("glFogfv"); return; }
  glue->glFogfv(pname, params);
}

void
SoGLContext_glTexEnvi(const SoGLContext * glue, GLenum target, GLenum pname, GLint param)
{
  if (!glue || !glue->glTexEnvi) { sogl_warn_core_func_unavailable("glTexEnvi"); return; }
  glue->glTexEnvi(target, pname, param);
}

void
SoGLContext_glTexEnvf(const SoGLContext * glue, GLenum target, GLenum pname, GLfloat param)
{
  if (!glue || !glue->glTexEnvf) { sogl_warn_core_func_unavailable("glTexEnvf"); return; }
  glue->glTexEnvf(target, pname, param);
}

void
SoGLContext_glTexEnvfv(const SoGLContext * glue, GLenum target, GLenum pname, const GLfloat * params)
{
  if (!glue || !glue->glTexEnvfv) { sogl_warn_core_func_unavailable("glTexEnvfv"); return; }
  glue->glTexEnvfv(target, pname, params);
}

void
SoGLContext_glTexGeni(const SoGLContext * glue, GLenum coord, GLenum pname, GLint param)
{
  if (!glue || !glue->glTexGeni) { sogl_warn_core_func_unavailable("glTexGeni"); return; }
  glue->glTexGeni(coord, pname, param);
}

void
SoGLContext_glTexGenf(const SoGLContext * glue, GLenum coord, GLenum pname, GLfloat param)
{
  if (!glue || !glue->glTexGenf) { sogl_warn_core_func_unavailable("glTexGenf"); return; }
  glue->glTexGenf(coord, pname, param);
}

void
SoGLContext_glTexGenfv(const SoGLContext * glue, GLenum coord, GLenum pname, const GLfloat * params)
{
  if (!glue || !glue->glTexGenfv) { sogl_warn_core_func_unavailable("glTexGenfv"); return; }
  glue->glTexGenfv(coord, pname, params);
}

void
SoGLContext_glCopyTexImage2D(const SoGLContext * glue,
                             GLenum target, GLint level,
                             GLenum internalformat,
                             GLint x, GLint y,
                             GLsizei width, GLsizei height, GLint border)
{
  if (!glue || !glue->glCopyTexImage2D) { sogl_warn_core_func_unavailable("glCopyTexImage2D"); return; }
  glue->glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
}

void
SoGLContext_glRasterPos2f(const SoGLContext * glue, GLfloat x, GLfloat y)
{
  if (!glue || !glue->glRasterPos2f) { sogl_warn_core_func_unavailable("glRasterPos2f"); return; }
  glue->glRasterPos2f(x, y);
}

void
SoGLContext_glRasterPos3f(const SoGLContext * glue, GLfloat x, GLfloat y, GLfloat z)
{
  if (!glue || !glue->glRasterPos3f) { sogl_warn_core_func_unavailable("glRasterPos3f"); return; }
  glue->glRasterPos3f(x, y, z);
}

void
SoGLContext_glBitmap(const SoGLContext * glue,
                     GLsizei width, GLsizei height,
                     GLfloat xorig, GLfloat yorig,
                     GLfloat xmove, GLfloat ymove,
                     const GLubyte * bitmap)
{
  if (!glue || !glue->glBitmap) { sogl_warn_core_func_unavailable("glBitmap"); return; }
  glue->glBitmap(width, height, xorig, yorig, xmove, ymove, bitmap);
}

void
SoGLContext_glDrawPixels(const SoGLContext * glue,
                         GLsizei width, GLsizei height,
                         GLenum format, GLenum type, const GLvoid * pixels)
{
  if (!glue || !glue->glDrawPixels) { sogl_warn_core_func_unavailable("glDrawPixels"); return; }
  glue->glDrawPixels(width, height, format, type, pixels);
}

void
SoGLContext_glPixelTransferf(const SoGLContext * glue, GLenum pname, GLfloat param)
{
  if (!glue || !glue->glPixelTransferf) { sogl_warn_core_func_unavailable("glPixelTransferf"); return; }
  glue->glPixelTransferf(pname, param);
}

void
SoGLContext_glPixelTransferi(const SoGLContext * glue, GLenum pname, GLint param)
{
  if (!glue || !glue->glPixelTransferi) { sogl_warn_core_func_unavailable("glPixelTransferi"); return; }
  glue->glPixelTransferi(pname, param);
}

void
SoGLContext_glPixelMapfv(const SoGLContext * glue,
                         GLenum map, GLint mapsize, const GLfloat * values)
{
  if (!glue || !glue->glPixelMapfv) { sogl_warn_core_func_unavailable("glPixelMapfv"); return; }
  glue->glPixelMapfv(map, mapsize, values);
}

void
SoGLContext_glPixelMapuiv(const SoGLContext * glue,
                          GLenum map, GLint mapsize, const GLuint * values)
{
  if (!glue || !glue->glPixelMapuiv) { sogl_warn_core_func_unavailable("glPixelMapuiv"); return; }
  glue->glPixelMapuiv(map, mapsize, values);
}

void
SoGLContext_glPixelZoom(const SoGLContext * glue, GLfloat xfactor, GLfloat yfactor)
{
  if (!glue || !glue->glPixelZoom) { sogl_warn_core_func_unavailable("glPixelZoom"); return; }
  glue->glPixelZoom(xfactor, yfactor);
}

void
SoGLContext_glViewport(const SoGLContext * glue,
                       GLint x, GLint y, GLsizei width, GLsizei height)
{
  if (!glue || !glue->glViewport) { sogl_warn_core_func_unavailable("glViewport"); return; }
  glue->glViewport(x, y, width, height);
}

void
SoGLContext_glScissor(const SoGLContext * glue,
                      GLint x, GLint y, GLsizei width, GLsizei height)
{
  if (!glue || !glue->glScissor) { sogl_warn_core_func_unavailable("glScissor"); return; }
  glue->glScissor(x, y, width, height);
}

void
SoGLContext_glDepthMask(const SoGLContext * glue, GLboolean flag)
{
  if (!glue || !glue->glDepthMask) { sogl_warn_core_func_unavailable("glDepthMask"); return; }
  glue->glDepthMask(flag);
}

void
SoGLContext_glDepthFunc(const SoGLContext * glue, GLenum func)
{
  if (!glue || !glue->glDepthFunc) { sogl_warn_core_func_unavailable("glDepthFunc"); return; }
  glue->glDepthFunc(func);
}

void
SoGLContext_glDepthRange(const SoGLContext * glue, GLclampd near_val, GLclampd far_val)
{
  if (!glue || !glue->glDepthRange) { sogl_warn_core_func_unavailable("glDepthRange"); return; }
  glue->glDepthRange(near_val, far_val);
}

void
SoGLContext_glStencilFunc(const SoGLContext * glue, GLenum func, GLint ref, GLuint mask)
{
  if (!glue || !glue->glStencilFunc) { sogl_warn_core_func_unavailable("glStencilFunc"); return; }
  glue->glStencilFunc(func, ref, mask);
}

void
SoGLContext_glStencilOp(const SoGLContext * glue, GLenum fail, GLenum zfail, GLenum zpass)
{
  if (!glue || !glue->glStencilOp) { sogl_warn_core_func_unavailable("glStencilOp"); return; }
  glue->glStencilOp(fail, zfail, zpass);
}

void
SoGLContext_glBlendFunc(const SoGLContext * glue, GLenum sfactor, GLenum dfactor)
{
  if (!glue || !glue->glBlendFunc) { sogl_warn_core_func_unavailable("glBlendFunc"); return; }
  glue->glBlendFunc(sfactor, dfactor);
}

void
SoGLContext_glAlphaFunc(const SoGLContext * glue, GLenum func, GLclampf ref)
{
  if (!glue || !glue->glAlphaFunc) { sogl_warn_core_func_unavailable("glAlphaFunc"); return; }
  glue->glAlphaFunc(func, ref);
}

void
SoGLContext_glFrontFace(const SoGLContext * glue, GLenum mode)
{
  if (!glue || !glue->glFrontFace) { sogl_warn_core_func_unavailable("glFrontFace"); return; }
  glue->glFrontFace(mode);
}

void
SoGLContext_glCullFace(const SoGLContext * glue, GLenum mode)
{
  if (!glue || !glue->glCullFace) { sogl_warn_core_func_unavailable("glCullFace"); return; }
  glue->glCullFace(mode);
}

void
SoGLContext_glPolygonMode(const SoGLContext * glue, GLenum face, GLenum mode)
{
  if (!glue || !glue->glPolygonMode) { sogl_warn_core_func_unavailable("glPolygonMode"); return; }
  glue->glPolygonMode(face, mode);
}

void
SoGLContext_glPolygonStipple(const SoGLContext * glue, const GLubyte * mask)
{
  if (!glue || !glue->glPolygonStipple) { sogl_warn_core_func_unavailable("glPolygonStipple"); return; }
  glue->glPolygonStipple(mask);
}

void
SoGLContext_glLineWidth(const SoGLContext * glue, GLfloat width)
{
  if (!glue || !glue->glLineWidth) { sogl_warn_core_func_unavailable("glLineWidth"); return; }
  glue->glLineWidth(width);
}

void
SoGLContext_glLineStipple(const SoGLContext * glue, GLint factor, GLushort pattern)
{
  if (!glue || !glue->glLineStipple) { sogl_warn_core_func_unavailable("glLineStipple"); return; }
  glue->glLineStipple(factor, pattern);
}

void
SoGLContext_glPointSize(const SoGLContext * glue, GLfloat size)
{
  if (!glue || !glue->glPointSize) { sogl_warn_core_func_unavailable("glPointSize"); return; }
  glue->glPointSize(size);
}

void
SoGLContext_glColorMask(const SoGLContext * glue,
                        GLboolean red, GLboolean green,
                        GLboolean blue, GLboolean alpha)
{
  if (!glue || !glue->glColorMask) { sogl_warn_core_func_unavailable("glColorMask"); return; }
  glue->glColorMask(red, green, blue, alpha);
}

void
SoGLContext_glClipPlane(const SoGLContext * glue,
                        GLenum plane, const GLdouble * equation)
{
  if (!glue || !glue->glClipPlane) { sogl_warn_core_func_unavailable("glClipPlane"); return; }
  glue->glClipPlane(plane, equation);
}

void
SoGLContext_glDrawBuffer(const SoGLContext * glue, GLenum mode)
{
  if (!glue || !glue->glDrawBuffer) { sogl_warn_core_func_unavailable("glDrawBuffer"); return; }
  glue->glDrawBuffer(mode);
}

void
SoGLContext_glClearIndex(const SoGLContext * glue, GLfloat c)
{
  if (!glue || !glue->glClearIndex) { sogl_warn_core_func_unavailable("glClearIndex"); return; }
  glue->glClearIndex(c);
}

void
SoGLContext_glClearStencil(const SoGLContext * glue, GLint s)
{
  if (!glue || !glue->glClearStencil) { sogl_warn_core_func_unavailable("glClearStencil"); return; }
  glue->glClearStencil(s);
}

void
SoGLContext_glAccum(const SoGLContext * glue, GLenum op, GLfloat value)
{
  if (!glue || !glue->glAccum) { sogl_warn_core_func_unavailable("glAccum"); return; }
  glue->glAccum(op, value);
}

void
SoGLContext_glGetBooleanv(const SoGLContext * glue, GLenum pname, GLboolean * params)
{
  if (!glue || !glue->glGetBooleanv) { sogl_warn_core_func_unavailable("glGetBooleanv"); return; }
  glue->glGetBooleanv(pname, params);
}

void
SoGLContext_glNewList(const SoGLContext * glue, GLuint list, GLenum mode)
{
  if (!glue || !glue->glNewList) { sogl_warn_core_func_unavailable("glNewList"); return; }
  glue->glNewList(list, mode);
}

void
SoGLContext_glEndList(const SoGLContext * glue)
{
  if (!glue || !glue->glEndList) { sogl_warn_core_func_unavailable("glEndList"); return; }
  glue->glEndList();
}

void
SoGLContext_glCallList(const SoGLContext * glue, GLuint list)
{
  if (!glue || !glue->glCallList) { sogl_warn_core_func_unavailable("glCallList"); return; }
  glue->glCallList(list);
}

void
SoGLContext_glDeleteLists(const SoGLContext * glue, GLuint list, GLsizei range)
{
  if (!glue || !glue->glDeleteLists) { sogl_warn_core_func_unavailable("glDeleteLists"); return; }
  glue->glDeleteLists(list, range);
}

GLuint
SoGLContext_glGenLists(const SoGLContext * glue, GLsizei range)
{
  if (!glue || !glue->glGenLists) { sogl_warn_core_func_unavailable("glGenLists"); return 0; }
  return glue->glGenLists(range);
}

void
SoGLContext_glPushAttrib(const SoGLContext * glue, GLbitfield mask)
{
  if (!glue || !glue->glPushAttrib) { sogl_warn_core_func_unavailable("glPushAttrib"); return; }
  glue->glPushAttrib(mask);
}

void
SoGLContext_glPopAttrib(const SoGLContext * glue)
{
  if (!glue || !glue->glPopAttrib) { sogl_warn_core_func_unavailable("glPopAttrib"); return; }
  glue->glPopAttrib();
}

/* ********************************************************************** */

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

/* -----------------------------------------------------------------------
 * Full OSMesa GL 1.0-1.3 feature-set dispatch wrappers.
 *
 * These wrap every core OpenGL function that external/osmesa provides,
 * so Inventor/gl.h can redirect any OSMesa-supported call through the
 * correct backend without falling back to a raw system-GL symbol.
 *
 * These dispatch through function pointers in the SoGLContext struct and
 * therefore work correctly for both system-GL and OSMesa contexts.  Only
 * one definition is needed, so exclude this block from the OSMesa
 * compilation unit (gl_osmesa.cpp) where SOGL_PREFIX_SET is defined.
 * --------------------------------------------------------------------- */
#ifndef SOGL_PREFIX_SET

GLboolean
SoGLContext_glAreTexturesResident(const SoGLContext * glue, GLsizei n, const GLuint * textures, GLboolean * residences)
{
  if (!glue || !glue->glAreTexturesResident) { sogl_warn_core_func_unavailable("glAreTexturesResident"); return GL_FALSE; }
  return glue->glAreTexturesResident(n, textures, residences);
}

void
SoGLContext_glBlendColor(const SoGLContext * glue, GLclampf red, GLclampf green, GLclampf blue, GLclampf alpha)
{
  if (!glue || !glue->glBlendColor) { sogl_warn_core_func_unavailable("glBlendColor"); return; }
  glue->glBlendColor(red, green, blue, alpha);
}

void
SoGLContext_glCallLists(const SoGLContext * glue, GLsizei n, GLenum type, const GLvoid * lists)
{
  if (!glue || !glue->glCallLists) { sogl_warn_core_func_unavailable("glCallLists"); return; }
  glue->glCallLists(n, type, lists);
}

void
SoGLContext_glClearAccum(const SoGLContext * glue, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
  if (!glue || !glue->glClearAccum) { sogl_warn_core_func_unavailable("glClearAccum"); return; }
  glue->glClearAccum(red, green, blue, alpha);
}

void
SoGLContext_glClearDepth(const SoGLContext * glue, GLclampd depth)
{
  if (!glue || !glue->glClearDepth) { sogl_warn_core_func_unavailable("glClearDepth"); return; }
  glue->glClearDepth(depth);
}

void
SoGLContext_glColor3b(const SoGLContext * glue, GLbyte red, GLbyte green, GLbyte blue)
{
  if (!glue || !glue->glColor3b) { sogl_warn_core_func_unavailable("glColor3b"); return; }
  glue->glColor3b(red, green, blue);
}

void
SoGLContext_glColor3bv(const SoGLContext * glue, const GLbyte * v)
{
  if (!glue || !glue->glColor3bv) { sogl_warn_core_func_unavailable("glColor3bv"); return; }
  glue->glColor3bv(v);
}

void
SoGLContext_glColor3d(const SoGLContext * glue, GLdouble red, GLdouble green, GLdouble blue)
{
  if (!glue || !glue->glColor3d) { sogl_warn_core_func_unavailable("glColor3d"); return; }
  glue->glColor3d(red, green, blue);
}

void
SoGLContext_glColor3dv(const SoGLContext * glue, const GLdouble * v)
{
  if (!glue || !glue->glColor3dv) { sogl_warn_core_func_unavailable("glColor3dv"); return; }
  glue->glColor3dv(v);
}

void
SoGLContext_glColor3i(const SoGLContext * glue, GLint red, GLint green, GLint blue)
{
  if (!glue || !glue->glColor3i) { sogl_warn_core_func_unavailable("glColor3i"); return; }
  glue->glColor3i(red, green, blue);
}

void
SoGLContext_glColor3iv(const SoGLContext * glue, const GLint * v)
{
  if (!glue || !glue->glColor3iv) { sogl_warn_core_func_unavailable("glColor3iv"); return; }
  glue->glColor3iv(v);
}

void
SoGLContext_glColor3s(const SoGLContext * glue, GLshort red, GLshort green, GLshort blue)
{
  if (!glue || !glue->glColor3s) { sogl_warn_core_func_unavailable("glColor3s"); return; }
  glue->glColor3s(red, green, blue);
}

void
SoGLContext_glColor3sv(const SoGLContext * glue, const GLshort * v)
{
  if (!glue || !glue->glColor3sv) { sogl_warn_core_func_unavailable("glColor3sv"); return; }
  glue->glColor3sv(v);
}

void
SoGLContext_glColor3ui(const SoGLContext * glue, GLuint red, GLuint green, GLuint blue)
{
  if (!glue || !glue->glColor3ui) { sogl_warn_core_func_unavailable("glColor3ui"); return; }
  glue->glColor3ui(red, green, blue);
}

void
SoGLContext_glColor3uiv(const SoGLContext * glue, const GLuint * v)
{
  if (!glue || !glue->glColor3uiv) { sogl_warn_core_func_unavailable("glColor3uiv"); return; }
  glue->glColor3uiv(v);
}

void
SoGLContext_glColor3us(const SoGLContext * glue, GLushort red, GLushort green, GLushort blue)
{
  if (!glue || !glue->glColor3us) { sogl_warn_core_func_unavailable("glColor3us"); return; }
  glue->glColor3us(red, green, blue);
}

void
SoGLContext_glColor3usv(const SoGLContext * glue, const GLushort * v)
{
  if (!glue || !glue->glColor3usv) { sogl_warn_core_func_unavailable("glColor3usv"); return; }
  glue->glColor3usv(v);
}

void
SoGLContext_glColor4b(const SoGLContext * glue, GLbyte red, GLbyte green, GLbyte blue, GLbyte alpha)
{
  if (!glue || !glue->glColor4b) { sogl_warn_core_func_unavailable("glColor4b"); return; }
  glue->glColor4b(red, green, blue, alpha);
}

void
SoGLContext_glColor4bv(const SoGLContext * glue, const GLbyte * v)
{
  if (!glue || !glue->glColor4bv) { sogl_warn_core_func_unavailable("glColor4bv"); return; }
  glue->glColor4bv(v);
}

void
SoGLContext_glColor4d(const SoGLContext * glue, GLdouble red, GLdouble green, GLdouble blue, GLdouble alpha)
{
  if (!glue || !glue->glColor4d) { sogl_warn_core_func_unavailable("glColor4d"); return; }
  glue->glColor4d(red, green, blue, alpha);
}

void
SoGLContext_glColor4dv(const SoGLContext * glue, const GLdouble * v)
{
  if (!glue || !glue->glColor4dv) { sogl_warn_core_func_unavailable("glColor4dv"); return; }
  glue->glColor4dv(v);
}

void
SoGLContext_glColor4f(const SoGLContext * glue, GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha)
{
  if (!glue || !glue->glColor4f) { sogl_warn_core_func_unavailable("glColor4f"); return; }
  glue->glColor4f(red, green, blue, alpha);
}

void
SoGLContext_glColor4fv(const SoGLContext * glue, const GLfloat * v)
{
  if (!glue || !glue->glColor4fv) { sogl_warn_core_func_unavailable("glColor4fv"); return; }
  glue->glColor4fv(v);
}

void
SoGLContext_glColor4i(const SoGLContext * glue, GLint red, GLint green, GLint blue, GLint alpha)
{
  if (!glue || !glue->glColor4i) { sogl_warn_core_func_unavailable("glColor4i"); return; }
  glue->glColor4i(red, green, blue, alpha);
}

void
SoGLContext_glColor4iv(const SoGLContext * glue, const GLint * v)
{
  if (!glue || !glue->glColor4iv) { sogl_warn_core_func_unavailable("glColor4iv"); return; }
  glue->glColor4iv(v);
}

void
SoGLContext_glColor4s(const SoGLContext * glue, GLshort red, GLshort green, GLshort blue, GLshort alpha)
{
  if (!glue || !glue->glColor4s) { sogl_warn_core_func_unavailable("glColor4s"); return; }
  glue->glColor4s(red, green, blue, alpha);
}

void
SoGLContext_glColor4sv(const SoGLContext * glue, const GLshort * v)
{
  if (!glue || !glue->glColor4sv) { sogl_warn_core_func_unavailable("glColor4sv"); return; }
  glue->glColor4sv(v);
}

void
SoGLContext_glColor4ubv(const SoGLContext * glue, const GLubyte * v)
{
  if (!glue || !glue->glColor4ubv) { sogl_warn_core_func_unavailable("glColor4ubv"); return; }
  glue->glColor4ubv(v);
}

void
SoGLContext_glColor4ui(const SoGLContext * glue, GLuint red, GLuint green, GLuint blue, GLuint alpha)
{
  if (!glue || !glue->glColor4ui) { sogl_warn_core_func_unavailable("glColor4ui"); return; }
  glue->glColor4ui(red, green, blue, alpha);
}

void
SoGLContext_glColor4uiv(const SoGLContext * glue, const GLuint * v)
{
  if (!glue || !glue->glColor4uiv) { sogl_warn_core_func_unavailable("glColor4uiv"); return; }
  glue->glColor4uiv(v);
}

void
SoGLContext_glColor4us(const SoGLContext * glue, GLushort red, GLushort green, GLushort blue, GLushort alpha)
{
  if (!glue || !glue->glColor4us) { sogl_warn_core_func_unavailable("glColor4us"); return; }
  glue->glColor4us(red, green, blue, alpha);
}

void
SoGLContext_glColor4usv(const SoGLContext * glue, const GLushort * v)
{
  if (!glue || !glue->glColor4usv) { sogl_warn_core_func_unavailable("glColor4usv"); return; }
  glue->glColor4usv(v);
}

void
SoGLContext_glColorTableParameterfv(const SoGLContext * glue, GLenum target, GLenum pname, const GLfloat * params)
{
  if (!glue || !glue->glColorTableParameterfv) { sogl_warn_core_func_unavailable("glColorTableParameterfv"); return; }
  glue->glColorTableParameterfv(target, pname, params);
}

void
SoGLContext_glColorTableParameteriv(const SoGLContext * glue, GLenum target, GLenum pname, const GLint * params)
{
  if (!glue || !glue->glColorTableParameteriv) { sogl_warn_core_func_unavailable("glColorTableParameteriv"); return; }
  glue->glColorTableParameteriv(target, pname, params);
}

void
SoGLContext_glConvolutionFilter1D(const SoGLContext * glue, GLenum target, GLenum internalformat, GLsizei width, GLenum format, GLenum type, const GLvoid * image)
{
  if (!glue || !glue->glConvolutionFilter1D) { sogl_warn_core_func_unavailable("glConvolutionFilter1D"); return; }
  glue->glConvolutionFilter1D(target, internalformat, width, format, type, image);
}

void
SoGLContext_glConvolutionFilter2D(const SoGLContext * glue, GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid * image)
{
  if (!glue || !glue->glConvolutionFilter2D) { sogl_warn_core_func_unavailable("glConvolutionFilter2D"); return; }
  glue->glConvolutionFilter2D(target, internalformat, width, height, format, type, image);
}

void
SoGLContext_glConvolutionParameterf(const SoGLContext * glue, GLenum target, GLenum pname, GLfloat params)
{
  if (!glue || !glue->glConvolutionParameterf) { sogl_warn_core_func_unavailable("glConvolutionParameterf"); return; }
  glue->glConvolutionParameterf(target, pname, params);
}

void
SoGLContext_glConvolutionParameterfv(const SoGLContext * glue, GLenum target, GLenum pname, const GLfloat * params)
{
  if (!glue || !glue->glConvolutionParameterfv) { sogl_warn_core_func_unavailable("glConvolutionParameterfv"); return; }
  glue->glConvolutionParameterfv(target, pname, params);
}

void
SoGLContext_glConvolutionParameteri(const SoGLContext * glue, GLenum target, GLenum pname, GLint params)
{
  if (!glue || !glue->glConvolutionParameteri) { sogl_warn_core_func_unavailable("glConvolutionParameteri"); return; }
  glue->glConvolutionParameteri(target, pname, params);
}

void
SoGLContext_glConvolutionParameteriv(const SoGLContext * glue, GLenum target, GLenum pname, const GLint * params)
{
  if (!glue || !glue->glConvolutionParameteriv) { sogl_warn_core_func_unavailable("glConvolutionParameteriv"); return; }
  glue->glConvolutionParameteriv(target, pname, params);
}

void
SoGLContext_glCopyColorSubTable(const SoGLContext * glue, GLenum target, GLsizei start, GLint x, GLint y, GLsizei width)
{
  if (!glue || !glue->glCopyColorSubTable) { sogl_warn_core_func_unavailable("glCopyColorSubTable"); return; }
  glue->glCopyColorSubTable(target, start, x, y, width);
}

void
SoGLContext_glCopyColorTable(const SoGLContext * glue, GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width)
{
  if (!glue || !glue->glCopyColorTable) { sogl_warn_core_func_unavailable("glCopyColorTable"); return; }
  glue->glCopyColorTable(target, internalformat, x, y, width);
}

void
SoGLContext_glCopyConvolutionFilter1D(const SoGLContext * glue, GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width)
{
  if (!glue || !glue->glCopyConvolutionFilter1D) { sogl_warn_core_func_unavailable("glCopyConvolutionFilter1D"); return; }
  glue->glCopyConvolutionFilter1D(target, internalformat, x, y, width);
}

void
SoGLContext_glCopyConvolutionFilter2D(const SoGLContext * glue, GLenum target, GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height)
{
  if (!glue || !glue->glCopyConvolutionFilter2D) { sogl_warn_core_func_unavailable("glCopyConvolutionFilter2D"); return; }
  glue->glCopyConvolutionFilter2D(target, internalformat, x, y, width, height);
}

void
SoGLContext_glCopyPixels(const SoGLContext * glue, GLint x, GLint y, GLsizei width, GLsizei height, GLenum type)
{
  if (!glue || !glue->glCopyPixels) { sogl_warn_core_func_unavailable("glCopyPixels"); return; }
  glue->glCopyPixels(x, y, width, height, type);
}

void
SoGLContext_glCopyTexImage1D(const SoGLContext * glue, GLenum target, GLint level, GLenum internalformat, GLint x, GLint y, GLsizei width, GLint border)
{
  if (!glue || !glue->glCopyTexImage1D) { sogl_warn_core_func_unavailable("glCopyTexImage1D"); return; }
  glue->glCopyTexImage1D(target, level, internalformat, x, y, width, border);
}

void
SoGLContext_glCopyTexSubImage1D(const SoGLContext * glue, GLenum target, GLint level, GLint xoffset, GLint x, GLint y, GLsizei width)
{
  if (!glue || !glue->glCopyTexSubImage1D) { sogl_warn_core_func_unavailable("glCopyTexSubImage1D"); return; }
  glue->glCopyTexSubImage1D(target, level, xoffset, x, y, width);
}

void
SoGLContext_glEdgeFlag(const SoGLContext * glue, GLboolean flag)
{
  if (!glue || !glue->glEdgeFlag) { sogl_warn_core_func_unavailable("glEdgeFlag"); return; }
  glue->glEdgeFlag(flag);
}

void
SoGLContext_glEdgeFlagPointer(const SoGLContext * glue, GLsizei stride, const GLvoid * ptr)
{
  if (!glue || !glue->glEdgeFlagPointer) { sogl_warn_core_func_unavailable("glEdgeFlagPointer"); return; }
  glue->glEdgeFlagPointer(stride, ptr);
}

void
SoGLContext_glEdgeFlagv(const SoGLContext * glue, const GLboolean * flag)
{
  if (!glue || !glue->glEdgeFlagv) { sogl_warn_core_func_unavailable("glEdgeFlagv"); return; }
  glue->glEdgeFlagv(flag);
}

void
SoGLContext_glEvalCoord1d(const SoGLContext * glue, GLdouble u)
{
  if (!glue || !glue->glEvalCoord1d) { sogl_warn_core_func_unavailable("glEvalCoord1d"); return; }
  glue->glEvalCoord1d(u);
}

void
SoGLContext_glEvalCoord1dv(const SoGLContext * glue, const GLdouble * u)
{
  if (!glue || !glue->glEvalCoord1dv) { sogl_warn_core_func_unavailable("glEvalCoord1dv"); return; }
  glue->glEvalCoord1dv(u);
}

void
SoGLContext_glEvalCoord1f(const SoGLContext * glue, GLfloat u)
{
  if (!glue || !glue->glEvalCoord1f) { sogl_warn_core_func_unavailable("glEvalCoord1f"); return; }
  glue->glEvalCoord1f(u);
}

void
SoGLContext_glEvalCoord1fv(const SoGLContext * glue, const GLfloat * u)
{
  if (!glue || !glue->glEvalCoord1fv) { sogl_warn_core_func_unavailable("glEvalCoord1fv"); return; }
  glue->glEvalCoord1fv(u);
}

void
SoGLContext_glEvalCoord2d(const SoGLContext * glue, GLdouble u, GLdouble v)
{
  if (!glue || !glue->glEvalCoord2d) { sogl_warn_core_func_unavailable("glEvalCoord2d"); return; }
  glue->glEvalCoord2d(u, v);
}

void
SoGLContext_glEvalCoord2dv(const SoGLContext * glue, const GLdouble * u)
{
  if (!glue || !glue->glEvalCoord2dv) { sogl_warn_core_func_unavailable("glEvalCoord2dv"); return; }
  glue->glEvalCoord2dv(u);
}

void
SoGLContext_glEvalCoord2f(const SoGLContext * glue, GLfloat u, GLfloat v)
{
  if (!glue || !glue->glEvalCoord2f) { sogl_warn_core_func_unavailable("glEvalCoord2f"); return; }
  glue->glEvalCoord2f(u, v);
}

void
SoGLContext_glEvalCoord2fv(const SoGLContext * glue, const GLfloat * u)
{
  if (!glue || !glue->glEvalCoord2fv) { sogl_warn_core_func_unavailable("glEvalCoord2fv"); return; }
  glue->glEvalCoord2fv(u);
}

void
SoGLContext_glEvalMesh1(const SoGLContext * glue, GLenum mode, GLint i1, GLint i2)
{
  if (!glue || !glue->glEvalMesh1) { sogl_warn_core_func_unavailable("glEvalMesh1"); return; }
  glue->glEvalMesh1(mode, i1, i2);
}

void
SoGLContext_glEvalMesh2(const SoGLContext * glue, GLenum mode, GLint i1, GLint i2, GLint j1, GLint j2)
{
  if (!glue || !glue->glEvalMesh2) { sogl_warn_core_func_unavailable("glEvalMesh2"); return; }
  glue->glEvalMesh2(mode, i1, i2, j1, j2);
}

void
SoGLContext_glEvalPoint1(const SoGLContext * glue, GLint i)
{
  if (!glue || !glue->glEvalPoint1) { sogl_warn_core_func_unavailable("glEvalPoint1"); return; }
  glue->glEvalPoint1(i);
}

void
SoGLContext_glEvalPoint2(const SoGLContext * glue, GLint i, GLint j)
{
  if (!glue || !glue->glEvalPoint2) { sogl_warn_core_func_unavailable("glEvalPoint2"); return; }
  glue->glEvalPoint2(i, j);
}

void
SoGLContext_glFeedbackBuffer(const SoGLContext * glue, GLsizei size, GLenum type, GLfloat * buffer)
{
  if (!glue || !glue->glFeedbackBuffer) { sogl_warn_core_func_unavailable("glFeedbackBuffer"); return; }
  glue->glFeedbackBuffer(size, type, buffer);
}

void
SoGLContext_glFogiv(const SoGLContext * glue, GLenum pname, const GLint * params)
{
  if (!glue || !glue->glFogiv) { sogl_warn_core_func_unavailable("glFogiv"); return; }
  glue->glFogiv(pname, params);
}

void
SoGLContext_glGetClipPlane(const SoGLContext * glue, GLenum plane, GLdouble * equation)
{
  if (!glue || !glue->glGetClipPlane) { sogl_warn_core_func_unavailable("glGetClipPlane"); return; }
  glue->glGetClipPlane(plane, equation);
}

void
SoGLContext_glGetConvolutionFilter(const SoGLContext * glue, GLenum target, GLenum format, GLenum type, GLvoid * image)
{
  if (!glue || !glue->glGetConvolutionFilter) { sogl_warn_core_func_unavailable("glGetConvolutionFilter"); return; }
  glue->glGetConvolutionFilter(target, format, type, image);
}

void
SoGLContext_glGetConvolutionParameterfv(const SoGLContext * glue, GLenum target, GLenum pname, GLfloat * params)
{
  if (!glue || !glue->glGetConvolutionParameterfv) { sogl_warn_core_func_unavailable("glGetConvolutionParameterfv"); return; }
  glue->glGetConvolutionParameterfv(target, pname, params);
}

void
SoGLContext_glGetConvolutionParameteriv(const SoGLContext * glue, GLenum target, GLenum pname, GLint * params)
{
  if (!glue || !glue->glGetConvolutionParameteriv) { sogl_warn_core_func_unavailable("glGetConvolutionParameteriv"); return; }
  glue->glGetConvolutionParameteriv(target, pname, params);
}

void
SoGLContext_glGetDoublev(const SoGLContext * glue, GLenum pname, GLdouble * params)
{
  if (!glue || !glue->glGetDoublev) { sogl_warn_core_func_unavailable("glGetDoublev"); return; }
  glue->glGetDoublev(pname, params);
}

void
SoGLContext_glGetHistogram(const SoGLContext * glue, GLenum target, GLboolean reset, GLenum format, GLenum type, GLvoid * values)
{
  if (!glue || !glue->glGetHistogram) { sogl_warn_core_func_unavailable("glGetHistogram"); return; }
  glue->glGetHistogram(target, reset, format, type, values);
}

void
SoGLContext_glGetHistogramParameterfv(const SoGLContext * glue, GLenum target, GLenum pname, GLfloat * params)
{
  if (!glue || !glue->glGetHistogramParameterfv) { sogl_warn_core_func_unavailable("glGetHistogramParameterfv"); return; }
  glue->glGetHistogramParameterfv(target, pname, params);
}

void
SoGLContext_glGetHistogramParameteriv(const SoGLContext * glue, GLenum target, GLenum pname, GLint * params)
{
  if (!glue || !glue->glGetHistogramParameteriv) { sogl_warn_core_func_unavailable("glGetHistogramParameteriv"); return; }
  glue->glGetHistogramParameteriv(target, pname, params);
}

void
SoGLContext_glGetLightfv(const SoGLContext * glue, GLenum light, GLenum pname, GLfloat * params)
{
  if (!glue || !glue->glGetLightfv) { sogl_warn_core_func_unavailable("glGetLightfv"); return; }
  glue->glGetLightfv(light, pname, params);
}

void
SoGLContext_glGetLightiv(const SoGLContext * glue, GLenum light, GLenum pname, GLint * params)
{
  if (!glue || !glue->glGetLightiv) { sogl_warn_core_func_unavailable("glGetLightiv"); return; }
  glue->glGetLightiv(light, pname, params);
}

void
SoGLContext_glGetMapdv(const SoGLContext * glue, GLenum target, GLenum query, GLdouble * v)
{
  if (!glue || !glue->glGetMapdv) { sogl_warn_core_func_unavailable("glGetMapdv"); return; }
  glue->glGetMapdv(target, query, v);
}

void
SoGLContext_glGetMapfv(const SoGLContext * glue, GLenum target, GLenum query, GLfloat * v)
{
  if (!glue || !glue->glGetMapfv) { sogl_warn_core_func_unavailable("glGetMapfv"); return; }
  glue->glGetMapfv(target, query, v);
}

void
SoGLContext_glGetMapiv(const SoGLContext * glue, GLenum target, GLenum query, GLint * v)
{
  if (!glue || !glue->glGetMapiv) { sogl_warn_core_func_unavailable("glGetMapiv"); return; }
  glue->glGetMapiv(target, query, v);
}

void
SoGLContext_glGetMaterialfv(const SoGLContext * glue, GLenum face, GLenum pname, GLfloat * params)
{
  if (!glue || !glue->glGetMaterialfv) { sogl_warn_core_func_unavailable("glGetMaterialfv"); return; }
  glue->glGetMaterialfv(face, pname, params);
}

void
SoGLContext_glGetMaterialiv(const SoGLContext * glue, GLenum face, GLenum pname, GLint * params)
{
  if (!glue || !glue->glGetMaterialiv) { sogl_warn_core_func_unavailable("glGetMaterialiv"); return; }
  glue->glGetMaterialiv(face, pname, params);
}

void
SoGLContext_glGetMinmax(const SoGLContext * glue, GLenum target, GLboolean reset, GLenum format, GLenum types, GLvoid * values)
{
  if (!glue || !glue->glGetMinmax) { sogl_warn_core_func_unavailable("glGetMinmax"); return; }
  glue->glGetMinmax(target, reset, format, types, values);
}

void
SoGLContext_glGetMinmaxParameterfv(const SoGLContext * glue, GLenum target, GLenum pname, GLfloat * params)
{
  if (!glue || !glue->glGetMinmaxParameterfv) { sogl_warn_core_func_unavailable("glGetMinmaxParameterfv"); return; }
  glue->glGetMinmaxParameterfv(target, pname, params);
}

void
SoGLContext_glGetMinmaxParameteriv(const SoGLContext * glue, GLenum target, GLenum pname, GLint * params)
{
  if (!glue || !glue->glGetMinmaxParameteriv) { sogl_warn_core_func_unavailable("glGetMinmaxParameteriv"); return; }
  glue->glGetMinmaxParameteriv(target, pname, params);
}

void
SoGLContext_glGetPixelMapfv(const SoGLContext * glue, GLenum map, GLfloat * values)
{
  if (!glue || !glue->glGetPixelMapfv) { sogl_warn_core_func_unavailable("glGetPixelMapfv"); return; }
  glue->glGetPixelMapfv(map, values);
}

void
SoGLContext_glGetPixelMapuiv(const SoGLContext * glue, GLenum map, GLuint * values)
{
  if (!glue || !glue->glGetPixelMapuiv) { sogl_warn_core_func_unavailable("glGetPixelMapuiv"); return; }
  glue->glGetPixelMapuiv(map, values);
}

void
SoGLContext_glGetPixelMapusv(const SoGLContext * glue, GLenum map, GLushort * values)
{
  if (!glue || !glue->glGetPixelMapusv) { sogl_warn_core_func_unavailable("glGetPixelMapusv"); return; }
  glue->glGetPixelMapusv(map, values);
}

void
SoGLContext_glGetPointerv(const SoGLContext * glue, GLenum pname, GLvoid ** params)
{
  if (!glue || !glue->glGetPointerv) { sogl_warn_core_func_unavailable("glGetPointerv"); return; }
  glue->glGetPointerv(pname, params);
}

void
SoGLContext_glGetPolygonStipple(const SoGLContext * glue, GLubyte * mask)
{
  if (!glue || !glue->glGetPolygonStipple) { sogl_warn_core_func_unavailable("glGetPolygonStipple"); return; }
  glue->glGetPolygonStipple(mask);
}

void
SoGLContext_glGetSeparableFilter(const SoGLContext * glue, GLenum target, GLenum format, GLenum type, GLvoid * row, GLvoid * column, GLvoid * span)
{
  if (!glue || !glue->glGetSeparableFilter) { sogl_warn_core_func_unavailable("glGetSeparableFilter"); return; }
  glue->glGetSeparableFilter(target, format, type, row, column, span);
}

void
SoGLContext_glGetTexEnvfv(const SoGLContext * glue, GLenum target, GLenum pname, GLfloat * params)
{
  if (!glue || !glue->glGetTexEnvfv) { sogl_warn_core_func_unavailable("glGetTexEnvfv"); return; }
  glue->glGetTexEnvfv(target, pname, params);
}

void
SoGLContext_glGetTexEnviv(const SoGLContext * glue, GLenum target, GLenum pname, GLint * params)
{
  if (!glue || !glue->glGetTexEnviv) { sogl_warn_core_func_unavailable("glGetTexEnviv"); return; }
  glue->glGetTexEnviv(target, pname, params);
}

void
SoGLContext_glGetTexGendv(const SoGLContext * glue, GLenum coord, GLenum pname, GLdouble * params)
{
  if (!glue || !glue->glGetTexGendv) { sogl_warn_core_func_unavailable("glGetTexGendv"); return; }
  glue->glGetTexGendv(coord, pname, params);
}

void
SoGLContext_glGetTexGenfv(const SoGLContext * glue, GLenum coord, GLenum pname, GLfloat * params)
{
  if (!glue || !glue->glGetTexGenfv) { sogl_warn_core_func_unavailable("glGetTexGenfv"); return; }
  glue->glGetTexGenfv(coord, pname, params);
}

void
SoGLContext_glGetTexGeniv(const SoGLContext * glue, GLenum coord, GLenum pname, GLint * params)
{
  if (!glue || !glue->glGetTexGeniv) { sogl_warn_core_func_unavailable("glGetTexGeniv"); return; }
  glue->glGetTexGeniv(coord, pname, params);
}

void
SoGLContext_glGetTexImage(const SoGLContext * glue, GLenum target, GLint level, GLenum format, GLenum type, GLvoid * pixels)
{
  if (!glue || !glue->glGetTexImage) { sogl_warn_core_func_unavailable("glGetTexImage"); return; }
  glue->glGetTexImage(target, level, format, type, pixels);
}

void
SoGLContext_glGetTexLevelParameterfv(const SoGLContext * glue, GLenum target, GLint level, GLenum pname, GLfloat * params)
{
  if (!glue || !glue->glGetTexLevelParameterfv) { sogl_warn_core_func_unavailable("glGetTexLevelParameterfv"); return; }
  glue->glGetTexLevelParameterfv(target, level, pname, params);
}

void
SoGLContext_glGetTexLevelParameteriv(const SoGLContext * glue, GLenum target, GLint level, GLenum pname, GLint * params)
{
  if (!glue || !glue->glGetTexLevelParameteriv) { sogl_warn_core_func_unavailable("glGetTexLevelParameteriv"); return; }
  glue->glGetTexLevelParameteriv(target, level, pname, params);
}

void
SoGLContext_glGetTexParameterfv(const SoGLContext * glue, GLenum target, GLenum pname, GLfloat * params)
{
  if (!glue || !glue->glGetTexParameterfv) { sogl_warn_core_func_unavailable("glGetTexParameterfv"); return; }
  glue->glGetTexParameterfv(target, pname, params);
}

void
SoGLContext_glGetTexParameteriv(const SoGLContext * glue, GLenum target, GLenum pname, GLint * params)
{
  if (!glue || !glue->glGetTexParameteriv) { sogl_warn_core_func_unavailable("glGetTexParameteriv"); return; }
  glue->glGetTexParameteriv(target, pname, params);
}

void
SoGLContext_glHint(const SoGLContext * glue, GLenum target, GLenum mode)
{
  if (!glue || !glue->glHint) { sogl_warn_core_func_unavailable("glHint"); return; }
  glue->glHint(target, mode);
}

void
SoGLContext_glHistogram(const SoGLContext * glue, GLenum target, GLsizei width, GLenum internalformat, GLboolean sink)
{
  if (!glue || !glue->glHistogram) { sogl_warn_core_func_unavailable("glHistogram"); return; }
  glue->glHistogram(target, width, internalformat, sink);
}

void
SoGLContext_glIndexMask(const SoGLContext * glue, GLuint mask)
{
  if (!glue || !glue->glIndexMask) { sogl_warn_core_func_unavailable("glIndexMask"); return; }
  glue->glIndexMask(mask);
}

void
SoGLContext_glIndexd(const SoGLContext * glue, GLdouble c)
{
  if (!glue || !glue->glIndexd) { sogl_warn_core_func_unavailable("glIndexd"); return; }
  glue->glIndexd(c);
}

void
SoGLContext_glIndexdv(const SoGLContext * glue, const GLdouble * c)
{
  if (!glue || !glue->glIndexdv) { sogl_warn_core_func_unavailable("glIndexdv"); return; }
  glue->glIndexdv(c);
}

void
SoGLContext_glIndexf(const SoGLContext * glue, GLfloat c)
{
  if (!glue || !glue->glIndexf) { sogl_warn_core_func_unavailable("glIndexf"); return; }
  glue->glIndexf(c);
}

void
SoGLContext_glIndexfv(const SoGLContext * glue, const GLfloat * c)
{
  if (!glue || !glue->glIndexfv) { sogl_warn_core_func_unavailable("glIndexfv"); return; }
  glue->glIndexfv(c);
}

void
SoGLContext_glIndexiv(const SoGLContext * glue, const GLint * c)
{
  if (!glue || !glue->glIndexiv) { sogl_warn_core_func_unavailable("glIndexiv"); return; }
  glue->glIndexiv(c);
}

void
SoGLContext_glIndexs(const SoGLContext * glue, GLshort c)
{
  if (!glue || !glue->glIndexs) { sogl_warn_core_func_unavailable("glIndexs"); return; }
  glue->glIndexs(c);
}

void
SoGLContext_glIndexsv(const SoGLContext * glue, const GLshort * c)
{
  if (!glue || !glue->glIndexsv) { sogl_warn_core_func_unavailable("glIndexsv"); return; }
  glue->glIndexsv(c);
}

void
SoGLContext_glIndexub(const SoGLContext * glue, GLubyte c)
{
  if (!glue || !glue->glIndexub) { sogl_warn_core_func_unavailable("glIndexub"); return; }
  glue->glIndexub(c);
}

void
SoGLContext_glIndexubv(const SoGLContext * glue, const GLubyte * c)
{
  if (!glue || !glue->glIndexubv) { sogl_warn_core_func_unavailable("glIndexubv"); return; }
  glue->glIndexubv(c);
}

void
SoGLContext_glInitNames(const SoGLContext * glue)
{
  if (!glue || !glue->glInitNames) { sogl_warn_core_func_unavailable("glInitNames"); return; }
  glue->glInitNames();
}

GLboolean
SoGLContext_glIsList(const SoGLContext * glue, GLuint list)
{
  if (!glue || !glue->glIsList) { sogl_warn_core_func_unavailable("glIsList"); return GL_FALSE; }
  return glue->glIsList(list);
}

GLboolean
SoGLContext_glIsTexture(const SoGLContext * glue, GLuint texture)
{
  if (!glue || !glue->glIsTexture) { sogl_warn_core_func_unavailable("glIsTexture"); return GL_FALSE; }
  return glue->glIsTexture(texture);
}

void
SoGLContext_glLightModelf(const SoGLContext * glue, GLenum pname, GLfloat param)
{
  if (!glue || !glue->glLightModelf) { sogl_warn_core_func_unavailable("glLightModelf"); return; }
  glue->glLightModelf(pname, param);
}

void
SoGLContext_glLightModeliv(const SoGLContext * glue, GLenum pname, const GLint * params)
{
  if (!glue || !glue->glLightModeliv) { sogl_warn_core_func_unavailable("glLightModeliv"); return; }
  glue->glLightModeliv(pname, params);
}

void
SoGLContext_glLighti(const SoGLContext * glue, GLenum light, GLenum pname, GLint param)
{
  if (!glue || !glue->glLighti) { sogl_warn_core_func_unavailable("glLighti"); return; }
  glue->glLighti(light, pname, param);
}

void
SoGLContext_glLightiv(const SoGLContext * glue, GLenum light, GLenum pname, const GLint * params)
{
  if (!glue || !glue->glLightiv) { sogl_warn_core_func_unavailable("glLightiv"); return; }
  glue->glLightiv(light, pname, params);
}

void
SoGLContext_glListBase(const SoGLContext * glue, GLuint base)
{
  if (!glue || !glue->glListBase) { sogl_warn_core_func_unavailable("glListBase"); return; }
  glue->glListBase(base);
}

void
SoGLContext_glLoadName(const SoGLContext * glue, GLuint name)
{
  if (!glue || !glue->glLoadName) { sogl_warn_core_func_unavailable("glLoadName"); return; }
  glue->glLoadName(name);
}

void
SoGLContext_glLoadTransposeMatrixd(const SoGLContext * glue, const GLdouble * m)
{
  if (!glue || !glue->glLoadTransposeMatrixd) { sogl_warn_core_func_unavailable("glLoadTransposeMatrixd"); return; }
  glue->glLoadTransposeMatrixd(m);
}

void
SoGLContext_glLoadTransposeMatrixf(const SoGLContext * glue, const GLfloat * m)
{
  if (!glue || !glue->glLoadTransposeMatrixf) { sogl_warn_core_func_unavailable("glLoadTransposeMatrixf"); return; }
  glue->glLoadTransposeMatrixf(m);
}

void
SoGLContext_glLogicOp(const SoGLContext * glue, GLenum opcode)
{
  if (!glue || !glue->glLogicOp) { sogl_warn_core_func_unavailable("glLogicOp"); return; }
  glue->glLogicOp(opcode);
}

void
SoGLContext_glMap1d(const SoGLContext * glue, GLenum target, GLdouble u1, GLdouble u2, GLint stride, GLint order, const GLdouble * points)
{
  if (!glue || !glue->glMap1d) { sogl_warn_core_func_unavailable("glMap1d"); return; }
  glue->glMap1d(target, u1, u2, stride, order, points);
}

void
SoGLContext_glMap1f(const SoGLContext * glue, GLenum target, GLfloat u1, GLfloat u2, GLint stride, GLint order, const GLfloat * points)
{
  if (!glue || !glue->glMap1f) { sogl_warn_core_func_unavailable("glMap1f"); return; }
  glue->glMap1f(target, u1, u2, stride, order, points);
}

void
SoGLContext_glMap2d(const SoGLContext * glue, GLenum target, GLdouble u1, GLdouble u2, GLint ustride, GLint uorder, GLdouble v1, GLdouble v2, GLint vstride, GLint vorder, const GLdouble * points)
{
  if (!glue || !glue->glMap2d) { sogl_warn_core_func_unavailable("glMap2d"); return; }
  glue->glMap2d(target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points);
}

void
SoGLContext_glMap2f(const SoGLContext * glue, GLenum target, GLfloat u1, GLfloat u2, GLint ustride, GLint uorder, GLfloat v1, GLfloat v2, GLint vstride, GLint vorder, const GLfloat * points)
{
  if (!glue || !glue->glMap2f) { sogl_warn_core_func_unavailable("glMap2f"); return; }
  glue->glMap2f(target, u1, u2, ustride, uorder, v1, v2, vstride, vorder, points);
}

void
SoGLContext_glMapGrid1d(const SoGLContext * glue, GLint un, GLdouble u1, GLdouble u2)
{
  if (!glue || !glue->glMapGrid1d) { sogl_warn_core_func_unavailable("glMapGrid1d"); return; }
  glue->glMapGrid1d(un, u1, u2);
}

void
SoGLContext_glMapGrid1f(const SoGLContext * glue, GLint un, GLfloat u1, GLfloat u2)
{
  if (!glue || !glue->glMapGrid1f) { sogl_warn_core_func_unavailable("glMapGrid1f"); return; }
  glue->glMapGrid1f(un, u1, u2);
}

void
SoGLContext_glMapGrid2d(const SoGLContext * glue, GLint un, GLdouble u1, GLdouble u2, GLint vn, GLdouble v1, GLdouble v2)
{
  if (!glue || !glue->glMapGrid2d) { sogl_warn_core_func_unavailable("glMapGrid2d"); return; }
  glue->glMapGrid2d(un, u1, u2, vn, v1, v2);
}

void
SoGLContext_glMapGrid2f(const SoGLContext * glue, GLint un, GLfloat u1, GLfloat u2, GLint vn, GLfloat v1, GLfloat v2)
{
  if (!glue || !glue->glMapGrid2f) { sogl_warn_core_func_unavailable("glMapGrid2f"); return; }
  glue->glMapGrid2f(un, u1, u2, vn, v1, v2);
}

void
SoGLContext_glMateriali(const SoGLContext * glue, GLenum face, GLenum pname, GLint param)
{
  if (!glue || !glue->glMateriali) { sogl_warn_core_func_unavailable("glMateriali"); return; }
  glue->glMateriali(face, pname, param);
}

void
SoGLContext_glMaterialiv(const SoGLContext * glue, GLenum face, GLenum pname, const GLint * params)
{
  if (!glue || !glue->glMaterialiv) { sogl_warn_core_func_unavailable("glMaterialiv"); return; }
  glue->glMaterialiv(face, pname, params);
}

void
SoGLContext_glMinmax(const SoGLContext * glue, GLenum target, GLenum internalformat, GLboolean sink)
{
  if (!glue || !glue->glMinmax) { sogl_warn_core_func_unavailable("glMinmax"); return; }
  glue->glMinmax(target, internalformat, sink);
}

void
SoGLContext_glMultMatrixd(const SoGLContext * glue, const GLdouble * m)
{
  if (!glue || !glue->glMultMatrixd) { sogl_warn_core_func_unavailable("glMultMatrixd"); return; }
  glue->glMultMatrixd(m);
}

void
SoGLContext_glMultTransposeMatrixd(const SoGLContext * glue, const GLdouble * m)
{
  if (!glue || !glue->glMultTransposeMatrixd) { sogl_warn_core_func_unavailable("glMultTransposeMatrixd"); return; }
  glue->glMultTransposeMatrixd(m);
}

void
SoGLContext_glMultTransposeMatrixf(const SoGLContext * glue, const GLfloat * m)
{
  if (!glue || !glue->glMultTransposeMatrixf) { sogl_warn_core_func_unavailable("glMultTransposeMatrixf"); return; }
  glue->glMultTransposeMatrixf(m);
}

void
SoGLContext_glMultiTexCoord1d(const SoGLContext * glue, GLenum target, GLdouble s)
{
  if (!glue || !glue->glMultiTexCoord1d) { sogl_warn_core_func_unavailable("glMultiTexCoord1d"); return; }
  glue->glMultiTexCoord1d(target, s);
}

void
SoGLContext_glMultiTexCoord1dv(const SoGLContext * glue, GLenum target, const GLdouble * v)
{
  if (!glue || !glue->glMultiTexCoord1dv) { sogl_warn_core_func_unavailable("glMultiTexCoord1dv"); return; }
  glue->glMultiTexCoord1dv(target, v);
}

void
SoGLContext_glMultiTexCoord1f(const SoGLContext * glue, GLenum target, GLfloat s)
{
  if (!glue || !glue->glMultiTexCoord1f) { sogl_warn_core_func_unavailable("glMultiTexCoord1f"); return; }
  glue->glMultiTexCoord1f(target, s);
}

void
SoGLContext_glMultiTexCoord1fv(const SoGLContext * glue, GLenum target, const GLfloat * v)
{
  if (!glue || !glue->glMultiTexCoord1fv) { sogl_warn_core_func_unavailable("glMultiTexCoord1fv"); return; }
  glue->glMultiTexCoord1fv(target, v);
}

void
SoGLContext_glMultiTexCoord1i(const SoGLContext * glue, GLenum target, GLint s)
{
  if (!glue || !glue->glMultiTexCoord1i) { sogl_warn_core_func_unavailable("glMultiTexCoord1i"); return; }
  glue->glMultiTexCoord1i(target, s);
}

void
SoGLContext_glMultiTexCoord1iv(const SoGLContext * glue, GLenum target, const GLint * v)
{
  if (!glue || !glue->glMultiTexCoord1iv) { sogl_warn_core_func_unavailable("glMultiTexCoord1iv"); return; }
  glue->glMultiTexCoord1iv(target, v);
}

void
SoGLContext_glMultiTexCoord1s(const SoGLContext * glue, GLenum target, GLshort s)
{
  if (!glue || !glue->glMultiTexCoord1s) { sogl_warn_core_func_unavailable("glMultiTexCoord1s"); return; }
  glue->glMultiTexCoord1s(target, s);
}

void
SoGLContext_glMultiTexCoord1sv(const SoGLContext * glue, GLenum target, const GLshort * v)
{
  if (!glue || !glue->glMultiTexCoord1sv) { sogl_warn_core_func_unavailable("glMultiTexCoord1sv"); return; }
  glue->glMultiTexCoord1sv(target, v);
}

void
SoGLContext_glMultiTexCoord2d(const SoGLContext * glue, GLenum target, GLdouble s, GLdouble t)
{
  if (!glue || !glue->glMultiTexCoord2d) { sogl_warn_core_func_unavailable("glMultiTexCoord2d"); return; }
  glue->glMultiTexCoord2d(target, s, t);
}

void
SoGLContext_glMultiTexCoord2dv(const SoGLContext * glue, GLenum target, const GLdouble * v)
{
  if (!glue || !glue->glMultiTexCoord2dv) { sogl_warn_core_func_unavailable("glMultiTexCoord2dv"); return; }
  glue->glMultiTexCoord2dv(target, v);
}

void
SoGLContext_glMultiTexCoord2i(const SoGLContext * glue, GLenum target, GLint s, GLint t)
{
  if (!glue || !glue->glMultiTexCoord2i) { sogl_warn_core_func_unavailable("glMultiTexCoord2i"); return; }
  glue->glMultiTexCoord2i(target, s, t);
}

void
SoGLContext_glMultiTexCoord2iv(const SoGLContext * glue, GLenum target, const GLint * v)
{
  if (!glue || !glue->glMultiTexCoord2iv) { sogl_warn_core_func_unavailable("glMultiTexCoord2iv"); return; }
  glue->glMultiTexCoord2iv(target, v);
}

void
SoGLContext_glMultiTexCoord2s(const SoGLContext * glue, GLenum target, GLshort s, GLshort t)
{
  if (!glue || !glue->glMultiTexCoord2s) { sogl_warn_core_func_unavailable("glMultiTexCoord2s"); return; }
  glue->glMultiTexCoord2s(target, s, t);
}

void
SoGLContext_glMultiTexCoord2sv(const SoGLContext * glue, GLenum target, const GLshort * v)
{
  if (!glue || !glue->glMultiTexCoord2sv) { sogl_warn_core_func_unavailable("glMultiTexCoord2sv"); return; }
  glue->glMultiTexCoord2sv(target, v);
}

void
SoGLContext_glMultiTexCoord3d(const SoGLContext * glue, GLenum target, GLdouble s, GLdouble t, GLdouble r)
{
  if (!glue || !glue->glMultiTexCoord3d) { sogl_warn_core_func_unavailable("glMultiTexCoord3d"); return; }
  glue->glMultiTexCoord3d(target, s, t, r);
}

void
SoGLContext_glMultiTexCoord3dv(const SoGLContext * glue, GLenum target, const GLdouble * v)
{
  if (!glue || !glue->glMultiTexCoord3dv) { sogl_warn_core_func_unavailable("glMultiTexCoord3dv"); return; }
  glue->glMultiTexCoord3dv(target, v);
}

void
SoGLContext_glMultiTexCoord3f(const SoGLContext * glue, GLenum target, GLfloat s, GLfloat t, GLfloat r)
{
  if (!glue || !glue->glMultiTexCoord3f) { sogl_warn_core_func_unavailable("glMultiTexCoord3f"); return; }
  glue->glMultiTexCoord3f(target, s, t, r);
}

void
SoGLContext_glMultiTexCoord3i(const SoGLContext * glue, GLenum target, GLint s, GLint t, GLint r)
{
  if (!glue || !glue->glMultiTexCoord3i) { sogl_warn_core_func_unavailable("glMultiTexCoord3i"); return; }
  glue->glMultiTexCoord3i(target, s, t, r);
}

void
SoGLContext_glMultiTexCoord3iv(const SoGLContext * glue, GLenum target, const GLint * v)
{
  if (!glue || !glue->glMultiTexCoord3iv) { sogl_warn_core_func_unavailable("glMultiTexCoord3iv"); return; }
  glue->glMultiTexCoord3iv(target, v);
}

void
SoGLContext_glMultiTexCoord3s(const SoGLContext * glue, GLenum target, GLshort s, GLshort t, GLshort r)
{
  if (!glue || !glue->glMultiTexCoord3s) { sogl_warn_core_func_unavailable("glMultiTexCoord3s"); return; }
  glue->glMultiTexCoord3s(target, s, t, r);
}

void
SoGLContext_glMultiTexCoord3sv(const SoGLContext * glue, GLenum target, const GLshort * v)
{
  if (!glue || !glue->glMultiTexCoord3sv) { sogl_warn_core_func_unavailable("glMultiTexCoord3sv"); return; }
  glue->glMultiTexCoord3sv(target, v);
}

void
SoGLContext_glMultiTexCoord4d(const SoGLContext * glue, GLenum target, GLdouble s, GLdouble t, GLdouble r, GLdouble q)
{
  if (!glue || !glue->glMultiTexCoord4d) { sogl_warn_core_func_unavailable("glMultiTexCoord4d"); return; }
  glue->glMultiTexCoord4d(target, s, t, r, q);
}

void
SoGLContext_glMultiTexCoord4dv(const SoGLContext * glue, GLenum target, const GLdouble * v)
{
  if (!glue || !glue->glMultiTexCoord4dv) { sogl_warn_core_func_unavailable("glMultiTexCoord4dv"); return; }
  glue->glMultiTexCoord4dv(target, v);
}

void
SoGLContext_glMultiTexCoord4f(const SoGLContext * glue, GLenum target, GLfloat s, GLfloat t, GLfloat r, GLfloat q)
{
  if (!glue || !glue->glMultiTexCoord4f) { sogl_warn_core_func_unavailable("glMultiTexCoord4f"); return; }
  glue->glMultiTexCoord4f(target, s, t, r, q);
}

void
SoGLContext_glMultiTexCoord4i(const SoGLContext * glue, GLenum target, GLint s, GLint t, GLint r, GLint q)
{
  if (!glue || !glue->glMultiTexCoord4i) { sogl_warn_core_func_unavailable("glMultiTexCoord4i"); return; }
  glue->glMultiTexCoord4i(target, s, t, r, q);
}

void
SoGLContext_glMultiTexCoord4iv(const SoGLContext * glue, GLenum target, const GLint * v)
{
  if (!glue || !glue->glMultiTexCoord4iv) { sogl_warn_core_func_unavailable("glMultiTexCoord4iv"); return; }
  glue->glMultiTexCoord4iv(target, v);
}

void
SoGLContext_glMultiTexCoord4s(const SoGLContext * glue, GLenum target, GLshort s, GLshort t, GLshort r, GLshort q)
{
  if (!glue || !glue->glMultiTexCoord4s) { sogl_warn_core_func_unavailable("glMultiTexCoord4s"); return; }
  glue->glMultiTexCoord4s(target, s, t, r, q);
}

void
SoGLContext_glMultiTexCoord4sv(const SoGLContext * glue, GLenum target, const GLshort * v)
{
  if (!glue || !glue->glMultiTexCoord4sv) { sogl_warn_core_func_unavailable("glMultiTexCoord4sv"); return; }
  glue->glMultiTexCoord4sv(target, v);
}

void
SoGLContext_glNormal3b(const SoGLContext * glue, GLbyte nx, GLbyte ny, GLbyte nz)
{
  if (!glue || !glue->glNormal3b) { sogl_warn_core_func_unavailable("glNormal3b"); return; }
  glue->glNormal3b(nx, ny, nz);
}

void
SoGLContext_glNormal3bv(const SoGLContext * glue, const GLbyte * v)
{
  if (!glue || !glue->glNormal3bv) { sogl_warn_core_func_unavailable("glNormal3bv"); return; }
  glue->glNormal3bv(v);
}

void
SoGLContext_glNormal3d(const SoGLContext * glue, GLdouble nx, GLdouble ny, GLdouble nz)
{
  if (!glue || !glue->glNormal3d) { sogl_warn_core_func_unavailable("glNormal3d"); return; }
  glue->glNormal3d(nx, ny, nz);
}

void
SoGLContext_glNormal3dv(const SoGLContext * glue, const GLdouble * v)
{
  if (!glue || !glue->glNormal3dv) { sogl_warn_core_func_unavailable("glNormal3dv"); return; }
  glue->glNormal3dv(v);
}

void
SoGLContext_glNormal3i(const SoGLContext * glue, GLint nx, GLint ny, GLint nz)
{
  if (!glue || !glue->glNormal3i) { sogl_warn_core_func_unavailable("glNormal3i"); return; }
  glue->glNormal3i(nx, ny, nz);
}

void
SoGLContext_glNormal3iv(const SoGLContext * glue, const GLint * v)
{
  if (!glue || !glue->glNormal3iv) { sogl_warn_core_func_unavailable("glNormal3iv"); return; }
  glue->glNormal3iv(v);
}

void
SoGLContext_glNormal3s(const SoGLContext * glue, GLshort nx, GLshort ny, GLshort nz)
{
  if (!glue || !glue->glNormal3s) { sogl_warn_core_func_unavailable("glNormal3s"); return; }
  glue->glNormal3s(nx, ny, nz);
}

void
SoGLContext_glNormal3sv(const SoGLContext * glue, const GLshort * v)
{
  if (!glue || !glue->glNormal3sv) { sogl_warn_core_func_unavailable("glNormal3sv"); return; }
  glue->glNormal3sv(v);
}

void
SoGLContext_glPassThrough(const SoGLContext * glue, GLfloat token)
{
  if (!glue || !glue->glPassThrough) { sogl_warn_core_func_unavailable("glPassThrough"); return; }
  glue->glPassThrough(token);
}

void
SoGLContext_glPixelMapusv(const SoGLContext * glue, GLenum map, GLsizei mapsize, const GLushort * values)
{
  if (!glue || !glue->glPixelMapusv) { sogl_warn_core_func_unavailable("glPixelMapusv"); return; }
  glue->glPixelMapusv(map, mapsize, values);
}

void
SoGLContext_glPixelStoref(const SoGLContext * glue, GLenum pname, GLfloat param)
{
  if (!glue || !glue->glPixelStoref) { sogl_warn_core_func_unavailable("glPixelStoref"); return; }
  glue->glPixelStoref(pname, param);
}

void
SoGLContext_glPopName(const SoGLContext * glue)
{
  if (!glue || !glue->glPopName) { sogl_warn_core_func_unavailable("glPopName"); return; }
  glue->glPopName();
}

void
SoGLContext_glPrioritizeTextures(const SoGLContext * glue, GLsizei n, const GLuint * textures, const GLclampf * priorities)
{
  if (!glue || !glue->glPrioritizeTextures) { sogl_warn_core_func_unavailable("glPrioritizeTextures"); return; }
  glue->glPrioritizeTextures(n, textures, priorities);
}

void
SoGLContext_glPushName(const SoGLContext * glue, GLuint name)
{
  if (!glue || !glue->glPushName) { sogl_warn_core_func_unavailable("glPushName"); return; }
  glue->glPushName(name);
}

void
SoGLContext_glRasterPos2d(const SoGLContext * glue, GLdouble x, GLdouble y)
{
  if (!glue || !glue->glRasterPos2d) { sogl_warn_core_func_unavailable("glRasterPos2d"); return; }
  glue->glRasterPos2d(x, y);
}

void
SoGLContext_glRasterPos2dv(const SoGLContext * glue, const GLdouble * v)
{
  if (!glue || !glue->glRasterPos2dv) { sogl_warn_core_func_unavailable("glRasterPos2dv"); return; }
  glue->glRasterPos2dv(v);
}

void
SoGLContext_glRasterPos2fv(const SoGLContext * glue, const GLfloat * v)
{
  if (!glue || !glue->glRasterPos2fv) { sogl_warn_core_func_unavailable("glRasterPos2fv"); return; }
  glue->glRasterPos2fv(v);
}

void
SoGLContext_glRasterPos2i(const SoGLContext * glue, GLint x, GLint y)
{
  if (!glue || !glue->glRasterPos2i) { sogl_warn_core_func_unavailable("glRasterPos2i"); return; }
  glue->glRasterPos2i(x, y);
}

void
SoGLContext_glRasterPos2iv(const SoGLContext * glue, const GLint * v)
{
  if (!glue || !glue->glRasterPos2iv) { sogl_warn_core_func_unavailable("glRasterPos2iv"); return; }
  glue->glRasterPos2iv(v);
}

void
SoGLContext_glRasterPos2s(const SoGLContext * glue, GLshort x, GLshort y)
{
  if (!glue || !glue->glRasterPos2s) { sogl_warn_core_func_unavailable("glRasterPos2s"); return; }
  glue->glRasterPos2s(x, y);
}

void
SoGLContext_glRasterPos2sv(const SoGLContext * glue, const GLshort * v)
{
  if (!glue || !glue->glRasterPos2sv) { sogl_warn_core_func_unavailable("glRasterPos2sv"); return; }
  glue->glRasterPos2sv(v);
}

void
SoGLContext_glRasterPos3d(const SoGLContext * glue, GLdouble x, GLdouble y, GLdouble z)
{
  if (!glue || !glue->glRasterPos3d) { sogl_warn_core_func_unavailable("glRasterPos3d"); return; }
  glue->glRasterPos3d(x, y, z);
}

void
SoGLContext_glRasterPos3dv(const SoGLContext * glue, const GLdouble * v)
{
  if (!glue || !glue->glRasterPos3dv) { sogl_warn_core_func_unavailable("glRasterPos3dv"); return; }
  glue->glRasterPos3dv(v);
}

void
SoGLContext_glRasterPos3fv(const SoGLContext * glue, const GLfloat * v)
{
  if (!glue || !glue->glRasterPos3fv) { sogl_warn_core_func_unavailable("glRasterPos3fv"); return; }
  glue->glRasterPos3fv(v);
}

void
SoGLContext_glRasterPos3i(const SoGLContext * glue, GLint x, GLint y, GLint z)
{
  if (!glue || !glue->glRasterPos3i) { sogl_warn_core_func_unavailable("glRasterPos3i"); return; }
  glue->glRasterPos3i(x, y, z);
}

void
SoGLContext_glRasterPos3iv(const SoGLContext * glue, const GLint * v)
{
  if (!glue || !glue->glRasterPos3iv) { sogl_warn_core_func_unavailable("glRasterPos3iv"); return; }
  glue->glRasterPos3iv(v);
}

void
SoGLContext_glRasterPos3s(const SoGLContext * glue, GLshort x, GLshort y, GLshort z)
{
  if (!glue || !glue->glRasterPos3s) { sogl_warn_core_func_unavailable("glRasterPos3s"); return; }
  glue->glRasterPos3s(x, y, z);
}

void
SoGLContext_glRasterPos3sv(const SoGLContext * glue, const GLshort * v)
{
  if (!glue || !glue->glRasterPos3sv) { sogl_warn_core_func_unavailable("glRasterPos3sv"); return; }
  glue->glRasterPos3sv(v);
}

void
SoGLContext_glRasterPos4d(const SoGLContext * glue, GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
  if (!glue || !glue->glRasterPos4d) { sogl_warn_core_func_unavailable("glRasterPos4d"); return; }
  glue->glRasterPos4d(x, y, z, w);
}

void
SoGLContext_glRasterPos4dv(const SoGLContext * glue, const GLdouble * v)
{
  if (!glue || !glue->glRasterPos4dv) { sogl_warn_core_func_unavailable("glRasterPos4dv"); return; }
  glue->glRasterPos4dv(v);
}

void
SoGLContext_glRasterPos4f(const SoGLContext * glue, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
  if (!glue || !glue->glRasterPos4f) { sogl_warn_core_func_unavailable("glRasterPos4f"); return; }
  glue->glRasterPos4f(x, y, z, w);
}

void
SoGLContext_glRasterPos4fv(const SoGLContext * glue, const GLfloat * v)
{
  if (!glue || !glue->glRasterPos4fv) { sogl_warn_core_func_unavailable("glRasterPos4fv"); return; }
  glue->glRasterPos4fv(v);
}

void
SoGLContext_glRasterPos4i(const SoGLContext * glue, GLint x, GLint y, GLint z, GLint w)
{
  if (!glue || !glue->glRasterPos4i) { sogl_warn_core_func_unavailable("glRasterPos4i"); return; }
  glue->glRasterPos4i(x, y, z, w);
}

void
SoGLContext_glRasterPos4iv(const SoGLContext * glue, const GLint * v)
{
  if (!glue || !glue->glRasterPos4iv) { sogl_warn_core_func_unavailable("glRasterPos4iv"); return; }
  glue->glRasterPos4iv(v);
}

void
SoGLContext_glRasterPos4s(const SoGLContext * glue, GLshort x, GLshort y, GLshort z, GLshort w)
{
  if (!glue || !glue->glRasterPos4s) { sogl_warn_core_func_unavailable("glRasterPos4s"); return; }
  glue->glRasterPos4s(x, y, z, w);
}

void
SoGLContext_glRasterPos4sv(const SoGLContext * glue, const GLshort * v)
{
  if (!glue || !glue->glRasterPos4sv) { sogl_warn_core_func_unavailable("glRasterPos4sv"); return; }
  glue->glRasterPos4sv(v);
}

void
SoGLContext_glReadBuffer(const SoGLContext * glue, GLenum mode)
{
  if (!glue || !glue->glReadBuffer) { sogl_warn_core_func_unavailable("glReadBuffer"); return; }
  glue->glReadBuffer(mode);
}

void
SoGLContext_glRectd(const SoGLContext * glue, GLdouble x1, GLdouble y1, GLdouble x2, GLdouble y2)
{
  if (!glue || !glue->glRectd) { sogl_warn_core_func_unavailable("glRectd"); return; }
  glue->glRectd(x1, y1, x2, y2);
}

void
SoGLContext_glRectdv(const SoGLContext * glue, const GLdouble * v1, const GLdouble * v2)
{
  if (!glue || !glue->glRectdv) { sogl_warn_core_func_unavailable("glRectdv"); return; }
  glue->glRectdv(v1, v2);
}

void
SoGLContext_glRectf(const SoGLContext * glue, GLfloat x1, GLfloat y1, GLfloat x2, GLfloat y2)
{
  if (!glue || !glue->glRectf) { sogl_warn_core_func_unavailable("glRectf"); return; }
  glue->glRectf(x1, y1, x2, y2);
}

void
SoGLContext_glRectfv(const SoGLContext * glue, const GLfloat * v1, const GLfloat * v2)
{
  if (!glue || !glue->glRectfv) { sogl_warn_core_func_unavailable("glRectfv"); return; }
  glue->glRectfv(v1, v2);
}

void
SoGLContext_glRecti(const SoGLContext * glue, GLint x1, GLint y1, GLint x2, GLint y2)
{
  if (!glue || !glue->glRecti) { sogl_warn_core_func_unavailable("glRecti"); return; }
  glue->glRecti(x1, y1, x2, y2);
}

void
SoGLContext_glRectiv(const SoGLContext * glue, const GLint * v1, const GLint * v2)
{
  if (!glue || !glue->glRectiv) { sogl_warn_core_func_unavailable("glRectiv"); return; }
  glue->glRectiv(v1, v2);
}

void
SoGLContext_glRects(const SoGLContext * glue, GLshort x1, GLshort y1, GLshort x2, GLshort y2)
{
  if (!glue || !glue->glRects) { sogl_warn_core_func_unavailable("glRects"); return; }
  glue->glRects(x1, y1, x2, y2);
}

void
SoGLContext_glRectsv(const SoGLContext * glue, const GLshort * v1, const GLshort * v2)
{
  if (!glue || !glue->glRectsv) { sogl_warn_core_func_unavailable("glRectsv"); return; }
  glue->glRectsv(v1, v2);
}

GLint
SoGLContext_glRenderMode(const SoGLContext * glue, GLenum mode)
{
  if (!glue || !glue->glRenderMode) { sogl_warn_core_func_unavailable("glRenderMode"); return 0; }
  return glue->glRenderMode(mode);
}

void
SoGLContext_glResetHistogram(const SoGLContext * glue, GLenum target)
{
  if (!glue || !glue->glResetHistogram) { sogl_warn_core_func_unavailable("glResetHistogram"); return; }
  glue->glResetHistogram(target);
}

void
SoGLContext_glResetMinmax(const SoGLContext * glue, GLenum target)
{
  if (!glue || !glue->glResetMinmax) { sogl_warn_core_func_unavailable("glResetMinmax"); return; }
  glue->glResetMinmax(target);
}

void
SoGLContext_glRotated(const SoGLContext * glue, GLdouble angle, GLdouble x, GLdouble y, GLdouble z)
{
  if (!glue || !glue->glRotated) { sogl_warn_core_func_unavailable("glRotated"); return; }
  glue->glRotated(angle, x, y, z);
}

void
SoGLContext_glSampleCoverage(const SoGLContext * glue, GLclampf value, GLboolean invert)
{
  if (!glue || !glue->glSampleCoverage) { sogl_warn_core_func_unavailable("glSampleCoverage"); return; }
  glue->glSampleCoverage(value, invert);
}

void
SoGLContext_glScaled(const SoGLContext * glue, GLdouble x, GLdouble y, GLdouble z)
{
  if (!glue || !glue->glScaled) { sogl_warn_core_func_unavailable("glScaled"); return; }
  glue->glScaled(x, y, z);
}

void
SoGLContext_glSelectBuffer(const SoGLContext * glue, GLsizei size, GLuint * buffer)
{
  if (!glue || !glue->glSelectBuffer) { sogl_warn_core_func_unavailable("glSelectBuffer"); return; }
  glue->glSelectBuffer(size, buffer);
}

void
SoGLContext_glSeparableFilter2D(const SoGLContext * glue, GLenum target, GLenum internalformat, GLsizei width, GLsizei height, GLenum format, GLenum type, const GLvoid * row, const GLvoid * column)
{
  if (!glue || !glue->glSeparableFilter2D) { sogl_warn_core_func_unavailable("glSeparableFilter2D"); return; }
  glue->glSeparableFilter2D(target, internalformat, width, height, format, type, row, column);
}

void
SoGLContext_glShadeModel(const SoGLContext * glue, GLenum mode)
{
  if (!glue || !glue->glShadeModel) { sogl_warn_core_func_unavailable("glShadeModel"); return; }
  glue->glShadeModel(mode);
}

void
SoGLContext_glStencilMask(const SoGLContext * glue, GLuint mask)
{
  if (!glue || !glue->glStencilMask) { sogl_warn_core_func_unavailable("glStencilMask"); return; }
  glue->glStencilMask(mask);
}

void
SoGLContext_glTexCoord1d(const SoGLContext * glue, GLdouble s)
{
  if (!glue || !glue->glTexCoord1d) { sogl_warn_core_func_unavailable("glTexCoord1d"); return; }
  glue->glTexCoord1d(s);
}

void
SoGLContext_glTexCoord1dv(const SoGLContext * glue, const GLdouble * v)
{
  if (!glue || !glue->glTexCoord1dv) { sogl_warn_core_func_unavailable("glTexCoord1dv"); return; }
  glue->glTexCoord1dv(v);
}

void
SoGLContext_glTexCoord1f(const SoGLContext * glue, GLfloat s)
{
  if (!glue || !glue->glTexCoord1f) { sogl_warn_core_func_unavailable("glTexCoord1f"); return; }
  glue->glTexCoord1f(s);
}

void
SoGLContext_glTexCoord1fv(const SoGLContext * glue, const GLfloat * v)
{
  if (!glue || !glue->glTexCoord1fv) { sogl_warn_core_func_unavailable("glTexCoord1fv"); return; }
  glue->glTexCoord1fv(v);
}

void
SoGLContext_glTexCoord1i(const SoGLContext * glue, GLint s)
{
  if (!glue || !glue->glTexCoord1i) { sogl_warn_core_func_unavailable("glTexCoord1i"); return; }
  glue->glTexCoord1i(s);
}

void
SoGLContext_glTexCoord1iv(const SoGLContext * glue, const GLint * v)
{
  if (!glue || !glue->glTexCoord1iv) { sogl_warn_core_func_unavailable("glTexCoord1iv"); return; }
  glue->glTexCoord1iv(v);
}

void
SoGLContext_glTexCoord1s(const SoGLContext * glue, GLshort s)
{
  if (!glue || !glue->glTexCoord1s) { sogl_warn_core_func_unavailable("glTexCoord1s"); return; }
  glue->glTexCoord1s(s);
}

void
SoGLContext_glTexCoord1sv(const SoGLContext * glue, const GLshort * v)
{
  if (!glue || !glue->glTexCoord1sv) { sogl_warn_core_func_unavailable("glTexCoord1sv"); return; }
  glue->glTexCoord1sv(v);
}

void
SoGLContext_glTexCoord2d(const SoGLContext * glue, GLdouble s, GLdouble t)
{
  if (!glue || !glue->glTexCoord2d) { sogl_warn_core_func_unavailable("glTexCoord2d"); return; }
  glue->glTexCoord2d(s, t);
}

void
SoGLContext_glTexCoord2dv(const SoGLContext * glue, const GLdouble * v)
{
  if (!glue || !glue->glTexCoord2dv) { sogl_warn_core_func_unavailable("glTexCoord2dv"); return; }
  glue->glTexCoord2dv(v);
}

void
SoGLContext_glTexCoord2i(const SoGLContext * glue, GLint s, GLint t)
{
  if (!glue || !glue->glTexCoord2i) { sogl_warn_core_func_unavailable("glTexCoord2i"); return; }
  glue->glTexCoord2i(s, t);
}

void
SoGLContext_glTexCoord2iv(const SoGLContext * glue, const GLint * v)
{
  if (!glue || !glue->glTexCoord2iv) { sogl_warn_core_func_unavailable("glTexCoord2iv"); return; }
  glue->glTexCoord2iv(v);
}

void
SoGLContext_glTexCoord2s(const SoGLContext * glue, GLshort s, GLshort t)
{
  if (!glue || !glue->glTexCoord2s) { sogl_warn_core_func_unavailable("glTexCoord2s"); return; }
  glue->glTexCoord2s(s, t);
}

void
SoGLContext_glTexCoord2sv(const SoGLContext * glue, const GLshort * v)
{
  if (!glue || !glue->glTexCoord2sv) { sogl_warn_core_func_unavailable("glTexCoord2sv"); return; }
  glue->glTexCoord2sv(v);
}

void
SoGLContext_glTexCoord3d(const SoGLContext * glue, GLdouble s, GLdouble t, GLdouble r)
{
  if (!glue || !glue->glTexCoord3d) { sogl_warn_core_func_unavailable("glTexCoord3d"); return; }
  glue->glTexCoord3d(s, t, r);
}

void
SoGLContext_glTexCoord3dv(const SoGLContext * glue, const GLdouble * v)
{
  if (!glue || !glue->glTexCoord3dv) { sogl_warn_core_func_unavailable("glTexCoord3dv"); return; }
  glue->glTexCoord3dv(v);
}

void
SoGLContext_glTexCoord3i(const SoGLContext * glue, GLint s, GLint t, GLint r)
{
  if (!glue || !glue->glTexCoord3i) { sogl_warn_core_func_unavailable("glTexCoord3i"); return; }
  glue->glTexCoord3i(s, t, r);
}

void
SoGLContext_glTexCoord3iv(const SoGLContext * glue, const GLint * v)
{
  if (!glue || !glue->glTexCoord3iv) { sogl_warn_core_func_unavailable("glTexCoord3iv"); return; }
  glue->glTexCoord3iv(v);
}

void
SoGLContext_glTexCoord3s(const SoGLContext * glue, GLshort s, GLshort t, GLshort r)
{
  if (!glue || !glue->glTexCoord3s) { sogl_warn_core_func_unavailable("glTexCoord3s"); return; }
  glue->glTexCoord3s(s, t, r);
}

void
SoGLContext_glTexCoord3sv(const SoGLContext * glue, const GLshort * v)
{
  if (!glue || !glue->glTexCoord3sv) { sogl_warn_core_func_unavailable("glTexCoord3sv"); return; }
  glue->glTexCoord3sv(v);
}

void
SoGLContext_glTexCoord4d(const SoGLContext * glue, GLdouble s, GLdouble t, GLdouble r, GLdouble q)
{
  if (!glue || !glue->glTexCoord4d) { sogl_warn_core_func_unavailable("glTexCoord4d"); return; }
  glue->glTexCoord4d(s, t, r, q);
}

void
SoGLContext_glTexCoord4dv(const SoGLContext * glue, const GLdouble * v)
{
  if (!glue || !glue->glTexCoord4dv) { sogl_warn_core_func_unavailable("glTexCoord4dv"); return; }
  glue->glTexCoord4dv(v);
}

void
SoGLContext_glTexCoord4f(const SoGLContext * glue, GLfloat s, GLfloat t, GLfloat r, GLfloat q)
{
  if (!glue || !glue->glTexCoord4f) { sogl_warn_core_func_unavailable("glTexCoord4f"); return; }
  glue->glTexCoord4f(s, t, r, q);
}

void
SoGLContext_glTexCoord4i(const SoGLContext * glue, GLint s, GLint t, GLint r, GLint q)
{
  if (!glue || !glue->glTexCoord4i) { sogl_warn_core_func_unavailable("glTexCoord4i"); return; }
  glue->glTexCoord4i(s, t, r, q);
}

void
SoGLContext_glTexCoord4iv(const SoGLContext * glue, const GLint * v)
{
  if (!glue || !glue->glTexCoord4iv) { sogl_warn_core_func_unavailable("glTexCoord4iv"); return; }
  glue->glTexCoord4iv(v);
}

void
SoGLContext_glTexCoord4s(const SoGLContext * glue, GLshort s, GLshort t, GLshort r, GLshort q)
{
  if (!glue || !glue->glTexCoord4s) { sogl_warn_core_func_unavailable("glTexCoord4s"); return; }
  glue->glTexCoord4s(s, t, r, q);
}

void
SoGLContext_glTexCoord4sv(const SoGLContext * glue, const GLshort * v)
{
  if (!glue || !glue->glTexCoord4sv) { sogl_warn_core_func_unavailable("glTexCoord4sv"); return; }
  glue->glTexCoord4sv(v);
}

void
SoGLContext_glTexEnviv(const SoGLContext * glue, GLenum target, GLenum pname, const GLint * params)
{
  if (!glue || !glue->glTexEnviv) { sogl_warn_core_func_unavailable("glTexEnviv"); return; }
  glue->glTexEnviv(target, pname, params);
}

void
SoGLContext_glTexGend(const SoGLContext * glue, GLenum coord, GLenum pname, GLdouble param)
{
  if (!glue || !glue->glTexGend) { sogl_warn_core_func_unavailable("glTexGend"); return; }
  glue->glTexGend(coord, pname, param);
}

void
SoGLContext_glTexGendv(const SoGLContext * glue, GLenum coord, GLenum pname, const GLdouble * params)
{
  if (!glue || !glue->glTexGendv) { sogl_warn_core_func_unavailable("glTexGendv"); return; }
  glue->glTexGendv(coord, pname, params);
}

void
SoGLContext_glTexGeniv(const SoGLContext * glue, GLenum coord, GLenum pname, const GLint * params)
{
  if (!glue || !glue->glTexGeniv) { sogl_warn_core_func_unavailable("glTexGeniv"); return; }
  glue->glTexGeniv(coord, pname, params);
}

void
SoGLContext_glTexImage1D(const SoGLContext * glue, GLenum target, GLint level, GLint internalFormat, GLsizei width, GLint border, GLenum format, GLenum type, const GLvoid * pixels)
{
  if (!glue || !glue->glTexImage1D) { sogl_warn_core_func_unavailable("glTexImage1D"); return; }
  glue->glTexImage1D(target, level, internalFormat, width, border, format, type, pixels);
}

void
SoGLContext_glTexParameterfv(const SoGLContext * glue, GLenum target, GLenum pname, const GLfloat * params)
{
  if (!glue || !glue->glTexParameterfv) { sogl_warn_core_func_unavailable("glTexParameterfv"); return; }
  glue->glTexParameterfv(target, pname, params);
}

void
SoGLContext_glTexParameteriv(const SoGLContext * glue, GLenum target, GLenum pname, const GLint * params)
{
  if (!glue || !glue->glTexParameteriv) { sogl_warn_core_func_unavailable("glTexParameteriv"); return; }
  glue->glTexParameteriv(target, pname, params);
}

void
SoGLContext_glTexSubImage1D(const SoGLContext * glue, GLenum target, GLint level, GLint xoffset, GLsizei width, GLenum format, GLenum type, const GLvoid * pixels)
{
  if (!glue || !glue->glTexSubImage1D) { sogl_warn_core_func_unavailable("glTexSubImage1D"); return; }
  glue->glTexSubImage1D(target, level, xoffset, width, format, type, pixels);
}

void
SoGLContext_glTranslated(const SoGLContext * glue, GLdouble x, GLdouble y, GLdouble z)
{
  if (!glue || !glue->glTranslated) { sogl_warn_core_func_unavailable("glTranslated"); return; }
  glue->glTranslated(x, y, z);
}

void
SoGLContext_glVertex2d(const SoGLContext * glue, GLdouble x, GLdouble y)
{
  if (!glue || !glue->glVertex2d) { sogl_warn_core_func_unavailable("glVertex2d"); return; }
  glue->glVertex2d(x, y);
}

void
SoGLContext_glVertex2dv(const SoGLContext * glue, const GLdouble * v)
{
  if (!glue || !glue->glVertex2dv) { sogl_warn_core_func_unavailable("glVertex2dv"); return; }
  glue->glVertex2dv(v);
}

void
SoGLContext_glVertex2fv(const SoGLContext * glue, const GLfloat * v)
{
  if (!glue || !glue->glVertex2fv) { sogl_warn_core_func_unavailable("glVertex2fv"); return; }
  glue->glVertex2fv(v);
}

void
SoGLContext_glVertex2i(const SoGLContext * glue, GLint x, GLint y)
{
  if (!glue || !glue->glVertex2i) { sogl_warn_core_func_unavailable("glVertex2i"); return; }
  glue->glVertex2i(x, y);
}

void
SoGLContext_glVertex2iv(const SoGLContext * glue, const GLint * v)
{
  if (!glue || !glue->glVertex2iv) { sogl_warn_core_func_unavailable("glVertex2iv"); return; }
  glue->glVertex2iv(v);
}

void
SoGLContext_glVertex2sv(const SoGLContext * glue, const GLshort * v)
{
  if (!glue || !glue->glVertex2sv) { sogl_warn_core_func_unavailable("glVertex2sv"); return; }
  glue->glVertex2sv(v);
}

void
SoGLContext_glVertex3d(const SoGLContext * glue, GLdouble x, GLdouble y, GLdouble z)
{
  if (!glue || !glue->glVertex3d) { sogl_warn_core_func_unavailable("glVertex3d"); return; }
  glue->glVertex3d(x, y, z);
}

void
SoGLContext_glVertex3dv(const SoGLContext * glue, const GLdouble * v)
{
  if (!glue || !glue->glVertex3dv) { sogl_warn_core_func_unavailable("glVertex3dv"); return; }
  glue->glVertex3dv(v);
}

void
SoGLContext_glVertex3i(const SoGLContext * glue, GLint x, GLint y, GLint z)
{
  if (!glue || !glue->glVertex3i) { sogl_warn_core_func_unavailable("glVertex3i"); return; }
  glue->glVertex3i(x, y, z);
}

void
SoGLContext_glVertex3iv(const SoGLContext * glue, const GLint * v)
{
  if (!glue || !glue->glVertex3iv) { sogl_warn_core_func_unavailable("glVertex3iv"); return; }
  glue->glVertex3iv(v);
}

void
SoGLContext_glVertex3s(const SoGLContext * glue, GLshort x, GLshort y, GLshort z)
{
  if (!glue || !glue->glVertex3s) { sogl_warn_core_func_unavailable("glVertex3s"); return; }
  glue->glVertex3s(x, y, z);
}

void
SoGLContext_glVertex3sv(const SoGLContext * glue, const GLshort * v)
{
  if (!glue || !glue->glVertex3sv) { sogl_warn_core_func_unavailable("glVertex3sv"); return; }
  glue->glVertex3sv(v);
}

void
SoGLContext_glVertex4d(const SoGLContext * glue, GLdouble x, GLdouble y, GLdouble z, GLdouble w)
{
  if (!glue || !glue->glVertex4d) { sogl_warn_core_func_unavailable("glVertex4d"); return; }
  glue->glVertex4d(x, y, z, w);
}

void
SoGLContext_glVertex4dv(const SoGLContext * glue, const GLdouble * v)
{
  if (!glue || !glue->glVertex4dv) { sogl_warn_core_func_unavailable("glVertex4dv"); return; }
  glue->glVertex4dv(v);
}

void
SoGLContext_glVertex4f(const SoGLContext * glue, GLfloat x, GLfloat y, GLfloat z, GLfloat w)
{
  if (!glue || !glue->glVertex4f) { sogl_warn_core_func_unavailable("glVertex4f"); return; }
  glue->glVertex4f(x, y, z, w);
}

void
SoGLContext_glVertex4i(const SoGLContext * glue, GLint x, GLint y, GLint z, GLint w)
{
  if (!glue || !glue->glVertex4i) { sogl_warn_core_func_unavailable("glVertex4i"); return; }
  glue->glVertex4i(x, y, z, w);
}

void
SoGLContext_glVertex4iv(const SoGLContext * glue, const GLint * v)
{
  if (!glue || !glue->glVertex4iv) { sogl_warn_core_func_unavailable("glVertex4iv"); return; }
  glue->glVertex4iv(v);
}

void
SoGLContext_glVertex4s(const SoGLContext * glue, GLshort x, GLshort y, GLshort z, GLshort w)
{
  if (!glue || !glue->glVertex4s) { sogl_warn_core_func_unavailable("glVertex4s"); return; }
  glue->glVertex4s(x, y, z, w);
}

void
SoGLContext_glVertex4sv(const SoGLContext * glue, const GLshort * v)
{
  if (!glue || !glue->glVertex4sv) { sogl_warn_core_func_unavailable("glVertex4sv"); return; }
  glue->glVertex4sv(v);
}

#endif /* !SOGL_PREFIX_SET */
