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

#include "base/heap.h"
#include <string>

#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <unordered_map>
#include <algorithm>

#include "base/dict.h"
#include "base/heapp.h"
#include "coindefs.h"

// Modern STL-based implementation that maintains C interface compatibility
struct cc_heap {
  std::vector<void*> array;
  cc_heap_compare_cb * compare;
  void * compareclosure;
  bool support_remove;
  std::unordered_map<void*, size_t> hash_map; // STL replacement for cc_dict when support_remove is true
  
  // Constructor
  cc_heap(unsigned int initial_size, cc_heap_compare_cb * comparecb, bool support_rem)
    : compare(comparecb), compareclosure(nullptr), support_remove(support_rem) {
    array.reserve(initial_size);
    if (support_remove) {
      hash_map.reserve(initial_size);
    }
  }
};

/* ********************************************************************** */

/*!
  \typedef int cc_heap_compare_cb(void * o1, void * o2)

  A type definition for heap compare callback function.
  The heap compare function yields true if the first argument of the call
  appears before the second in the strict weak ordering relation 
  induced by this type, and false otherwise, i.e. cc_heap_compare_cb(x, x) == false
*/

/*! 
  \typedef struct cc_heap cc_heap

  A type definition for the cc_heap structure
*/

/* ********************************************************************** */
/* private functions */

#define HEAP_PARENT(i) (((i) - 1) / 2)
#define HEAP_LEFT(i) ((i) * 2 + 1)
#define HEAP_RIGHT(i) ((i) * 2 + 2)

static void
heap_heapify_down(cc_heap * h, size_t i)
{
  size_t largest = i;
  size_t size = h->array.size();

  while (true) {
    i = largest;
    size_t left = HEAP_LEFT(i);
    size_t right = HEAP_RIGHT(i);

    if (left < size && h->compare(h->array[left], h->array[largest]) > 0) {
      largest = left;
    }
    
    if (right < size && h->compare(h->array[right], h->array[largest]) > 0) {
      largest = right;
    }

    if (largest == i) break;

    std::swap(h->array[i], h->array[largest]);
    
    if (h->support_remove) {
      h->hash_map[h->array[i]] = i;
      h->hash_map[h->array[largest]] = largest;
    }
  }
}

static void
heap_heapify_up(cc_heap * h, size_t i)
{
  while (i > 0) {
    size_t parent = HEAP_PARENT(i);
    if (h->compare(h->array[i], h->array[parent]) <= 0) {
      break;
    }
    
    std::swap(h->array[i], h->array[parent]);
    
    if (h->support_remove) {
      h->hash_map[h->array[i]] = i;
      h->hash_map[h->array[parent]] = parent;
    }
    
    i = parent;
  }
}

/* ********************************************************************** */
/* public api */

/*!

  Construct a heap. \a size is the initial array size.

  For a minimum heap \a comparecb should return 1 if the first element
  is less than the second, zero if they are equal or the first element
  is greater than the second.
  For a maximum heap \a comparecb should return 1 if the first element
  is greater than the second, zero if they are equal or the first element
  is less than the second.

  \a support_remove specifies if the heap should support removal of
  elements (other than the top element) after they are added; this
  requires use of a hash table to be efficient, but as a slight runtime
  overhead will be incurred for the add and extract_top functions the
  support can be disabled if you don't need it.

*/

cc_heap *
cc_heap_construct(unsigned int size,
                  cc_heap_compare_cb * comparecb,
                  SbBool support_remove)
{
  try {
    return new cc_heap(size, comparecb, support_remove != FALSE);
  } catch (const std::bad_alloc&) {
    return nullptr;
  }
}

/*!
  Destruct the heap \a h.
*/
void
cc_heap_destruct(cc_heap * h)
{
  delete h;
}

/*!
  Clear/remove all elements in the heap \a h.
*/
void cc_heap_clear(cc_heap * h)
{
  if (!h) return;
  h->array.clear();
  if (h->support_remove) {
    h->hash_map.clear();
  }
}

/*!
  Add the element \a o to the heap \a h.
*/
void
cc_heap_add(cc_heap * h, void * o)
{
  if (!h) return;
  
  size_t i = h->array.size();
  h->array.push_back(o);
  
  if (h->support_remove) {
    h->hash_map[o] = i;
  }

  heap_heapify_up(h, i);
}

/*!
  Returns the top element from the heap \a h. If the heap is empty,
  NULL is returned.
*/
void *
cc_heap_get_top(cc_heap * h)
{
  if (!h || h->array.empty()) return nullptr;
  return h->array[0];
}

/*!
  Returns and removes the top element from the heap \a h. If the
  heap is empty, NULL is returned.
*/
void *
cc_heap_extract_top(cc_heap * h)
{
  if (!h || h->array.empty()) return nullptr;

  void * top = h->array[0];
  h->array[0] = h->array.back();
  h->array.pop_back();

  if (h->support_remove) {
    if (!h->array.empty()) {
      h->hash_map[h->array[0]] = 0;
    }
    h->hash_map.erase(top);
  }

  if (!h->array.empty()) {
    heap_heapify_down(h, 0);
  }

  return top;
}

/*!
  Remove \a o from the heap \a h; if present TRUE is returned,
  otherwise FALSE.  Please note that the heap must have been created
  with support_remove.
*/
int
cc_heap_remove(cc_heap * h, void * o)
{
  if (!h || !h->support_remove) return FALSE;

  auto it = h->hash_map.find(o);
  if (it == h->hash_map.end()) {
    return FALSE;
  }

  size_t i = it->second;
  assert(i < h->array.size());
  assert(h->array[i] == o);

  // Replace with last element and remove last
  h->array[i] = h->array.back();
  h->array.pop_back();
  
  // Update hash map
  h->hash_map.erase(it);
  if (i < h->array.size()) {
    h->hash_map[h->array[i]] = i;
    // Restore heap property
    heap_heapify_down(h, i);
  }

  return TRUE;
}

/*!
  Updates the heap \a h for new value of existent key \a o; if key is present TRUE is returned,
  otherwise FALSE.
*/
int
cc_heap_update(cc_heap * h, void * o)
{
  if (!h || !h->support_remove) return FALSE;
  
  auto it = h->hash_map.find(o);
  if (it == h->hash_map.end()) {
    return FALSE;
  }

  size_t i = it->second;
  assert(i < h->array.size());
  assert(h->array[i] == o);

  // Try to heapify up first, then down
  if (i > 0 && h->compare(h->array[i], h->array[HEAP_PARENT(i)]) > 0) {
    heap_heapify_up(h, i);
  }
  else {
    heap_heapify_down(h, i);
  }

  return TRUE;
}

/*!
  Returns the number of elements in the heap \a h.
*/
unsigned int
cc_heap_elements(cc_heap * h)
{
  if (!h) return 0;
  return static_cast<unsigned int>(h->array.size());
}

/*!
  Returns TRUE of the heap \a h is empty, otherwise FALSE.
*/
SbBool
cc_heap_empty(cc_heap * h)
{
  if (!h) return TRUE;
  return h->array.empty() ? TRUE : FALSE;
}

/*!
  Print heap \a h using a specified callback \a printcb.
*/
void
cc_heap_print(cc_heap * h, cc_heap_print_cb * printcb, SbString& str, SbBool printLeveled/* = FALSE*/)
{
  if (!h) return;
  
  size_t elements = h->array.size();
  if (!printLeveled) {
    for (size_t i = 0; i < elements; ++i) {
      printcb(h->array[i], str);
      str += ' ';
    }
  }
  else {
    unsigned int level = 0;
    unsigned int level_items = 1;
    unsigned int printed_items = 0;
    for (size_t i = 0; i < elements; ++i)
    {
      if (printed_items == 0 ) {
        SbString level_str;
        level_str.sprintf("\nlevel #%d : ", level);
        str += level_str;
      }

      printcb(h->array[i], str);
      str += ' ';
      ++printed_items;

      if (printed_items == level_items)
      {
        ++level;
        level_items *= 2; // next level has at most twice as many items
        printed_items = 0;
      }
    }
    str += '\n';
  }
}

#undef HEAP_LEFT
#undef HEAP_PARENT
#undef HEAP_RIGHT

