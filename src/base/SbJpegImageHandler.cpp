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
#include <limits>
#include <memory>

// stb_image is used only as a JPEG decoder here.  Restricting the compiled
// implementation avoids carrying its unrelated format decoders into Obol.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wcast-qual"
#pragma clang diagnostic ignored "-Wunused-function"
#pragma clang diagnostic ignored "-Wunused-parameter"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#define STBI_ONLY_JPEG
#define STB_IMAGE_IMPLEMENTATION
#include "../../external/stb/stb_image.h"
#undef STB_IMAGE_IMPLEMENTATION
#undef STBI_ONLY_JPEG
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

// Include TooJPEG implementation
#define TOOJPEG_IMPLEMENTATION
#include "../glue/toojpeg.h"

// Global context pointer for write context
thread_local SbJpegImageHandler::JpegWriteContext *
SbJpegImageHandler::currentContext = nullptr;

SbJpegImageHandler::SbJpegImageHandler()
{
}

const char* SbJpegImageHandler::getFormatName() const
{
  return "jpeg";
}

const char* SbJpegImageHandler::getDescription() const
{
  return "JPEG image format using stb_image and TooJPEG";
}

std::vector<std::string> SbJpegImageHandler::getExtensions() const
{
  return {"jpg", "jpeg"};
}

unsigned char* SbJpegImageHandler::readImage(const char* filename, int* width, int* height, int* components)
{
  setError("");
  if (width) *width = 0;
  if (height) *height = 0;
  if (components) *components = 0;
  if (!filename) {
    setError("Null filename provided for JPEG read");
    return nullptr;
  }

  int imagewidth = 0;
  int imageheight = 0;
  int imagecomponents = 0;
  unsigned char * data = stbi_load(filename, &imagewidth, &imageheight,
                                   &imagecomponents, 0);
  if (!data) {
    const char * reason = stbi_failure_reason();
    setError(reason ? reason : "JPEG decoding failed");
    return nullptr;
  }
  if (imagewidth <= 0 || imageheight <= 0 ||
      imagecomponents < 1 || imagecomponents > 4) {
    stbi_image_free(data);
    setError("JPEG decoder returned invalid image dimensions or components");
    return nullptr;
  }

  if (width) *width = imagewidth;
  if (height) *height = imageheight;
  if (components) *components = imagecomponents;
  return data;
}

bool SbJpegImageHandler::saveImage(const char* filename, const unsigned char* imagedata,
                                  int width, int height, int components)
{
  setError("");
  if (!filename || !imagedata || width <= 0 || height <= 0 ||
      components < 1 || components > 4) {
    setError("Invalid parameters for JPEG save");
    return false;
  }

  const size_t pixelcount = static_cast<size_t>(width) *
                            static_cast<size_t>(height);
  if (pixelcount > std::numeric_limits<size_t>::max() / 3u) {
    setError("JPEG image dimensions are too large");
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
      // Convert RGBA to RGB by discarding alpha channel.
      std::unique_ptr<unsigned char[]> rgbData(new unsigned char[pixelcount * 3u]);
      
      for (size_t i = 0; i < pixelcount; i++) {
        rgbData[i * 3] = imagedata[i * 4];
        rgbData[i * 3 + 1] = imagedata[i * 4 + 1];
        rgbData[i * 3 + 2] = imagedata[i * 4 + 2];
      }
      
      success = TooJpeg::writeJpeg(writeCallback, rgbData.get(), width, height, true, 90);
    } else if (components == 2) {
      // TooJPEG accepts one-byte grayscale or three-byte RGB input.  Strip
      // the alpha byte from grayscale-alpha images instead of accidentally
      // treating the interleaved alpha bytes as neighboring pixels.
      std::unique_ptr<unsigned char[]> grayData(new unsigned char[pixelcount]);
      for (size_t i = 0; i < pixelcount; ++i) grayData[i] = imagedata[i * 2];
      success = TooJpeg::writeJpeg(writeCallback, grayData.get(), width, height, false, 90);
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
    if (getLastError()[0] == '\0') {
      setError("JPEG encoding failed");
    }
  }
  
  return success;
}

void
SbJpegImageHandler::freeImageData(unsigned char * imagedata)
{
  stbi_image_free(imagedata);
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
