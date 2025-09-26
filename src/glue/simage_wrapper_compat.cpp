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

#include "simage_wrapper_compat.h"
#include "../base/SbImageCompat.h"

static unsigned char *
compat_simage_read_image(const char * filename, int * w, int * h, int * nc)
{
  return sbimage_wrapper_read_image(filename, w, h, nc);
}

static void
compat_simage_free_image(unsigned char * imagedata)
{
  sbimage_wrapper_free_image(imagedata);
}

static const char *
compat_simage_get_last_error(void)
{
  return sbimage_wrapper_get_last_error();
}

static int
compat_versionMatchesAtLeast(int major, int minor, int micro)
{
  return sbimage_wrapper_version_matches_at_least(major, minor, micro);
}

static unsigned char *
compat_simage_resize(unsigned char * imagedata, int width, int height, int nc,
                    int newwidth, int newheight)
{
  return sbimage_wrapper_resize(imagedata, width, height, nc, newwidth, newheight);
}

static unsigned char *
compat_simage_resize3d(unsigned char * imagedata, int width, int height, int depth, int nc,
                      int newwidth, int newheight, int newdepth)
{
  return sbimage_wrapper_resize3d(imagedata, width, height, depth, nc, newwidth, newheight, newdepth);
}

static int
compat_simage_check_save_supported(const char * filename)
{
  return sbimage_wrapper_check_save_supported(filename);
}

static int
compat_simage_get_num_savers(void)
{
  return sbimage_wrapper_get_num_savers();
}

static void *
compat_simage_get_saver_handle(int idx)
{
  return sbimage_wrapper_get_saver_handle(idx);
}

static const char *
compat_simage_get_saver_extensions(void * handle)
{
  return sbimage_wrapper_get_saver_extensions(handle);
}

static const char *
compat_simage_get_saver_fullname(void * handle)
{
  return sbimage_wrapper_get_saver_fullname(handle);
}

static const char *
compat_simage_get_saver_description(void * handle)
{
  return sbimage_wrapper_get_saver_description(handle);
}

static void
compat_simage_version(int * major, int * minor, int * micro)
{
  sbimage_wrapper_version(major, minor, micro);
}

static int
compat_simage_save_image(const char * filename, const unsigned char * imagedata,
                        int width, int height, int nc, const char * filetypeext)
{
  return sbimage_wrapper_save_image(filename, imagedata, width, height, nc, filetypeext);
}

static cc_simage_wrapper simage_instance = {
  1, /* available = TRUE */
  compat_simage_read_image,
  compat_simage_free_image,
  compat_simage_get_last_error,
  compat_versionMatchesAtLeast,
  compat_simage_resize,
  compat_simage_resize3d,
  compat_simage_check_save_supported,
  compat_simage_get_num_savers,
  compat_simage_get_saver_handle,
  compat_simage_get_saver_extensions,
  compat_simage_get_saver_fullname,
  compat_simage_get_saver_description,
  compat_simage_version,
  compat_simage_save_image
};

const cc_simage_wrapper *
simage_wrapper(void)
{
  return &simage_instance;
}