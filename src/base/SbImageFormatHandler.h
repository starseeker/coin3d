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

#ifndef COIN_SBIMAGEFORMATHANDLER_H
#define COIN_SBIMAGEFORMATHANDLER_H

#include <string>
#include <vector>
#include <memory>

// Forward declarations
class SbString;

/*!
  \class SbImageFormatHandler SbImageFormatHandler.h
  \brief Abstract base class for image format handlers.
  
  This class provides a clean C++ interface for handling different image formats.
  Each format (JPEG, PNG, etc.) should implement this interface.
*/
class SbImageFormatHandler {
public:
  virtual ~SbImageFormatHandler() = default;
  
  // Format identification
  virtual const char* getFormatName() const = 0;
  virtual const char* getDescription() const = 0;
  virtual std::vector<std::string> getExtensions() const = 0;
  virtual bool canHandleExtension(const std::string& extension) const;
  
  // Image I/O operations
  virtual unsigned char* readImage(const char* filename, int* width, int* height, int* components) = 0;
  virtual bool saveImage(const char* filename, const unsigned char* imagedata,
                        int width, int height, int components) = 0;
  
  // Memory management
  virtual void freeImageData(unsigned char* imagedata);
  
  // Optional operations (default implementations do nothing/return null)
  virtual unsigned char* resizeImage(unsigned char* imagedata, int width, int height, int components,
                                   int newwidth, int newheight);
  virtual unsigned char* resize3DImage(unsigned char* imagedata, int width, int height, int depth, int components,
                                     int newwidth, int newheight, int newdepth);
  
  // Version info
  virtual void getVersion(int* major, int* minor, int* micro) const;
  
  // Error handling
  virtual const char* getLastError() const;

protected:
  mutable std::string lastError;
  void setError(const std::string& error) const;
};

/*!
  \class SbImageFormatRegistry SbImageFormatHandler.h  
  \brief Registry for managing image format handlers.
  
  This singleton class manages all available image format handlers and provides
  a unified interface for image operations.
*/
class SbImageFormatRegistry {
public:
  static SbImageFormatRegistry& getInstance();
  
  // Registry management
  void registerHandler(std::unique_ptr<SbImageFormatHandler> handler);
  SbImageFormatHandler* getHandlerForExtension(const std::string& extension) const;
  SbImageFormatHandler* getHandlerForFile(const char* filename) const;
  
  // Unified interface operations
  unsigned char* readImage(const char* filename, int* width, int* height, int* components);
  bool saveImage(const char* filename, const unsigned char* imagedata,
                int width, int height, int components);
  void freeImageData(unsigned char* imagedata);
  
  // Image resize operations
  unsigned char* resizeImage(unsigned char* imagedata, int width, int height, int components,
                           int newwidth, int newheight, bool highQuality = true);
  unsigned char* resize3DImage(unsigned char* imagedata, int width, int height, int depth, int components,
                             int newwidth, int newheight, int newdepth, bool highQuality = true);
  
  // Format capability queries
  bool isExtensionSupported(const std::string& extension) const;
  bool isSaveSupported(const char* filename) const;
  std::vector<std::string> getSupportedExtensions() const;
  
  // Handler enumeration (for compatibility with old simage API)
  int getNumHandlers() const;
  SbImageFormatHandler* getHandler(int index) const;
  
  // Error handling
  const char* getLastError() const;

private:
  SbImageFormatRegistry() = default;
  ~SbImageFormatRegistry() = default;
  SbImageFormatRegistry(const SbImageFormatRegistry&) = delete;
  SbImageFormatRegistry& operator=(const SbImageFormatRegistry&) = delete;
  
  std::vector<std::unique_ptr<SbImageFormatHandler>> handlers;
  mutable std::string lastError;
  
  std::string getFileExtension(const char* filename) const;
  void setError(const std::string& error) const;
};

#endif // COIN_SBIMAGEFORMATHANDLER_H