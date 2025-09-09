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

#include "glue/simage_wrapper.h"
#include <cstddef>

/* Minimal stub implementation for simage functionality - disabled for minimal build */

static unsigned char *
stub_simage_read_image(const char * filename, int * w, int * h, int * nc)
{
  /* No image loading support in minimal build */
  return NULL;
}

static void
stub_simage_free_image(unsigned char * imagedata)
{
  /* No-op stub */
}

static const char *
stub_simage_get_last_error(void)
{
  return "Image loading disabled in minimal build";
}

static int
stub_versionMatchesAtLeast(int major, int minor, int micro)
{
  return 0; /* Version always too old */
}

static unsigned char *
stub_simage_resize(unsigned char * imagedata, int width, int height, int nc,
                   int newwidth, int newheight)
{
  return NULL; /* Image resizing disabled in minimal build */
}

static unsigned char *
stub_simage_resize3d(unsigned char * imagedata, int width, int height, int depth, int nc,
                     int newwidth, int newheight, int newdepth)
{
  return NULL; /* Image resizing disabled in minimal build */
}

static int
stub_simage_check_save_supported(const char * filename)
{
  return 0; /* Image saving disabled in minimal build */
}

static int
stub_simage_get_num_savers(void)
{
  return 0; /* Image saving disabled in minimal build */
}

static void *
stub_simage_get_saver_handle(int idx)
{
  return NULL; /* Image saving disabled in minimal build */
}

static const char *
stub_simage_get_saver_extensions(void * handle)
{
  return ""; /* Image saving disabled in minimal build */
}

static const char *
stub_simage_get_saver_fullname(void * handle)
{
  return "None"; /* Image saving disabled in minimal build */
}

static const char *
stub_simage_get_saver_description(void * handle)
{
  return "Image saving disabled in minimal build";
}

static void
stub_simage_version(int * major, int * minor, int * micro)
{
  if (major) *major = 0;
  if (minor) *minor = 0;
  if (micro) *micro = 0;
}

static int
stub_simage_save_image(const char * filename, const unsigned char * imagedata,
                       int width, int height, int nc, const char * filetypeext)
{
  return 0; /* Image saving disabled in minimal build */
}

static cc_simage_wrapper simage_instance = {
  0, /* available = FALSE */
  stub_simage_read_image,
  stub_simage_free_image,
  stub_simage_get_last_error,
  stub_versionMatchesAtLeast,
  stub_simage_resize,
  stub_simage_resize3d,
  stub_simage_check_save_supported,
  stub_simage_get_num_savers,
  stub_simage_get_saver_handle,
  stub_simage_get_saver_extensions,
  stub_simage_get_saver_fullname,
  stub_simage_get_saver_description,
  stub_simage_version,
  stub_simage_save_image
};

const cc_simage_wrapper *
simage_wrapper(void)
{
  return &simage_instance;
}