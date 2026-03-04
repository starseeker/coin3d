#ifndef OBOL_LISTS_SOCALLBACKLIST_H
#define OBOL_LISTS_SOCALLBACKLIST_H

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

// NB: make sure the ifdef-test above wrapping this includefile is
// _not_ checking on and setting the same define-string as the other
// SoCallbackList.h file in misc/.

#include <Inventor/lists/SbPList.h>

typedef void SoCallbackListCB(void * userdata, void * callbackdata);

/*!
  \class SoCallbackList SoCallbackList.h Inventor/lists/SoCallbackList.h
  \brief Manages a list of callback functions with user-data pointers.

  \ingroup coin_lists

  SoCallbackList stores (function, userdata) pairs and provides
  methods to invoke all registered callbacks in order.

  \sa SbPList
*/
class OBOL_DLL_API SoCallbackList {
public:
  SoCallbackList(void);
  ~SoCallbackList();

  void addCallback(SoCallbackListCB * f, void * userData = NULL);
  void removeCallback(SoCallbackListCB * f, void * userdata = NULL);

  void clearCallbacks(void);
  int getNumCallbacks(void) const;

  void invokeCallbacks(void * callbackdata);

private:
  SbPList funclist;
  SbPList datalist;
};

#endif // !OBOL_LISTS_SOCALLBACKLIST_H
