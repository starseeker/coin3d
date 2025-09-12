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

#include "base/list.h"

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <vector>
#include <algorithm>

#include "coindefs.h"

#define CC_LIST_DEFAULT_SIZE 4

// Modern STL-based implementation that maintains C interface compatibility
struct cc_list {
  std::vector<void*> items;
  
  // Constructor with capacity
  explicit cc_list(int initial_capacity = CC_LIST_DEFAULT_SIZE) {
    if (initial_capacity > 0) {
      items.reserve(initial_capacity);
    }
  }
};

/* ********************************************************************** */

cc_list *
cc_list_construct(void)
{
  return cc_list_construct_sized(CC_LIST_DEFAULT_SIZE);
}

cc_list *
cc_list_construct_sized(int size)
{
  try {
    return new cc_list(size > 0 ? size : CC_LIST_DEFAULT_SIZE);
  } catch (const std::bad_alloc&) {
    return nullptr;
  }
}

cc_list * 
cc_list_clone(cc_list * list)
{
  if (!list) return nullptr;
  
  try {
    cc_list * cloned = new cc_list(static_cast<int>(list->items.size()));
    cloned->items = list->items; // STL vector copy
    return cloned;
  } catch (const std::bad_alloc&) {
    return nullptr;
  }
}

void
cc_list_destruct(cc_list * list)
{
  delete list;
}

void
cc_list_append(cc_list * list, void * item)
{
  if (!list) return;
  list->items.push_back(item);
}

int
cc_list_find(cc_list * list, void * item)
{
  if (!list) return -1;
  
  auto it = std::find(list->items.begin(), list->items.end(), item);
  if (it != list->items.end()) {
    return static_cast<int>(std::distance(list->items.begin(), it));
  }
  return -1;
}

void
cc_list_insert(cc_list * list, void * item, int insertbefore)
{
  if (!list) return;
  
#ifdef COIN_EXTRA_DEBUG
  assert(insertbefore >= 0 && insertbefore <= static_cast<int>(list->items.size()));
#endif /* COIN_EXTRA_DEBUG */
  
  if (insertbefore >= 0 && insertbefore <= static_cast<int>(list->items.size())) {
    list->items.insert(list->items.begin() + insertbefore, item);
  }
}

void
cc_list_remove(cc_list * list, int index)
{
  if (!list) return;
  
#ifdef COIN_EXTRA_DEBUG
  assert(index >= 0 && index < static_cast<int>(list->items.size()));
#endif /* COIN_EXTRA_DEBUG */
  
  if (index >= 0 && index < static_cast<int>(list->items.size())) {
    list->items.erase(list->items.begin() + index);
  }
}

void
cc_list_remove_item(cc_list * list, void * item)
{
  if (!list) return;
  
  int idx = cc_list_find(list, item);
#ifdef COIN_EXTRA_DEBUG
  assert(idx != -1);
#endif /* COIN_EXTRA_DEBUG */
  if (idx != -1) {
    cc_list_remove(list, idx);
  }
}

void
cc_list_remove_fast(cc_list * list, int index)
{
  if (!list) return;
  
#ifdef COIN_EXTRA_DEBUG
  assert(index >= 0 && index < static_cast<int>(list->items.size()));
#endif /* COIN_EXTRA_DEBUG */
  
  if (index >= 0 && index < static_cast<int>(list->items.size())) {
    // Fast removal: swap with last element and pop
    if (index != static_cast<int>(list->items.size()) - 1) {
      list->items[index] = list->items.back();
    }
    list->items.pop_back();
  }
}

void
cc_list_fit(cc_list * list)
{
  if (!list) return;
  
  // STL shrink_to_fit provides similar functionality
  list->items.shrink_to_fit();
}

void
cc_list_truncate(cc_list * list, int length)
{
  if (!list) return;
  
#ifdef COIN_EXTRA_DEBUG
  assert(length <= static_cast<int>(list->items.size()));
#endif /* COIN_EXTRA_DEBUG */
  
  if (length >= 0 && length <= static_cast<int>(list->items.size())) {
    list->items.resize(length);
  }
}

void
cc_list_truncate_fit(cc_list * list, int length)
{
  if (!list) return;
  
#ifdef COIN_EXTRA_DEBUG
  assert(length <= static_cast<int>(list->items.size()));
#endif /* COIN_EXTRA_DEBUG */
  
  cc_list_truncate(list, length);
  cc_list_fit(list);
}

int
cc_list_get_length(cc_list * list)
{
  if (!list) return 0;
  return static_cast<int>(list->items.size());
}

void **
cc_list_get_array(cc_list * list)
{
  if (!list || list->items.empty()) return nullptr;
  return list->items.data();
}

void * 
cc_list_get(cc_list * list, int itempos)
{
  if (!list) return nullptr;
  
#ifdef COIN_EXTRA_DEBUG
  assert(itempos < static_cast<int>(list->items.size()));
#endif /* COIN_EXTRA_DEBUG */
  
  if (itempos >= 0 && itempos < static_cast<int>(list->items.size())) {
    return list->items[itempos];
  }
  return nullptr;
}

void
cc_list_push(cc_list * list, void * item)
{
  cc_list_append(list, item);
}

void *
cc_list_pop(cc_list * list)
{
  if (!list || list->items.empty()) return nullptr;
  
#ifdef COIN_EXTRA_DEBUG
  assert(!list->items.empty());
#endif /* COIN_EXTRA_DEBUG */
  
  void * item = list->items.back();
  list->items.pop_back();
  return item;
}

#undef CC_LIST_DEFAULT_SIZE

/* ********************************************************************** */
