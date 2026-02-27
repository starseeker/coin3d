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

// FIXME: this class has not been implemented yet. 20000627 mortene.

#include <Inventor/misc/SoByteStream.h>
#include "config.h" // OBOL_STUB
#include <cstdlib>


SoByteStream::SoByteStream(void)
{
  OBOL_STUB();
}

SoByteStream::~SoByteStream()
{
  OBOL_STUB();
}

void
SoByteStream::convert(SoNode * OBOL_UNUSED_ARG(node), SbBool OBOL_UNUSED_ARG(binary))
{
  OBOL_STUB();
}

void
SoByteStream::convert(SoPath * OBOL_UNUSED_ARG(path), SbBool OBOL_UNUSED_ARG(binary))
{
  OBOL_STUB();
}

void
SoByteStream::convert(SoPathList * OBOL_UNUSED_ARG(pl), SbBool OBOL_UNUSED_ARG(binary))
{
  OBOL_STUB();
}

void *
SoByteStream::getData(void)
{
  OBOL_STUB();
  return NULL;
}

uint32_t
SoByteStream::getNumBytes(void)
{
  OBOL_STUB();
  return 0;
}

SoPathList *
SoByteStream::unconvert(SoByteStream * OBOL_UNUSED_ARG(stream))
{
  OBOL_STUB();
  return NULL;
}

SoPathList *
SoByteStream::unconvert(void * OBOL_UNUSED_ARG(data), uint32_t OBOL_UNUSED_ARG(bytesinstream))
{
  OBOL_STUB();
  return NULL;
}

void
SoByteStream::copy(void * OBOL_UNUSED_ARG(d), size_t OBOL_UNUSED_ARG(len))
{
  OBOL_STUB();
}

SbBool
SoByteStream::isRawData(void) const
{
  OBOL_STUB();
  return FALSE;
}
