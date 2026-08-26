/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the conditions in the project
 * license are met.
\**************************************************************************/

#include "SbPngImageHandler.h"

#include "../../external/lodepng.h"

#include <cstdlib>
#include <limits>
#include <string>

const char *
SbPngImageHandler::getFormatName() const
{
  return "png";
}

const char *
SbPngImageHandler::getDescription() const
{
  return "PNG image format using bundled lodepng";
}

std::vector<std::string>
SbPngImageHandler::getExtensions() const
{
  return {"png"};
}

unsigned char *
SbPngImageHandler::readImage(const char * filename, int * width, int * height,
                             int * components)
{
  setError("");
  if (width) *width = 0;
  if (height) *height = 0;
  if (components) *components = 0;
  if (!filename) {
    setError("Null filename provided for PNG read");
    return nullptr;
  }

  unsigned char * data = nullptr;
  unsigned imagewidth = 0;
  unsigned imageheight = 0;
  const unsigned error =
    lodepng_decode32_file(&data, &imagewidth, &imageheight, filename);
  if (error != 0) {
    setError(lodepng_error_text(error));
    std::free(data);
    return nullptr;
  }
  if (imagewidth > static_cast<unsigned>(std::numeric_limits<int>::max()) ||
      imageheight > static_cast<unsigned>(std::numeric_limits<int>::max())) {
    std::free(data);
    setError("PNG dimensions exceed the supported integer range");
    return nullptr;
  }

  if (width) *width = static_cast<int>(imagewidth);
  if (height) *height = static_cast<int>(imageheight);
  if (components) *components = 4;
  return data;
}

bool
SbPngImageHandler::saveImage(const char * filename,
                             const unsigned char * imagedata,
                             int width, int height, int components)
{
  setError("");
  if (!filename || !imagedata || width <= 0 || height <= 0 ||
      components < 1 || components > 4) {
    setError("Invalid parameters for PNG save");
    return false;
  }

  static const LodePNGColorType color_types[] = {
    LCT_GREY, LCT_GREY_ALPHA, LCT_RGB, LCT_RGBA
  };
  const unsigned error = lodepng_encode_file(
    filename, imagedata, static_cast<unsigned>(width),
    static_cast<unsigned>(height), color_types[components - 1], 8);
  if (error != 0) {
    setError(lodepng_error_text(error));
    return false;
  }
  return true;
}

void
SbPngImageHandler::freeImageData(unsigned char * imagedata)
{
  std::free(imagedata);
}

void
SbPngImageHandler::getVersion(int * major, int * minor, int * micro) const
{
  // LodePNG identifies releases by an eight-digit YYYYMMDD string.
  int year = 0;
  int month = 0;
  int day = 0;
  if (LODEPNG_VERSION_STRING) {
    const std::string version(LODEPNG_VERSION_STRING);
    if (version.size() == 8) {
      year = std::atoi(version.substr(0, 4).c_str());
      month = std::atoi(version.substr(4, 2).c_str());
      day = std::atoi(version.substr(6, 2).c_str());
    }
  }
  if (major) *major = year;
  if (minor) *minor = month;
  if (micro) *micro = day;
}
