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


#include "config.h"

#include <Inventor/SoInput.h>

#include "io/SoInputP.h"
#include "io/SoInput_FileInfo.h"
#include "misc/SoEnvironment.h"

// *************************************************************************

SbBool
SoInputP::debug(void)
{
  static int dbg = -1;
  if (dbg == -1) {
    const char * env = CoinInternal::getEnvironmentVariableRaw("OBOL_DEBUG_IMPORT");
    dbg = (env && (atoi(env) > 0)) ? 1 : 0;
  }
  return dbg;
}

SbBool
SoInputP::debugBinary(void)
{
  static int debug = -1;
  if (debug == -1) {
    const char * env = CoinInternal::getEnvironmentVariableRaw("OBOL_DEBUG_BINARY_INPUT");
    debug = (env && (atoi(env) > 0)) ? 1 : 0;
  }
  return debug ? TRUE : FALSE;
}

// *************************************************************************
/*
  Important note: Up until Coin 3.1.1 we used to have a bug in SoInput
  when reading files from other files (SoFile). The SoInput
  dictionary was global for all files pushed onto the SoInput filestack,
  and DEFs in extra files could therefore overwrite DEFs in the original
  file. This minimal case reproduces this bug:

  #Inventor V2.1 ascii

  Switch {
    whichChild -1
    DEF cube Cube {}
  }

  File { name "minimal_ref.iv" }
  USE cube

  minimal_ref.iv looks something like this:

  #Inventor V2.1 ascii
  DEF cube Info {}

  In older versions of Coin you'll not see the Cube when loading the first
  file.

 */
// *************************************************************************


// Helper function that pops the stack when the current file is at
// EOF.  Then it returns the file at the top of the stack.
SoInput_FileInfo *
SoInputP::getTopOfStackPopOnEOF(void)
{
  SoInput_FileInfo * fi = owner->getTopOfStack();
  assert(fi); // Should always have a top of stack, because the last
              // element on the stack is never removed until the
              // SoInput is closed

  // Pop the stack if end of current file
  if (fi->isEndOfFile()) {
    (void) owner->popFile(); // Only pops if more than one file is on
                             // the stack.
    fi = owner->getTopOfStack();
    assert(fi);
  }

  return fi;
}

SbBool
SoInputP::isNameStartChar(unsigned char c, SbBool validIdent)
{
  if (validIdent) return SbName::isIdentStartChar(c);
  return (c > 0x20); // Not control characters
}

// See SoInputP::isNameStartChar for more information
SbBool
SoInputP::isNameChar(unsigned char c, SbBool validIdent)
{
  if (validIdent) return SbName::isIdentChar(c);
  return (c > 0x20); // Not control characters
}
