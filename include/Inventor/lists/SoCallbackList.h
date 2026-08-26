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
  SoCallbackList(const SoCallbackList & other);
  SoCallbackList & operator=(const SoCallbackList & other);
  ~SoCallbackList();

  void addCallback(SoCallbackListCB * f, void * userData = NULL);
  void removeCallback(SoCallbackListCB * f, void * userdata = NULL);

  // Preserve strongly typed callback signatures.  Historically callers cast
  // callbacks such as void(void *, SoDragger *) to SoCallbackListCB and the
  // list invoked them through that incompatible type.  That happens to work
  // on common ABIs, but is undefined C++.  These overloads erase only the
  // callback data pointer and invoke the function through its exact type.
  template <typename CallbackData>
  void addCallback(void (*f)(void *, CallbackData *),
                   void * userData = NULL);
  template <typename CallbackData>
  void removeCallback(void (*f)(void *, CallbackData *),
                      void * userData = NULL);

  void clearCallbacks(void);
  int getNumCallbacks(void) const;

  void invokeCallbacks(void * callbackdata);

private:
  class CallbackEntry {
  public:
    virtual ~CallbackEntry() = default;
    virtual CallbackEntry * clone() const = 0;
    virtual void invoke(void * userdata, void * callbackdata) const = 0;
  };

  template <typename CallbackData>
  class TypedCallbackEntry final : public CallbackEntry {
  public:
    explicit TypedCallbackEntry(void (*callback)(void *, CallbackData *))
      : function(callback) { }

    CallbackEntry * clone() const override {
      return new TypedCallbackEntry(this->function);
    }
    void invoke(void * userdata, void * callbackdata) const override {
      this->function(userdata, static_cast<CallbackData *>(callbackdata));
    }

    void (*function)(void *, CallbackData *);
  };

  void copyCallbacks(const SoCallbackList & other);
  void reportMissingCallback(void) const;

  // funclist owns CallbackEntry pointers.  datalist retains the associated
  // caller-owned userdata.  Keeping the two historical SbPList members also
  // preserves the public class layout.
  SbPList funclist;
  SbPList datalist;
};

template <typename CallbackData>
void
SoCallbackList::addCallback(void (*f)(void *, CallbackData *), void * userdata)
{
  this->funclist.append(new TypedCallbackEntry<CallbackData>(f));
  this->datalist.append(userdata);
}

template <typename CallbackData>
void
SoCallbackList::removeCallback(void (*f)(void *, CallbackData *),
                               void * userdata)
{
  for (int idx = this->getNumCallbacks() - 1; idx >= 0; --idx) {
    CallbackEntry * base = static_cast<CallbackEntry *>(this->funclist[idx]);
    TypedCallbackEntry<CallbackData> * entry =
      dynamic_cast<TypedCallbackEntry<CallbackData> *>(base);
    if (entry && entry->function == f && this->datalist[idx] == userdata) {
      delete entry;
      this->funclist.remove(idx);
      this->datalist.remove(idx);
      return;
    }
  }
  this->reportMissingCallback();
}

#endif // !OBOL_LISTS_SOCALLBACKLIST_H
