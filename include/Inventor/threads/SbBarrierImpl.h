#ifndef COIN_SBBARRIERIMPL_H
#define COIN_SBBARRIERIMPL_H

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

#include <mutex>
#include <condition_variable>

// C++17 compatible barrier implementation (std::barrier is C++20)
class SbBarrierImpl {
public:
  explicit SbBarrierImpl(unsigned int count) 
    : numthreads(count), counter(0), generation(0) {}
  
  ~SbBarrierImpl() = default;

  // Non-copyable, non-movable
  SbBarrierImpl(const SbBarrierImpl&) = delete;
  SbBarrierImpl& operator=(const SbBarrierImpl&) = delete;
  SbBarrierImpl(SbBarrierImpl&&) = delete;
  SbBarrierImpl& operator=(SbBarrierImpl&&) = delete;

  // Returns 1 if this thread was the last to arrive (similar to cc_barrier_enter)
  int enter() {
    std::unique_lock<std::mutex> lock(mutex);
    
    unsigned int gen = generation;
    ++counter;
    
    if (counter == numthreads) {
      // Last thread to arrive
      ++generation;
      counter = 0;
      condvar.notify_all();
      return 1;
    } else {
      // Wait for all threads to arrive
      condvar.wait(lock, [this, gen] { return gen != generation; });
      return 0;
    }
  }

private:
  unsigned int numthreads;
  unsigned int counter;
  unsigned int generation;  // Prevents spurious wakeups
  std::mutex mutex;
  std::condition_variable condvar;
};

#endif // !COIN_SBBARRIERIMPL_H