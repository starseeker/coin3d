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

#include <Inventor/nodekits/SoNodeKit.h>

#ifdef HAVE_NODEKITS
// When nodekits are enabled, the real implementation should be used
#error "This minimal SoNodeKit implementation should only be used when HAVE_NODEKITS is disabled"
#endif

/*!
  \class SoNodeKit SoNodeKit.h Inventor/nodekits/SoNodeKit.h
  \brief Minimal SoNodeKit implementation when nodekits are disabled.
  
  This provides a minimal implementation of SoNodeKit::init() that can be
  safely called when the full nodekit system is not compiled in.
*/

/*!
  Initialize the SoNodeKit class.
  
  When HAVE_NODEKITS is disabled, this is a minimal implementation that
  does nothing. The full nodekit system initialization is handled by
  individual nodekit classes when HAVE_NODEKITS is enabled.
*/
void
SoNodeKit::init(void)
{
  // Minimal implementation for when nodekits are disabled.
  // The real nodekit classes are conditionally initialized 
  // in SoInteraction::init() when HAVE_NODEKITS is defined.
}