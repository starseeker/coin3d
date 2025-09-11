#ifndef COIN_SBTHREADAUTOLOCK_H
#define COIN_SBTHREADAUTOLOCK_H

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

#include <Inventor/threads/SbThreadMutex.h>
#include <Inventor/threads/SbMutex.h>
#include <mutex>
#include <memory>

class SbThreadAutoLock
{
protected:
  SbMutex * mutex;
  SbThreadMutex * recmutex;
  std::unique_ptr<std::lock_guard<std::mutex>> mutex_lock;
  std::unique_ptr<std::lock_guard<std::recursive_mutex>> recmutex_lock;

public:
  SbThreadAutoLock(SbMutex * mutexptr) {
    this->mutex = mutexptr;
    this->mutex_lock = std::make_unique<std::lock_guard<std::mutex>>(mutexptr->mutex);
    this->recmutex = nullptr;
  }
  SbThreadAutoLock(SbThreadMutex * mutexptr) {
    this->recmutex = mutexptr;
    this->recmutex_lock = std::make_unique<std::lock_guard<std::recursive_mutex>>(mutexptr->mutex);
    this->mutex = nullptr;
  }

  ~SbThreadAutoLock() {
    // std::lock_guard automatically unlocks in destructor
  }
};

#endif // !COIN_SBTHREADAUTOLOCK_H
