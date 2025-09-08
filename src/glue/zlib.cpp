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

#include "glue/zlib.h"
#include <cstdio>

/* Minimal stub implementation for zlib functionality - disabled for minimal build */

SbBool 
cc_zlibglue_available(void)
{
  return FALSE; /* No compression support in minimal build */
}

void *
cc_zlibglue_gzdopen(int fd, const char * mode)
{
  return NULL; /* No compression support in minimal build */
}

int
cc_zlibglue_gzread(void * file, void * buf, unsigned int len)
{
  return -1; /* No compression support in minimal build */
}

int
cc_zlibglue_gzclose(void * file)
{
  return -1; /* No compression support in minimal build */
}

int
cc_zlibglue_gzwrite(void * file, const void * buf, unsigned int len)
{
  return -1; /* No compression support in minimal build */
}

long
cc_zlibglue_gztell(void * file)
{
  return -1; /* No compression support in minimal build */
}

int
cc_zlibglue_gzrewind(void * file)
{
  return -1; /* No compression support in minimal build */
}

unsigned long
cc_zlibglue_crc32(unsigned long crc, const char * buf, unsigned int len)
{
  return 0; /* No compression support in minimal build */
}

int
cc_zlibglue_inflateInit2(void * strm, int windowBits)
{
  return -1; /* No compression support in minimal build */
}

int
cc_zlibglue_inflateEnd(void * strm)
{
  return -1; /* No compression support in minimal build */
}

int
cc_zlibglue_inflate(void * strm, int flush)
{
  return -1; /* No compression support in minimal build */
}

int
cc_zlibglue_inflateReset(void * strm)
{
  return -1; /* No compression support in minimal build */
}