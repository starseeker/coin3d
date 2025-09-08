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

#include "glue/bzip2.h"
#include <cstdio>

/* Minimal stub implementation for bzip2 functionality - disabled for minimal build */

SbBool 
cc_bzip2glue_available(void)
{
  return FALSE; /* No compression support in minimal build */
}

/* Aliases for backward compatibility with expected function names */
SbBool 
cc_bzglue_available(void)
{
  return FALSE; /* No compression support in minimal build */
}

void *
cc_bzglue_BZ2_bzReadOpen(int * bzerror, FILE * f, int small, int verbosity, void * unused, int nunused)
{
  if (bzerror) *bzerror = -1;
  return NULL;
}

int
cc_bzglue_BZ2_bzRead(int * bzerror, void * b, void * buf, int len)
{
  if (bzerror) *bzerror = -1;
  return -1;
}

void
cc_bzglue_BZ2_bzReadClose(int * bzerror, void * b)
{
  if (bzerror) *bzerror = 0;
}

void *
cc_bzglue_BZ2_bzWriteOpen(int * bzerror, FILE * f, int blockSize100k, int verbosity, int workFactor)
{
  if (bzerror) *bzerror = -1;
  return NULL;
}

void
cc_bzglue_BZ2_bzWrite(int * bzerror, void * b, void * buf, int len)
{
  if (bzerror) *bzerror = -1;
}

void
cc_bzglue_BZ2_bzWriteClose(int * bzerror, void * b, int abandon, unsigned int * nbytes_in, unsigned int * nbytes_out)
{
  if (bzerror) *bzerror = 0;
  if (nbytes_in) *nbytes_in = 0;
  if (nbytes_out) *nbytes_out = 0;
}