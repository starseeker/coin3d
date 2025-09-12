#ifndef COIN_SBPIMPLPTR_HPP
#define COIN_SBPIMPLPTR_HPP

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

#ifndef COIN_SBPIMPLPTR_H
#error do not include Inventor/tools/SbPimplPtr.hpp directly, use Inventor/tools/SbPimplPtr.h
#endif // !COIN_SBPIMPLPTR_H

/* ********************************************************************** */

template <typename T>
SbPimplPtr<T>::SbPimplPtr(void)
{
  this->ptr = std::make_unique<T>();
}

template <typename T>
SbPimplPtr<T>::SbPimplPtr(T * initial)
{
  this->ptr.reset(initial);
}

template <typename T>
SbPimplPtr<T>::SbPimplPtr(const SbPimplPtr<T> & copy)
{
  if (copy.ptr) {
    this->ptr = std::make_unique<T>(*copy.ptr);
  }
}

template <typename T>
SbPimplPtr<T>::~SbPimplPtr(void)
{
  // std::unique_ptr automatically cleans up
}

template <typename T>
void
SbPimplPtr<T>::set(T * value)
{
  this->ptr.reset(value);
}

template <typename T>
T &
SbPimplPtr<T>::get(void) const
{
  return *this->ptr;
}

template <typename T>
T *
SbPimplPtr<T>::getNew(void) const
{
  return new T;
}

template <typename T>
SbPimplPtr<T> &
SbPimplPtr<T>::operator = (const SbPimplPtr<T> & copy)
{
  if (this != &copy) {
    if (copy.ptr) {
      this->ptr = std::make_unique<T>(*copy.ptr);
    } else {
      this->ptr.reset();
    }
  }
  return *this;
}

template <typename T>
SbBool
SbPimplPtr<T>::operator == (const SbPimplPtr<T> & rhs) const
{
  return this->get() == rhs.get();
}

template <typename T>
SbBool
SbPimplPtr<T>::operator != (const SbPimplPtr<T> & rhs) const
{
  return !(*this == rhs);
}

template <typename T>
const T *
SbPimplPtr<T>::operator -> (void) const
{
  return this->ptr.get();
}

template <typename T>
T *
SbPimplPtr<T>::operator -> (void)
{
  return this->ptr.get();
}

/* ********************************************************************** */

#endif // !COIN_SBPIMPLPTR_HPP
