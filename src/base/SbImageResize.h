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
  high-quality and fast resizing algorithms, with support for both simple
  quality levels and precise filter control.
  
  The API offers two approaches:
  - Quality-based: SB_IMAGE_RESIZE_FAST, SB_IMAGE_RESIZE_BILINEAR, SB_IMAGE_RESIZE_HIGH
  - Filter-specific: Bell, B-spline, Lanczos3, Mitchell filters from original simage library
*/

enum SbImageResizeQuality {
  SB_IMAGE_RESIZE_FAST,      // Fast, lower quality resize (good for interactive use)
  SB_IMAGE_RESIZE_BILINEAR,  // Bilinear interpolation (good balance)
  SB_IMAGE_RESIZE_HIGH       // High quality resize (uses Bell filter by default)
};

/*!
  \brief Filter types for high-quality image resizing.
  
  These filters provide different characteristics for image resizing:
  - Bell: Excellent balance of quality and performance (default for high quality)
  - B-spline: Smooth results, good for photographic content
  - Lanczos3: Sharp results, excellent for preserving fine details
  - Mitchell: Good general-purpose filter with balanced sharpness
*/
enum SbImageResizeFilter {
  SB_IMAGE_RESIZE_FILTER_BELL,      // Bell filter (default for high quality)
  SB_IMAGE_RESIZE_FILTER_B_SPLINE,  // B-spline filter  
  SB_IMAGE_RESIZE_FILTER_LANCZOS3,  // Lanczos3 filter
  SB_IMAGE_RESIZE_FILTER_MITCHELL   // Mitchell filter
};

/*!
  \brief Resize a 2D image using the specified quality setting.
  
  This function provides backward-compatible quality-based resizing.
  For precise filter control, use SbImageResize_resize2D_filter() instead.
  
  Quality mapping:
  - SB_IMAGE_RESIZE_FAST: Nearest-neighbor scaling
  - SB_IMAGE_RESIZE_BILINEAR: Bilinear interpolation  
  - SB_IMAGE_RESIZE_HIGH: Bell filter (same as SB_IMAGE_RESIZE_FILTER_BELL)
  
  \param src Source image data
  \param width Source image width
  \param height Source image height
  \param components Number of components per pixel (1=grayscale, 3=RGB, 4=RGBA)
  \param newwidth Target image width
  \param newheight Target image height
  \param quality Resize quality setting
  \return Newly allocated resized image data, or NULL on failure. Caller must free with delete[].
*/
unsigned char* SbImageResize_resize2D(const unsigned char* src,
                                     int width, int height, int components,
                                     int newwidth, int newheight,
                                     SbImageResizeQuality quality = SB_IMAGE_RESIZE_HIGH);

/*!
  \brief Resize a 3D image (volume) using the specified quality setting.
  
  \param src Source image data
  \param width Source image width
  \param height Source image height
  \param depth Source image depth
  \param components Number of components per pixel
  \param newwidth Target image width
  \param newheight Target image height
  \param newdepth Target image depth
  \param quality Resize quality setting
  \return Newly allocated resized image data, or NULL on failure. Caller must free with delete[].
*/
unsigned char* SbImageResize_resize3D(const unsigned char* src,
                                     int width, int height, int depth, int components,
                                     int newwidth, int newheight, int newdepth,
                                     SbImageResizeQuality quality = SB_IMAGE_RESIZE_HIGH);

/*!
  \brief In-place resize a 2D image into pre-allocated destination buffer.
  
  \param src Source image data
  \param dest Destination buffer (must be pre-allocated to newwidth*newheight*components bytes)
  \param width Source image width
  \param height Source image height
  \param components Number of components per pixel
  \param newwidth Target image width
  \param newheight Target image height
  \param quality Resize quality setting
  \return true on success, false on failure
*/
bool SbImageResize_resize2D_inplace(const unsigned char* src, unsigned char* dest,
                                   int width, int height, int components,
                                   int newwidth, int newheight,
                                   SbImageResizeQuality quality = SB_IMAGE_RESIZE_HIGH);

/*!
  \brief Resize a 2D image using a specific filter type.
  
  This function allows precise control over the filtering algorithm used for resizing.
  Different filters have different characteristics - see SbImageResizeFilter for details.
  
  \param src Source image data
  \param width Source image width
  \param height Source image height
  \param components Number of components per pixel (1=grayscale, 3=RGB, 4=RGBA)
  \param newwidth Target image width
  \param newheight Target image height
  \param filter Filter type to use for resizing
  \return Newly allocated resized image data, or NULL on failure. Caller must free with delete[].
*/
unsigned char* SbImageResize_resize2D_filter(const unsigned char* src,
                                            int width, int height, int components,
                                            int newwidth, int newheight,
                                            SbImageResizeFilter filter);

/*!
  \brief In-place resize a 2D image using a specific filter type.
  
  This function allows precise control over the filtering algorithm used for resizing.
  
  \param src Source image data
  \param dest Destination buffer (must be pre-allocated to newwidth*newheight*components bytes)
  \param width Source image width
  \param height Source image height
  \param components Number of components per pixel
  \param newwidth Target image width
  \param newheight Target image height
  \param filter Filter type to use for resizing
  \return true on success, false on failure
*/
bool SbImageResize_resize2D_inplace_filter(const unsigned char* src, unsigned char* dest,
                                          int width, int height, int components,
                                          int newwidth, int newheight,
                                          SbImageResizeFilter filter);

#endif // COIN_SBIMAGERESIZE_H