#ifndef COIN_SBBASICP_H
#define COIN_SBBASICP_H

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

#include <Inventor/misc/SoBase.h>

class SoAction;
class SoDetail;
class SoElement;
class SoEvent;
class SoPath;
class ScXMLObject;

template <typename Type>
struct coin_depointer {
  enum { valid = false };
};

template <typename Type>
struct coin_depointer<Type *> {
  enum { valid = true };
  typedef Type type;
};

template <typename Type>
struct coin_depointer<Type * const> {
  enum { valid = true };
  typedef Type type;
};

template<typename To,typename From>
To coin_internal_safe_cast2(From * ptr) {
  if((ptr != NULL) && ptr->getTypeId().isDerivedFrom(coin_depointer<To>::type::getClassTypeId()))
  return static_cast<To>(ptr);
  return NULL;
}

template<typename To,typename From>
To
coin_internal_safe_cast(From * ptr) {
  if((ptr != NULL) && ptr->isOfType(coin_depointer<To>::type::getClassTypeId()))
    return static_cast<To>(ptr);
  return NULL;
}

template<typename To>
To coin_safe_cast(const SoBase * ptr) { return coin_internal_safe_cast<To>(ptr); }
template<typename To>
To coin_safe_cast(SoBase * ptr) { return coin_internal_safe_cast<To>(ptr); }
template<typename To>
To coin_safe_cast(SoAction * ptr) { return coin_internal_safe_cast<To>(ptr); }
template<typename To>
To coin_safe_cast(SoDetail * ptr) { return coin_internal_safe_cast<To>(ptr); }
template<typename To>
To coin_safe_cast(const SoDetail * ptr) { return coin_internal_safe_cast<To>(ptr); }
template<typename To>
To coin_safe_cast(SoField * ptr) { return coin_internal_safe_cast<To>(ptr); }
template<typename To>
To coin_safe_cast(SoElement * ptr) { return coin_internal_safe_cast2<To>(ptr); }
template<typename To>
To coin_safe_cast(const SoElement * ptr) { return coin_internal_safe_cast2<To>(ptr); }
template<typename To>
To coin_safe_cast(SoEvent * ptr) { return coin_internal_safe_cast2<To>(ptr); }
template<typename To>
To coin_safe_cast(const SoEvent * ptr) { return coin_internal_safe_cast2<To>(ptr); }
template<typename To>
To coin_safe_cast(ScXMLObject * ptr) { return coin_internal_safe_cast2<To>(ptr); }
template<typename To>
To coin_safe_cast(const ScXMLObject * ptr) { return coin_internal_safe_cast2<To>(ptr); }

#include "config.h"

template<typename To,typename From>
To
coin_internal_assert_cast(From * ptr) {
  To retVal = coin_safe_cast<To>(ptr);
  //NOTE if we ever get an assert here, the error is on the caller,
  //not here. Although it will be prudent to disable this assert in
  //any release before we have tested the calling code well enough. -
  //BFG 20080916
#ifdef COIN_BETA_VERSION // COIN_BETA_VERSION is not defined in release versions
  assert(retVal && "ptr was not of correct type");
#endif // COIN_BETA_VERSION
  return retVal;
}

template<typename To>
To coin_assert_cast(const SoBase * ptr) { return coin_internal_assert_cast<To>(ptr); }
template<typename To>
To coin_assert_cast(SoBase * ptr) { return coin_internal_assert_cast<To>(ptr); }
template<typename To>
To coin_assert_cast(SoAction * ptr) { return coin_internal_assert_cast<To>(ptr); }
template<typename To>
To coin_assert_cast(const SoDetail * ptr) { return coin_internal_assert_cast<To>(ptr); }
template<typename To>
To coin_assert_cast(SoField * ptr) { return coin_internal_assert_cast<To>(ptr); }
template<typename To>
To coin_assert_cast(SoElement * ptr) { return coin_internal_assert_cast<To>(ptr); }
template<typename To>
To coin_assert_cast(const SoElement * ptr) { return coin_internal_assert_cast<To>(ptr); }
template<typename To>
To coin_assert_cast(SoEvent * ptr) { return coin_internal_assert_cast<To>(ptr); }
template<typename To>
To coin_assert_cast(const SoEvent * ptr) { return coin_internal_assert_cast<To>(ptr); }
template<typename To>
To coin_assert_cast(ScXMLObject * ptr) { return coin_internal_assert_cast<To>(ptr); }
template<typename To>
To coin_assert_cast(const ScXMLObject * ptr) { return coin_internal_assert_cast<To>(ptr); }

#endif // !COIN_SBBASICP_H
