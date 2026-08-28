/**************************************************************************\
 * Copyright (c) Kongsberg Oil & Gas Technologies AS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the conditions in the project
 * license are met.
\**************************************************************************/

#ifndef OBOL_SBPNGIMAGEHANDLER_H
#define OBOL_SBPNGIMAGEHANDLER_H

#include "SbImageFormatHandler.h"

class SbPngImageHandler final : public SbImageFormatHandler {
public:
  const char * getFormatName() const override;
  const char * getDescription() const override;
  const std::vector<std::string> & getExtensions() const override;

  unsigned char * readImage(const char * filename, int * width, int * height,
                            int * components) override;
  bool saveImage(const char * filename, const unsigned char * imagedata,
                 int width, int height, int components) override;
  void freeImageData(unsigned char * imagedata) override;
  void getVersion(int * major, int * minor, int * micro) const override;
};

#endif // OBOL_SBPNGIMAGEHANDLER_H
