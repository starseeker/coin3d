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
#include <cstdio>
#include <cstring>
#include <cstdlib>

/* Functional image saving implementation using toojpeg */

#define TOOJPEG_IMPLEMENTATION
#include "toojpeg.h"

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
  if (!filename) return 0;
  
  const char* ext = strrchr(filename, '.');
  if (!ext) return 0;
  
  /* Check if it's a supported format */
  if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0 || 
      strcmp(ext, ".JPG") == 0 || strcmp(ext, ".JPEG") == 0) {
    return 1; /* JPEG supported */
  }
  return 0; /* Format not supported */
}

static int
stub_simage_get_num_savers(void)
{
  return 2; /* JPEG and PNG savers available */
}

static void *
stub_simage_get_saver_handle(int idx)
{
  static const char* savers[] = {"jpeg"};
  if (idx >= 0 && idx < 1) {
    return (void*)savers[idx];
  }
  return NULL;
}

static const char *
stub_simage_get_saver_extensions(void * handle)
{
  if (!handle) return "";
  
  const char* saver = (const char*)handle;
  if (strcmp(saver, "jpeg") == 0) {
    return "jpg,jpeg";
  }
  return "";
}

static const char *
stub_simage_get_saver_fullname(void * handle)
{
  if (!handle) return "None";
  
  const char* saver = (const char*)handle;
  if (strcmp(saver, "jpeg") == 0) {
    return "JPEG File Format";
  }
  return "None";
}

static const char *
stub_simage_get_saver_description(void * handle)
{
  if (!handle) return "Image saving disabled in minimal build";
  
  const char* saver = (const char*)handle;
  if (strcmp(saver, "jpeg") == 0) {
    return "JPEG image saver using TooJPEG library";
  }
  return "Image saving disabled in minimal build";
}

static void
stub_simage_version(int * major, int * minor, int * micro)
{
  if (major) *major = 1;
  if (minor) *minor = 0;
  if (micro) *micro = 0;
}

/* Helper structures for image writing */
struct jpeg_write_context {
  FILE* file;
  int error;
};

/* Global context pointer - not thread-safe, but matches simage API design */
static jpeg_write_context* jpeg_write_context_ptr = NULL;

static void jpeg_write_callback(unsigned char byte)
{
  jpeg_write_context* ctx = (jpeg_write_context*)jpeg_write_context_ptr;
  if (ctx && ctx->file && !ctx->error) {
    if (fputc(byte, ctx->file) == EOF) {
      ctx->error = 1;
    }
  }
}

static int
stub_simage_save_image(const char * filename, const unsigned char * imagedata,
                       int width, int height, int nc, const char * filetypeext)
{
  if (!filename || !imagedata || width <= 0 || height <= 0 || nc <= 0) {
    return 0;
  }
  
  const char* ext = filetypeext;
  if (!ext) {
    ext = strrchr(filename, '.');
    if (!ext) return 0;
    ext++; /* Skip the dot */
  }
  
  FILE* file = fopen(filename, "wb");
  if (!file) return 0;
  
  int success = 0;
  
  if (strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0 || 
      strcmp(ext, "JPG") == 0 || strcmp(ext, "JPEG") == 0) {
    /* Save as JPEG using TooJPEG */
    jpeg_write_context ctx;
    ctx.file = file;
    ctx.error = 0;
    jpeg_write_context_ptr = &ctx;
    
    /* Convert data format if needed */
    bool isRGB = (nc >= 3);
    if (nc == 4) {
      /* Convert RGBA to RGB by discarding alpha channel */
      unsigned char* rgb_data = (unsigned char*)malloc(width * height * 3);
      if (rgb_data) {
        for (int i = 0; i < width * height; i++) {
          rgb_data[i*3] = imagedata[i*4];
          rgb_data[i*3+1] = imagedata[i*4+1];
          rgb_data[i*3+2] = imagedata[i*4+2];
        }
        success = TooJpeg::writeJpeg(jpeg_write_callback, rgb_data, width, height, true, 90) ? 1 : 0;
        free(rgb_data);
      }
    } else {
      success = TooJpeg::writeJpeg(jpeg_write_callback, imagedata, width, height, isRGB, 90) ? 1 : 0;
    }
    
    jpeg_write_context_ptr = NULL;
    
    if (ctx.error) success = 0;
    
  }
  
  fclose(file);
  return success;
}

static cc_simage_wrapper simage_instance = {
  1, /* available = TRUE */
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
