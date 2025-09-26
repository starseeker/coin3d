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

#ifndef COIN_SBIMAGERESIZE_H
#define COIN_SBIMAGERESIZE_H

/*!
  \file SbImageResize.h
  \brief Image resizing utilities for the Coin3D image format system.
  
  This header provides centralized image resizing capabilities that can be
  used by format handlers and other parts of the system. It provides both
  high-quality and fast resizing algorithms with unified filter control.
*/

/*!
  \brief Unified resize filter/quality options for image resizing.
  
  This enum combines both legacy quality levels and specific filter types
  into a single unified interface. Legacy quality options are maintained
  for backward compatibility:
  
  **Legacy Quality Options:**
  - SB_IMAGE_RESIZE_FAST: Fast nearest-neighbor scaling
  - SB_IMAGE_RESIZE_BILINEAR: Bilinear interpolation
  - SB_IMAGE_RESIZE_HIGH: High quality using Bell filter (same as SB_IMAGE_RESIZE_FILTER_BELL)
  
  **Specific Filter Types:**
  - SB_IMAGE_RESIZE_FILTER_BELL: Excellent balance of quality and performance  
  - SB_IMAGE_RESIZE_FILTER_B_SPLINE: Smooth results, good for photographic content
  - SB_IMAGE_RESIZE_FILTER_LANCZOS3: Sharp results, excellent for preserving fine details
  - SB_IMAGE_RESIZE_FILTER_MITCHELL: Good general-purpose filter with balanced sharpness
*/
enum SbImageResizeFilter {
  // Legacy quality options for backward compatibility
  SB_IMAGE_RESIZE_FAST = 0,      // Fast, lower quality resize (good for interactive use)
  SB_IMAGE_RESIZE_BILINEAR = 1,  // Bilinear interpolation (good balance)
  SB_IMAGE_RESIZE_HIGH = 2,      // High quality resize (uses Bell filter)
  
  // Specific filter types from original simage library  
  SB_IMAGE_RESIZE_FILTER_BELL = 2,      // Bell filter (same as SB_IMAGE_RESIZE_HIGH)
  SB_IMAGE_RESIZE_FILTER_B_SPLINE = 3,  // B-spline filter  
  SB_IMAGE_RESIZE_FILTER_LANCZOS3 = 4,  // Lanczos3 filter
  SB_IMAGE_RESIZE_FILTER_MITCHELL = 5   // Mitchell filter
};

/*!
  \brief Resize a 2D image using the specified filter/quality setting.
  
  This function supports both legacy quality options and specific filter types
  through the unified SbImageResizeFilter enum.
  
  \param src Source image data
  \param width Source image width
  \param height Source image height
  \param components Number of components per pixel (1=grayscale, 3=RGB, 4=RGBA)
  \param newwidth Target image width
  \param newheight Target image height
  \param filter Filter/quality setting to use for resizing
  \return Newly allocated resized image data, or NULL on failure. Caller must free with delete[].
*/
unsigned char* SbImageResize_resize2D(const unsigned char* src,
                                     int width, int height, int components,
                                     int newwidth, int newheight,
                                     SbImageResizeFilter filter = SB_IMAGE_RESIZE_HIGH);

/*!
  \brief Resize a 3D image (volume) using the specified filter/quality setting.
  
  \param src Source image data
  \param width Source image width
  \param height Source image height
  \param depth Source image depth
  \param components Number of components per pixel
  \param newwidth Target image width
  \param newheight Target image height
  \param newdepth Target image depth
  \param filter Filter/quality setting to use for resizing
  \return Newly allocated resized image data, or NULL on failure. Caller must free with delete[].
*/
unsigned char* SbImageResize_resize3D(const unsigned char* src,
                                     int width, int height, int depth, int components,
                                     int newwidth, int newheight, int newdepth,
                                     SbImageResizeFilter filter = SB_IMAGE_RESIZE_HIGH);

/*!
  \brief In-place resize a 2D image into pre-allocated destination buffer.
  
  \param src Source image data
  \param dest Destination buffer (must be pre-allocated to newwidth*newheight*components bytes)
  \param width Source image width
  \param height Source image height
  \param components Number of components per pixel
  \param newwidth Target image width
  \param newheight Target image height
  \param filter Filter/quality setting to use for resizing
  \return true on success, false on failure
*/
bool SbImageResize_resize2D_inplace(const unsigned char* src, unsigned char* dest,
                                   int width, int height, int components,
                                   int newwidth, int newheight,
                                   SbImageResizeFilter filter = SB_IMAGE_RESIZE_HIGH);

#endif // COIN_SBIMAGERESIZE_H
