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

#include "io/SoInput_Reader.h"

#include <cstring>
#include <cassert>
#include <iostream>
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif // HAVE_CONFIG_H

#ifdef HAVE_UNISTD_H
#include <unistd.h> // dup()
#endif // HAVE_UNISTD_H

#ifdef HAVE_IO_H
#include <io.h> // Win32 dup()
#endif // HAVE_IO_H

#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#include <Inventor/errors/SoDebugError.h>

// We don't want to include bzlib.h, so we just define the constants
// we use here
#ifndef BZ_OK
#define BZ_OK 0
#endif // BZ_OK

#ifndef BZ_STREAM_END
#define BZ_STREAM_END 4
#endif // BZ_STREAM_END

//
// abstract class
//

SoInput_Reader::SoInput_Reader(void)
  : dummyname("")
{
}

SoInput_Reader::~SoInput_Reader()
{
}

const SbString &
SoInput_Reader::getFilename(void)
{
  return this->dummyname;
}

FILE *
SoInput_Reader::getFilePointer(void)
{
  return NULL;
}

// creates the correct reader based on the file type in fp (will
// examine the file header). If fullname is empty, it's assumed that
// file FILE pointer is passed from the user, and that we cannot
// necessarily find the file handle.
SoInput_Reader *
SoInput_Reader::createReader(FILE * fp, const SbString & fullname)
{
  SoInput_Reader * reader = new SoInput_FileReader(fullname.getString(), fp);
  return reader;
}

//
// standard FILE * class
//

SoInput_FileReader::SoInput_FileReader(const char * const filenamearg, FILE * filepointer)
{
  this->fp = filepointer;
  this->filename = filenamearg;
}

SoInput_FileReader::~SoInput_FileReader()
{
  // Close files which are not a memory buffer nor the stdin and
  // which we do have a filename for (if we don't have a filename,
  // the FILE ptr was just passed in through setFilePointer() and
  // is the library programmer's responsibility).
  if (this->fp &&
      (this->filename != "<stdin>") &&
      (this->filename.getLength())) {
    fclose(this->fp);
  }
}

SoInput_Reader::ReaderType
SoInput_FileReader::getType(void) const
{
  return REGULAR_FILE;
}

size_t
SoInput_FileReader::readBuffer(char * buf, const size_t readlen)
{
  return fread(buf, 1, readlen, this->fp);
}

const SbString &
SoInput_FileReader::getFilename(void)
{
  return this->filename;
}

FILE *
SoInput_FileReader::getFilePointer(void)
{
  return this->fp;
}

//
// standard membuffer class
//

SoInput_MemBufferReader::SoInput_MemBufferReader(const void * bufPointer, size_t bufSize)
{
  this->buf = (char*) bufPointer;
  this->buflen = bufSize;
  this->bufpos = 0;
}

SoInput_MemBufferReader::~SoInput_MemBufferReader()
{
}

SoInput_Reader::ReaderType
SoInput_MemBufferReader::getType(void) const
{
  return MEMBUFFER;
}

size_t
SoInput_MemBufferReader::readBuffer(char * buffer, const size_t readlen)
{
  size_t len = this->buflen - this->bufpos;
  if (len > readlen) len = readlen;

  memcpy(buffer, this->buf + this->bufpos, len);
  this->bufpos += len;

  return len;
}

//
// iostream stream reader class
//

SoInput_StreamReader::SoInput_StreamReader(std::istream * streamarg)
{
  this->stream = streamarg;
  this->streamname = "<iostream>";
}

SoInput_StreamReader::~SoInput_StreamReader()
{
  // Don't delete the stream - we don't own it
}

SoInput_Reader::ReaderType
SoInput_StreamReader::getType(void) const
{
  return IOSTREAM;
}

size_t
SoInput_StreamReader::readBuffer(char * buffer, const size_t readlen)
{
  if (!this->stream || !this->stream->good()) {
    return 0;
  }

  this->stream->read(buffer, readlen);
  return static_cast<size_t>(this->stream->gcount());
}

const SbString &
SoInput_StreamReader::getFilename(void)
{
  return this->streamname;
}

