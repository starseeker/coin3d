#ifndef COIN_SBFIFO_H
#define COIN_SBFIFO_H

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
#include <deque>
#include <mutex>
#include <condition_variable>
#include <algorithm>

class SbFifo {
public:
  SbFifo(void) = default;
  ~SbFifo(void) = default;

  void assign(void * ptr, uint32_t type) {
    std::lock_guard<std::mutex> lock(mutex);
    queue.emplace_back(ptr, type);
    cv.notify_one();
  }
  
  void retrieve(void *& ptr, uint32_t &type) {
    std::unique_lock<std::mutex> lock(mutex);
    cv.wait(lock, [this] { return !queue.empty(); });
    
    auto item = queue.front();
    queue.pop_front();
    ptr = item.first;
    type = item.second;
  }
  
  SbBool tryRetrieve(void *& ptr, uint32_t & type) {
    std::lock_guard<std::mutex> lock(mutex);
    if (queue.empty()) {
      return FALSE;
    }
    
    auto item = queue.front();
    queue.pop_front();
    ptr = item.first;
    type = item.second;
    return TRUE;
  }

  unsigned int size(void) const { 
    std::lock_guard<std::mutex> lock(mutex);
    return static_cast<unsigned int>(queue.size()); 
  }

  void lock(void) const { mutex.lock(); }
  void unlock(void) const { mutex.unlock(); }

  // lock/unlock only needed around the following operations:
  SbBool peek(void *& item, uint32_t & type) const {
    // Note: mutex should already be locked by caller
    if (queue.empty()) {
      return FALSE;
    }
    auto front_item = queue.front();
    item = front_item.first;
    type = front_item.second;
    return TRUE;
  }
  
  SbBool contains(void * item) const {
    // Note: mutex should already be locked by caller
    return std::any_of(queue.begin(), queue.end(),
                      [item](const std::pair<void*, uint32_t>& entry) {
                        return entry.first == item;
                      });
  }
  
  SbBool reclaim(void * item) {
    // Note: mutex should already be locked by caller
    auto it = std::find_if(queue.begin(), queue.end(),
                          [item](const std::pair<void*, uint32_t>& entry) {
                            return entry.first == item;
                          });
    if (it != queue.end()) {
      queue.erase(it);
      return TRUE;
    }
    return FALSE;
  }

private:
  mutable std::mutex mutex;
  std::condition_variable cv;
  std::deque<std::pair<void*, uint32_t>> queue;
  
  // NOTE: Custom C++17 thread-safe FIFO implementation since std::concurrent_queue
  // is not available in C++17. Uses std::deque + mutex + condition_variable
  // to provide thread-safe FIFO semantics with blocking/non-blocking operations.
  // For C++20+ migration: Consider using specialized concurrent data structures
  // or lockfree queues for better performance.
};

#endif // !COIN_SBFIFO_H
