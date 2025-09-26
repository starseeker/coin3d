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

#include "SbJpegImageHandler.h"
#include <cstdlib>
#include <memory>

// Include TooJPEG implementation
#define TOOJPEG_IMPLEMENTATION
#include "../glue/toojpeg.h"

// Global context pointer for write context
SbJpegImageHandler::JpegWriteContext* SbJpegImageHandler::currentContext = nullptr;

SbJpegImageHandler::SbJpegImageHandler()
{
}

const char* SbJpegImageHandler::getFormatName() const
{
  return "jpeg";
}

const char* SbJpegImageHandler::getDescription() const
{
  return "JPEG image format using TooJPEG library";
}

std::vector<std::string> SbJpegImageHandler::getExtensions() const
{
  return {"jpg", "jpeg"};
}

unsigned char* SbJpegImageHandler::readImage(const char* filename, int* width, int* height, int* components)
{
  // Image reading is not currently supported in the minimal build
  setError("JPEG image reading not supported in minimal build");
  if (width) *width = 0;
  if (height) *height = 0;
  if (components) *components = 0;
  return nullptr;
}

bool SbJpegImageHandler::saveImage(const char* filename, const unsigned char* imagedata,
                                  int width, int height, int components)
{
  if (!filename || !imagedata || width <= 0 || height <= 0 || components <= 0) {
    setError("Invalid parameters for JPEG save");
    return false;
  }
  
  FILE* file = fopen(filename, "wb");
  if (!file) {
    setError(std::string("Cannot open file for writing: ") + filename);
    return false;
  }
  
  bool success = false;
  JpegWriteContext context;
  context.file = file;
  context.error = false;
  
  // Set the context for this thread
  currentContext = &context;
  
  try {
    if (components == 4) {
      // Convert RGBA to RGB by discarding alpha channel
      std::unique_ptr<unsigned char[]> rgbData(new unsigned char[width * height * 3]);
      
      for (int i = 0; i < width * height; i++) {
        rgbData[i * 3] = imagedata[i * 4];
        rgbData[i * 3 + 1] = imagedata[i * 4 + 1];
        rgbData[i * 3 + 2] = imagedata[i * 4 + 2];
      }
      
      success = TooJpeg::writeJpeg(writeCallback, rgbData.get(), width, height, true, 90);
    } else {
      // Use data directly (RGB or grayscale)
      bool isRGB = (components >= 3);
      success = TooJpeg::writeJpeg(writeCallback, imagedata, width, height, isRGB, 90);
    }
    
    if (context.error) {
      success = false;
    }
  }
  catch (const std::exception& e) {
    setError(std::string("Exception during JPEG encoding: ") + e.what());
    success = false;
  }
  catch (...) {
    setError("Unknown exception during JPEG encoding");
    success = false;
  }
  
  // Clean up
  currentContext = nullptr;
  fclose(file);
  
  if (!success) {
    if (lastError.empty()) {
      setError("JPEG encoding failed");
    }
  }
  
  return success;
}

void SbJpegImageHandler::getVersion(int* major, int* minor, int* micro) const
{
  // Version based on TooJPEG (simplified)
  if (major) *major = 1;
  if (minor) *minor = 4;
  if (micro) *micro = 0;
}

void SbJpegImageHandler::writeCallback(unsigned char byte)
{
  if (currentContext && currentContext->file && !currentContext->error) {
    if (fputc(byte, currentContext->file) == EOF) {
      currentContext->error = true;
    }
  }
}