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

#include "Inventor/C/basic.h"
#include "C/glue/gl.h"
#include "C/errors/debugerror.h"
#include "misc/SoEnvironment.h"

#include "gl_fbo.h"
#include "glp.h"

/* ********************************************************************** */

static SbBool fbo_debug(void)
{
  static int fbo_debug_value = -1;
  if (fbo_debug_value == -1) {
    auto env = CoinInternal::getEnvironmentVariable("COIN_DEBUG_FBO");
    fbo_debug_value = env.has_value() ? std::atoi(env->c_str()) : 0;
  }
  return fbo_debug_value > 0;
}

/* Check if FBO offscreen rendering is available */
SbBool
fbo_offscreen_available(void)
{
  /* Try to get the current glue instance - this might fail if no context is current */
  const cc_glglue * glue = NULL;
  
  /* First try to see if we have any valid GL context at all */
  /* We need to be very careful here as glGetError() might not be safe to call */
  /* without a context. Let's just try to get the glue instance directly. */
  
  /* We use contextid 0 which should be safe even without a real context */
  /* but the glue instance might not be created yet */
  
  /* Actually, let's be safer and not try to access GL at all without context */
  /* Just return FALSE if we can't safely determine FBO availability */
  
  return FALSE; /* Conservative approach - require explicit context setup */
}

/* Create FBO-based offscreen context */
void *
fbo_context_create_offscreen(unsigned int width, unsigned int height)
{
  const cc_glglue * glue = cc_glglue_instance(0);
  if (!glue || !cc_glglue_has_framebuffer_objects(glue)) {
    if (fbo_debug()) {
      cc_debugerror_postinfo("fbo_context_create_offscreen",
                             "FBO not available");
    }
    return NULL;
  }

  struct fbo_offscreen_data * context = 
    (struct fbo_offscreen_data *)malloc(sizeof(struct fbo_offscreen_data));
  
  if (!context) {
    if (fbo_debug()) {
      cc_debugerror_postwarning("fbo_context_create_offscreen",
                               "Could not allocate memory for FBO context");
    }
    return NULL;
  }

  memset(context, 0, sizeof(struct fbo_offscreen_data));
  context->width = width;
  context->height = height;
  context->glue = glue;
  context->previous_framebuffer = 0;

  /* Generate framebuffer */
  cc_glglue_glGenFramebuffers(glue, 1, &context->framebuffer);
  if (context->framebuffer == 0) {
    if (fbo_debug()) {
      cc_debugerror_postwarning("fbo_context_create_offscreen",
                               "Could not generate framebuffer");
    }
    free(context);
    return NULL;
  }

  /* Bind framebuffer for setup */
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &context->previous_framebuffer);
  cc_glglue_glBindFramebuffer(glue, GL_FRAMEBUFFER, context->framebuffer);

  /* Generate and setup color buffer */
  cc_glglue_glGenRenderbuffers(glue, 1, &context->colorbuffer);
  cc_glglue_glBindRenderbuffer(glue, GL_RENDERBUFFER, context->colorbuffer);
  cc_glglue_glRenderbufferStorage(glue, GL_RENDERBUFFER, GL_RGBA8, width, height);
  cc_glglue_glFramebufferRenderbuffer(glue, GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                      GL_RENDERBUFFER, context->colorbuffer);

  /* Generate and setup depth buffer */
  cc_glglue_glGenRenderbuffers(glue, 1, &context->depthbuffer);
  cc_glglue_glBindRenderbuffer(glue, GL_RENDERBUFFER, context->depthbuffer);
  cc_glglue_glRenderbufferStorage(glue, GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
  cc_glglue_glFramebufferRenderbuffer(glue, GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                      GL_RENDERBUFFER, context->depthbuffer);

  /* Check framebuffer completeness */
  GLenum status = cc_glglue_glCheckFramebufferStatus(glue, GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    if (fbo_debug()) {
      cc_debugerror_postwarning("fbo_context_create_offscreen",
                               "Framebuffer not complete: 0x%x", status);
    }
    
    /* Cleanup on failure */
    cc_glglue_glDeleteRenderbuffers(glue, 1, &context->colorbuffer);
    cc_glglue_glDeleteRenderbuffers(glue, 1, &context->depthbuffer);
    cc_glglue_glDeleteFramebuffers(glue, 1, &context->framebuffer);
    cc_glglue_glBindFramebuffer(glue, GL_FRAMEBUFFER, context->previous_framebuffer);
    free(context);
    return NULL;
  }

  /* Restore previous framebuffer */
  cc_glglue_glBindFramebuffer(glue, GL_FRAMEBUFFER, context->previous_framebuffer);

  if (fbo_debug()) {
    cc_debugerror_postinfo("fbo_context_create_offscreen",
                          "Created FBO offscreen context %dx%d, FBO id=%u",
                          width, height, context->framebuffer);
  }

  return context;
}

/* Make FBO context current */
SbBool
fbo_context_make_current(void * ctx)
{
  struct fbo_offscreen_data * context = (struct fbo_offscreen_data *)ctx;
  if (!context || !context->glue) {
    return FALSE;
  }

  /* Store current framebuffer binding */
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &context->previous_framebuffer);
  
  /* Bind our framebuffer */
  cc_glglue_glBindFramebuffer(context->glue, GL_FRAMEBUFFER, context->framebuffer);
  
  /* Set viewport to match framebuffer size */
  glViewport(0, 0, context->width, context->height);

  if (fbo_debug()) {
    cc_debugerror_postinfo("fbo_context_make_current",
                          "Made FBO context current, FBO id=%u, previous=%d",
                          context->framebuffer, context->previous_framebuffer);
  }

  return TRUE;
}

/* Reinstate previous framebuffer */
void
fbo_context_reinstate_previous(void * ctx)
{
  struct fbo_offscreen_data * context = (struct fbo_offscreen_data *)ctx;
  if (!context || !context->glue) {
    return;
  }

  /* Restore previous framebuffer */
  cc_glglue_glBindFramebuffer(context->glue, GL_FRAMEBUFFER, context->previous_framebuffer);

  if (fbo_debug()) {
    cc_debugerror_postinfo("fbo_context_reinstate_previous",
                          "Restored framebuffer to %d", context->previous_framebuffer);
  }
}

/* Cleanup FBO context */
void
fbo_context_destruct(void * ctx)
{
  struct fbo_offscreen_data * context = (struct fbo_offscreen_data *)ctx;
  if (!context || !context->glue) {
    return;
  }

  if (fbo_debug()) {
    cc_debugerror_postinfo("fbo_context_destruct",
                          "Destroying FBO context, FBO id=%u", context->framebuffer);
  }

  /* Delete FBO resources */
  if (context->colorbuffer) {
    cc_glglue_glDeleteRenderbuffers(context->glue, 1, &context->colorbuffer);
  }
  if (context->depthbuffer) {
    cc_glglue_glDeleteRenderbuffers(context->glue, 1, &context->depthbuffer);
  }
  if (context->framebuffer) {
    cc_glglue_glDeleteFramebuffers(context->glue, 1, &context->framebuffer);
  }

  free(context);
}