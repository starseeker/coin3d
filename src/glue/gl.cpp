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

#define GRAPHICS_API_COUNT (((defined(HAVE_WGL) ? 1 : 0) + \
                            (defined(HAVE_EGL) ? 1 : 0) + \
                            ((defined(HAVE_AGL) || defined(HAVE_CGL)) ? 1 : 0)))

#if GRAPHICS_API_COUNT == 0
// Define HAVE_NOGL if no platform GL binding exists
#define HAVE_NOGL 1
#elif GRAPHICS_API_COUNT > 1
#error More than one of HAVE_WGL, HAVE_EGL, and HAVE_AGL|HAVE_CGL set simultaneously!
#endif

#undef GRAPHICS_API_COUNT

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
 * When OBOL_BUILD_DUAL_GL is defined and this is the primary (system-GL)
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

#if defined(OBOL_BUILD_DUAL_GL)
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
#endif /* OBOL_BUILD_DUAL_GL */

/* Public C API: register an OSMesa-backed render-context ID.
   Must be called (once, at context-ID assignment time) by the application
   or CoinOffscreenGLCanvas before the first SoGLContext_instance() call
   for that ID when OBOL_BUILD_DUAL_GL is enabled.
   Safe to call even when OBOL_BUILD_DUAL_GL is not defined (no-op). */
void
coingl_register_osmesa_context(int contextid)
{
#if defined(OBOL_BUILD_DUAL_GL)
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
   Safe to call even when OBOL_BUILD_DUAL_GL is not defined (no-op). */
void
coingl_unregister_osmesa_context(int contextid)
{
#if defined(OBOL_BUILD_DUAL_GL)
  std::lock_guard<std::mutex> lock(coingl_osmesa_context_mutex);
  if (coingl_osmesa_context_ids) {
    coingl_osmesa_context_ids->erase(contextid);
  }
#else
  (void)contextid;
#endif
}

/* Query whether a context ID was registered as an OSMesa context. */
static int
coingl_context_backend_is_osmesa(int contextid)
{
#if defined(OBOL_BUILD_DUAL_GL)
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
glglue_allow_newer_opengl(const SoGLContext * w)
{
  static int force1_0_initialized = 0;
  static SbBool force1_0 = FALSE;

  if (!force1_0_initialized) {
    force1_0 = (glglue_resolve_envvar("OBOL_FORCE_GL1_0_ONLY") > 0);
    force1_0_initialized = 1;
  }

  if (force1_0) return FALSE;

#ifdef OBOL_OSMESA_BUILD
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

#if defined(OBOL_OSMESA_BUILD) || defined(SOGL_PREFIX_SET)
  /* OSMesa path: resolve via OSMesaGetProcAddress first to guarantee we
     get an OSMesa function pointer and never accidentally pick up a system
     GL symbol from the process handle (the two implementations have
     different dispatch tables and mixing them causes subtle corruption). */
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
     pointers so dual-GL builds (OBOL_BUILD_DUAL_GL) dispatch through
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
#if defined(OBOL_BUILD_DUAL_GL) && !defined(SOGL_PREFIX_SET)
  /* Dual-GL dispatch: if this context ID was registered as an OSMesa
     context, forward to the osmesa_ variant compiled in gl_osmesa.cpp. */
  if (coingl_context_backend_is_osmesa(contextid)) {
    return osmesa_SoGLContext_instance(contextid);
  }
#endif

  // Add debugging for OSMesa builds at function entry
#ifdef OBOL_OSMESA_BUILD
  if (SoGLContext_debug()) {
    cc_debugerror_postinfo("SoGLContext_instance", "ENTRY: contextid=%d", contextid);
  }
#endif

  SbBool found;
  void * ptr;
  GLint gltmp;

  SoGLContext * gi = NULL;

#ifdef OBOL_OSMESA_BUILD
  if (SoGLContext_debug()) {
    cc_debugerror_postinfo("SoGLContext_instance", "About to begin sync");
  }
#endif


  /* check environment variables */
#ifdef OBOL_OSMESA_BUILD
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

#ifdef OBOL_OSMESA_BUILD
  if (SoGLContext_debug()) {
    cc_debugerror_postinfo("SoGLContext_instance", "Checking global dict");
  }
#endif
  if (!gldict) {  /* First invocation, do initializations. */
    gldict = cc_dict_construct(16, 0.75f);
    coin_atexit((coin_atexit_f *)glglue_cleanup, CC_ATEXIT_NORMAL);
  }

#ifdef OBOL_OSMESA_BUILD
  if (SoGLContext_debug()) {
    cc_debugerror_postinfo("SoGLContext_instance", "Looking up context %d in dict", contextid);
  }
#endif
  found = cc_dict_get(gldict, (uintptr_t)contextid, &ptr);

  if (!found) {
#ifdef OBOL_OSMESA_BUILD
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
#ifdef OBOL_OSMESA_BUILD
    if (SoGLContext_debug()) {
      cc_debugerror_postinfo("SoGLContext_instance", "Context check flag: %d", chk);
    }
#endif
    if (chk) {
#ifdef OBOL_OSMESA_BUILD
      if (SoGLContext_debug()) {
        cc_debugerror_postinfo("SoGLContext_instance", "About to call coin_gl_current_context()");
      }
#endif
      const void * current_ctx = coin_gl_current_context();
#ifdef OBOL_OSMESA_BUILD
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
#ifdef OBOL_OSMESA_BUILD
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
    
    assert(gi->versionstr && "could not call glGetString() -- no current GL context?");
    assert(glGetError() == GL_NO_ERROR && "GL error when calling glGetString() -- no current GL context?");

    if (!gi->versionstr) {
      cc_debugerror_postwarning("SoGLContext_instance",
                                "glGetString(GL_VERSION) returned NULL -- no current GL context?");
      (void)cc_dict_remove(gldict, (uintptr_t)contextid);
      if (gi->glextdict) { cc_dict_destruct(gi->glextdict); }
      free(gi);
      return NULL;
    }

    glglue_set_glVersion(gi);

#ifdef OBOL_OSMESA_BUILD
    if (SoGLContext_debug()) {
      cc_debugerror_postinfo("SoGLContext_instance", "About to call glGetString(GL_VENDOR)");
    }
#endif

  /* Platform-specific initialization is no longer needed with callback-based contexts.
     Applications are responsible for providing complete OpenGL contexts through callbacks. */

    gi->vendorstr = (const char *)glGetString(GL_VENDOR);

#ifdef OBOL_OSMESA_BUILD
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

#ifdef OBOL_OSMESA_BUILD
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
    
#ifdef OBOL_OSMESA_BUILD
    if (SoGLContext_debug()) {
      cc_debugerror_postinfo("SoGLContext_instance", "GL_RENDERER call completed, rendererstr = %p", gi->rendererstr);
      if (gi->rendererstr) {
        cc_debugerror_postinfo("SoGLContext_instance", "Renderer string: %s", gi->rendererstr);
      }
    }
#endif
    
    gi->extensionsstr = (const char *)glGetString(GL_EXTENSIONS);

#ifdef OBOL_OSMESA_BUILD
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
#ifdef OBOL_OSMESA_BUILD
      if (SoGLContext_debug()) {
        cc_debugerror_postinfo("SoGLContext_instance", "Extensions string is NULL, trying glGetStringi fallback");
      }
#endif
      OBOL_PFNGLGETSTRINGIPROC glGetStringi = NULL;
      glGetStringi = (OBOL_PFNGLGETSTRINGIPROC)SoGLContext_getprocaddress(gi, "glGetStringi");
#ifdef OBOL_OSMESA_BUILD
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


#ifdef OBOL_OSMESA_BUILD
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

#ifdef OBOL_OSMESA_BUILD
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
#if defined(OBOL_BUILD_DUAL_GL) && !defined(SOGL_PREFIX_SET)
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

  assert(w->glPolygonOffset ||  w->glPolygonOffsetEXT);

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
  assert(w->glGenTextures);
  w->glGenTextures(n, textures);

}

void
SoGLContext_glBindTexture(const SoGLContext * w, GLenum target, GLuint texture)
{
  assert(w->glBindTexture);
  w->glBindTexture(target, texture);

}

void
SoGLContext_glDeleteTextures(const SoGLContext * w, GLsizei n, const GLuint * textures)
{
  assert(w->glDeleteTextures);
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
  assert(w->glTexSubImage2D);
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
  assert(w->glPushClientAttrib);
  w->glPushClientAttrib(mask);
}

void
SoGLContext_glPopClientAttrib(const SoGLContext * w)
{
  if (!glglue_allow_newer_opengl(w)) return;
  assert(w->glPopClientAttrib);
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
  assert(w->glTexImage3D);
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
  assert(w->glTexSubImage3D);
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
  assert(w->glCopyTexSubImage3D);
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
  assert(w->glActiveTexture);
  w->glActiveTexture(texture);
}

void
SoGLContext_glClientActiveTexture(const SoGLContext * w,
                                GLenum texture)
{
  if (!w->glClientActiveTexture && texture == GL_TEXTURE0)
    return;
  assert(w->glClientActiveTexture);
  w->glClientActiveTexture(texture);
}
void
SoGLContext_glMultiTexCoord2f(const SoGLContext * w,
                            GLenum target,
                            GLfloat s,
                            GLfloat t)
{
  assert(w->glMultiTexCoord2f);
  w->glMultiTexCoord2f(target, s, t);
}

void
SoGLContext_glMultiTexCoord2fv(const SoGLContext * w,
                             GLenum target,
                             const GLfloat * v)
{
  assert(w->glMultiTexCoord2fv);
  w->glMultiTexCoord2fv(target, v);
}

void
SoGLContext_glMultiTexCoord3fv(const SoGLContext * w,
                             GLenum target,
                             const GLfloat * v)
{
  assert(w->glMultiTexCoord3fv);
  w->glMultiTexCoord3fv(target, v);
}

void
SoGLContext_glMultiTexCoord4fv(const SoGLContext * w,
                             GLenum target,
                             const GLfloat * v)
{
  assert(w->glMultiTexCoord4fv);
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
  assert(glue->glCompressedTexImage3D);
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
  assert(glue->glCompressedTexImage2D);
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
  assert(glue->glCompressedTexImage1D);
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
  assert(glue->glCompressedTexSubImage3D);
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
  assert(glue->glCompressedTexSubImage2D);
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
  assert(glue->glCompressedTexSubImage1D);
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
  assert(glue->glGetCompressedTexImage);
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
  assert(glue->glColorTable);
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
  assert(glue->glColorSubTable);
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
  assert(glue->glGetColorTable);
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
  assert(glue->glGetColorTableParameteriv);
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
  assert(glue->glGetColorTableParameterfv);
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
  assert(glue->glBlendEquation || glue->glBlendEquationEXT);

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
  assert(glue->glBlendFuncSeparate);
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
  assert(glue->glVertexPointer);
  glue->glVertexPointer(size, type, stride, pointer);
}

void
SoGLContext_glTexCoordPointer(const SoGLContext * glue,
                            GLint size, GLenum type,
                            GLsizei stride, const GLvoid * pointer)
{
  assert(glue->glTexCoordPointer);
  glue->glTexCoordPointer(size, type, stride, pointer);
}

void
SoGLContext_glNormalPointer(const SoGLContext * glue,
                          GLenum type, GLsizei stride, const GLvoid *pointer)
{
  assert(glue->glNormalPointer);
  glue->glNormalPointer(type, stride, pointer);
}

void
SoGLContext_glColorPointer(const SoGLContext * glue,
                         GLint size, GLenum type,
                         GLsizei stride, const GLvoid * pointer)
{
  assert(glue->glColorPointer);
  glue->glColorPointer(size, type, stride, pointer);
}

void
SoGLContext_glIndexPointer(const SoGLContext * glue,
                         GLenum type, GLsizei stride, const GLvoid * pointer)
{
  assert(glue->glIndexPointer);
  glue->glIndexPointer(type, stride, pointer);
}

void
SoGLContext_glEnableClientState(const SoGLContext * glue, GLenum array)
{
  assert(glue->glEnableClientState);
  glue->glEnableClientState(array);
}

void
SoGLContext_glDisableClientState(const SoGLContext * glue, GLenum array)
{
  assert(glue->glDisableClientState);
  glue->glDisableClientState(array);
}

void
SoGLContext_glInterleavedArrays(const SoGLContext * glue,
                              GLenum format, GLsizei stride, const GLvoid * pointer)
{
  assert(glue->glInterleavedArrays);
  glue->glInterleavedArrays(format, stride, pointer);
}

void
SoGLContext_glDrawArrays(const SoGLContext * glue,
                       GLenum mode, GLint first, GLsizei count)
{
  assert(glue->glDrawArrays);
  glue->glDrawArrays(mode, first, count);
}

void
SoGLContext_glDrawElements(const SoGLContext * glue,
                         GLenum mode, GLsizei count, GLenum type,
                         const GLvoid * indices)
{
  assert(glue->glDrawElements);
  glue->glDrawElements(mode, count, type, indices);
}

void
SoGLContext_glDrawRangeElements(const SoGLContext * glue,
                              GLenum mode, GLuint start, GLuint end, GLsizei count, GLenum type,
                              const GLvoid * indices)
{
  assert(glue->glDrawRangeElements);
  glue->glDrawRangeElements(mode, start, end, count, type, indices);
}

void
SoGLContext_glArrayElement(const SoGLContext * glue, GLint i)
{
  assert(glue->glArrayElement);
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
  assert(glue->glMultiDrawArrays);
  glue->glMultiDrawArrays(mode, first, count, primcount);
}

void
SoGLContext_glMultiDrawElements(const SoGLContext * glue, GLenum mode, const GLsizei * count,
                              GLenum type, const GLvoid ** indices, GLsizei primcount)
{
  assert(glue->glMultiDrawElements);
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
  assert(glue->glFlushVertexArrayRangeNV);
  glue->glFlushVertexArrayRangeNV();
}

void
SoGLContext_glVertexArrayRangeNV(const SoGLContext * glue, GLsizei size, const GLvoid * pointer)
{
  assert(glue->glVertexArrayRangeNV);
  glue->glVertexArrayRangeNV(size, pointer);
}

void *
SoGLContext_glAllocateMemoryNV(const SoGLContext * glue,
                             GLsizei size, GLfloat readfreq,
                             GLfloat writefreq, GLfloat priority)
{
  assert(glue->glAllocateMemoryNV);
  return glue->glAllocateMemoryNV(size, readfreq, writefreq, priority);
}

void
SoGLContext_glFreeMemoryNV(const SoGLContext * glue, GLvoid * buffer)
{
  assert(glue->glFreeMemoryNV);
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
  assert(glue->glBindBuffer);
  glue->glBindBuffer(target, buffer);
}

void
SoGLContext_glDeleteBuffers(const SoGLContext * glue, GLsizei n, const GLuint *buffers)
{
  assert(glue->glDeleteBuffers);
  glue->glDeleteBuffers(n, buffers);
}

void
SoGLContext_glGenBuffers(const SoGLContext * glue, GLsizei n, GLuint *buffers)
{
  assert(glue->glGenBuffers);
  glue->glGenBuffers(n, buffers);
}

GLboolean
SoGLContext_glIsBuffer(const SoGLContext * glue, GLuint buffer)
{
  assert(glue->glIsBuffer);
  return glue->glIsBuffer(buffer);
}

void
SoGLContext_glBufferData(const SoGLContext * glue,
                       GLenum target,
                       intptr_t size, /* 64 bit on 64 bit systems */
                       const GLvoid *data,
                       GLenum usage)
{
  assert(glue->glBufferData);
  glue->glBufferData(target, size, data, usage);
}

void
SoGLContext_glBufferSubData(const SoGLContext * glue,
                          GLenum target,
                          intptr_t offset, /* 64 bit */
                          intptr_t size, /* 64 bit */
                          const GLvoid * data)
{
  assert(glue->glBufferSubData);
  glue->glBufferSubData(target, offset, size, data);
}

void
SoGLContext_glGetBufferSubData(const SoGLContext * glue,
                             GLenum target,
                             intptr_t offset, /* 64 bit */
                             intptr_t size, /* 64 bit */
                             GLvoid *data)
{
  assert(glue->glGetBufferSubData);
  glue->glGetBufferSubData(target, offset, size, data);
}

GLvoid *
SoGLContext_glMapBuffer(const SoGLContext * glue,
                      GLenum target, GLenum access)
{
  assert(glue->glMapBuffer);
  return glue->glMapBuffer(target, access);
}

GLboolean
SoGLContext_glUnmapBuffer(const SoGLContext * glue,
                        GLenum target)
{
  assert(glue->glUnmapBuffer);
  return glue->glUnmapBuffer(target);
}

void
SoGLContext_glGetBufferParameteriv(const SoGLContext * glue,
                                 GLenum target,
                                 GLenum pname,
                                 GLint * params)
{
  assert(glue->glGetBufferParameteriv);
  glue->glGetBufferParameteriv(target, pname, params);
}

void
SoGLContext_glGetBufferPointerv(const SoGLContext * glue,
                              GLenum target,
                              GLenum pname,
                              GLvoid ** params)
{
  assert(glue->glGetBufferPointerv);
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

/* Thread-local pointer to the SoGLContext for the render pass that is
   currently in progress on this thread.  Set by SoGLRenderAction before
   each traversal pass so that GL element updategl() methods (which do not
   receive a state pointer) can still dispatch through the correct GL
   backend in dual-GL builds. */
static thread_local const SoGLContext * sogl_tls_render_glue = NULL;

const SoGLContext *
sogl_current_render_glue(void)
{
  return sogl_tls_render_glue;
}

void
sogl_set_current_render_glue(const SoGLContext * glue)
{
  sogl_tls_render_glue = glue;
}

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
  assert(glue->has_fbo);
  glue->glIsRenderbuffer(renderbuffer);
}

void
SoGLContext_glBindRenderbuffer(const SoGLContext * glue, GLenum target, GLuint renderbuffer)
{
  assert(glue->has_fbo);
  glue->glBindRenderbuffer(target, renderbuffer);
}

void
SoGLContext_glDeleteRenderbuffers(const SoGLContext * glue, GLsizei n, const GLuint *renderbuffers)
{
  assert(glue->has_fbo);
  glue->glDeleteRenderbuffers(n, renderbuffers);
}

void
SoGLContext_glGenRenderbuffers(const SoGLContext * glue, GLsizei n, GLuint *renderbuffers)
{
  assert(glue->has_fbo);
  glue->glGenRenderbuffers(n, renderbuffers);
}

void
SoGLContext_glRenderbufferStorage(const SoGLContext * glue, GLenum target, GLenum internalformat, GLsizei width, GLsizei height)
{
  assert(glue->has_fbo);
  glue->glRenderbufferStorage(target, internalformat, width, height);
}

void
SoGLContext_glGetRenderbufferParameteriv(const SoGLContext * glue, GLenum target, GLenum pname, GLint * params)
{
  assert(glue->has_fbo);
  glue->glGetRenderbufferParameteriv(target, pname, params);
}

GLboolean
SoGLContext_glIsFramebuffer(const SoGLContext * glue, GLuint framebuffer)
{
  assert(glue->has_fbo);
  return glue->glIsFramebuffer(framebuffer);
}

void
SoGLContext_glBindFramebuffer(const SoGLContext * glue, GLenum target, GLuint framebuffer)
{
  assert(glue->has_fbo);
  glue->glBindFramebuffer(target, framebuffer);
}

void
SoGLContext_glDeleteFramebuffers(const SoGLContext * glue, GLsizei n, const GLuint * framebuffers)
{
  assert(glue->has_fbo);
  glue->glDeleteFramebuffers(n, framebuffers);
}

void
SoGLContext_glGenFramebuffers(const SoGLContext * glue, GLsizei n, GLuint * framebuffers)
{
  assert(glue->has_fbo);
  glue->glGenFramebuffers(n, framebuffers);
}

GLenum
SoGLContext_glCheckFramebufferStatus(const SoGLContext * glue, GLenum target)
{
  assert(glue->has_fbo);
  return glue->glCheckFramebufferStatus(target);
}

void
SoGLContext_glFramebufferTexture1D(const SoGLContext * glue, GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
  assert(glue->has_fbo);
  glue->glFramebufferTexture1D(target, attachment, textarget, texture, level);
}

void
SoGLContext_glFramebufferTexture2D(const SoGLContext * glue, GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level)
{
  assert(glue->has_fbo);
  glue->glFramebufferTexture2D(target, attachment, textarget, texture, level);
}

void
SoGLContext_glFramebufferTexture3D(const SoGLContext * glue, GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level, GLint zoffset)
{
  assert(glue->has_fbo);
  glue->glFramebufferTexture3D(target, attachment, textarget, texture, level,zoffset);
}

void
SoGLContext_glFramebufferRenderbuffer(const SoGLContext * glue, GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer)
{
  assert(glue->has_fbo);
  glue->glFramebufferRenderbuffer(target, attachment, renderbuffertarget, renderbuffer);
}

void
SoGLContext_glGetFramebufferAttachmentParameteriv(const SoGLContext * glue, GLenum target, GLenum attachment, GLenum pname, GLint * params)
{
  assert(glue->has_fbo);
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
 * process (OBOL_BUILD_DUAL_GL).  The function pointers were filled in
 * by glglue_resolve_symbols() from the correct backend. */

void
SoGLContext_glTexImage2D(const SoGLContext * glue,
                         GLenum target, GLint level, GLint internalformat,
                         GLsizei width, GLsizei height, GLint border,
                         GLenum format, GLenum type, const GLvoid * pixels)
{
  assert(glue->glTexImage2D);
  glue->glTexImage2D(target, level, internalformat, width, height,
                     border, format, type, pixels);
}

void
SoGLContext_glTexParameteri(const SoGLContext * glue,
                            GLenum target, GLenum pname, GLint param)
{
  assert(glue->glTexParameteri);
  glue->glTexParameteri(target, pname, param);
}

void
SoGLContext_glTexParameterf(const SoGLContext * glue,
                            GLenum target, GLenum pname, GLfloat param)
{
  assert(glue->glTexParameterf);
  glue->glTexParameterf(target, pname, param);
}

void
SoGLContext_glGetIntegerv(const SoGLContext * glue,
                          GLenum pname, GLint * params)
{
  assert(glue->glGetIntegerv);
  glue->glGetIntegerv(pname, params);
}

void
SoGLContext_glGetFloatv(const SoGLContext * glue,
                        GLenum pname, GLfloat * params)
{
  assert(glue->glGetFloatv);
  glue->glGetFloatv(pname, params);
}

void
SoGLContext_glClearColor(const SoGLContext * glue,
                         GLclampf red, GLclampf green,
                         GLclampf blue, GLclampf alpha)
{
  assert(glue->glClearColor);
  glue->glClearColor(red, green, blue, alpha);
}

void
SoGLContext_glClear(const SoGLContext * glue, GLbitfield mask)
{
  assert(glue->glClear);
  glue->glClear(mask);
}

void
SoGLContext_glFlush(const SoGLContext * glue)
{
  assert(glue->glFlush);
  glue->glFlush();
}

void
SoGLContext_glFinish(const SoGLContext * glue)
{
  assert(glue->glFinish);
  glue->glFinish();
}

GLenum
SoGLContext_glGetError(const SoGLContext * glue)
{
  assert(glue->glGetError);
  return glue->glGetError();
}

const GLubyte *
SoGLContext_glGetString(const SoGLContext * glue, GLenum name)
{
  assert(glue->glGetString);
  return glue->glGetString(name);
}

void
SoGLContext_glEnable(const SoGLContext * glue, GLenum cap)
{
  assert(glue->glEnable);
  glue->glEnable(cap);
}

void
SoGLContext_glDisable(const SoGLContext * glue, GLenum cap)
{
  assert(glue->glDisable);
  glue->glDisable(cap);
}

GLboolean
SoGLContext_glIsEnabled(const SoGLContext * glue, GLenum cap)
{
  assert(glue->glIsEnabled);
  return glue->glIsEnabled(cap);
}

void
SoGLContext_glPixelStorei(const SoGLContext * glue,
                          GLenum pname, GLint param)
{
  assert(glue->glPixelStorei);
  glue->glPixelStorei(pname, param);
}

void
SoGLContext_glReadPixels(const SoGLContext * glue,
                         GLint x, GLint y, GLsizei width, GLsizei height,
                         GLenum format, GLenum type, GLvoid * pixels)
{
  assert(glue->glReadPixels);
  glue->glReadPixels(x, y, width, height, format, type, pixels);
}

void
SoGLContext_glCopyTexSubImage2D(const SoGLContext * glue,
                                GLenum target, GLint level,
                                GLint xoffset, GLint yoffset,
                                GLint x, GLint y,
                                GLsizei width, GLsizei height)
{
  assert(glue->glCopyTexSubImage2D);
  glue->glCopyTexSubImage2D(target, level, xoffset, yoffset,
                            x, y, width, height);
}

/* -----------------------------------------------------------------------
 * Additional core GL 1.0/1.1 dispatch wrappers.
 *
 * These functions dispatch through the SoGLContext function-pointer table
 * so that dual-GL builds (OBOL_BUILD_DUAL_GL) correctly call either system
 * OpenGL or OSMesa based on which backend owns the current context, without
 * ever mixing backends within the same render pass.
 * --------------------------------------------------------------------- */

void
SoGLContext_glBegin(const SoGLContext * glue, GLenum mode)
{
  assert(glue->glBegin);
  glue->glBegin(mode);
}

void
SoGLContext_glEnd(const SoGLContext * glue)
{
  assert(glue->glEnd);
  glue->glEnd();
}

void
SoGLContext_glVertex2f(const SoGLContext * glue, GLfloat x, GLfloat y)
{
  assert(glue->glVertex2f);
  glue->glVertex2f(x, y);
}

void
SoGLContext_glVertex2s(const SoGLContext * glue, GLshort x, GLshort y)
{
  assert(glue->glVertex2s);
  glue->glVertex2s(x, y);
}

void
SoGLContext_glVertex3f(const SoGLContext * glue, GLfloat x, GLfloat y, GLfloat z)
{
  assert(glue->glVertex3f);
  glue->glVertex3f(x, y, z);
}

void
SoGLContext_glVertex3fv(const SoGLContext * glue, const GLfloat * v)
{
  assert(glue->glVertex3fv);
  glue->glVertex3fv(v);
}

void
SoGLContext_glVertex4fv(const SoGLContext * glue, const GLfloat * v)
{
  assert(glue->glVertex4fv);
  glue->glVertex4fv(v);
}

void
SoGLContext_glNormal3f(const SoGLContext * glue, GLfloat nx, GLfloat ny, GLfloat nz)
{
  assert(glue->glNormal3f);
  glue->glNormal3f(nx, ny, nz);
}

void
SoGLContext_glNormal3fv(const SoGLContext * glue, const GLfloat * v)
{
  assert(glue->glNormal3fv);
  glue->glNormal3fv(v);
}

void
SoGLContext_glColor3f(const SoGLContext * glue, GLfloat r, GLfloat g, GLfloat b)
{
  assert(glue->glColor3f);
  glue->glColor3f(r, g, b);
}

void
SoGLContext_glColor3fv(const SoGLContext * glue, const GLfloat * v)
{
  assert(glue->glColor3fv);
  glue->glColor3fv(v);
}

void
SoGLContext_glColor3ub(const SoGLContext * glue, GLubyte r, GLubyte g, GLubyte b)
{
  assert(glue->glColor3ub);
  glue->glColor3ub(r, g, b);
}

void
SoGLContext_glColor3ubv(const SoGLContext * glue, const GLubyte * v)
{
  assert(glue->glColor3ubv);
  glue->glColor3ubv(v);
}

void
SoGLContext_glColor4ub(const SoGLContext * glue, GLubyte r, GLubyte g, GLubyte b, GLubyte a)
{
  assert(glue->glColor4ub);
  glue->glColor4ub(r, g, b, a);
}

void
SoGLContext_glTexCoord2f(const SoGLContext * glue, GLfloat s, GLfloat t)
{
  assert(glue->glTexCoord2f);
  glue->glTexCoord2f(s, t);
}

void
SoGLContext_glTexCoord2fv(const SoGLContext * glue, const GLfloat * v)
{
  assert(glue->glTexCoord2fv);
  glue->glTexCoord2fv(v);
}

void
SoGLContext_glTexCoord3f(const SoGLContext * glue, GLfloat s, GLfloat t, GLfloat r)
{
  assert(glue->glTexCoord3f);
  glue->glTexCoord3f(s, t, r);
}

void
SoGLContext_glTexCoord3fv(const SoGLContext * glue, const GLfloat * v)
{
  assert(glue->glTexCoord3fv);
  glue->glTexCoord3fv(v);
}

void
SoGLContext_glTexCoord4fv(const SoGLContext * glue, const GLfloat * v)
{
  assert(glue->glTexCoord4fv);
  glue->glTexCoord4fv(v);
}

void
SoGLContext_glIndexi(const SoGLContext * glue, GLint c)
{
  assert(glue->glIndexi);
  glue->glIndexi(c);
}

void
SoGLContext_glMatrixMode(const SoGLContext * glue, GLenum mode)
{
  assert(glue->glMatrixMode);
  glue->glMatrixMode(mode);
}

void
SoGLContext_glLoadIdentity(const SoGLContext * glue)
{
  assert(glue->glLoadIdentity);
  glue->glLoadIdentity();
}

void
SoGLContext_glLoadMatrixf(const SoGLContext * glue, const GLfloat * m)
{
  assert(glue->glLoadMatrixf);
  glue->glLoadMatrixf(m);
}

void
SoGLContext_glLoadMatrixd(const SoGLContext * glue, const GLdouble * m)
{
  assert(glue->glLoadMatrixd);
  glue->glLoadMatrixd(m);
}

void
SoGLContext_glMultMatrixf(const SoGLContext * glue, const GLfloat * m)
{
  assert(glue->glMultMatrixf);
  glue->glMultMatrixf(m);
}

void
SoGLContext_glPushMatrix(const SoGLContext * glue)
{
  assert(glue->glPushMatrix);
  glue->glPushMatrix();
}

void
SoGLContext_glPopMatrix(const SoGLContext * glue)
{
  assert(glue->glPopMatrix);
  glue->glPopMatrix();
}

void
SoGLContext_glOrtho(const SoGLContext * glue,
                    GLdouble left, GLdouble right,
                    GLdouble bottom, GLdouble top,
                    GLdouble near_val, GLdouble far_val)
{
  assert(glue->glOrtho);
  glue->glOrtho(left, right, bottom, top, near_val, far_val);
}

void
SoGLContext_glFrustum(const SoGLContext * glue,
                      GLdouble left, GLdouble right,
                      GLdouble bottom, GLdouble top,
                      GLdouble near_val, GLdouble far_val)
{
  assert(glue->glFrustum);
  glue->glFrustum(left, right, bottom, top, near_val, far_val);
}

void
SoGLContext_glTranslatef(const SoGLContext * glue, GLfloat x, GLfloat y, GLfloat z)
{
  assert(glue->glTranslatef);
  glue->glTranslatef(x, y, z);
}

void
SoGLContext_glRotatef(const SoGLContext * glue, GLfloat angle, GLfloat x, GLfloat y, GLfloat z)
{
  assert(glue->glRotatef);
  glue->glRotatef(angle, x, y, z);
}

void
SoGLContext_glScalef(const SoGLContext * glue, GLfloat x, GLfloat y, GLfloat z)
{
  assert(glue->glScalef);
  glue->glScalef(x, y, z);
}

void
SoGLContext_glLightf(const SoGLContext * glue, GLenum light, GLenum pname, GLfloat param)
{
  assert(glue->glLightf);
  glue->glLightf(light, pname, param);
}

void
SoGLContext_glLightfv(const SoGLContext * glue, GLenum light, GLenum pname, const GLfloat * params)
{
  assert(glue->glLightfv);
  glue->glLightfv(light, pname, params);
}

void
SoGLContext_glLightModeli(const SoGLContext * glue, GLenum pname, GLint param)
{
  assert(glue->glLightModeli);
  glue->glLightModeli(pname, param);
}

void
SoGLContext_glLightModelfv(const SoGLContext * glue, GLenum pname, const GLfloat * params)
{
  assert(glue->glLightModelfv);
  glue->glLightModelfv(pname, params);
}

void
SoGLContext_glMaterialf(const SoGLContext * glue, GLenum face, GLenum pname, GLfloat param)
{
  assert(glue->glMaterialf);
  glue->glMaterialf(face, pname, param);
}

void
SoGLContext_glMaterialfv(const SoGLContext * glue, GLenum face, GLenum pname, const GLfloat * params)
{
  assert(glue->glMaterialfv);
  glue->glMaterialfv(face, pname, params);
}

void
SoGLContext_glColorMaterial(const SoGLContext * glue, GLenum face, GLenum mode)
{
  assert(glue->glColorMaterial);
  glue->glColorMaterial(face, mode);
}

void
SoGLContext_glFogi(const SoGLContext * glue, GLenum pname, GLint param)
{
  assert(glue->glFogi);
  glue->glFogi(pname, param);
}

void
SoGLContext_glFogf(const SoGLContext * glue, GLenum pname, GLfloat param)
{
  assert(glue->glFogf);
  glue->glFogf(pname, param);
}

void
SoGLContext_glFogfv(const SoGLContext * glue, GLenum pname, const GLfloat * params)
{
  assert(glue->glFogfv);
  glue->glFogfv(pname, params);
}

void
SoGLContext_glTexEnvi(const SoGLContext * glue, GLenum target, GLenum pname, GLint param)
{
  assert(glue->glTexEnvi);
  glue->glTexEnvi(target, pname, param);
}

void
SoGLContext_glTexEnvf(const SoGLContext * glue, GLenum target, GLenum pname, GLfloat param)
{
  assert(glue->glTexEnvf);
  glue->glTexEnvf(target, pname, param);
}

void
SoGLContext_glTexEnvfv(const SoGLContext * glue, GLenum target, GLenum pname, const GLfloat * params)
{
  assert(glue->glTexEnvfv);
  glue->glTexEnvfv(target, pname, params);
}

void
SoGLContext_glTexGeni(const SoGLContext * glue, GLenum coord, GLenum pname, GLint param)
{
  assert(glue->glTexGeni);
  glue->glTexGeni(coord, pname, param);
}

void
SoGLContext_glTexGenf(const SoGLContext * glue, GLenum coord, GLenum pname, GLfloat param)
{
  assert(glue->glTexGenf);
  glue->glTexGenf(coord, pname, param);
}

void
SoGLContext_glTexGenfv(const SoGLContext * glue, GLenum coord, GLenum pname, const GLfloat * params)
{
  assert(glue->glTexGenfv);
  glue->glTexGenfv(coord, pname, params);
}

void
SoGLContext_glCopyTexImage2D(const SoGLContext * glue,
                             GLenum target, GLint level,
                             GLenum internalformat,
                             GLint x, GLint y,
                             GLsizei width, GLsizei height, GLint border)
{
  assert(glue->glCopyTexImage2D);
  glue->glCopyTexImage2D(target, level, internalformat, x, y, width, height, border);
}

void
SoGLContext_glRasterPos2f(const SoGLContext * glue, GLfloat x, GLfloat y)
{
  assert(glue->glRasterPos2f);
  glue->glRasterPos2f(x, y);
}

void
SoGLContext_glRasterPos3f(const SoGLContext * glue, GLfloat x, GLfloat y, GLfloat z)
{
  assert(glue->glRasterPos3f);
  glue->glRasterPos3f(x, y, z);
}

void
SoGLContext_glBitmap(const SoGLContext * glue,
                     GLsizei width, GLsizei height,
                     GLfloat xorig, GLfloat yorig,
                     GLfloat xmove, GLfloat ymove,
                     const GLubyte * bitmap)
{
  assert(glue->glBitmap);
  glue->glBitmap(width, height, xorig, yorig, xmove, ymove, bitmap);
}

void
SoGLContext_glDrawPixels(const SoGLContext * glue,
                         GLsizei width, GLsizei height,
                         GLenum format, GLenum type, const GLvoid * pixels)
{
  assert(glue->glDrawPixels);
  glue->glDrawPixels(width, height, format, type, pixels);
}

void
SoGLContext_glPixelTransferf(const SoGLContext * glue, GLenum pname, GLfloat param)
{
  assert(glue->glPixelTransferf);
  glue->glPixelTransferf(pname, param);
}

void
SoGLContext_glPixelTransferi(const SoGLContext * glue, GLenum pname, GLint param)
{
  assert(glue->glPixelTransferi);
  glue->glPixelTransferi(pname, param);
}

void
SoGLContext_glPixelMapfv(const SoGLContext * glue,
                         GLenum map, GLint mapsize, const GLfloat * values)
{
  assert(glue->glPixelMapfv);
  glue->glPixelMapfv(map, mapsize, values);
}

void
SoGLContext_glPixelMapuiv(const SoGLContext * glue,
                          GLenum map, GLint mapsize, const GLuint * values)
{
  assert(glue->glPixelMapuiv);
  glue->glPixelMapuiv(map, mapsize, values);
}

void
SoGLContext_glPixelZoom(const SoGLContext * glue, GLfloat xfactor, GLfloat yfactor)
{
  assert(glue->glPixelZoom);
  glue->glPixelZoom(xfactor, yfactor);
}

void
SoGLContext_glViewport(const SoGLContext * glue,
                       GLint x, GLint y, GLsizei width, GLsizei height)
{
  assert(glue->glViewport);
  glue->glViewport(x, y, width, height);
}

void
SoGLContext_glScissor(const SoGLContext * glue,
                      GLint x, GLint y, GLsizei width, GLsizei height)
{
  assert(glue->glScissor);
  glue->glScissor(x, y, width, height);
}

void
SoGLContext_glDepthMask(const SoGLContext * glue, GLboolean flag)
{
  assert(glue->glDepthMask);
  glue->glDepthMask(flag);
}

void
SoGLContext_glDepthFunc(const SoGLContext * glue, GLenum func)
{
  assert(glue->glDepthFunc);
  glue->glDepthFunc(func);
}

void
SoGLContext_glDepthRange(const SoGLContext * glue, GLclampd near_val, GLclampd far_val)
{
  assert(glue->glDepthRange);
  glue->glDepthRange(near_val, far_val);
}

void
SoGLContext_glStencilFunc(const SoGLContext * glue, GLenum func, GLint ref, GLuint mask)
{
  assert(glue->glStencilFunc);
  glue->glStencilFunc(func, ref, mask);
}

void
SoGLContext_glStencilOp(const SoGLContext * glue, GLenum fail, GLenum zfail, GLenum zpass)
{
  assert(glue->glStencilOp);
  glue->glStencilOp(fail, zfail, zpass);
}

void
SoGLContext_glBlendFunc(const SoGLContext * glue, GLenum sfactor, GLenum dfactor)
{
  assert(glue->glBlendFunc);
  glue->glBlendFunc(sfactor, dfactor);
}

void
SoGLContext_glAlphaFunc(const SoGLContext * glue, GLenum func, GLclampf ref)
{
  assert(glue->glAlphaFunc);
  glue->glAlphaFunc(func, ref);
}

void
SoGLContext_glFrontFace(const SoGLContext * glue, GLenum mode)
{
  assert(glue->glFrontFace);
  glue->glFrontFace(mode);
}

void
SoGLContext_glCullFace(const SoGLContext * glue, GLenum mode)
{
  assert(glue->glCullFace);
  glue->glCullFace(mode);
}

void
SoGLContext_glPolygonMode(const SoGLContext * glue, GLenum face, GLenum mode)
{
  assert(glue->glPolygonMode);
  glue->glPolygonMode(face, mode);
}

void
SoGLContext_glPolygonStipple(const SoGLContext * glue, const GLubyte * mask)
{
  assert(glue->glPolygonStipple);
  glue->glPolygonStipple(mask);
}

void
SoGLContext_glLineWidth(const SoGLContext * glue, GLfloat width)
{
  assert(glue->glLineWidth);
  glue->glLineWidth(width);
}

void
SoGLContext_glLineStipple(const SoGLContext * glue, GLint factor, GLushort pattern)
{
  assert(glue->glLineStipple);
  glue->glLineStipple(factor, pattern);
}

void
SoGLContext_glPointSize(const SoGLContext * glue, GLfloat size)
{
  assert(glue->glPointSize);
  glue->glPointSize(size);
}

void
SoGLContext_glColorMask(const SoGLContext * glue,
                        GLboolean red, GLboolean green,
                        GLboolean blue, GLboolean alpha)
{
  assert(glue->glColorMask);
  glue->glColorMask(red, green, blue, alpha);
}

void
SoGLContext_glClipPlane(const SoGLContext * glue,
                        GLenum plane, const GLdouble * equation)
{
  assert(glue->glClipPlane);
  glue->glClipPlane(plane, equation);
}

void
SoGLContext_glDrawBuffer(const SoGLContext * glue, GLenum mode)
{
  assert(glue->glDrawBuffer);
  glue->glDrawBuffer(mode);
}

void
SoGLContext_glClearIndex(const SoGLContext * glue, GLfloat c)
{
  assert(glue->glClearIndex);
  glue->glClearIndex(c);
}

void
SoGLContext_glClearStencil(const SoGLContext * glue, GLint s)
{
  assert(glue->glClearStencil);
  glue->glClearStencil(s);
}

void
SoGLContext_glAccum(const SoGLContext * glue, GLenum op, GLfloat value)
{
  assert(glue->glAccum);
  glue->glAccum(op, value);
}

void
SoGLContext_glGetBooleanv(const SoGLContext * glue, GLenum pname, GLboolean * params)
{
  assert(glue->glGetBooleanv);
  glue->glGetBooleanv(pname, params);
}

void
SoGLContext_glNewList(const SoGLContext * glue, GLuint list, GLenum mode)
{
  assert(glue->glNewList);
  glue->glNewList(list, mode);
}

void
SoGLContext_glEndList(const SoGLContext * glue)
{
  assert(glue->glEndList);
  glue->glEndList();
}

void
SoGLContext_glCallList(const SoGLContext * glue, GLuint list)
{
  assert(glue->glCallList);
  glue->glCallList(list);
}

void
SoGLContext_glDeleteLists(const SoGLContext * glue, GLuint list, GLsizei range)
{
  assert(glue->glDeleteLists);
  glue->glDeleteLists(list, range);
}

GLuint
SoGLContext_glGenLists(const SoGLContext * glue, GLsizei range)
{
  assert(glue->glGenLists);
  return glue->glGenLists(range);
}

void
SoGLContext_glPushAttrib(const SoGLContext * glue, GLbitfield mask)
{
  assert(glue->glPushAttrib);
  glue->glPushAttrib(mask);
}

void
SoGLContext_glPopAttrib(const SoGLContext * glue)
{
  assert(glue->glPopAttrib);
  glue->glPopAttrib();
}

/* ********************************************************************** */

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
