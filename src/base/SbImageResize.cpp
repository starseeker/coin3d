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

/*
 * This file implements high-quality image resizing algorithms.
 * The high-quality resize implementation incorporates sophisticated filtering
 * algorithms from the original simage library's resize.c, including:
 * - Bell filter (default for high quality)
 * - B-spline filter  
 * - Lanczos3 filter
 * - Mitchell filter
 * 
 * These provide superior quality compared to simple bilinear interpolation.
 */

#include "SbImageResize.h"
#include <cstring>
#include <algorithm>
#include <cmath>
#include <cstdlib>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// High-quality resize internal structures and functions
// Adapted from resize.c (original simage implementation)

typedef struct {
  int pixel;
  float weight;
} CONTRIB;

typedef struct {
  int n;                /* number of contributors */
  CONTRIB* p;           /* pointer to list of contributions */
} CLIST;

typedef struct {
  int xsize;            /* horizontal size of the image in Pixels */
  int ysize;            /* vertical size of the image in Pixels */  
  int bpp;              /* bytes per pixel */
  unsigned char* data;  /* pointer to first scanline of image */
  int span;             /* byte offset between two scanlines */
} Image;

// High-quality filter functions

static float bell_filter(float t) 
{
  if (t < 0.0f) t = -t;
  if (t < 0.5f) return (0.75f - (t * t));
  if (t < 1.5f) {
    t = (t - 1.5f);
    return (0.5f * (t * t));
  }
  return (0.0f);
}
#define bell_support (1.5f)

static float B_spline_filter(float t)
{
  float tt;
  if (t < 0.0f) t = -t;
  if (t < 1.0f) {
    tt = t * t;
    return ((.5f * tt * t) - tt + (2.0f / 3.0f));
  } else if (t < 2.0f) {
    t = 2.0f - t;
    return ((1.0f / 6.0f) * (t * t * t));
  }
  return (0.0f);
}
#define B_spline_support (2.0f)

static float sinc(float x)
{
  x *= (float)M_PI;
  if (x != 0.0f) return (float)(sin(x) / x);
  return (1.0f);
}

static float Lanczos3_filter(float t)
{
  if (t < 0.0f) t = -t;
  if (t < 3.0f) return (sinc(t) * sinc(t/3.0f));
  return (0.0f);
}
#define Lanczos3_support (3.0f)

#define Mitchell_B (1.0f / 3.0f)
#define Mitchell_C (1.0f / 3.0f)
static float Mitchell_filter(float t)
{
  float tt = t * t;
  if (t < 0.0f) t = -t;
  if (t < 1.0f) {
    t = (((12.0f - 9.0f * Mitchell_B - 6.0f * Mitchell_C) * (t * tt))
         + ((-18.0f + 12.0f * Mitchell_B + 6.0f * Mitchell_C) * tt)
         + (6.0f - 2.0f * Mitchell_B));
    return (t / 6.0f);
  }
  else if (t < 2.0f) {
    t = (((-1.0f * Mitchell_B - 6.0f * Mitchell_C) * (t * tt))
         + ((6.0f * Mitchell_B + 30.0f * Mitchell_C) * tt)
         + ((-12.0f * Mitchell_B - 48.0f * Mitchell_C) * t)
         + (8.0f * Mitchell_B + 24.0f * Mitchell_C));
    return (t / 6.0f);
  }
  return (0.0f);
}
#define Mitchell_support (2.0f)

// Filter selection helper function
static void get_filter_function(SbImageResizeFilter filter, float (**filter_func)(float), float* support)
{
  switch (filter) {
    case SB_IMAGE_RESIZE_FILTER_BELL:
      *filter_func = bell_filter;
      *support = bell_support;
      break;
    case SB_IMAGE_RESIZE_FILTER_B_SPLINE:
      *filter_func = B_spline_filter;
      *support = B_spline_support;
      break;
    case SB_IMAGE_RESIZE_FILTER_LANCZOS3:
      *filter_func = Lanczos3_filter;
      *support = Lanczos3_support;
      break;
    case SB_IMAGE_RESIZE_FILTER_MITCHELL:
      *filter_func = Mitchell_filter;
      *support = Mitchell_support;
      break;
    default:
      // Default to Bell filter
      *filter_func = bell_filter;
      *support = bell_support;
      break;
  }
}

// Image utility functions

static void get_row(unsigned char* row, const Image* image, int y)
{
  if (y < 0 || y >= image->ysize) return;
  std::memcpy(row, image->data + (y * image->span), image->bpp * image->xsize);
}

static void get_column(unsigned char* column, const Image* image, int x)
{
  if (x < 0 || x >= image->xsize) return;
  
  int bpp = image->bpp;
  int ysize = image->ysize;
  int span = image->span;
  unsigned char* p = image->data + x * bpp;
  
  for (int i = 0; i < ysize; i++, p += span) {
    for (int j = 0; j < bpp; j++) {
      *column++ = p[j];
    }
  }
}

static void put_pixel(const Image* image, int x, int y, float* data)
{
  if (x < 0 || x >= image->xsize || y < 0 || y >= image->ysize) return;
  
  int bpp = image->bpp;
  unsigned char* p = image->data + image->span * y + x * bpp;
  
  for (int i = 0; i < bpp; i++) {
    float val = data[i];
    if (val < 0.0f) val = 0.0f;
    else if (val > 255.0f) val = 255.0f;
    *p++ = (unsigned char)val;
  }
}

static Image* new_image(int xsize, int ysize, int bpp, unsigned char* data)
{
  Image* img = new Image;
  img->xsize = xsize;
  img->ysize = ysize;
  img->bpp = bpp;
  img->span = xsize * bpp;
  img->data = data;
  if (data == nullptr) {
    img->data = new unsigned char[img->span * img->ysize];
  }
  return img;
}

static void free_image(Image* img)
{
  if (img) {
    delete img;
  }
}

// High-quality zoom function adapted from resize.c
static void zoom(Image* dst, const Image* src, float (*filterf)(float), float fwidth)
{
  CLIST* contrib;
  Image* tmp;
  float xscale, yscale;
  int i, j, k, b;
  int n;
  int left, right;
  float center;
  float width, fscale, weight;
  unsigned char* raster;
  float pixel[4];
  int bpp;
  unsigned char* dstptr;
  int dstxsize, dstysize;

  bpp = src->bpp;
  dstxsize = dst->xsize;
  dstysize = dst->ysize;

  // Create intermediate image to hold horizontal zoom
  tmp = new_image(dstxsize, src->ysize, dst->bpp, nullptr);
  xscale = (float)dstxsize / (float)src->xsize;
  yscale = (float)dstysize / (float)src->ysize;

  // Pre-calculate filter contributions for a row
  contrib = (CLIST*)std::calloc(dstxsize, sizeof(CLIST));
  if (xscale < 1.0f) {
    width = fwidth / xscale;
    fscale = 1.0f / xscale;
    for (i = 0; i < dstxsize; i++) {
      contrib[i].n = 0;
      contrib[i].p = (CONTRIB*)std::calloc((int)(width * 2 + 1), sizeof(CONTRIB));
      center = (float)i / xscale;
      left = (int)std::ceil(center - width);
      right = (int)std::floor(center + width);
      for (j = left; j <= right; j++) {
        weight = center - (float)j;
        weight = (*filterf)(weight / fscale) / fscale;
        if (j < 0) {
          n = -j;
        } else if (j >= src->xsize) {
          n = (src->xsize - j) + src->xsize - 1;
        } else {
          n = j;
        }
        k = contrib[i].n++;
        contrib[i].p[k].pixel = n * bpp;
        contrib[i].p[k].weight = weight;
      }
    }
  } else {
    for (i = 0; i < dstxsize; i++) {
      contrib[i].n = 0;
      contrib[i].p = (CONTRIB*)std::calloc((int)(fwidth * 2 + 1), sizeof(CONTRIB));
      center = (float)i / xscale;
      left = (int)std::ceil(center - fwidth);
      right = (int)std::floor(center + fwidth);
      for (j = left; j <= right; j++) {
        weight = center - (float)j;
        weight = (*filterf)(weight);
        if (j < 0) {
          n = -j;
        } else if (j >= src->xsize) {
          n = (src->xsize - j) + src->xsize - 1;
        } else {
          n = j;
        }
        k = contrib[i].n++;
        contrib[i].p[k].pixel = n * bpp;
        contrib[i].p[k].weight = weight;
      }
    }
  }

  // Apply filter to zoom horizontally from src to tmp
  raster = (unsigned char*)std::calloc(src->xsize, src->bpp);
  dstptr = tmp->data;

  for (k = 0; k < tmp->ysize; k++) {
    get_row(raster, src, k);
    for (i = 0; i < tmp->xsize; i++) {
      for (b = 0; b < bpp; b++) pixel[b] = 0.0f;
      for (j = 0; j < contrib[i].n; j++) {
        for (b = 0; b < bpp; b++) {
          pixel[b] += raster[contrib[i].p[j].pixel + b] * contrib[i].p[j].weight;
        }
      }
      put_pixel(tmp, i, k, pixel);
    }
  }
  std::free(raster);

  // Free the memory allocated for horizontal filter weights
  for (i = 0; i < tmp->xsize; i++) {
    std::free(contrib[i].p);
  }
  std::free(contrib);

  // Pre-calculate filter contributions for a column
  contrib = (CLIST*)std::calloc(dstysize, sizeof(CLIST));
  if (yscale < 1.0f) {
    width = fwidth / yscale;
    fscale = 1.0f / yscale;
    for (i = 0; i < dstysize; i++) {
      contrib[i].n = 0;
      contrib[i].p = (CONTRIB*)std::calloc((int)(width * 2 + 1), sizeof(CONTRIB));
      center = (float)i / yscale;
      left = (int)std::ceil(center - width);
      right = (int)std::floor(center + width);
      for (j = left; j <= right; j++) {
        weight = center - (float)j;
        weight = (*filterf)(weight / fscale) / fscale;
        if (j < 0) {
          n = -j;
        } else if (j >= tmp->ysize) {
          n = (tmp->ysize - j) + tmp->ysize - 1;
        } else {
          n = j;
        }
        k = contrib[i].n++;
        contrib[i].p[k].pixel = n * bpp;
        contrib[i].p[k].weight = weight;
      }
    }
  } else {
    for (i = 0; i < dstysize; i++) {
      contrib[i].n = 0;
      contrib[i].p = (CONTRIB*)std::calloc((int)(fwidth * 2 + 1), sizeof(CONTRIB));
      center = (float)i / yscale;
      left = (int)std::ceil(center - fwidth);
      right = (int)std::floor(center + fwidth);
      for (j = left; j <= right; j++) {
        weight = center - (float)j;
        weight = (*filterf)(weight);
        if (j < 0) {
          n = -j;
        } else if (j >= tmp->ysize) {
          n = (tmp->ysize - j) + tmp->ysize - 1;
        } else {
          n = j;
        }
        k = contrib[i].n++;
        contrib[i].p[k].pixel = n * bpp;
        contrib[i].p[k].weight = weight;
      }
    }
  }

  // Apply filter to zoom vertically from tmp to dst
  raster = (unsigned char*)std::calloc(tmp->ysize, tmp->bpp);
  for (k = 0; k < dstxsize; k++) {
    get_column(raster, tmp, k);
    dstptr = dst->data + k * bpp;
    for (i = 0; i < dstysize; i++) {
      for (b = 0; b < bpp; b++) pixel[b] = 0.0f;
      for (j = 0; j < contrib[i].n; ++j) {
        for (b = 0; b < bpp; b++) {
          pixel[b] += raster[contrib[i].p[j].pixel + b] * contrib[i].p[j].weight;
        }
      }
      put_pixel(dst, k, i, pixel);
      dstptr += bpp * dstxsize;
    }
  }

  std::free(raster);

  // Free the memory allocated for vertical filter weights
  for (i = 0; i < dstysize; ++i) {
    std::free(contrib[i].p);
  }
  std::free(contrib);
  delete[] tmp->data;
  free_image(tmp);
}

// Forward declarations
static void filter_resize_2d(const unsigned char* src, unsigned char* dest,
                            int width, int height, int components,
                            int newwidth, int newheight, SbImageResizeFilter filter);

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

// High-quality resize implementation using Bell filter from original simage
// Replaces the previous stub that fell back to bilinear interpolation
static void high_quality_resize_2d(const unsigned char* src, unsigned char* dest,
                                  int width, int height, int components,
                                  int newwidth, int newheight)
{
  // Use Bell filter as default for backward compatibility
  filter_resize_2d(src, dest, width, height, components, newwidth, newheight, SB_IMAGE_RESIZE_FILTER_BELL);
}

// Generalized high-quality resize function with filter selection
static void filter_resize_2d(const unsigned char* src, unsigned char* dest,
                            int width, int height, int components,
                            int newwidth, int newheight, SbImageResizeFilter filter)
{
  float (*filter_func)(float);
  float support;
  get_filter_function(filter, &filter_func, &support);
  
  Image* srcimg = new_image(width, height, components, const_cast<unsigned char*>(src));
  Image* dstimg = new_image(newwidth, newheight, components, dest);
  
  // Apply the selected filter
  zoom(dstimg, srcimg, filter_func, support);
  
  // Clean up (don't delete the data as it's owned by caller)
  srcimg->data = nullptr;  // Don't delete src data - it belongs to caller
  dstimg->data = nullptr;  // Don't delete dest data - it belongs to caller
  free_image(srcimg);
  free_image(dstimg);
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

// New filter-based API functions

unsigned char* SbImageResize_resize2D_filter(const unsigned char* src,
                                            int width, int height, int components,
                                            int newwidth, int newheight,
                                            SbImageResizeFilter filter)
{
  if (!src || width <= 0 || height <= 0 || components <= 0 || 
      newwidth <= 0 || newheight <= 0) {
    return nullptr;
  }
  
  size_t dest_size = (size_t)newwidth * newheight * components;
  unsigned char* dest = new unsigned char[dest_size];
  
  if (!SbImageResize_resize2D_inplace_filter(src, dest, width, height, components, 
                                            newwidth, newheight, filter)) {
    delete[] dest;
    return nullptr;
  }
  
  return dest;
}

bool SbImageResize_resize2D_inplace_filter(const unsigned char* src, unsigned char* dest,
                                          int width, int height, int components,
                                          int newwidth, int newheight,
                                          SbImageResizeFilter filter)
{
  if (!src || !dest || width <= 0 || height <= 0 || components <= 0 || 
      newwidth <= 0 || newheight <= 0) {
    return false;
  }
  
  filter_resize_2d(src, dest, width, height, components, newwidth, newheight, filter);
  return true;
}