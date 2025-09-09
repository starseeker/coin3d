#ifndef COIN_SIMAGE_WRAPPER_H
#define COIN_SIMAGE_WRAPPER_H

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

/* Minimal stub wrapper for simage functionality - disabled for minimal build */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Stub simage wrapper structure */
typedef struct cc_simage_wrapper {
  int available;
  unsigned char * (*simage_read_image)(const char * filename, int * w, int * h, int * nc);
  void (*simage_free_image)(unsigned char * imagedata);
  const char * (*simage_get_last_error)(void);
  int (*versionMatchesAtLeast)(int major, int minor, int micro);
  unsigned char * (*simage_resize)(unsigned char * imagedata, int width, int height, int nc,
                                   int newwidth, int newheight);
  unsigned char * (*simage_resize3d)(unsigned char * imagedata, int width, int height, int depth, int nc,
                                      int newwidth, int newheight, int newdepth);
  int (*simage_check_save_supported)(const char * filename);
  int (*simage_get_num_savers)(void);
  void * (*simage_get_saver_handle)(int idx);
  const char * (*simage_get_saver_extensions)(void * handle);
  const char * (*simage_get_saver_fullname)(void * handle);
  const char * (*simage_get_saver_description)(void * handle);
  void (*simage_version)(int * major, int * minor, int * micro);
  int (*simage_save_image)(const char * filename, const unsigned char * imagedata,
                           int width, int height, int nc, const char * filetypeext);
} cc_simage_wrapper;

const cc_simage_wrapper * simage_wrapper(void);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* !COIN_SIMAGE_WRAPPER_H */