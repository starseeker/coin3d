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

#ifndef COIN_SBJPEGIMAGEHANDLER_H
#define COIN_SBJPEGIMAGEHANDLER_H

#include "SbImageFormatHandler.h"
#include <cstdio>

/*!
  \class SbJpegImageHandler SbJpegImageHandler.h
  \brief JPEG image format handler using TooJPEG library.
  
  This handler provides JPEG image saving capability using the embedded
  TooJPEG library. Reading is not currently supported (returns null).
*/
class SbJpegImageHandler : public SbImageFormatHandler {
public:
  SbJpegImageHandler();
  virtual ~SbJpegImageHandler() = default;
  
  // Format identification
  const char* getFormatName() const override;
  const char* getDescription() const override;
  std::vector<std::string> getExtensions() const override;
  
  // Image I/O operations
  unsigned char* readImage(const char* filename, int* width, int* height, int* components) override;
  bool saveImage(const char* filename, const unsigned char* imagedata,
                int width, int height, int components) override;
  
  // Version info
  void getVersion(int* major, int* minor, int* micro) const override;

private:
  // JPEG writing context for TooJPEG callback
  struct JpegWriteContext {
    FILE* file;
    bool error;
    
    JpegWriteContext() : file(nullptr), error(false) {}
  };
  
  // TooJPEG callback function
  static void writeCallback(unsigned char byte);
  
  // Global context pointer for write context (not thread-safe but compatible with existing code)
  static JpegWriteContext* currentContext;
};

#endif // COIN_SBJPEGIMAGEHANDLER_H