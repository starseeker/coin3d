#ifndef COIN_SBBARRIER_H
#define COIN_SBBARRIER_H

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

class SbBarrier {
public:
  SbBarrier(unsigned int count) : total_count(count), 
                                  current_count(0), 
                                  generation(0) {
  }
  ~SbBarrier(void) = default;

  int enter(void) { 
    std::unique_lock<std::mutex> lock(mutex);
    
    unsigned int gen = generation;
    
    if (++current_count == total_count) {
      // Last thread to arrive - wake up all waiting threads
      generation++;
      current_count = 0;
      cv.notify_all();
      return 1; // Indicate that this thread triggered the barrier
    } else {
      // Wait for the barrier to be triggered
      cv.wait(lock, [this, gen] { return gen != generation; });
      return 0;
    }
  }

private:
  std::mutex mutex;
  std::condition_variable cv;
  const unsigned int total_count;
  unsigned int current_count;
  unsigned int generation; // Used to distinguish different barrier cycles
  
  // NOTE: Custom C++17 barrier implementation since std::barrier requires C++20.
  // For C++20 migration: Replace with std::barrier which provides similar semantics
  // and potentially better performance.
};

#endif // !COIN_SBBARRIER_H
