#ifndef COIN_SONODEKITPATH_H
#define COIN_SONODEKITPATH_H

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

#include <Inventor/SoPath.h>

class SoNode;
class SoSearchAction;
class SoBaseKit;

class COIN_DLL_API SoNodeKitPath : public SoPath {
  typedef SoPath inherited;

public:
  SoNodeKitPath(const int approxLength = 4);
  virtual ~SoNodeKitPath();

  virtual int getLength(void) const;
  virtual SoNode * getTail(void) const;
  virtual SoNode * getNode(const int idx) const;
  virtual SoNode * getNodeFromTail(const int idx) const;
  virtual void truncate(const int length);
  virtual void pop(void);
  virtual void append(SoNode * const node);
  virtual void append(const SoNodeKitPath * const path);
  void append(SoBaseKit * childKit);
  void append(const int);
  void append(const SoPath *);
  virtual SbBool containsNode(const SoNode * const node) const;
  SbBool containsNode(SoBaseKit * node) const;
  virtual int findFork(const SoPath * const path) const;
  int findFork(const SoNodeKitPath * path) const;

  SbBool containsOnlyNodeKits(void) const;
  
  // Additional methods from implementation
  static void clean(void);
  static SoSearchAction * getSearchAction(void);
  void push(const int);
  int getIndex(const int) const;
  int getIndexFromTail(const int) const;
  void insertIndex(SoNode *, const int);
  void removeIndex(SoNode *, const int);
  void replaceIndex(SoNode *, const int, SoNode *);

private:
  static SoSearchAction * searchAction;
  friend class SoBaseKit;
};

#endif // !COIN_SONODEKITPATH_H