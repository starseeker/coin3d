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

#include "SbImageResize.h"
#include <cstring>
#include <algorithm>

// Fast resize implementation (extracted from SoGLImage.cpp)
static void fast_resize_2d(const unsigned char* src, unsigned char* dest,
                          int width, int height, int components,
                          int newwidth, int newheight)
{
  // Simple nearest-neighbor scaling for speed
  float x_ratio = (float)width / newwidth;
  float y_ratio = (float)height / newheight;
  
  for (int y = 0; y < newheight; y++) {
    int src_y = (int)(y * y_ratio);
    if (src_y >= height) src_y = height - 1;
    
    for (int x = 0; x < newwidth; x++) {
      int src_x = (int)(x * x_ratio);
      if (src_x >= width) src_x = width - 1;
      
      int src_idx = (src_y * width + src_x) * components;
      int dest_idx = (y * newwidth + x) * components;
      
      for (int c = 0; c < components; c++) {
        dest[dest_idx + c] = src[src_idx + c];
      }
    }
  }
}

// Bilinear resize implementation (extracted and enhanced from SoImage.cpp)
static void bilinear_resize_2d(const unsigned char* src, unsigned char* dest,
                              int src_width, int src_height, int components,
                              int dest_width, int dest_height)
{
  float x_ratio = ((float)(src_width - 1)) / dest_width;
  float y_ratio = ((float)(src_height - 1)) / dest_height;
  
  for (int i = 0; i < dest_height; i++) {
    for (int j = 0; j < dest_width; j++) {
      float x_l = j * x_ratio;
      float y_l = i * y_ratio;
      int x = (int)x_l;
      int y = (int)y_l;
      float x_diff = x_l - x;
      float y_diff = y_l - y;
      
      int x1 = std::min(x + 1, src_width - 1);
      int y1 = std::min(y + 1, src_height - 1);
      
      for (int c = 0; c < components; c++) {
        int index = (y * src_width + x) * components + c;
        int index_x = (y * src_width + x1) * components + c;
        int index_y = (y1 * src_width + x) * components + c;
        int index_xy = (y1 * src_width + x1) * components + c;
        
        float A = src[index] * (1 - x_diff) * (1 - y_diff);
        float B = src[index_x] * x_diff * (1 - y_diff);
        float C = src[index_y] * (1 - x_diff) * y_diff;
        float D = src[index_xy] * x_diff * y_diff;
        
        dest[(i * dest_width + j) * components + c] = (unsigned char)(A + B + C + D);
      }
    }
  }
}

// Fast 3D resize implementation (extracted from SoGLImage.cpp)
static void fast_resize_3d(const unsigned char* src, unsigned char* dest,
                          int width, int height, int depth, int components,
                          int newwidth, int newheight, int newdepth)
{
  // Simple nearest-neighbor scaling for speed
  float x_ratio = (float)width / newwidth;
  float y_ratio = (float)height / newheight;
  float z_ratio = (float)depth / newdepth;
  
  for (int z = 0; z < newdepth; z++) {
    int src_z = (int)(z * z_ratio);
    if (src_z >= depth) src_z = depth - 1;
    
    for (int y = 0; y < newheight; y++) {
      int src_y = (int)(y * y_ratio);
      if (src_y >= height) src_y = height - 1;
      
      for (int x = 0; x < newwidth; x++) {
        int src_x = (int)(x * x_ratio);
        if (src_x >= width) src_x = width - 1;
        
        int src_idx = ((src_z * height + src_y) * width + src_x) * components;
        int dest_idx = ((z * newheight + y) * newwidth + x) * components;
        
        for (int c = 0; c < components; c++) {
          dest[dest_idx + c] = src[src_idx + c];
        }
      }
    }
  }
}

// High-quality resize placeholder (currently falls back to bilinear)
// TODO: Implement advanced algorithms like bicubic, Lanczos, etc.
static void high_quality_resize_2d(const unsigned char* src, unsigned char* dest,
                                  int width, int height, int components,
                                  int newwidth, int newheight)
{
  // For now, use bilinear interpolation as high quality
  // Future enhancement: implement bicubic or Lanczos algorithms
  bilinear_resize_2d(src, dest, width, height, components, newwidth, newheight);
}

static void high_quality_resize_3d(const unsigned char* src, unsigned char* dest,
                                  int width, int height, int depth, int components,
                                  int newwidth, int newheight, int newdepth)
{
  // For now, fall back to fast resize for 3D
  // Future enhancement: implement trilinear or higher quality 3D algorithms
  fast_resize_3d(src, dest, width, height, depth, components, newwidth, newheight, newdepth);
}

// Public API implementations

unsigned char* SbImageResize_resize2D(const unsigned char* src,
                                     int width, int height, int components,
                                     int newwidth, int newheight,
                                     SbImageResizeQuality quality)
{
  if (!src || width <= 0 || height <= 0 || components <= 0 || 
      newwidth <= 0 || newheight <= 0) {
    return nullptr;
  }
  
  size_t dest_size = (size_t)newwidth * newheight * components;
  unsigned char* dest = new unsigned char[dest_size];
  
  if (!SbImageResize_resize2D_inplace(src, dest, width, height, components, 
                                     newwidth, newheight, quality)) {
    delete[] dest;
    return nullptr;
  }
  
  return dest;
}

unsigned char* SbImageResize_resize3D(const unsigned char* src,
                                     int width, int height, int depth, int components,
                                     int newwidth, int newheight, int newdepth,
                                     SbImageResizeQuality quality)
{
  if (!src || width <= 0 || height <= 0 || depth <= 0 || components <= 0 || 
      newwidth <= 0 || newheight <= 0 || newdepth <= 0) {
    return nullptr;
  }
  
  size_t dest_size = (size_t)newwidth * newheight * newdepth * components;
  unsigned char* dest = new unsigned char[dest_size];
  
  switch (quality) {
    case SB_IMAGE_RESIZE_FAST:
      fast_resize_3d(src, dest, width, height, depth, components, 
                    newwidth, newheight, newdepth);
      break;
    case SB_IMAGE_RESIZE_BILINEAR:
    case SB_IMAGE_RESIZE_HIGH:
      high_quality_resize_3d(src, dest, width, height, depth, components, 
                            newwidth, newheight, newdepth);
      break;
  }
  
  return dest;
}

bool SbImageResize_resize2D_inplace(const unsigned char* src, unsigned char* dest,
                                   int width, int height, int components,
                                   int newwidth, int newheight,
                                   SbImageResizeQuality quality)
{
  if (!src || !dest || width <= 0 || height <= 0 || components <= 0 || 
      newwidth <= 0 || newheight <= 0) {
    return false;
  }
  
  switch (quality) {
    case SB_IMAGE_RESIZE_FAST:
      fast_resize_2d(src, dest, width, height, components, newwidth, newheight);
      break;
    case SB_IMAGE_RESIZE_BILINEAR:
      bilinear_resize_2d(src, dest, width, height, components, newwidth, newheight);
      break;
    case SB_IMAGE_RESIZE_HIGH:
      high_quality_resize_2d(src, dest, width, height, components, newwidth, newheight);
      break;
  }
  
  return true;
}