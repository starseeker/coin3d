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

/*!
  \class SbStorage SbStorage.h Inventor/threads/SbStorage.h
  \brief The SbStorage class manages thread-local memory.

  \ingroup coin_threads

  This class manages thread-local memory. When different threads access
  the memory an SbStorage object manages, they will receive different
  memory blocks back.

  This provides a mechanism for sharing read/write static data.
*/

#include <Inventor/threads/SbStorage.h>
#include "threads/threads.h"
#include "threads/storagep.h"

SbStorage::SbStorage(unsigned int size)
{
  this->impl = cc_storage_construct(size);
}

SbStorage::SbStorage(unsigned int size, SbStorageConstructFunc * constr,
                     SbStorageConstructFunc * destr)
{
  this->impl = cc_storage_construct_etc(size,
    reinterpret_cast<cc_storage_f *>(constr),
    reinterpret_cast<cc_storage_f *>(destr));
}

SbStorage::~SbStorage(void)
{
  cc_storage_destruct(reinterpret_cast<cc_storage *>(this->impl));
}

void *
SbStorage::get(void)
{
  return cc_storage_get(reinterpret_cast<cc_storage *>(this->impl));
}

void
SbStorage::applyToAll(SbStorageApplyFunc * func, void * closure)
{
  cc_storage_apply_to_all(reinterpret_cast<cc_storage *>(this->impl),
    reinterpret_cast<cc_storage_apply_func *>(func), closure);
}
