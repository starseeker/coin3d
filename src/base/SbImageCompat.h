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

#ifndef COIN_SBIMAGECOMPAT_H
#define COIN_SBIMAGECOMPAT_H

#include "SbImageFormatHandler.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*!
  \brief Compatibility wrapper for old simage_wrapper API using new format handlers.
  
  This provides a C-compatible interface that mimics the old simage_wrapper API
  but uses the new SbImageFormatHandler system internally.
*/

// Compatibility function prototypes matching old simage_wrapper API
int sbimage_wrapper_available(void);
unsigned char* sbimage_wrapper_read_image(const char* filename, int* w, int* h, int* nc);
void sbimage_wrapper_free_image(unsigned char* imagedata);
int sbimage_wrapper_save_image(const char* filename, const unsigned char* imagedata,
                              int width, int height, int nc, const char* filetypeext);
int sbimage_wrapper_check_save_supported(const char* filename);
int sbimage_wrapper_get_num_savers(void);
void* sbimage_wrapper_get_saver_handle(int idx);
const char* sbimage_wrapper_get_saver_extensions(void* handle);
const char* sbimage_wrapper_get_saver_fullname(void* handle);
const char* sbimage_wrapper_get_saver_description(void* handle);
void sbimage_wrapper_version(int* major, int* minor, int* micro);
int sbimage_wrapper_version_matches_at_least(int major, int minor, int micro);
const char* sbimage_wrapper_get_last_error(void);
unsigned char* sbimage_wrapper_resize(unsigned char* imagedata, int width, int height, int nc,
                                     int newwidth, int newheight);
unsigned char* sbimage_wrapper_resize3d(unsigned char* imagedata, int width, int height, int depth, int nc,
                                       int newwidth, int newheight, int newdepth);

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif /* !COIN_SBIMAGECOMPAT_H */