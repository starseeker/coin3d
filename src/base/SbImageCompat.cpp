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

#include "SbImageCompat.h"
#include "SbJpegImageHandler.h"
#include <cstring>

// Ensure format handlers are initialized
static void ensure_handlers_initialized() {
  static bool initialized = false;
  if (!initialized) {
    auto& registry = SbImageFormatRegistry::getInstance();
    registry.registerHandler(std::make_unique<SbJpegImageHandler>());
    initialized = true;
  }
}

extern "C" {

int sbimage_wrapper_available(void) {
  ensure_handlers_initialized();
  return 1; // Always available with built-in handlers
}

unsigned char* sbimage_wrapper_read_image(const char* filename, int* w, int* h, int* nc) {
  ensure_handlers_initialized();
  auto& registry = SbImageFormatRegistry::getInstance();
  return registry.readImage(filename, w, h, nc);
}

void sbimage_wrapper_free_image(unsigned char* imagedata) {
  if (imagedata) {
    SbImageFormatRegistry::getInstance().freeImageData(imagedata);
  }
}

int sbimage_wrapper_save_image(const char* filename, const unsigned char* imagedata,
                              int width, int height, int nc, const char* filetypeext) {
  ensure_handlers_initialized();
  auto& registry = SbImageFormatRegistry::getInstance();
  return registry.saveImage(filename, imagedata, width, height, nc) ? 1 : 0;
}

int sbimage_wrapper_check_save_supported(const char* filename) {
  ensure_handlers_initialized();
  auto& registry = SbImageFormatRegistry::getInstance();
  return registry.isSaveSupported(filename) ? 1 : 0;
}

int sbimage_wrapper_get_num_savers(void) {
  ensure_handlers_initialized();
  auto& registry = SbImageFormatRegistry::getInstance();
  return registry.getNumHandlers();
}

void* sbimage_wrapper_get_saver_handle(int idx) {
  ensure_handlers_initialized();
  auto& registry = SbImageFormatRegistry::getInstance();
  return registry.getHandler(idx);
}

const char* sbimage_wrapper_get_saver_extensions(void* handle) {
  if (!handle) return "";
  
  SbImageFormatHandler* handler = static_cast<SbImageFormatHandler*>(handle);
  auto extensions = handler->getExtensions();
  
  // Build comma-separated string - note: this is not thread-safe but matches old API
  static std::string result;
  result.clear();
  for (size_t i = 0; i < extensions.size(); ++i) {
    if (i > 0) result += ",";
    result += extensions[i];
  }
  
  return result.c_str();
}

const char* sbimage_wrapper_get_saver_fullname(void* handle) {
  if (!handle) return "None";
  
  SbImageFormatHandler* handler = static_cast<SbImageFormatHandler*>(handle);
  return handler->getFormatName();
}

const char* sbimage_wrapper_get_saver_description(void* handle) {
  if (!handle) return "Image saving disabled in minimal build";
  
  SbImageFormatHandler* handler = static_cast<SbImageFormatHandler*>(handle);
  return handler->getDescription();
}

void sbimage_wrapper_version(int* major, int* minor, int* micro) {
  // Return version 1.4.0 to satisfy version checks
  if (major) *major = 1;
  if (minor) *minor = 4;
  if (micro) *micro = 0;
}

int sbimage_wrapper_version_matches_at_least(int major, int minor, int micro) {
  // Our compatibility layer supports version checks up to 1.4.0
  if (major < 1) return 1;
  if (major > 1) return 0;
  if (minor < 4) return 1;
  if (minor > 4) return 0;
  return micro <= 0;
}

const char* sbimage_wrapper_get_last_error(void) {
  auto& registry = SbImageFormatRegistry::getInstance();
  return registry.getLastError();
}

unsigned char* sbimage_wrapper_resize(unsigned char* imagedata, int width, int height, int nc,
                                     int newwidth, int newheight) {
  // Resizing not supported in minimal build
  return nullptr;
}

unsigned char* sbimage_wrapper_resize3d(unsigned char* imagedata, int width, int height, int depth, int nc,
                                       int newwidth, int newheight, int newdepth) {
  // 3D resizing not supported in minimal build
  return nullptr;
}

} // extern "C"