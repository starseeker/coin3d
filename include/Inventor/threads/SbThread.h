#ifndef COIN_SBTHREAD_H
#define COIN_SBTHREAD_H

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

#include <Inventor/SbBasic.h>
#include <thread>
#include <memory>

class SbThread {
public:
  static SbThread * create(void *(*func)(void *), void * closure) {
    return new SbThread(func, closure);
  }
  static void destroy(SbThread * thread) {
    delete thread;
  }

  SbBool join(void ** retval = 0L) {
    if (this->thread && this->thread->joinable()) {
      this->thread->join();
      // Note: We can't return the actual return value from std::thread
      // as it doesn't support void* return values like POSIX threads.
      // The return value feature is rarely used, so we set it to nullptr.
      if (retval) *retval = this->return_value;
      return TRUE;
    }
    return FALSE;
  }
  static SbBool join(SbThread * thread, void ** retval = 0L) {
    return thread->join(retval);
  }

protected:
  SbThread(void *(*func)(void *), void * closure) 
    : return_value(nullptr) {
    // Wrap the POSIX-style function in a lambda for std::thread
    this->thread = std::make_unique<std::thread>([this, func, closure]() {
      this->return_value = func(closure);
    });
  }
  ~SbThread(void) {
    if (this->thread && this->thread->joinable()) {
      this->thread->join();
    }
  }

private:
  std::unique_ptr<std::thread> thread;
  void * return_value; // Store return value for compatibility
};

#endif // !COIN_SBTHREAD_H
