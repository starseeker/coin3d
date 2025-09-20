#ifndef COIN_SBPROFILINGDATA_H
#define COIN_SBPROFILINGDATA_H

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
#include <Inventor/SbTime.h>
#include <Inventor/SoType.h>
#include <Inventor/SbName.h>
#include <Inventor/lists/SbList.h>
#include <cstddef>
#include <cstdint>

class SoFullPath;
class SoPath;
class SoNode;
class SbProfilingDataP;
class SbProfilingData;

// Type definitions for profiling keys
typedef void * SbProfilingNodeKey;
typedef int16_t SbProfilingNodeTypeKey;
typedef const char * SbProfilingNodeNameKey;

// Callback type for profiling data reporting
typedef void SbProfilingDataCB(void * userdata, const SbProfilingData & data, 
                               const SbList<SoNode*> & pointers, 
                               const SbList<int> & indices, int idx);

class COIN_DLL_API SbProfilingData {
public:
  SbProfilingData(void);
  SbProfilingData(const SbProfilingData & rhs);
  ~SbProfilingData(void);

  // Enums for footprint types and node flags
  enum FootprintType {
    MEMORY_SIZE,
    VIDEO_MEMORY_SIZE
  };

  enum NodeFlag {
    GL_CACHED_FLAG,
    CULLED_FLAG
  };

  // Constants for flags
  enum {
    INCLUDE_CHILDREN = 0x01
  };

  // Assignment operators
  SbProfilingData & operator = (const SbProfilingData & rhs);
  SbProfilingData & operator += (const SbProfilingData & rhs);

  // Comparison operators
  int operator == (const SbProfilingData & rhs) const;
  int operator != (const SbProfilingData & rhs) const;

  // Reset and initialization
  void reset(void);

  // Action type management
  void setActionType(SoType actiontype);
  SoType getActionType(void) const;

  // Action timing
  void setActionStartTime(SbTime starttime);
  SbTime getActionStartTime(void) const;
  void setActionStopTime(SbTime stoptime);
  SbTime getActionStopTime(void) const;
  SbTime getActionDuration(void) const;

  // Path/node indexing
  int getIndex(const SoPath * path, SbBool create);
  int getParentIndex(int index) const;

  // Node timing
  void setNodeTiming(const SoPath * path, SbTime timing);
  void setNodeTiming(int index, SbTime timing);
  void preOffsetNodeTiming(int index, SbTime timing);
  SbTime getNodeTiming(const SoPath * path, unsigned int flags = 0) const;
  SbTime getNodeTiming(int index, unsigned int flags = 0) const;

  // Node footprint (memory usage)
  void setNodeFootprint(const SoPath * path, FootprintType footprinttype, size_t footprint);
  void setNodeFootprint(int index, FootprintType footprinttype, size_t footprint);
  size_t getNodeFootprint(const SoPath * path, FootprintType footprinttype, unsigned int flags = 0) const;
  size_t getNodeFootprint(int index, FootprintType footprinttype, unsigned int flags = 0) const;

  // Node flags
  void setNodeFlag(const SoPath * path, NodeFlag flag, SbBool on);
  void setNodeFlag(int index, NodeFlag flag, SbBool on);
  SbBool getNodeFlag(const SoPath * path, NodeFlag flag) const;
  SbBool getNodeFlag(int index, NodeFlag flag) const;

  // Node information
  SoType getNodeType(int index) const;
  SbName getNodeName(int index) const;
  int getLongestNameLength(void) const;
  int getLongestTypeNameLength(void) const;
  int getNumNodeEntries(void) const;

  // Data size and reporting
  size_t getProfilingDataSize(void) const;
  void reportAll(SbProfilingDataCB * callback, void * userdata) const;

  // Statistics by type
  void getStatsForTypesKeyList(SbList<SbProfilingNodeTypeKey> & keys_out) const;
  void getStatsForType(SbProfilingNodeTypeKey type, SbTime & total_out, 
                       SbTime & max_out, uint32_t & count_out) const;

  // Statistics by name
  void getStatsForNamesKeyList(SbList<SbProfilingNodeNameKey> & keys_out) const;
  void getStatsForName(SbProfilingNodeNameKey name, SbTime & total_out,
                       SbTime & max_out, uint32_t & count_out) const;

private:
  // Constructor helper
  void constructorInit(void);

  // Internal methods for path handling
  SbBool isPathMatch(const SoFullPath * fullpath, int pathlen, int idx) const;
  int getIndexCreate(const SoFullPath * fullpath, int pathlen);
  int getIndexNoCreate(const SoPath * path, int pathlen) const;
  int getIndexForwardCreate(const SoFullPath * fullpath, int pathlen, int parentindex);
  int getIndexForwardNoCreate(const SoFullPath * fullpath, int pathlen, int parentindex) const;

  // Private implementation
  SbProfilingDataP * pimpl;

  // Public data members (for compatibility with existing code)
  SoType actionType;
  SbTime actionStartTime;
  SbTime actionStopTime;
};

#endif // !COIN_SBPROFILINGDATA_H