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

/* Minimal stub implementation for gzip compression functionality - disabled for minimal build */

#include "io/gzmemio.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif /* HAVE_CONFIG_H */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

void * cc_gzm_open(const uint8_t * buffer, uint32_t len)
{
  return NULL; /* Compression support disabled in minimal build */
}

int cc_gzm_read(void * file, void * buf, uint32_t len)
{
  return -1; /* Compression support disabled in minimal build */
}

int cc_gzm_getc(void * file)
{
  return -1; /* Compression support disabled in minimal build */
}

char * cc_gzm_gets(void * file, char * buf, int len)
{
  return NULL; /* Compression support disabled in minimal build */
}

off_t cc_gzm_seek(void * file, off_t offset, int whence)
{
  return -1; /* Compression support disabled in minimal build */
}

int cc_gzm_rewind(void * file)
{
  return -1; /* Compression support disabled in minimal build */
}

off_t cc_gzm_tell(void * file)
{
  return -1; /* Compression support disabled in minimal build */
}

int cc_gzm_eof(void * file)
{
  return 1; /* Compression support disabled in minimal build */
}

int cc_gzm_close(void * file)
{
  return 0; /* Compression support disabled in minimal build */
}

int cc_gzm_sizeof_z_stream(void)
{
  return 0; /* Compression support disabled in minimal build */
}

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */
