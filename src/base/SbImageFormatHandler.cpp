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

#include "SbImageFormatHandler.h"
#include "SbImageResize.h"
#include <algorithm>
#include <cstring>
#include <cstdlib>

//
// SbImageFormatHandler implementation
//

bool SbImageFormatHandler::canHandleExtension(const std::string& extension) const
{
  auto exts = getExtensions();
  std::string lowerExt = extension;
  std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::tolower);
  
  return std::find(exts.begin(), exts.end(), lowerExt) != exts.end();
}

void SbImageFormatHandler::freeImageData(unsigned char* imagedata)
{
  delete[] imagedata;
}

unsigned char* SbImageFormatHandler::resizeImage(unsigned char* imagedata, int width, int height, int components,
                                               int newwidth, int newheight)
{
  setError("Image resizing not supported by this format handler");
  return nullptr;
}

unsigned char* SbImageFormatHandler::resize3DImage(unsigned char* imagedata, int width, int height, int depth, int components,
                                                 int newwidth, int newheight, int newdepth)
{
  setError("3D image resizing not supported by this format handler");
  return nullptr;
}

void SbImageFormatHandler::getVersion(int* major, int* minor, int* micro) const
{
  if (major) *major = 1;
  if (minor) *minor = 0;
  if (micro) *micro = 0;
}

const char* SbImageFormatHandler::getLastError() const
{
  return lastError.c_str();
}

void SbImageFormatHandler::setError(const std::string& error) const
{
  lastError = error;
}

//
// SbImageFormatRegistry implementation
//

SbImageFormatRegistry& SbImageFormatRegistry::getInstance()
{
  static SbImageFormatRegistry instance;
  return instance;
}

void SbImageFormatRegistry::registerHandler(std::unique_ptr<SbImageFormatHandler> handler)
{
  if (handler) {
    handlers.push_back(std::move(handler));
  }
}

SbImageFormatHandler* SbImageFormatRegistry::getHandlerForExtension(const std::string& extension) const
{
  for (const auto& handler : handlers) {
    if (handler->canHandleExtension(extension)) {
      return handler.get();
    }
  }
  return nullptr;
}

SbImageFormatHandler* SbImageFormatRegistry::getHandlerForFile(const char* filename) const
{
  if (!filename) {
    setError("Null filename provided");
    return nullptr;
  }
  
  std::string ext = getFileExtension(filename);
  if (ext.empty()) {
    setError("No file extension found");
    return nullptr;
  }
  
  return getHandlerForExtension(ext);
}

unsigned char* SbImageFormatRegistry::readImage(const char* filename, int* width, int* height, int* components)
{
  SbImageFormatHandler* handler = getHandlerForFile(filename);
  if (!handler) {
    setError("No handler found for file: " + std::string(filename ? filename : "null"));
    return nullptr;
  }
  
  unsigned char* result = handler->readImage(filename, width, height, components);
  if (!result) {
    setError(std::string("Failed to read image: ") + handler->getLastError());
  }
  return result;
}

bool SbImageFormatRegistry::saveImage(const char* filename, const unsigned char* imagedata,
                                    int width, int height, int components)
{
  SbImageFormatHandler* handler = getHandlerForFile(filename);
  if (!handler) {
    setError("No handler found for file: " + std::string(filename ? filename : "null"));
    return false;
  }
  
  bool result = handler->saveImage(filename, imagedata, width, height, components);
  if (!result) {
    setError(std::string("Failed to save image: ") + handler->getLastError());
  }
  return result;
}

void SbImageFormatRegistry::freeImageData(unsigned char* imagedata)
{
  if (imagedata) {
    // For now, use standard delete[] - individual handlers can override if needed
    delete[] imagedata;
  }
}

unsigned char* SbImageFormatRegistry::resizeImage(unsigned char* imagedata, int width, int height, int components,
                                                 int newwidth, int newheight, bool highQuality)
{
  if (!imagedata) {
    setError("Null image data provided for resizing");
    return nullptr;
  }
  
  // Try to find a handler that supports high-quality resizing
  if (highQuality) {
    for (const auto& handler : handlers) {
      unsigned char* result = handler->resizeImage(imagedata, width, height, components, newwidth, newheight);
      if (result) {
        return result;
      }
    }
  }
  
  // Fall back to built-in resize algorithms
  SbImageResizeFilter quality = highQuality ? SB_IMAGE_RESIZE_HIGH : SB_IMAGE_RESIZE_FAST;
  return SbImageResize_resize2D(imagedata, width, height, components, newwidth, newheight, quality);
}

unsigned char* SbImageFormatRegistry::resize3DImage(unsigned char* imagedata, int width, int height, int depth, int components,
                                                   int newwidth, int newheight, int newdepth, bool highQuality)
{
  if (!imagedata) {
    setError("Null image data provided for 3D resizing");
    return nullptr;
  }
  
  // Try to find a handler that supports high-quality 3D resizing
  if (highQuality) {
    for (const auto& handler : handlers) {
      unsigned char* result = handler->resize3DImage(imagedata, width, height, depth, components, 
                                                    newwidth, newheight, newdepth);
      if (result) {
        return result;
      }
    }
  }
  
  // Fall back to built-in 3D resize algorithms
  SbImageResizeFilter quality = highQuality ? SB_IMAGE_RESIZE_HIGH : SB_IMAGE_RESIZE_FAST;
  return SbImageResize_resize3D(imagedata, width, height, depth, components, 
                               newwidth, newheight, newdepth, quality);
}

bool SbImageFormatRegistry::isExtensionSupported(const std::string& extension) const
{
  return getHandlerForExtension(extension) != nullptr;
}

bool SbImageFormatRegistry::isSaveSupported(const char* filename) const
{
  return getHandlerForFile(filename) != nullptr;
}

std::vector<std::string> SbImageFormatRegistry::getSupportedExtensions() const
{
  std::vector<std::string> allExtensions;
  for (const auto& handler : handlers) {
    auto handlerExts = handler->getExtensions();
    allExtensions.insert(allExtensions.end(), handlerExts.begin(), handlerExts.end());
  }
  
  // Remove duplicates
  std::sort(allExtensions.begin(), allExtensions.end());
  allExtensions.erase(std::unique(allExtensions.begin(), allExtensions.end()), allExtensions.end());
  
  return allExtensions;
}

int SbImageFormatRegistry::getNumHandlers() const
{
  return static_cast<int>(handlers.size());
}

SbImageFormatHandler* SbImageFormatRegistry::getHandler(int index) const
{
  if (index >= 0 && index < static_cast<int>(handlers.size())) {
    return handlers[index].get();
  }
  return nullptr;
}

const char* SbImageFormatRegistry::getLastError() const
{
  return lastError.c_str();
}

std::string SbImageFormatRegistry::getFileExtension(const char* filename) const
{
  if (!filename) return "";
  
  const char* lastDot = strrchr(filename, '.');
  if (!lastDot || lastDot == filename) return "";
  
  std::string ext(lastDot + 1);
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  return ext;
}

void SbImageFormatRegistry::setError(const std::string& error) const
{
  lastError = error;
}
