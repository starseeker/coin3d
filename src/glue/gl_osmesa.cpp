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

#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cassert>

#include "Inventor/C/basic.h"
#include "C/glue/gl.h"
#include "C/errors/debugerror.h"
#include "misc/SoEnvironment.h"

#include "gl_osmesa.h"
#include "glp.h"

/* ********************************************************************** */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

/* Only compile OSMesa glue when OSMesa is available */
#ifdef HAVE_OSMESA

/* Include OSMesa headers based on build configuration */
#ifdef COIN3D_OSMESA_BUILD
/* starseeker/osmesa build - use MGL namespaced headers */
#include <OSMesa/osmesa.h>
#include <OSMesa/gl.h>
#else
/* Standard OSMesa build */
#include <GL/osmesa.h>
#include <GL/gl.h>
#endif

/* ********************************************************************** */

/* OSMesa context data structure */
struct osmesaglue_contextdata {
  OSMesaContext context;
  void * buffer;
  unsigned int width;
  unsigned int height;
  OSMesaContext previous_context;
  void * previous_buffer;
  GLint previous_width;
  GLint previous_height;
};

/* ********************************************************************** */

static SbBool osmesa_debug(void)
{
  static int osmesa_debug_value = -1;
  if (osmesa_debug_value == -1) {
    auto env = CoinInternal::getEnvironmentVariable("COIN_DEBUG_OSMESA");
    osmesa_debug_value = env.has_value() ? std::atoi(env->c_str()) : 0;
  }
  return osmesa_debug_value > 0;
}

/* Check if OSMesa is available */
SbBool
osmesaglue_available(void)
{
  /* OSMesa should always be available when compiled with HAVE_OSMESA */
  return TRUE;
}

/* Initialize OSMesa glue */
void
osmesaglue_init(cc_glglue * w)
{
  /* OSMesa provides software rendering - set appropriate flags */
  w->vendor_is_SGI = FALSE;
  w->vendor_is_nvidia = FALSE;
  w->vendor_is_intel = FALSE;
  w->vendor_is_ati = FALSE;
  w->vendor_is_3dlabs = FALSE;
  
  if (osmesa_debug()) {
    cc_debugerror_postinfo("osmesaglue_init", "OSMesa glue initialized");
  }
}

/* Get procedure address - OSMesa uses standard GL function names */
void *
osmesaglue_getprocaddress(const cc_glglue * glue, const char * fname)
{
  /* OSMesa uses standard GL function names, no special handling needed */
  return NULL; /* Let the standard GL loader handle this */
}

/* Check extension support */
int
osmesaglue_ext_supported(const cc_glglue * w, const char * extension)
{
  /* Query OpenGL extensions in standard way */
  const char * extensions = (const char *)glGetString(GL_EXTENSIONS);
  if (!extensions) return 0;
  
  return strstr(extensions, extension) != NULL;
}

/* Create OSMesa offscreen context */
void *
osmesaglue_context_create_offscreen(unsigned int width, unsigned int height)
{
  struct osmesaglue_contextdata * context;
  
  if (osmesa_debug()) {
    cc_debugerror_postinfo("osmesaglue_context_create_offscreen",
                          "Creating OSMesa context %dx%d", width, height);
  }
  
  context = (struct osmesaglue_contextdata *)malloc(sizeof(struct osmesaglue_contextdata));
  if (!context) {
    if (osmesa_debug()) {
      cc_debugerror_postwarning("osmesaglue_context_create_offscreen",
                               "Could not allocate memory for OSMesa context");
    }
    return NULL;
  }
  
  memset(context, 0, sizeof(struct osmesaglue_contextdata));
  context->width = width;
  context->height = height;
  
  /* Create OSMesa context - use RGBA format with 16-bit depth buffer */
  context->context = OSMesaCreateContextExt(OSMESA_RGBA, 16, 0, 0, NULL);
  if (!context->context) {
    if (osmesa_debug()) {
      cc_debugerror_postwarning("osmesaglue_context_create_offscreen",
                               "OSMesaCreateContextExt() failed");
    }
    free(context);
    return NULL;
  }
  
  /* Allocate render buffer - 4 bytes per pixel for RGBA */
  context->buffer = malloc(width * height * 4);
  if (!context->buffer) {
    if (osmesa_debug()) {
      cc_debugerror_postwarning("osmesaglue_context_create_offscreen",
                               "Could not allocate render buffer");
    }
    OSMesaDestroyContext(context->context);
    free(context);
    return NULL;
  }
  
  if (osmesa_debug()) {
    cc_debugerror_postinfo("osmesaglue_context_create_offscreen",
                          "Created OSMesa context %p, buffer %p",
                          context->context, context->buffer);
  }
  
  return context;
}

/* Make OSMesa context current */
SbBool
osmesaglue_context_make_current(void * ctx)
{
  struct osmesaglue_contextdata * context = (struct osmesaglue_contextdata *)ctx;
  GLboolean result;
  
  if (!context) return FALSE;
  
  /* Store current context for restoration */
  context->previous_context = OSMesaGetCurrentContext();
  if (context->previous_context) {
    OSMesaGetColorBuffer(context->previous_context, &context->previous_width,
                        &context->previous_height, NULL, &context->previous_buffer);
  }
  
  /* Make our context current */
  result = OSMesaMakeCurrent(context->context, context->buffer, GL_UNSIGNED_BYTE,
                            context->width, context->height);
  
  if (osmesa_debug()) {
    cc_debugerror_postinfo("osmesaglue_context_make_current",
                          "OSMesaMakeCurrent() %s", result ? "succeeded" : "failed");
  }
  
  return result ? TRUE : FALSE;
}

/* Reinstate previous OSMesa context */
void
osmesaglue_context_reinstate_previous(void * ctx)
{
  struct osmesaglue_contextdata * context = (struct osmesaglue_contextdata *)ctx;
  
  if (!context) return;
  
  if (context->previous_context) {
    OSMesaMakeCurrent(context->previous_context, context->previous_buffer,
                     GL_UNSIGNED_BYTE, context->previous_width, context->previous_height);
    
    if (osmesa_debug()) {
      cc_debugerror_postinfo("osmesaglue_context_reinstate_previous",
                            "Restored previous OSMesa context");
    }
  } else {
    /* No previous context - just clear current */
    OSMesaMakeCurrent(NULL, NULL, GL_UNSIGNED_BYTE, 0, 0);
    
    if (osmesa_debug()) {
      cc_debugerror_postinfo("osmesaglue_context_reinstate_previous",
                            "Cleared OSMesa context (no previous)");
    }
  }
}

/* Destroy OSMesa context */
void
osmesaglue_context_destruct(void * ctx)
{
  struct osmesaglue_contextdata * context = (struct osmesaglue_contextdata *)ctx;
  
  if (!context) return;
  
  if (osmesa_debug()) {
    cc_debugerror_postinfo("osmesaglue_context_destruct",
                          "Destroying OSMesa context %p", context->context);
  }
  
  /* Clear context if it's current */
  if (OSMesaGetCurrentContext() == context->context) {
    OSMesaMakeCurrent(NULL, NULL, GL_UNSIGNED_BYTE, 0, 0);
  }
  
  /* Destroy context and free buffer */
  if (context->context) {
    OSMesaDestroyContext(context->context);
  }
  if (context->buffer) {
    free(context->buffer);
  }
  
  free(context);
}

/* Get maximum pbuffer dimensions - OSMesa is limited by available memory */
SbBool
osmesaglue_context_pbuffer_max(void * ctx, unsigned int * lims)
{
  /* OSMesa is software rendering - practical limits depend on available memory
     Return conservative values that should work on most systems */
  lims[0] = 4096;  /* width */
  lims[1] = 4096;  /* height */
  
  if (osmesa_debug()) {
    cc_debugerror_postinfo("osmesaglue_context_pbuffer_max",
                          "Returning max dimensions: %dx%d", lims[0], lims[1]);
  }
  
  return TRUE;
}

/* Cleanup OSMesa glue */
void
osmesaglue_cleanup(void)
{
  if (osmesa_debug()) {
    cc_debugerror_postinfo("osmesaglue_cleanup", "OSMesa glue cleanup");
  }
  /* Nothing special needed for cleanup */
}

#else /* !HAVE_OSMESA */

/* Dummy implementations when OSMesa is not available */

void osmesaglue_init(cc_glglue * w) {
  /* OSMesa not available */
}

void * osmesaglue_getprocaddress(const cc_glglue * glue, const char * fname) { return NULL; }
int osmesaglue_ext_supported(const cc_glglue * w, const char * extension) { return 0; }

void * osmesaglue_context_create_offscreen(unsigned int width, unsigned int height) { 
  assert(FALSE && "OSMesa not available"); 
  return NULL; 
}
SbBool osmesaglue_context_make_current(void * ctx) { 
  assert(FALSE && "OSMesa not available"); 
  return FALSE; 
}
void osmesaglue_context_reinstate_previous(void * ctx) { 
  assert(FALSE && "OSMesa not available"); 
}
void osmesaglue_context_destruct(void * ctx) { 
  assert(FALSE && "OSMesa not available"); 
}

SbBool osmesaglue_context_pbuffer_max(void * ctx, unsigned int * lims) { 
  assert(FALSE && "OSMesa not available"); 
  return FALSE; 
}

void osmesaglue_cleanup(void) {
  /* OSMesa not available */
}

SbBool osmesaglue_available(void) {
  return FALSE;
}

#endif /* HAVE_OSMESA */